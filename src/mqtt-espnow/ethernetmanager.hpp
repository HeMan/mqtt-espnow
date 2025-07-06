#include <expected>
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_eth_phy.h"
#include "esp_eth_mac.h"
#include "esp_eth_netif_glue.h"
#include "esp_eth_driver.h"
#include "esp_netif_types.h"
#include "esp_netif_ip_addr.h"
#include "esp_netif_defaults.h"

#include "esp_netif.h"
#include "driver/gpio.h"
#include <lwip/ip6_addr.h>

#define RETURN_IF_ERROR(expr)             \
    if (auto err = (expr); err != ESP_OK) \
        return std::unexpected(err);

class EthernetManager
{
public:
    EthernetManager(int eth_mdc_pin, int eth_mdio_pin)
        : eth_netif(nullptr), eth_handle(nullptr)
    {
        if (auto result = init(eth_mdc_pin, eth_mdio_pin); !result.has_value())
        {
            ESP_LOGE(TAG, "Failed to initialize EthernetManager: %s", esp_err_to_name(result.error()));
        }
        ESP_LOGI(TAG, "EthernetManager initialized successfully");
    }
    ~EthernetManager()
    {
        if (eth_handle)
        {
            esp_eth_stop(eth_handle);
            esp_eth_driver_uninstall(eth_handle);
        }
        if (eth_netif)
        {
            esp_netif_destroy(eth_netif);
        }
    }

    std::expected<void, esp_err_t> init(int eth_mdc_pin, int eth_mdio_pin)
    {
        esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
        eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
        eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
        eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
        esp32_emac_config.smi_gpio.mdc_num = eth_mdc_pin;
        esp32_emac_config.smi_gpio.mdio_num = eth_mdio_pin;
        esp32_emac_config.clock_config.rmii.clock_mode = EMAC_CLK_OUT;
        esp32_emac_config.clock_config.rmii.clock_gpio = EMAC_CLK_OUT_180_GPIO;
        eth_netif = esp_netif_new(&cfg);

        RETURN_IF_ERROR(esp_event_handler_instance_register(ETH_EVENT, ESP_EVENT_ANY_ID, &EthernetManager::event_handler_static, this, nullptr));
        RETURN_IF_ERROR(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &EthernetManager::event_handler_static, this, nullptr));
        RETURN_IF_ERROR(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_GOT_IP6, &EthernetManager::event_handler_static, this, nullptr));

        auto *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
        auto *phy = esp_eth_phy_new_lan87xx(&phy_config);

        esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
        RETURN_IF_ERROR(esp_eth_driver_install(&eth_config, &eth_handle));
        RETURN_IF_ERROR(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));
        RETURN_IF_ERROR(esp_eth_start(eth_handle));
        RETURN_IF_ERROR(esp_netif_set_hostname(eth_netif, "mqtt-espnow"));
        RETURN_IF_ERROR(esp_netif_create_ip6_linklocal(eth_netif));
        const TickType_t start_tick = xTaskGetTickCount();
        while (!got_global_ipv6 && (xTaskGetTickCount() - start_tick < pdMS_TO_TICKS(3000)))
        {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        if (!got_global_ipv6)
        {
            return std::unexpected(ESP_ERR_TIMEOUT);
        }
        return {};
    }

private:
    static constexpr const char *TAG = "EthernetManager";
    esp_netif_t *eth_netif = nullptr;
    esp_eth_handle_t eth_handle = nullptr;
    bool got_global_ipv6 = false;

    static void event_handler_static(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
    {
        auto *self = static_cast<EthernetManager *>(arg);
        if (event_base == ETH_EVENT)
        {
            self->eth_event_handler(event_base, event_id, event_data);
            return;
        }
        if (event_base == IP_EVENT)
        {
            if (event_id == IP_EVENT_ETH_GOT_IP)
            {
                self->got_ip_event_handler(event_base, event_id, event_data);
            }
            else if (event_id == IP_EVENT_GOT_IP6)
            {
                self->got_ip6_event_handler(event_base, event_id, event_data);
            }
            return;
        }
        // Handle other event bases if necessary
        ESP_LOGW(TAG, "Unhandled event base: %s, event_id: %d", event_base, event_id);
    }

    void eth_event_handler(esp_event_base_t event_base, int32_t event_id, void *event_data)
    {
        if (event_id == ETHERNET_EVENT_CONNECTED)
        {
            ESP_LOGI(TAG, "Ethernet Link Up");
        }
        else if (event_id == ETHERNET_EVENT_DISCONNECTED)
        {
            ESP_LOGI(TAG, "Ethernet Link Down");
        }
        else if (event_id == ETHERNET_EVENT_START)
        {
            ESP_LOGI(TAG, "Ethernet Started");
        }
        else if (event_id == ETHERNET_EVENT_STOP)
        {
            ESP_LOGI(TAG, "Ethernet Stopped");
        }
    }

    void got_ip_event_handler(esp_event_base_t event_base, int32_t event_id, void *event_data)
    {
        auto *event = static_cast<ip_event_got_ip_t *>(event_data);
        ESP_LOGI(TAG, "Got IP Address:" IPSTR, IP2STR(&event->ip_info.ip));
    }

    void got_ip6_event_handler(esp_event_base_t event_base, int32_t event_id, void *event_data)
    {
        auto *event = static_cast<ip_event_got_ip6_t *>(event_data);
        ESP_LOGI(TAG, "Got IPv6 Address: " IPV6STR, IPV62STR(event->ip6_info.ip));
        if (!ip6_addr_islinklocal(&event->ip6_info.ip))
        {
            got_global_ipv6 = true;
        }
    }
};
