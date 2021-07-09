// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#pragma once

#include <mqtt_client.h>

class MQTTClient
{
public:
  MQTTClient(const char* host);
  esp_err_t handle_event(esp_mqtt_event_handle_t event);

  int publish(const char *topic, const char *data, int len=0, int qos=0, int retain=0);
private:
  esp_mqtt_client_handle_t _client;
};
