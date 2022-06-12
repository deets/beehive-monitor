// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved
#pragma once
#include "mqtt_client.h"
#include "pins.hpp"

#include "esp_event.h"
#include "esp_event_base.h"

#include <optional>
#include <vector>
#include <functional>

#ifdef __cplusplus
extern "C" {
#endif

  ESP_EVENT_DECLARE_BASE(CONFIG_EVENTS);
  ESP_EVENT_DECLARE_BASE(SENSOR_EVENTS);
  ESP_EVENT_DECLARE_BASE(BUTTON_EVENTS);
  ESP_EVENT_DECLARE_BASE(SDCARD_EVENTS);
  ESP_EVENT_DECLARE_BASE(LORA_EVENTS);
  ESP_EVENT_DECLARE_BASE(BEEHIVE_MQTT_EVENTS);
  ESP_EVENT_DECLARE_BASE(OTA_EVENTS);

#ifdef __cplusplus
}
#endif

namespace beehive::events {

namespace lora {

enum lora_events_t
{
  STATS,
};

struct lora_stats_t
{
  size_t package_count;
  size_t malformed_package_count;
};

void send_stats(size_t package_count, size_t malformed_package_count);
std::optional<lora_stats_t> receive_stats(lora_events_t, void *event_data);

} // namespace lora

namespace buttons {

enum button_events_t
{
  OTA = PIN_NUM_OTA,
  MODE = PIN_NUM_MODE
};

void register_button_callback(button_events_t,
                              std::function<void(button_events_t)>);

}

namespace mqtt {

enum mqtt_events_t { PUBLISHED };

void published(size_t);

}

namespace sensors {

enum sensor_events_t
{
  SHT3XDIS_COUNT,
  SHT3XDIS_READINGS
};

struct sht3xdis_value_t
{
  uint8_t busno;
  uint8_t address;
  float humidity;
  float temperature;
  uint16_t raw_humidity;
  uint16_t raw_temperature;
};

void send_readings(const std::vector<sht3xdis_value_t> &);
std::optional<std::vector<sht3xdis_value_t>> receive_readings(sensor_events_t,
                                                              void *event_data);

} // namespace sensors

namespace ota {

enum ota_events_t
{
  NONE,
  STARTED,
  FOUND,
};

void started();
void found();
void none();

}

namespace sdcard {


enum sdcard_events_t
{
  MOUNTED,
  DATASET_WRITTEN,
  FILE_COUNT,
  NO_FILE
};

}

namespace config {

enum config_events_t
{
  MQTT_HOST,
  SYSTEM_NAME,
  SLEEPTIME,
  LORA_DBM,
};

void system_name(const char *system_name);
void sleeptime(uint32_t sleeptime);
void lora_dbm(uint32_t lora_dbm);

namespace mqtt {

void hostname(const char *hostname);
std::optional<std::string> hostname(config_events_t, void* event_data);

} // namespace mqtt

} // namespace config
} // namespace beehive::events::config
