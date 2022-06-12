#include "display.hpp"
#include "appstate.hpp"
#include "beehive_events.hpp"
#include "util.hpp"
#include "lora.hpp"

#include <array>
#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <esp_ota_ops.h>
#include <esp_event_base.h>
#include <esp_log.h>

#include <u8g2.h>

#include <initializer_list>
#include <math.h>
#include <memory>
#include <optional>
#include <type_traits>
#include <sstream>


#define TAG "disp"

namespace {
const int64_t STATE_SHOW_TIME = 3000000;

std::string uptime()
{
  std::stringstream ss;
  const auto seconds = esp_timer_get_time() / 1000000;
  const auto minutes = seconds / 60;
  const auto hours = minutes / 60;
  const auto days = hours / 24;
  if(days)
  {
    ss << days << "d";
  }
  if(days || hours)
  {
    ss << hours % 24 << "h";
  }
  ss << minutes % 60 << "m";
  return ss.str();
}

}


Display::event_listener_base_t::event_listener_base_t(std::initializer_list<esp_event_base_t> bases)
{
  for(const auto event_base : bases)
  {
    ESP_ERROR_CHECK(esp_event_handler_instance_register(event_base,
                                                        ESP_EVENT_ANY_ID,
                                                        &Display::event_listener_base_t::s_event_handler,
                                                        this,
                                                        NULL));
  }
}

void Display::event_listener_base_t::s_event_handler(void* arg, esp_event_base_t event_base,
                         int32_t event_id, void* event_data)
{
  static_cast<Display::event_listener_base_t*>(arg)->event_handler(
    event_base, event_id, event_data
    );
}


Display::wifi_info_t::wifi_info_t() : Display::event_listener_base_t{{WIFI_EVENT, IP_EVENT}} {}

void Display::wifi_info_t::event_handler(esp_event_base_t event_base,
                         int32_t event_id, void* event_data)
{
  if (event_base == WIFI_EVENT)
  {
    switch(event_id)
    {
    case WIFI_EVENT_STA_DISCONNECTED:
      connected = false;
      break;
    case WIFI_EVENT_STA_CONNECTED:
      connected = true;
      break;
    }
  }
  else if(event_base == IP_EVENT)
  {
    switch(event_id)
    {
    case IP_EVENT_STA_GOT_IP:
      {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ip_address = event->ip_info.ip;
      }
      break;
    case IP_EVENT_STA_LOST_IP:
      ip_address = std::nullopt;
      break;
    }
  }
}

void Display::wifi_info_t::show(Display& display)
{
  auto y = NORMAL.size + 1;
  display.font_render(NORMAL, "WIFI", 2, y);
  y += 4 + NORMAL.size;
  auto x = 4;
  x += display.font_render(NORMAL, "Connected: ", x, y);
  display.font_render(NORMAL, connected ? "YES" : "NO", x, y);
  y += 4 + NORMAL.size;
  x = 4;
  x += display.font_render(NORMAL, "IP: ", x, y);
  if(ip_address)
  {
    const auto ip = *ip_address;
    std::array<char, 3 * 4 + 3 + 1> ip_addr_str{0};
    snprintf(ip_addr_str.data(), ip_addr_str.size(), IPSTR, IP2STR(&ip));
    display.font_render(NORMAL, ip_addr_str.data(), x, y);
  }
  else
  {
    display.font_render(NORMAL, "<UNKNOWN>", x, y);
  }
}

#ifdef USE_LORA
Display::lora_info_t::lora_info_t() : Display::event_listener_base_t{{LORA_EVENTS}} {}

void Display::lora_info_t::event_handler(esp_event_base_t event_base,
                                           int32_t event_id, void *event_data)
{
  using namespace beehive::events::lora;
  const auto stats = receive_stats(lora_events_t(event_id), event_data);
  if(stats)
  {
    package_count = stats->package_count;
    malformed_package_count = stats->malformed_package_count;
  }
}

void Display::lora_info_t::show(Display& display)
{
  auto y = NORMAL.size + 1;
  display.font_render(NORMAL, "LORA", 2, y);
  y += 4 + NORMAL.size;
  auto x = 4;
  x += display.font_render(NORMAL, "Role: ", x, y);
  display.font_render(NORMAL, beehive::lora::is_field_device() ? "FIELD" : "BASE", x, y);

  y += 4 + NORMAL.size;
  x = 4;
  x += display.font_render(NORMAL, "Packages: ", x, y);
  display.font_render(NORMAL, package_count, x, y);
  if(beehive::lora::is_field_device())
  {
    y += 4 + NORMAL.size;
    x = 4;
    x += display.font_render(NORMAL, "dbm: ", x, y);
    display.font_render(NORMAL, beehive::appstate::lora_dbm(), x, y);
  }
  else // BASE
  {
    y += 4 + NORMAL.size;
    x = 4;
    x += display.font_render(NORMAL, "Malformed: ", x, y);
    display.font_render(NORMAL, malformed_package_count, x, y);
  }
}
#endif


