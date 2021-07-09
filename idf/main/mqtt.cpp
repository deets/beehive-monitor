// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#include "mqtt.hpp"
#include "mqtt_client.h"

#include <esp_log.h>
#include <cstring>

#define TAG "mqtt"

namespace {
esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
  return static_cast<MQTTClient*>(event->user_context)->handle_event(event);
}

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGI(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(esp_mqtt_event_handle_t(event_data));
}

}

MQTTClient::MQTTClient(const char *host)
{
  esp_mqtt_client_config_t config;
  std::memset(&config, 0, sizeof(esp_mqtt_client_config_t));
  config.host = host;
  config.user_context = this;
  _client = esp_mqtt_client_init(&config);
  esp_mqtt_client_register_event(_client, esp_mqtt_event_id_t(ESP_EVENT_ANY_ID), mqtt_event_handler, _client);
  esp_mqtt_client_start(_client);
}

int MQTTClient::publish(const char *topic, const char *data, int len, int qos,
                        int retain) {
  return esp_mqtt_client_publish(_client, topic, data, len, qos, retain);
}


esp_err_t MQTTClient::handle_event(esp_mqtt_event_handle_t event)
{
  // your_context_t *context = event->context;
  switch (event->event_id) {
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
    break;
  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
    break;
  case MQTT_EVENT_SUBSCRIBED:
    ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED");
    break;
  case MQTT_EVENT_UNSUBSCRIBED:
    ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
    break;
  case MQTT_EVENT_PUBLISHED:
    ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
    break;
  case MQTT_EVENT_DATA:
    ESP_LOGI(TAG, "MQTT_EVENT_DATA");
    printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
    printf("DATA=%.*s\r\n", event->data_len, event->data);
    break;
  case MQTT_EVENT_ERROR:
    ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
    break;
  default:
    ESP_LOGI(TAG, "Other event id:%d", event->event_id);
    break;
  }
  return ESP_OK;
}
