// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#include "mqtt.hpp"
#include "mqtt_client.h"

#include <esp_log.h>
#include <cstring>

#define TAG "mqtt"


MQTTClient::MQTTClient()
{
  std::memset(&_config, 0, sizeof(esp_mqtt_client_config_t));
  _config.user_context = this;
  // we *must* give a host, otherwise the
  // next call bails out.
  _config.host = "127.0.0.1";
  _client = esp_mqtt_client_init(&_config);
  esp_mqtt_client_register_event(
    _client,
    esp_mqtt_event_id_t(ESP_EVENT_ANY_ID),
    MQTTClient::s_handle_mqtt_event, this);
  esp_mqtt_client_start(_client);

  ESP_ERROR_CHECK(esp_event_handler_instance_register(CONFIG_EVENTS, CONFIG_EVENT_MQTT_HOST, MQTTClient::s_config_event_handler, this, NULL));
}

int MQTTClient::publish(const char *topic, const char *data, int len, int qos,
                        int retain) {
  return esp_mqtt_client_publish(_client, topic, data, len, qos, retain);
}

void MQTTClient::s_handle_mqtt_event(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  static_cast<MQTTClient*>(event_handler_arg)->handle_mqtt_event(event_base, event_id, event_data);
}


void MQTTClient::handle_mqtt_event(esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  const auto event = esp_mqtt_event_handle_t(event_data);
  // your_context_t *context = event->context;
  switch (event_id) {
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
    ESP_LOGI(TAG, "Other event id:%d", event_id);
    break;
  }
}


void MQTTClient::s_config_event_handler(void *handler_args,
                                        esp_event_base_t event_base, int32_t event_id,
                                        void *event_data)
{
  static_cast<MQTTClient*>(handler_args)->config_event_handler(event_base, config_events_t(event_id), event_data);
}


void MQTTClient::config_event_handler(esp_event_base_t base, config_events_t id, void* event_data)
{
  switch(id)
  {
  case CONFIG_EVENT_MQTT_HOST:
    {
      const auto config = static_cast<const config_event_mqtt_host_t*>(event_data);
      ESP_LOGE(TAG, "CONFIG_EVENT_MQTT_HOST: %s", config->host);
      _config.host = config->host;
      esp_mqtt_set_config(_client, &_config);
    }
    break;
  }
}