Display::sdcard_info_t::sdcard_info_t() : Display::event_listener_base_t{{SDCARD_EVENTS}} {}

void Display::sdcard_info_t::event_handler(esp_event_base_t event_base,
                         int32_t event_id, void* event_data)
{
  const auto sdcard_event_id = beehive::events::sdcard::sdcard_events_t(event_id);
  switch(sdcard_event_id)
  {
  case beehive::events::sdcard::MOUNTED:
    mounted = true;
    break;
  case beehive::events::sdcard::DATASET_WRITTEN:
    datasets_written = *static_cast<size_t*>(event_data);
    no_file = false;
    break;
  case beehive::events::sdcard::FILE_COUNT:
    file_count = *static_cast<size_t*>(event_data);
    no_file = false;
    break;
  case beehive::events::sdcard::NO_FILE:
    no_file = true;
    break;
  }
}

void Display::sdcard_info_t::show(Display& display)
{
  auto y = NORMAL.size + 1;
  display.font_render(NORMAL, "SDCARD", 2, y);
  if(mounted)
  {
    y += 4 + NORMAL.size;
    auto x = 4;
    x += display.font_render(NORMAL, "Writes: ", x, y);
    display.font_render(NORMAL, datasets_written, x, y);

    y += 4 + NORMAL.size;
    x = 4;
    x += display.font_render(NORMAL, "File#: ", x, y);
    display.font_render(NORMAL, file_count, x, y);
  }
  else
  {
    const auto x = (128 - display.font_text_width(NORMAL, "NOT_MOUNTED")) / 2;
    const auto y = (64 - NORMAL.size) / 2;
    display.font_render(NORMAL, "NOT MOUNTED", x, y);
  }
}


Display::ota_info_t::ota_info_t() : Display::event_listener_base_t{{OTA_EVENTS}} {}

void Display::ota_info_t::event_handler(esp_event_base_t event_base,
                         int32_t event_id, void* event_data)
{
  using namespace beehive::events::ota;
  state = ota_events_t(event_id);
  until = esp_timer_get_time() + STATE_SHOW_TIME;
}

bool Display::ota_info_t::ongoing()
{
  using namespace beehive::events::ota;
  return state == STARTED || esp_timer_get_time() < until;
}

void Display::ota_info_t::show(Display& display)
{
  auto y = NORMAL.size + 1;
  display.font_render(NORMAL, "OTA", 2, y);
}


Display::sensor_info_t::sensor_info_t() : Display::event_listener_base_t{{SENSOR_EVENTS}} {}

void Display::sensor_info_t::event_handler(esp_event_base_t event_base,
                         int32_t event_id, void* event_data)
{
  const auto sensor_event_id = beehive::events::sensors::sensor_events_t(event_id);
  switch(sensor_event_id)
  {
  case beehive::events::sensors::SHT3XDIS_COUNT:
    sensor_count = *static_cast<size_t*>(event_data);
    break;
  case beehive::events::sensors::SHT3XDIS_READINGS:
    ++sensor_readings;
    break;
  }
}

void Display::sensor_info_t::show(Display& display)
{
  auto y = NORMAL.size + 1;
  display.font_render(NORMAL, "SENSOR", 2, y);
  y += 4 + NORMAL.size;
  auto x = 4;
  x += display.font_render(NORMAL, "Readings: ", x, y);
  display.font_render(NORMAL, sensor_readings, x, y);
  y += 4 + NORMAL.size;
  x = 4;
  x += display.font_render(NORMAL, "Sensor#: ", x, y);
  display.font_render(NORMAL, sensor_count, x, y);
  x = 4;
  y += 4 + NORMAL.size;
  x += display.font_render(NORMAL, "Sleeptime: ", x, y);
  x += display.font_render(NORMAL, size_t(beehive::appstate::sleeptime()), x, y);
  x += display.font_render(NORMAL, "s", x, y);
}


Display::mqtt_info_t::mqtt_info_t() : Display::event_listener_base_t{{BEEHIVE_MQTT_EVENTS}} {}

