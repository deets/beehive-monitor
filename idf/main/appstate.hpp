// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#pragma once

#include <string>

namespace beehive::appstate {

void init();
void promote_configuration();
void set_mqtt_host(const std::string &);

void set_system_name(const std::string &);
const std::string& system_name();

void set_sleeptime(uint32_t);
uint32_t sleeptime();

}
