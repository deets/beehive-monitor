// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#pragma once

#include "beehive_events.hpp"

#include <mqtt_client.h>

#include <set>

namespace beehive::mqtt {

class MQTTClient
{
public:
  MQTTClient(size_t counter);

  int publish(const char *topic, const char *data, int len=0, int qos=0, int retain=0);
private:

  void publish_message_backlog_count();

  static void s_handle_mqtt_event(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
  void handle_mqtt_event(esp_event_base_t event_base, int32_t event_id, void *event_data);

  static void s_config_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data);
  void config_event_handler(esp_event_base_t base, beehive::events::config::config_events_t id, void* event_data);

  static void s_sensor_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data);
  void sensor_event_handler(esp_event_base_t base, beehive::events::sensors::sensor_events_t id, void* event_data);

  esp_mqtt_client_config_t _config;
  esp_mqtt_client_handle_t _client;

  char _client_id[200];
  char _hostname[200];

  size_t _counter;

  std::set<int> _published_messages;
};

} // namespace beehive::mqtt