void Display::mqtt_info_t::event_handler(esp_event_base_t event_base,
                         int32_t event_id, void* event_data)
{
  const auto mqtt_event_id = beehive::events::mqtt::mqtt_events_t(event_id);
  switch(mqtt_event_id)
  {
  case beehive::events::mqtt::PUBLISHED:
    message_backlog = *static_cast<size_t*>(event_data);
    break;
  }
}

void Display::mqtt_info_t::show(Display& display)
{
  auto y = NORMAL.size + 1;
  display.font_render(NORMAL, "MQTT", 2, y);
  y += 4 + NORMAL.size;
  auto x = 4;
  x += display.font_render(NORMAL, "Host: ", x, y);
  display.font_render(NORMAL, beehive::appstate::mqtt_host().c_str(), x, y);
  y += 4 + NORMAL.size;
  x = 4;
  x += display.font_render(NORMAL, "Backlog#: ", x, y);
  display.font_render(NORMAL, message_backlog, x, y);
}


void Display::start_info_t::show(Display &display)
{
  auto y = NORMAL.size + 1;
  display.font_render(NORMAL, "START", 2, y);
  const auto app_desc = esp_ota_get_app_description();
  y = 64 - 2;
  auto x = 128 - 2 - display.font_text_width(SMALL, app_desc->version);
  display.font_render(SMALL, app_desc->version, x, y);
}

void Display::system_info_t::show(Display &display)
{
  auto y = NORMAL.size + 1;
  display.font_render(NORMAL, "SYSTEM", 2, y);
  y += 4 + NORMAL.size;
  auto x = 4;
  x += display.font_render(NORMAL, "Name: ", x, y);
  display.font_render(NORMAL, beehive::appstate::system_name().c_str(), x, y);

  const auto dt = beehive::util::isoformat();
  const auto date = dt.substr(0, 10);
  const auto time = dt.substr(11, 8);

  y += 4 + NORMAL.size;
  x = 4;
  x += display.font_render(NORMAL, "Date: ", x, y);
  display.font_render(NORMAL, date.c_str(), x, y);
  y += 4 + NORMAL.size;
  x = 4;
  x += display.font_render(NORMAL, "Time: ", x, y);
  x += display.font_render(NORMAL, time.c_str(), x, y);
  x += display.font_render(NORMAL, " UTC", x, y);
  y += 4 + NORMAL.size;
  x = 4;
  x += display.font_render(NORMAL, "Uptime: ", x, y);
  x += display.font_render(NORMAL, uptime(), x, y);
}

Display::Display(I2CHost &bus)
  : _bus(bus)
{
  _u8g2 = std::unique_ptr<u8g2_struct>(new u8g2_t{});
  u8g2_Setup_ssd1306_i2c_128x64_noname_f(
    _u8g2.get(),
    U8G2_R0,
    Display::s_u8g2_esp32_i2c_byte_cb,
    Display::s_u8g2_esp32_i2c_byte_cb);
  _u8g2->u8x8.user_ptr = this;
  u8g2_InitDisplay(_u8g2.get()); // send init sequence to the display, display is in sleep mode after this,
  u8g2_SetPowerSave(_u8g2.get(), 0); // wake up display
  u8g2_SetFlipMode(_u8g2.get(), 1);
  _state_switch_timestamp = esp_timer_get_time() + STATE_SHOW_TIME;
}

Display::~Display()
{
}

void Display::start_task()
{
  xTaskCreatePinnedToCore(Display::s_task, "display", 8192, this, uxTaskPriorityGet(NULL), NULL, 0);
}

void Display::s_task(void* user_data)
{
  static_cast<Display*>(user_data)->task();
}

void Display::task()
{
  while(true)
  {
    vTaskDelay(100 / portTICK_PERIOD_MS);
    work();
  }
}


void Display::work()
{

  progress_state();
  clear();
  if(_ota_info.ongoing())
  {
    _ota_info.show(*this);
  }
  else
  {
    switch(_state)
    {
    case WIFI:
      _wifi_info.show(*this);
      break;
#ifdef USE_LORA
    case LORA:
      _lora_info.show(*this);
      break;
#endif
    case START:
      _start_info.show(*this);
      break;
    case SDCARD:
      _sdcard_info.show(*this);
      break;
    case SYSTEM:
      _system_info.show(*this);
        break;
    case SENSORS:
      _sensor_info.show(*this);
      break;
    case MQTT:
      _mqtt_info.show(*this);
      break;
    }
  }
  update();
}


