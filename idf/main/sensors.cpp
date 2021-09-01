// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#include "sensors.hpp"
#include "beehive_events.hpp"
#include "esp_timer.h"
#include "pins.hpp"

#include "i2c.hh"
#include "tca9548a.hpp"
#include "sht3xdis.hpp"

#include "sdkconfig.h"

#include <esp_log.h>
#include <math.h>

#include <set>
#include <tuple>

#define TAG "sensors"

namespace {

using namespace beehive::events::sensors;

const double HZ = 0.1;

void fake_sensor_data(std::vector<sht3xdis_value_t>& readings, const std::set<std::tuple<uint8_t, uint8_t>>& sensors_seen)
{
  for(const auto busno : { 4, 5, 6, 7 })
  {
    for(const auto address : { 44, 45 })
    {
      const auto id = std::make_tuple(busno, address);
      if(sensors_seen.count(id) == 0)
      {
	const auto seconds = double(esp_timer_get_time()) / 1000000.0;
	const auto s = sin(seconds * HZ);
	const auto c = cos(seconds * HZ);
	const auto raw_humidity = uint16_t(30000.0 + 20000.0 * s);
	const auto raw_temperature = uint16_t(10000.0 + 5000.0 * c);
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

} // namespace



Sensors::Sensors()
  : _bus(std::unique_ptr<I2C>(new I2CHost{0, SDA, SCL}))
  , _mux(std::unique_ptr<TCA9548A>(new TCA9548A{*_bus}))
{
  for(uint8_t busno=0; busno < 8; ++busno)
  {
    for(auto& address : _mux->bus(busno).scan())
    {
      ESP_LOGI(TAG, "on bus %i found address: %x", busno, address);
      if(sht3xdis::SHT3XDIS::valid_address(address))
      {
	auto sensor = std::unique_ptr<sht3xdis::SHT3XDIS>(new sht3xdis::SHT3XDIS(_mux->bus(busno), address));
	auto entry = Sensors::sensor_t{ busno, address, std::move(sensor) };
	_sensors.push_back(std::move(entry));
      }
    }
  }
}

Sensors::~Sensors() {}

void Sensors::work()
{
  using namespace beehive::events::sensors;

  std::vector<sht3xdis_value_t> readings;

  #ifdef CONFIG_BEEHIVE_FAKE_SENSOR_DATA
  std::set<std::tuple<uint8_t, uint8_t>> sensors_seen;
  #endif

  for(auto& entry : _sensors)
  {
    const auto raw_values = entry.sensor->raw_values();
    const auto values = entry.sensor->values();
    readings.push_back(
      {
	entry.busno, entry.address,
	values.humidity,
	values.temperature,
	raw_values.humidity,
	raw_values.temperature
      });
  }
  #ifdef CONFIG_BEEHIVE_FAKE_SENSOR_DATA
  fake_sensor_data(readings, sensors_seen);
  #endif
  send_readings(readings);
}
