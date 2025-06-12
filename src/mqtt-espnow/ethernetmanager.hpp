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

// Removed global TAG, will add as private member in class


class EthernetManager
{
public:
    EthernetManager(int eth_mdc_pin, int eth_mdio_pin)
        : eth_netif(nullptr), eth_handle(nullptr)
    {
        init(eth_mdc_pin, eth_mdio_pin);
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
    
    void init(int eth_mdc_pin, int eth_mdio_pin)
    {
        esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
        eth_netif = esp_netif_new(&cfg);

        ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &EthernetManager::eth_event_handler_static, this));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &EthernetManager::got_ip_event_handler_static, this));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_GOT_IP6, &EthernetManager::got_ip6_event_handler_static, this));

        eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
        eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
        eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
        esp32_emac_config.smi_gpio.mdc_num = eth_mdc_pin;
        esp32_emac_config.smi_gpio.mdio_num = eth_mdio_pin;
        esp32_emac_config.clock_config.rmii.clock_mode = EMAC_CLK_OUT;
        esp32_emac_config.clock_config.rmii.clock_gpio = EMAC_CLK_OUT_180_GPIO;

        auto *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
        auto *phy = esp_eth_phy_new_lan87xx(&phy_config);

        esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
        ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config, &eth_handle));
        ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));
        ESP_ERROR_CHECK(esp_eth_start(eth_handle));
        ESP_ERROR_CHECK(esp_netif_set_hostname(eth_netif, "mqtt-espnow"));
        ESP_ERROR_CHECK(esp_netif_create_ip6_linklocal(eth_netif));
    }

private:
    static constexpr const char *TAG = "EthernetManager";
    esp_netif_t *eth_netif = nullptr;
    esp_eth_handle_t eth_handle = nullptr;

    static void eth_event_handler_static(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
    {
        auto *self = static_cast<EthernetManager *>(arg);
        self->eth_event_handler(event_base, event_id, event_data);
    }

    static void got_ip_event_handler_static(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
    {
        auto *self = static_cast<EthernetManager *>(arg);
        self->got_ip_event_handler(event_base, event_id, event_data);
    }

    static void got_ip6_event_handler_static(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
    {
        auto *self = static_cast<EthernetManager *>(arg);
        self->got_ip6_event_handler(event_base, event_id, event_data);
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
    }
};
