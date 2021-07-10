// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#include "sensors.hpp"
#include "i2c.hh"

#include <tca9548a.hpp>
#include <esp_log.h>

#define TAG "sensors"

namespace {

const auto SDA = gpio_num_t(26);
const auto SCL = gpio_num_t(27);

} // namespace


Sensors::Sensors()
  : _bus(std::unique_ptr<I2C>(new I2CHost{0, SDA, SCL}))
  , _mux(std::unique_ptr<TCA9548A>(new TCA9548A{*_bus}))
{
  for(uint8_t busno=0; busno < 8; ++busno)
  {
    for(auto& address : _mux->bus(busno).scan())
    {
      ESP_LOGE(TAG, "on bus %i found address: %x", busno, address);
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
  for(auto& entry : _sensors)
  {
    const auto values = entry.sensor->raw_values();
    sensor_values({ entry.busno, entry.address, values });
  }
}
