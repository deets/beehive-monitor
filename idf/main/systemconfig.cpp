// Copyright: 2022, Diez B. Roggisch, Berlin, all rights reserved
#include "systemconfig.hpp"

namespace {

std::array<uint8_t, 6> LORA_SENDER_MAC = {0x30, 0x83, 0x98, 0xdc, 0xca, 0xfc};

}
namespace deets::beehive::systemconfig {


bool is_lora_sender(const std::array<uint8_t, 6>& mac)
{
  return LORA_SENDER_MAC == mac;
}

}
