// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#pragma once
#include "beehive_events.hpp"

namespace beehive::iobuttons {

void setup();

using button_events_t = beehive::events::buttons::button_events_t;

void register_button_callback(button_events_t,
                              std::function<void(button_events_t)>);

bool mode_on();

}
