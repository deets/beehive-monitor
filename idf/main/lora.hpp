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
  static constexpr int FIFO_SIZE = RF95::FIFO_SIZE;

  LoRaLink();
  void send(const uint8_t* buffer, size_t len);
  size_t recv(std::array<uint8_t, FIFO_SIZE>&);
  bool channel_active();

private:
  RF95 _lora;
};

} // namespace beehive::lora
#endif
