// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#include "beehive_events.hpp"
#include <cstring>
#include <optional>

#ifdef __cplusplus
extern "C" {
#endif

ESP_EVENT_DEFINE_BASE(CONFIG_EVENTS);
ESP_EVENT_DEFINE_BASE(SENSOR_EVENTS);


#ifdef __cplusplus
}
#endif

namespace {

struct sht3xdis_event_t
{
  size_t count;
  beehive::events::sensors::sht3xdis_value_t values[1];
};

struct config_event_mqtt_host_t
{
  char host[200];
};

} // namespace
namespace beehive::events {

namespace sensors {

void send_readings(const std::vector<sht3xdis_value_t>& readings)
{
  const auto payload_size = readings.size() * sizeof(std::remove_reference<decltype(readings)>::type::value_type);
  std::vector<uint8_t> block(
    sizeof(size_t) +
    payload_size);

  auto p = (sht3xdis_event_t*)block.data();
  p->count = readings.size();
  std::memcpy(&p->values[0], readings.data(), payload_size);

  esp_event_post(
    SENSOR_EVENTS, SENSOR_EVENT_SHT3XDIS_READINGS,
    block.data(), block.size(),
    0);
}

std::optional<std::vector<sht3xdis_value_t>> receive_readings(sensor_events_t kind, void *event_data)
{
  switch(kind)
  {
  case SENSOR_EVENT_SHT3XDIS_READINGS:
    {
      const auto p = (sht3xdis_event_t*)event_data;
      std::vector<sht3xdis_value_t> result(p->count);
      for(size_t i=0; i < p->count; ++i)
      {
	result[i] = p->values[i];
      }
      return result;
    }
    break;
  default:
    return std::nullopt;
  }
}


} // namespace sensors

namespace config::mqtt {

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

} // namespace config::mqtt

} // namespace beehive::events
