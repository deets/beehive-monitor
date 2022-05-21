// Copyright: 2022, Diez B. Roggisch, Berlin, all rights reserved

#pragma once
#ifdef USE_LORA
#include <array>
#include <cinttypes>

#include "rf95.hpp"

namespace beehive::lora {

bool is_field_device();

class LoRaLink
{
public:
  LoRaLink();
  void send(const uint8_t* buffer, size_t len);

private:
  RF95 _lora;
};

} // namespace beehive::lora
#endif
