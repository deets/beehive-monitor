// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved
#pragma once
#include "esp_event.h"
#include "esp_event_base.h"

#include <optional>

#ifdef __cplusplus
extern "C" {
#endif

  ESP_EVENT_DECLARE_BASE(CONFIG_EVENTS);

#ifdef __cplusplus
}
#endif

typedef enum {
  CONFIG_EVENT_MQTT_HOST
} config_events_t;


namespace beehive::events::config::mqtt {

void hostname(const char *hostname);
std::optional<std::string> hostname(config_events_t, void* event_data);

}
