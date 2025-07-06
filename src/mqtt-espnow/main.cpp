#include <stdio.h>
#include <cstring> // for memcmp
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_flash.h"

#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_err.h"

#include "ethernetmanager.hpp"
#include "espnowmanager.hpp"
#include "mqttmanager.hpp"

#define ETH_MDC_PIN 23
#define ETH_MDIO_PIN 18
#define ETH_CLK_MODE ETH_CLOCK_GPIO17_OUT

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

extern "C" void app_main(void)
{
    init_system();
    EthernetManager eth_manager(ETH_MDC_PIN, ETH_MDIO_PIN);
    printf("System initialized. Waiting for Ethernet connection...\n");
    EspNowManager espnow_manager;
    printf("ESP-NOW initialized. Waiting for connection...\n");
    MqttManager mqtt_manager("mqtt://broker.hivemq.com");

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
    printf("MQTT client initialized. Waiting for connection...\n");
    mqtt_manager.connect();

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
        auto msg_id = mqtt_manager.publish("/topic/qos0", "data_1");
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