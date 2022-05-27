// Copyright: 2022, Diez B. Roggisch, Berlin, all rights reserved

#pragma once
#ifdef USE_LORA

#include "rf95.hpp"
#include "beehive_events.hpp"

#include <array>
#include <cinttypes>

namespace beehive::lora {

bool is_field_device();

class LoRaLink
{
public:
  LoRaLink();

  void setup_field_work(size_t sequence_num);

private:

  static void s_sensor_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data);
  void sensor_event_handler(esp_event_base_t base, beehive::events::sensors::sensor_events_t id, void* event_data);

  RF95 _lora;
  size_t _sequence_num;

};

} // namespace beehive::lora
#endif // USE_LORA
