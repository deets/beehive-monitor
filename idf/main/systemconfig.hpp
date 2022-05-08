// Copyright: 2022, Diez B. Roggisch, Berlin, all rights reserved

#pragma once

#include <array>
#include <cinttypes>

namespace deets::beehive::systemconfig {

bool is_lora_sender(const std::array<uint8_t, 6>& mac);

}
