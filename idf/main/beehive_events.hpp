// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved
#pragma once
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

  ESP_EVENT_DECLARE_BASE(CONFIG_EVENTS);

#ifdef __cplusplus
}
#endif

struct config_event_mqtt_host_t
{
  char host[200];
};

typedef enum {
  CONFIG_EVENT_MQTT_HOST
} config_events_t;


void post_config_event(const config_event_mqtt_host_t&);
