// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved
#pragma once
#include "esp_event.h"
#include "esp_event_base.h"

#include <optional>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

  ESP_EVENT_DECLARE_BASE(CONFIG_EVENTS);
  ESP_EVENT_DECLARE_BASE(SENSOR_EVENTS);

#ifdef __cplusplus
}
#endif

namespace beehive::events {

namespace sensors {

enum sensor_events_t
{
  SENSOR_EVENT_SHT3XDIS_READINGS
};

struct sht3xdis_value_t
{
  uint8_t busno;
  uint8_t address;
  uint16_t humidity;
  uint16_t temperature;
};

void send_readings(const std::vector<sht3xdis_value_t> &);
std::optional<std::vector<sht3xdis_value_t>> receive_readings(sensor_events_t,
                                                              void *event_data);

} // namespace sensors

namespace config {

enum config_events_t
{
  CONFIG_EVENT_MQTT_HOST
};

namespace mqtt {

void hostname(const char *hostname);
std::optional<std::string> hostname(config_events_t, void* event_data);

} // namespace mqtt
} // namespace config
} // namespace beehive::events::config
