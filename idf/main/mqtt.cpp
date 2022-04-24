// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#include "mqtt.hpp"
#include "appstate.hpp"
#include "beehive_events.hpp"
#include "roland.hpp"
#include "mqtt_client.h"
#include "util.hpp"

#include <esp_log.h>
#include <cstring>
#include <sstream>
#include <iomanip>

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#define TAG "mqtt"

namespace beehive::mqtt {

namespace {

const auto QOS = 1;
const auto RETAIN = 0;
const auto SEPARATOR = ";";

void native_publish(
  const size_t counter,
  const std::vector<events::sensors::sht3xdis_value_t> &readings,
  std::function < void(const char *topic, const char *data, int len, int qos, int retain)> publish
  )
{
  std::stringstream topic;
  size_t readings_count = 0;

  topic << "beehive/" << beehive::appstate::system_name();

  std::stringstream ss;
  ss << counter << "," << beehive::util::isoformat() << SEPARATOR;

  for(const auto& entry : readings)
  {
    ss << std::hex << std::setw(2) << std::setfill('0') << int(entry.busno);
    ss << std::hex << std::setw(2) << std::setfill('0') << int(entry.address) << ",";
    ss << "T" << std::hex << std::setw(4) << std::setfill('0') << entry.raw_temperature << ",";
    ss << "H" << std::hex << std::setw(4) << std::setfill('0') << entry.raw_humidity;
    if(++readings_count < readings.size())
    {
      ss << SEPARATOR;
    }
  }
  const auto payload = ss.str();
  const auto topic_ = topic.str();
  publish(topic_.c_str(), payload.c_str(), payload.size(), QOS, RETAIN);
}

}
MQTTClient::MQTTClient(size_t counter)
  : _counter(counter)
{
  std::memset(&_config, 0, sizeof(esp_mqtt_client_config_t));
  _config.user_context = this;
  std::strncpy(_client_id, beehive::appstate::system_name().c_str(), sizeof(_client_id));
  // we *must* give a host, otherwise the
  // next call bails out.
  std::strncpy(_hostname, "127.0.0.1", sizeof(_hostname));
  _config.client_id = _client_id;
  _config.host = _hostname;
  _client = esp_mqtt_client_init(&_config);
  esp_mqtt_client_register_event(
    _client,
    esp_mqtt_event_id_t(ESP_EVENT_ANY_ID),
    MQTTClient::s_handle_mqtt_event, this);
  esp_mqtt_client_start(_client);

  ESP_ERROR_CHECK(esp_event_handler_instance_register(CONFIG_EVENTS, esp_mqtt_event_id_t(ESP_EVENT_ANY_ID), MQTTClient::s_config_event_handler, this, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(SENSOR_EVENTS, beehive::events::sensors::SHT3XDIS_READINGS, MQTTClient::s_sensor_event_handler, this, NULL));
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
    ESP_LOGD(TAG, "MQTT_EVENT_CONNECTED");
    break;
  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGD(TAG, "MQTT_EVENT_DISCONNECTED");
    break;
  case MQTT_EVENT_SUBSCRIBED:
    ESP_LOGD(TAG, "MQTT_EVENT_SUBSCRIBED");
    break;
  case MQTT_EVENT_UNSUBSCRIBED:
    ESP_LOGD(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
    break;
  case MQTT_EVENT_PUBLISHED:
    ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
    // The MQTT_EVENTS are scoped to the client, so
    // I create this forwarding.
    _published_messages.erase(event->msg_id);
    beehive::events::mqtt::published(_published_messages.size());
    break;
  case MQTT_EVENT_DATA:
    ESP_LOGD(TAG, "MQTT_EVENT_DATA");
    printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
    printf("DATA=%.*s\r\n", event->data_len, event->data);
    break;
  case MQTT_EVENT_ERROR:
    ESP_LOGD(TAG, "MQTT_EVENT_ERROR");
    break;
  default:
    ESP_LOGD(TAG, "Other event id:%d", event_id);
    break;
  }
}


void MQTTClient::s_config_event_handler(void *handler_args,
                                        esp_event_base_t event_base, int32_t event_id,
                                        void *event_data)
{
  static_cast<MQTTClient*>(handler_args)->config_event_handler(event_base, beehive::events::config::config_events_t(event_id), event_data);
}


void MQTTClient::config_event_handler(esp_event_base_t base, beehive::events::config::config_events_t id, void* event_data)
{
  switch(id)
  {
  case beehive::events::config::MQTT_HOST:
    {
      const auto hostname = beehive::events::config::mqtt::hostname(id, event_data);
      if(hostname)
      {
	std::strncpy(_hostname, hostname->c_str(), sizeof(_hostname));
	ESP_LOGD(TAG, "MQTT_HOST: %s", _hostname);
	esp_mqtt_set_config(_client, &_config);
      }
    }
    break;
  case beehive::events::config::SYSTEM_NAME:
    std::strncpy(_client_id, beehive::appstate::system_name().c_str(), sizeof(_client_id));
    ESP_LOGD(TAG, "MQTT_CLIENT_ID: %s", _client_id);
    // It appears as if this re-setting of the client id does only take effect
    // after a reboot. I'm ok with this as in normal operation, we'd do that through
    // deep sleep anyway.
    esp_mqtt_set_config(_client, &_config);
    break;
    // All ignored
  case beehive::events::config::SLEEPTIME:
    break;
  }
}

void MQTTClient::s_sensor_event_handler(void *handler_args,
                                        esp_event_base_t base, int32_t id,
                                        void *event_data) {

  static_cast<MQTTClient*>(handler_args)->sensor_event_handler(base, beehive::events::sensors::sensor_events_t(id), event_data);
}

void MQTTClient::sensor_event_handler(esp_event_base_t base, beehive::events::sensors::sensor_events_t id, void* event_data)
{
  const auto readings = beehive::events::sensors::receive_readings(id, event_data);
  if(readings)
  {
    native_publish(++_counter, *readings,
		    [this]
		    (const char *topic, const char *data, int len, int qos, int retain) {
		      const auto message_id = publish(topic, data, len, qos, retain);
		      ESP_LOGD(TAG, "beehive published message %i", message_id);
		      _published_messages.insert(message_id);
                      beehive::events::mqtt::published(_published_messages.size());
		    });

    roland::publish(_counter, *readings,
		    [this]
		    (const char *topic, const char *data, int len, int qos, int retain) {
		      const auto message_id = publish(topic, data, len, qos, retain);
		      ESP_LOGD(TAG, "roland published message %i", message_id);
		      _published_messages.insert(message_id);
                      beehive::events::mqtt::published(_published_messages.size());
		    });
    for(const auto message_id : _published_messages)
    {
      ESP_LOGD(TAG, "id %i", message_id);
    }
  }
}
} // namespace beehive::mqtt