void Display::progress_state()
{
  if(esp_timer_get_time() > _state_switch_timestamp)
  {
    switch(_state)
    {
    case START:
      _state = WIFI;
      break;
#ifdef USE_LORA
    case WIFI:
      _state = LORA;
      break;
    case LORA:
      // The base has no SD Card attached.
      if(beehive::lora::is_field_device())
      {
        _state = SDCARD;
      }
      else
      {
        _state = SYSTEM;
      }
      break;
#else
    case WIFI:
      _state = SDCARD;
      break;
#endif
    case SDCARD:
      _state = SYSTEM;
      break;
    case SYSTEM:
      _state = SENSORS;
      break;
    case SENSORS:
#ifdef USE_LORA
      // The field device doesn't send MQTT
      if(beehive::lora::is_field_device())
      {
        _state = START;
      }
      else
      {
        _state = MQTT;
      }
#else
      _state = MQTT;
#endif
      break;
    case MQTT:
      _state = START;
      break;
    }
    _state_switch_timestamp += STATE_SHOW_TIME;
  }
}

float Display::elapsed() const {
  return float(esp_timer_get_time()) / 1000000.0;
}

void Display::update() { u8g2_UpdateDisplay(_u8g2.get()); }

void Display::clear() { u8g2_ClearBuffer(_u8g2.get()); }


uint8_t Display::s_u8g2_esp32_i2c_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  return static_cast<Display*>(u8x8->user_ptr)->u8g2_esp32_i2c_byte_cb(u8x8, msg, arg_int, arg_ptr);
}

uint8_t Display::u8g2_esp32_i2c_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  switch(msg)
  {
  case U8X8_MSG_BYTE_SET_DC:
    /* ignored for i2c */
    break;
  case U8X8_MSG_BYTE_START_TRANSFER:
    _i2c_buffer.clear();
    break;
  case U8X8_MSG_BYTE_SEND:
    {
      auto buf = static_cast<uint8_t*>(arg_ptr);
      std::copy(buf, buf + arg_int, std::back_inserter(_i2c_buffer));
    }
    break;
  case U8X8_MSG_BYTE_END_TRANSFER:
    ESP_LOG_BUFFER_HEXDUMP(TAG, _i2c_buffer.data(), _i2c_buffer.size(), ESP_LOG_DEBUG);
    _bus.write_buffer_to_address(0x3c, _i2c_buffer.data(), _i2c_buffer.size());
    break;
  default:
    ESP_LOGD(TAG, "u8g2_cb: msg%i, arg_int=%i, arg_ptr=%p", msg, arg_int, arg_ptr);
    break;
  }
  return 1;
}


int Display::font_render(const font_info_t& font, auto value, int x, int y)
{
  using ArgT = typename std::remove_const<decltype(value)>::type;
  u8g2_SetFontMode(_u8g2.get(), 0); // draw solid (background is rendered)
  u8g2_SetDrawColor(_u8g2.get(), 1); // draw with white
  u8g2_SetFont(_u8g2.get(), font.font);
  if constexpr (std::is_same_v<ArgT, char*> || std::is_same_v<ArgT, const char*>)
  {
    return u8g2_DrawStr(_u8g2.get(), x, y + font.y_adjust, value);
  }
  else if constexpr (std::is_same_v<ArgT, std::string>)
  {
    return u8g2_DrawStr(_u8g2.get(), x, y + font.y_adjust, value.c_str());
  }
  else if constexpr (std::is_same_v<ArgT, size_t>)
  {
    std::array<char, 20> value_str{0};
    snprintf(value_str.data(), value_str.size(),
             "%u", value);
    return u8g2_DrawStr(_u8g2.get(), x, y + font.y_adjust, value_str.data());
  }
  else {
    static_assert(!sizeof(ArgT*),
               "Don't know what you are asking me to do.");
  }
  return 0;
}

int Display::font_text_width(const font_info_t& font, auto value)
{
  using ArgT = typename std::remove_const<decltype(value)>::type;
  u8g2_SetFont(_u8g2.get(), font.font);
  if constexpr (std::is_same_v<ArgT, char*> || std::is_same_v<ArgT, const char*>)
  {
    return u8g2_GetStrWidth(_u8g2.get(), value);
  }
  else if constexpr (std::is_same_v<ArgT, size_t>)
  {
    std::array<char, 20> value_str{0};
    snprintf(value_str.data(), value_str.size(),
             "%u", value);
    return u8g2_GetStrWidth(_u8g2.get(), value_str.data());
  }
  else {
    static_assert(!sizeof(ArgT*),
               "Don't know what you are asking me to do.");
  }
  return 0;
}

void Display::hline(int x, int x2, int y)
{
  u8g2_SetDrawColor(_u8g2.get(), 1);
  u8g2_DrawHLine(
    _u8g2.get(),
    x, y, abs(x2 - x) + 1
    );
}
