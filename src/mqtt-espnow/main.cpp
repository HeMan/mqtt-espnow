#include <stdio.h>
#include <cstring> // for memcmp
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_flash.h"

#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_err.h"

#include "esp_now.h"
#include "esp_wifi.h"

#include "mqtt_client.h"

#include "ethernetmanager.hpp"

#define ETH_MDC_PIN 23
#define ETH_MDIO_PIN 18
#define ETH_CLK_MODE ETH_CLOCK_GPIO17_OUT

constexpr uint8_t s_example_broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#define IS_BROADCAST_ADDR(addr) (std::memcmp(addr, s_example_broadcast_mac, ESP_NOW_ETH_ALEN) == 0)

constexpr const char *TAG = "mqtt-espnow";
void init_system(void)
{
    auto ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
}


static void example_espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    auto *mac_addr = recv_info->src_addr;
    auto *des_addr = recv_info->des_addr;

    if (mac_addr == nullptr || data == nullptr || len <= 0)
    {
        ESP_LOGE(TAG, "Receive cb arg error");
        return;
    }

    if (IS_BROADCAST_ADDR(des_addr))
    {
        ESP_LOGD(TAG, "Receive broadcast ESPNOW data");
    }
    else
    {
        ESP_LOGD(TAG, "Receive unicast ESPNOW data");
    }
}

void init_espnow(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(example_espnow_recv_cb));

    ESP_LOGI("espnow_init", "ESP-NOW initialized");
}

static const char *MQTT_TAG = "mqtt_client";

esp_mqtt_client_handle_t mqtt_client = nullptr;

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_CONNECTED");
        esp_mqtt_client_subscribe(event->client, "/espnow/test", 0);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(MQTT_TAG, "Other event id:%d", event->event_id);
        break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    mqtt_event_handler_cb(static_cast<esp_mqtt_event_handle_t>(event_data));
}

void init_mqtt(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = "mqtt://broker.hivemq.com";

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt_client, MQTT_EVENT_ANY, mqtt_event_handler, nullptr));
    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));
}

extern "C" void app_main(void)
{
    int msg_id = 0;
    init_system();
    EthernetManager eth_manager(ETH_MDC_PIN, ETH_MDIO_PIN);
    printf("System initialized. Waiting for Ethernet connection...\n");
    init_espnow();
    printf("ESP-NOW initialized. Waiting for connection...\n");


    vTaskDelay(3000 / portTICK_PERIOD_MS);
    // Log DNS server(s) after Ethernet got IP
    esp_netif_dns_info_t dns_info;
    if (esp_netif_get_dns_info(esp_netif_get_handle_from_ifkey("ETH_DEF"), ESP_NETIF_DNS_MAIN, &dns_info) == ESP_OK)
    {
        if (dns_info.ip.type == ESP_IPADDR_TYPE_V4)
        {
            ESP_LOGI(TAG, "DNS Server: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        }
        else if (dns_info.ip.type == ESP_IPADDR_TYPE_V6)
        {
            ESP_LOGI(TAG, "DNS Server: " IPV6STR, IPV62STR(dns_info.ip.u_addr.ip6));
        }
    }
    else
    {
        ESP_LOGI(TAG, "No DNS server info available");
    }
    init_mqtt();
    printf("MQTT client initialized. Waiting for connection...\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    printf("This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    uint32_t flash_size = 0;
    esp_flash_get_size(nullptr, &flash_size);
    printf("%luMB %s flash\n", flash_size / (1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Hello world!\n");
    while (true)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        printf("Running...\n");
        msg_id = esp_mqtt_client_publish(mqtt_client, "/topic/qos1", "data_3", 0, 1, 0);
        ESP_LOGI("mqtt-publish", "sent publish successful, msg_id=%d", msg_id);
    }

    /*for (int i = 10; i >= 0; i--)
    {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();*/
}
