// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#include "beehive_events.hpp"
#include "io-buttons.hpp"

#include <cstring>
#include <optional>

#ifdef __cplusplus
extern "C" {
#endif

ESP_EVENT_DEFINE_BASE(CONFIG_EVENTS);
ESP_EVENT_DEFINE_BASE(SENSOR_EVENTS);
ESP_EVENT_DEFINE_BASE(BUTTON_EVENTS);
ESP_EVENT_DEFINE_BASE(SDCARD_EVENTS);
// Needs the BEEHIVE_ because MQTT_EVENTS is from the system
ESP_EVENT_DEFINE_BASE(BEEHIVE_MQTT_EVENTS);

#ifdef __cplusplus
}
#endif

namespace {

struct sht3xdis_event_t
{
  size_t count;
  beehive::events::sensors::sht3xdis_value_t values[1];
};

struct config_event_name_t
{
  char name[200];
};

} // namespace
namespace beehive::events {

namespace mqtt {

void published(size_t message_backlog_count)
{
  esp_event_post(
    BEEHIVE_MQTT_EVENTS, PUBLISHED,
    &message_backlog_count, sizeof(message_backlog_count), 0);
}

} // namespace mqtt

namespace buttons {

void register_button_callback(button_events_t e,
                              std::function<void(button_events_t)> cb)
{
  beehive::iobuttons::register_button_callback(e, cb);
}

} // namespace buttons

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
    SENSOR_EVENTS, SHT3XDIS_READINGS,
    block.data(), block.size(),
    0);
}

std::optional<std::vector<sht3xdis_value_t>> receive_readings(sensor_events_t kind, void *event_data)
{
  switch(kind)
  {
  case SHT3XDIS_READINGS:
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

namespace config {

void system_name(const char *system_name)
{
  config_event_name_t config;
  strncpy(config.name, system_name, sizeof(config_event_name_t::name));
  esp_event_post(CONFIG_EVENTS, SYSTEM_NAME, (void*)&config, sizeof(config), 0);
}

void sleeptime(uint32_t sleeptime)
{
  esp_event_post(CONFIG_EVENTS, SLEEPTIME, (void*)&sleeptime, sizeof(sleeptime), 0);
}

namespace mqtt {

void hostname(const char *hostname)
{
  config_event_name_t config;
  strncpy(config.name, hostname, sizeof(config_event_name_t::name));
  esp_event_post(CONFIG_EVENTS, MQTT_HOST, (void*)&config, sizeof(config), 0);
}

std::optional<std::string> hostname(config_events_t kind, void* event_data)
{
  switch(kind)
  {
  case MQTT_HOST:
    {
      const auto config = (config_event_name_t*)event_data;
      return std::string{config->name, strlen(config->name)};
    }
    break;
  default:
    return std::nullopt;
  }
}

} // namespace mqtt

} // namespace config

} // namespace beehive::events
