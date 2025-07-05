#pragma once

#include "mqtt_client.h"
#include "esp_log.h"
#include <string>
#include <expected>

class MqttManager
{
public:
    MqttManager(const std::string &broker_uri = "mqtt://broker.hivemq.com")
        : broker_uri_(broker_uri), mqtt_client_(nullptr)
    {
    }

    std::expected<void, esp_err_t> connect()
    {
        esp_mqtt_client_config_t mqtt_cfg = {};
        mqtt_cfg.broker.address.uri = broker_uri_.c_str();

        mqtt_client_ = esp_mqtt_client_init(&mqtt_cfg);
        if (!mqtt_client_)
        {
            ESP_LOGE(MQTT_TAG, "Failed to initialize MQTT client");
            return std::unexpected(ESP_FAIL);
        }

        esp_err_t err = esp_mqtt_client_register_event(mqtt_client_, MQTT_EVENT_ANY, &MqttManager::event_handler, this);
        if (err != ESP_OK)
        {
            ESP_LOGE(MQTT_TAG, "Failed to register MQTT event handler: %d", err);
            esp_mqtt_client_destroy(mqtt_client_);
            mqtt_client_ = nullptr;
            return std::unexpected(err);
        }

        err = esp_mqtt_client_start(mqtt_client_);
        if (err != ESP_OK)
        {
            ESP_LOGE(MQTT_TAG, "Failed to start MQTT client: %d", err);
            esp_mqtt_client_destroy(mqtt_client_);
            mqtt_client_ = nullptr;
            return std::unexpected(err);
        }

        ESP_LOGI(MQTT_TAG, "MQTT client initialized with broker URI: %s", broker_uri_.c_str());
        return {};
    }

    ~MqttManager()
    {
        if (mqtt_client_)
        {
            esp_mqtt_client_stop(mqtt_client_);
            esp_mqtt_client_destroy(mqtt_client_);
        }
    }
    std::expected<uint32_t, esp_err_t> publish(const std::string &topic, const std::string &message, int qos = 0, bool retain = false)
    {
        if (!mqtt_client_)
        {
            ESP_LOGE(MQTT_TAG, "MQTT client not initialized");
            return std::unexpected(ESP_FAIL);
        }

        auto msg_id = esp_mqtt_client_publish(mqtt_client_, topic.c_str(), message.c_str(), message.size(), qos, retain);
        if (msg_id < 0)
        {
            ESP_LOGE(MQTT_TAG, "Failed to publish message to topic %s", topic.c_str());
            return std::unexpected(ESP_FAIL);
        }

        ESP_LOGI(MQTT_TAG, "Published message to topic %s, msg_id=%d", topic.c_str(), msg_id);
        return msg_id;
    }
    esp_mqtt_client_handle_t client() const { return mqtt_client_; }

private:
    static constexpr const char *MQTT_TAG = "mqtt_client";
    std::string broker_uri_;
    esp_mqtt_client_handle_t mqtt_client_;

    static void event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
    {
        auto *self = static_cast<MqttManager *>(handler_args);
        self->handle_event(static_cast<esp_mqtt_event_handle_t>(event_data));
    }

    void handle_event(esp_mqtt_event_handle_t event)
    {
        switch (event->event_id)
        {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(MQTT_TAG, "MQTT_EVENT_CONNECTED");
            esp_mqtt_client_subscribe(event->client, "/espnow/test", 0);
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
    }
};
