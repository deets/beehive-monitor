#include "font.h"

#include "i2c.hh"

#include <esp_event_base.h>
#include <esp_event.h>

#include <memory>
#include <optional>
#include <vector>

struct u8g2_struct;
struct u8x8_struct;

class Display {
  struct wifi_info_t {
    wifi_info_t();
    static void s_event_handler(void* arg, esp_event_base_t event_base,
                         int32_t event_id, void* event_data);
    void event_handler(esp_event_base_t event_base,
                         int32_t event_id, void* event_data);

    void show(Display&);

    bool connected = false;
    std::optional<esp_ip4_addr> ip_address;
  };

public:
  Display(I2CHost&);
  ~Display();

  void update();
  void clear();
  int font_render(const font_info_t& font, const char*, int x, int y);
  int font_text_width(const font_info_t& font, const char*);
  void hline(int x, int x2, int y);

private:
  static void s_task(void*);
  void task();

  float elapsed() const;

  static uint8_t s_u8g2_esp32_i2c_byte_cb(u8x8_struct *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
  uint8_t u8g2_esp32_i2c_byte_cb(u8x8_struct *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

  I2CHost& _bus;
  std::unique_ptr<u8g2_struct> _u8g2;
  std::vector<uint8_t> _i2c_buffer;

  wifi_info_t _wifi_info;
};
