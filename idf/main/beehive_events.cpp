// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#include "beehive_events.hpp"
#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif

ESP_EVENT_DEFINE_BASE(CONFIG_EVENTS);


#ifdef __cplusplus
}
#endif

namespace {

struct config_event_mqtt_host_t
{
  char host[200];
};

}

namespace beehive::events::config::mqtt {

void hostname(const char *hostname)
{
  config_event_mqtt_host_t config;
  strncpy(config.host, hostname, sizeof(config_event_mqtt_host_t::host));
  esp_event_post(CONFIG_EVENTS, CONFIG_EVENT_MQTT_HOST, (void*)&config, sizeof(config), 0);
}

std::optional<std::string> hostname(config_events_t kind, void* event_data)
{
  switch(kind)
  {
  case CONFIG_EVENT_MQTT_HOST:
    {
      const auto config = (config_event_mqtt_host_t*)event_data;
      return std::string{config->host, strlen(config->host)};
    }
    break;
  default:
    return std::nullopt;
  }
}
}
