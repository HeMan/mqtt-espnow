#pragma once

#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <cstring>

constexpr uint8_t s_example_broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#define IS_BROADCAST_ADDR(addr) (std::memcmp(addr, s_example_broadcast_mac, ESP_NOW_ETH_ALEN) == 0)

class EspNowManager
{
public:
    EspNowManager(const char *tag = "EspNowManager")
        : TAG(tag)
    {

        // All initialization is handled in the constructor; init() is not needed.
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_ERROR_CHECK(esp_now_init());
        ESP_ERROR_CHECK(esp_now_register_recv_cb(&EspNowManager::recv_cb_static));
        instance() = this;
        ESP_LOGI(TAG, "ESP-NOW initialized");
    }

protected:
    virtual void on_receive(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
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

private:
    static void recv_cb_static(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
    {
        if (instance())
            instance()->on_receive(recv_info, data, len);
    }

    static EspNowManager *&instance()
    {
        static EspNowManager *inst = nullptr;
        return inst;
    }

    const char *TAG;
};
