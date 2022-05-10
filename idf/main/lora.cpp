// Copyright: 2022, Diez B. Roggisch, Berlin, all rights reserved
#include "lora.hpp"

#include "esp_mac.h"

namespace {

std::array<uint8_t, 6> LORA_SENDER_MAC = {0x30, 0x83, 0x98, 0xdc, 0xca, 0xfc};

}

namespace beehive::lora {

bool is_field_device()
{
  std::array<uint8_t, 6> mac;
  esp_read_mac(mac.data(), ESP_MAC_WIFI_STA);
  return LORA_SENDER_MAC == mac;
}

}
