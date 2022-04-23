// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#pragma once

#include "i2c.hh"

#include <memory>
#include <vector>

namespace sht3xdis {
class SHT3XDIS;
};

class I2C;
class TCA9548A;

class Sensors
{
  struct sensor_t
  {
    uint8_t busno;
    uint8_t address;
    std::unique_ptr<sht3xdis::SHT3XDIS> sensor;
  };

public:
  Sensors(I2CHost& bus);
  ~Sensors();

  void work();

private:

  I2C& _bus;
  std::unique_ptr<TCA9548A> _mux;
  std::vector<sensor_t> _sensors;
};
