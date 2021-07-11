// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#include "beehive_events.hpp"

#ifdef __cplusplus
extern "C" {
#endif

ESP_EVENT_DEFINE_BASE(CONFIG_EVENTS);


#ifdef __cplusplus
}
#endif


void post_config_event(const config_event_mqtt_host_t& config)
{
  esp_event_post(CONFIG_EVENTS, CONFIG_EVENT_MQTT_HOST, (void*)&config, sizeof(config), 0);
}
