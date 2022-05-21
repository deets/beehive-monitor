// Copyright: 2022, Diez B. Roggisch, Berlin, all rights reserved
#ifdef USE_LORA
#include "lora.hpp"
#include "pins.hpp"

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

LoRaLink::LoRaLink()
  : _lora(VSPI_HOST, LORA_CS, LORA_SCLK, LORA_MOSI, LORA_MISO, LORA_SPI_SPEED)
{
}

void LoRaLink::send(const uint8_t *buffer, size_t len)
{
  _lora.send(buffer, len);
}

} // namespace beehive::lora


#endif // USE_LORA
