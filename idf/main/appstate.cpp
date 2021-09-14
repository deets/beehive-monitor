// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#include "appstate.hpp"
#include "beehive_events.hpp"
#include "esp_err.h"

#include <nvs_flash.h>
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

#include <sstream>

namespace beehive::appstate {

namespace {
#define TAG "state"

#define NVS_NAMESPACE "beehive"

#define MQTT_HOSTNAME_DEFAULT "10.0.0.1"
std::string s_mqtt_host;

#define SYSTEM_NAME_DEFAULT "beehive"
std::string s_system_name;

#define SLEEPTIME_DEFAULT 300
uint32_t s_sleeptime;

nvs_handle s_nvs_handle;

std::string hash(const char* arg)
{
  const auto h = std::hash<std::string>{}(arg);
  std::stringstream s;
  s << h;
  return s.str();
}

template<typename T>
struct NVSLoadStore
{
  esp_err_t store(const T&);
  esp_err_t restore(T*);
};

template<>
struct NVSLoadStore<std::string>
{
  esp_err_t store(nvs_handle nvs_handle, const char* name, const std::string& value)
  {
    return nvs_set_str(nvs_handle, name, value.c_str());
  }

  esp_err_t restore(nvs_handle nvs_handle, const char* name, std::string* value)
  {
    std::vector<char> buffer;
    size_t length = 0;
    auto res = nvs_get_str(nvs_handle, name, nullptr, &length);
    ESP_LOGD(TAG, "std::string restore '%s', length: %i", name, length);
    if(res == ESP_OK)
    {
      buffer.resize(length);
      res = nvs_get_str(nvs_handle, name, buffer.data(), &length);
      // We want to skip the trailing 0 as this otherwise is included in
      // the string twice.
      *value = std::string(buffer.data(), std::min(strlen(buffer.data()), buffer.size()));
      ESP_LOGD(TAG, "std::string restore '%s', value: %s", name, value->c_str());
    }
    return res;
  }

};

template<>
struct NVSLoadStore<uint32_t>
{
  esp_err_t store(nvs_handle nvs_handle, const char* name, const uint32_t& value)
  {
    return nvs_set_u32(nvs_handle, name, value);
  }

  esp_err_t restore(nvs_handle nvs_handle, const char* name, uint32_t* value)
  {
    return nvs_get_u32(nvs_handle, name, value);
  }

};

} // namespace

void init()
{
  //Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_nvs_handle);
  ESP_ERROR_CHECK(ret);

  {
    auto sr = NVSLoadStore<std::string>{};

    if(sr.restore(s_nvs_handle, hash("mqtt_hostname").c_str(), &s_mqtt_host) != ESP_OK)
    {
      s_mqtt_host = MQTT_HOSTNAME_DEFAULT;
    }
    if(sr.restore(s_nvs_handle, hash("system_name").c_str(), &s_system_name) != ESP_OK)
    {
      s_system_name = SYSTEM_NAME_DEFAULT;
    }
  }
  {
    auto sr = NVSLoadStore<uint32_t>{};
    if(sr.restore(s_nvs_handle, hash("sleeptime").c_str(), &s_sleeptime) != ESP_OK)
    {
      s_sleeptime = SLEEPTIME_DEFAULT;
    }
  }
}

void promote_configuration()
{
  beehive::events::config::mqtt::hostname(s_mqtt_host.c_str());
  beehive::events::config::system_name(s_system_name.c_str());
  beehive::events::config::sleeptime(s_sleeptime);
}

void set_mqtt_host(const std::string& host)
{
  s_mqtt_host = host;
  auto sr = NVSLoadStore<std::string>{};
  sr.store(s_nvs_handle, hash("mqtt_hostname").c_str(), s_mqtt_host);
  beehive::events::config::mqtt::hostname(s_mqtt_host.c_str());
}

const std::string& mqtt_host() { return s_mqtt_host; }

void set_system_name(const std::string& system_name) {
  s_system_name = system_name;
  auto sr = NVSLoadStore<std::string>{};
  sr.store(s_nvs_handle, hash("system_name").c_str(), s_system_name);
  beehive::events::config::system_name(s_system_name.c_str());
}

const std::string& system_name() { return s_system_name; }

void set_sleeptime(uint32_t sleeptime) {
  s_sleeptime = sleeptime;
  auto sr = NVSLoadStore<decltype(sleeptime)>{};
  sr.store(s_nvs_handle, hash("sleeptime").c_str(), s_sleeptime);
  ESP_LOGD(TAG, "sleeptime: %i", s_sleeptime);
  beehive::events::config::sleeptime(s_sleeptime);
}

uint32_t sleeptime() { return s_sleeptime; }

const char *ntp_server() { return "pool.ntp.org"; }

} // namespace beehive::appstate
