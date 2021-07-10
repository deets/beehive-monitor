// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#pragma once
#include "nod.hpp"
#include "sht3xdis.hpp"
#include <memory>
#include <vector>

class I2C;
class TCA9548A;

struct sht3xdis_value_t
{
  uint8_t busno;
  uint8_t address;
  sht3xdis::RawValues values;
};

class Sensors
{
  struct sensor_t
  {
    uint8_t busno;
    uint8_t address;
    std::unique_ptr<sht3xdis::SHT3XDIS> sensor;
  };

public:
  Sensors();
  ~Sensors();

  void work();

  nod::signal<void(sht3xdis_value_t)> sensor_values;

private:
  std::unique_ptr<I2C> _bus;
  std::unique_ptr<TCA9548A> _mux;
  std::vector<sensor_t> _sensors;
};
