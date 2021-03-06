// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#pragma once

#include <string>

namespace beehive::appstate {

void init();
void promote_configuration();
void set_mqtt_host(const std::string &);
const std::string& mqtt_host();

void set_system_name(const std::string &);
const std::string& system_name();

void set_sleeptime(uint32_t);
uint32_t sleeptime();

const char* ntp_server();

std::string version();

#ifdef USE_LORA
uint32_t lora_dbm();
void set_lora_dbm(uint32_t);
#endif
}
