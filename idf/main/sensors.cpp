// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#include "sensors.hpp"
#include "beehive_events.hpp"
#include "esp_timer.h"
#include "pins.hpp"
#include "appstate.hpp"
#include "util.hpp"

#include "deets/i2c/tca9548a.hpp"
#include "deets/i2c/sht3xdis.hpp"

#include "sdkconfig.h"
#include <esp_task_wdt.h>

#include <math.h>

#include <set>
#include <tuple>
#include <map>
#include <chrono>

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include <esp_log.h>

#define TAG "sensors"
#define READINGS_TAG "sreads"

namespace beehive::sensors {

namespace {

using namespace std::chrono_literals;

using namespace beehive::events::sensors;

const auto SENSOR_READING_COUNT = 16;
const auto SENSOR_READING_TIMEOUT = 200ms;

#ifdef CONFIG_BEEHIVE_FAKE_SENSOR_DATA
const double HZ = 0.1;

void fake_sensor_data(std::vector<sht3xdis_value_t>& readings, const std::set<std::tuple<uint8_t, uint8_t>>& sensors_seen)
{
  for(const auto busno : { 4, 5, 6, 7 })
  {
    for(const auto address : { 0x44, 0x45 })
    {
      const auto id = std::make_tuple(busno, address);
      if(sensors_seen.count(id) == 0)
      {
	const auto seconds = double(esp_timer_get_time()) / 1000000.0;
	const auto s = sin(seconds * HZ);
	const auto c = cos(seconds * HZ);
	const auto raw_humidity = uint16_t(30000.0 + 20000.0 * s + busno * address);
	const auto raw_temperature = uint16_t(30000.0 + 5000.0 * c + busno * address);
	const auto humidity = sht3xdis::SHT3XDIS::raw2humidity(raw_humidity);
	const auto temperature = sht3xdis::SHT3XDIS::raw2temperature(raw_temperature);
	readings.push_back(
	  {
	    uint8_t(busno), uint8_t(address),
	    humidity,
	    temperature,
	    raw_humidity,
	    raw_temperature
	  });
      }
    }
  }
}

#endif // CONFIG_BEEHIVE_FAKE_SENSOR_DATA


void sensor_task(void* user_pointer)
{
  deets::i2c::I2CHost* bus = static_cast<deets::i2c::I2CHost*>(user_pointer);
  Sensors sensors(*bus);
  vTaskDelay(2000 / portTICK_PERIOD_MS);

  const auto millis = std::chrono::seconds(beehive::appstate::sleeptime()) / 1ms;
  while(true)
  {
    ESP_LOGD(TAG, "Doing sensor work, then sleep for %dms", int(millis));
    sensors.work();
    vTaskDelay(millis / portTICK_PERIOD_MS);
  }
}

} // namespace



Sensors::Sensors(deets::i2c::I2CHost& bus)
  : _bus(bus)
  , _mux(std::unique_ptr<deets::i2c::TCA9548A>(new deets::i2c::TCA9548A{_bus}))
{
  for(uint8_t busno=0; busno < 8; ++busno)
  {
    for(auto& address : _mux->bus(busno).scan())
    {
      ESP_LOGD(TAG, "on bus %i found address: %x", busno, address);
      if(deets::i2c::sht3xdis::SHT3XDIS::valid_address(address))
      {
	auto sensor = std::unique_ptr<deets::i2c::sht3xdis::SHT3XDIS>(new deets::i2c::sht3xdis::SHT3XDIS(_mux->bus(busno), address));
	auto entry = Sensors::sensor_t{ busno, address, std::move(sensor) };
	_sensors.push_back(std::move(entry));
      }
    }
  }
  auto sensor_count = _sensors.size();
  esp_event_post(
    SENSOR_EVENTS, SHT3XDIS_COUNT,
    &sensor_count, sizeof(sensor_count),
    0);
}

Sensors::~Sensors() {}

void Sensors::work()
{
  using namespace beehive::events::sensors;
  using namespace std::chrono_literals;

  using sensor_id_t = std::tuple<uint8_t, uint8_t>;

  std::vector<sht3xdis_value_t> readings;

  #ifdef CONFIG_BEEHIVE_FAKE_SENSOR_DATA
  std::set<sensor_id_t> sensors_seen;
  #endif

  const auto sleeptime_in_ms = SENSOR_READING_TIMEOUT / 1ms;
  std::map<sensor_id_t, std::tuple<uint32_t, uint32_t>> readings_accus;

  for(auto i=0; i < SENSOR_READING_COUNT; ++i)
  {
    for(auto& entry : _sensors)
    {
      const auto raw_values = entry.sensor->raw_values();
      const sensor_id_t id = {entry.busno, entry.address};
      auto [ temperature, humidity ] = readings_accus[id];
      temperature += raw_values.temperature;
      humidity += raw_values.humidity;
      readings_accus[id] = { temperature, humidity };
    }
    esp_task_wdt_reset();
    vTaskDelay(sleeptime_in_ms / portTICK_PERIOD_MS);
  }

  ESP_LOGE(READINGS_TAG, "%s", beehive::util::isoformat().c_str());

  for(auto& entry : _sensors)
  {
    const sensor_id_t id = {entry.busno, entry.address};
    const auto [ acc_temperature, acc_humidity ] = readings_accus[id];

    const auto raw_values = deets::i2c::sht3xdis::RawValues{
      uint16_t(acc_humidity / SENSOR_READING_COUNT),
      uint16_t(acc_temperature / SENSOR_READING_COUNT)
    };
    const auto values = deets::i2c::sht3xdis::Values::from_raw(raw_values);
    readings.push_back(
      {
	entry.busno, entry.address,
	values.humidity,
	values.temperature,
	raw_values.humidity,
	raw_values.temperature
      });
    ESP_LOGE(READINGS_TAG, "%02X:%02X -> %04XH, %04XT", entry.busno, entry.address, raw_values.humidity, raw_values.temperature);
    #ifdef CONFIG_BEEHIVE_FAKE_SENSOR_DATA
    sensors_seen.insert({entry.busno, entry.address});
    #endif
  }
  #ifdef CONFIG_BEEHIVE_FAKE_SENSOR_DATA
  fake_sensor_data(readings, sensors_seen);
  #endif
  send_readings(readings);
}

void setup_sensor_task(deets::i2c::I2CHost& i2c_bus)
{
  // it seems if I don't bind this to core 0, the i2c
  // subsystem fails randomly.
  xTaskCreatePinnedToCore(sensor_task, "sensor", 8192, &i2c_bus, uxTaskPriorityGet(NULL), NULL, 0);
}


} // namespace beehive::sensors
