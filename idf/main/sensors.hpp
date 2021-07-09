// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#pragma once
#include <memory>
#include <vector>

class I2C;
class TCA9548A;
class SHT3XDIS;

class Sensors
{
public:
  Sensors();
  ~Sensors();

  void work();
private:
  std::unique_ptr<I2C> _bus;
  std::unique_ptr<TCA9548A> _mux;
  std::vector<std::unique_ptr<SHT3XDIS>> _sensors;
};
