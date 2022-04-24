#include "display.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>

#include <math.h>
#include <memory>
#include <optional>
#include <u8g2.h>

//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include <esp_log.h>

#define TAG "disp"

Display::wifi_info_t::wifi_info_t()
{
  for(const auto event_base : {WIFI_EVENT, IP_EVENT})
  {
    ESP_ERROR_CHECK(esp_event_handler_instance_register(event_base,
                                                        ESP_EVENT_ANY_ID,
                                                        &Display::wifi_info_t::s_event_handler,
                                                        this,
                                                        NULL));
  }
}

void Display::wifi_info_t::s_event_handler(void* arg, esp_event_base_t event_base,
                         int32_t event_id, void* event_data)
{
  static_cast<Display::wifi_info_t*>(arg)->event_handler(
    event_base, event_id, event_data
    );
}

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
  y += 2 + NORMAL.size;
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

  xTaskCreatePinnedToCore(Display::s_task, "sensor", 8192, this, uxTaskPriorityGet(NULL), NULL, 0);
}

Display::~Display()
{
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
    clear();
    _wifi_info.show(*this);
    update();
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
    ESP_LOGE(TAG, "u8g2_cb: msg%i, arg_int=%i, arg_ptr=%p", msg, arg_int, arg_ptr);
    break;
  }
  return 1;
}


int Display::font_render(const font_info_t& font, const char* text, int x, int y)
{
  u8g2_SetFontMode(_u8g2.get(), 0); // draw solid (background is rendered)
  u8g2_SetDrawColor(_u8g2.get(), 1); // draw with white
  u8g2_SetFont(_u8g2.get(), font.font);
  return u8g2_DrawStr(_u8g2.get(), x, y + font.y_adjust, text);
}

int Display::font_text_width(const font_info_t& font, const char* text)
{
  u8g2_SetFont(_u8g2.get(), font.font);
  return u8g2_GetStrWidth(_u8g2.get(), text);
}

void Display::hline(int x, int x2, int y)
{
  u8g2_SetDrawColor(_u8g2.get(), 1);
  u8g2_DrawHLine(
    _u8g2.get(),
    x, y, abs(x2 - x) + 1
    );
}
