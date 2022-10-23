// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#pragma once

#include "deets/i2c.hpp"

#include <memory>
#include <vector>

namespace deets::i2c {

class TCA9548A;

namespace sht3xdis {

class SHT3XDIS;

} // namespace sht3xdis
} // namespace deets::i2c


namespace beehive::sensors {

class Sensors
{
  struct sensor_t
  {
    uint8_t busno;
    uint8_t address;
    std::unique_ptr<deets::i2c::sht3xdis::SHT3XDIS> sensor;
  };

public:
  Sensors(deets::i2c::I2CHost& bus);
  ~Sensors();

  void work();

private:

  deets::i2c::I2C& _bus;
  std::unique_ptr<deets::i2c::TCA9548A> _mux;
  std::vector<sensor_t> _sensors;
};

void setup_sensor_task(deets::i2c::I2CHost& bus);

} // namespace beehive::sensors
