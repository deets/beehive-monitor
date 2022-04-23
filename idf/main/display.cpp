#include "display.hpp"

#include <memory>
#include <u8g2.h>

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include <esp_log.h>

#define TAG "disp"


Display::Display(I2CHost &bus)
  : _bus(bus)
{
  _u8g2 = std::unique_ptr<u8g2_struct>(new u8g2_t{});
  u8g2_Setup_ssd1306_i2c_128x64_noname_1(
    _u8g2.get(),
    U8G2_R0,
    Display::s_u8g2_esp32_i2c_byte_cb,
    Display::s_u8g2_esp32_i2c_byte_cb);
  _u8g2->u8x8.user_ptr = this;
  u8g2_InitDisplay(_u8g2.get()); // send init sequence to the display, display is in sleep mode after this,
  u8g2_SetPowerSave(_u8g2.get(), 0); // wake up display
  u8g2_SetDrawColor(_u8g2.get(), 1);
  ESP_LOGD(TAG, "u8g2_cb: Drawvline");
  u8g2_DrawVLine(_u8g2.get(), 10, 5, 20);
  u8g2_UpdateDisplay(_u8g2.get());
}


Display::~Display()
{
}

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
