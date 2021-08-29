// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#pragma once
#include "sensors.hpp"
#include "beehive_events.hpp"

#include <functional>

namespace beehive::mqtt::roland {

void publish(
  const size_t counter,
  const std::vector<events::sensors::sht3xdis_value_t> &readings,
  std::function < void(const char *topic, const char *data, int len, int qos, int retain)> publish
  );
}
