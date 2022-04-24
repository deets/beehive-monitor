#include "font.h"

#include "i2c.hh"

#include <cstdint>
#include <esp_event_base.h>
#include <esp_event.h>

#include <initializer_list>
#include <memory>
#include <optional>
#include <vector>

struct u8g2_struct;
struct u8x8_struct;

class Display {

  enum display_state_e
  {
    START,
    WIFI,
    SDCARD
  };

  struct event_listener_base_t
  {
    event_listener_base_t(std::initializer_list<esp_event_base_t>);
    static void s_event_handler(void* arg, esp_event_base_t event_base,
                         int32_t event_id, void* event_data);
    virtual void event_handler(esp_event_base_t event_base,
                       int32_t event_id, void* event_data) = 0;

  };

  struct start_info_t {
    void show(Display&);
  };

  struct wifi_info_t : event_listener_base_t {

    wifi_info_t();
    void event_handler(esp_event_base_t event_base,
                       int32_t event_id, void* event_data) override;

    void show(Display&);

    bool connected = false;
    std::optional<esp_ip4_addr> ip_address;
  };

  struct sdcard_info_t : event_listener_base_t {

    sdcard_info_t();
    void event_handler(esp_event_base_t event_base,
                       int32_t event_id, void* event_data) override;

    void show(Display&);

    bool mounted = false;
    size_t datasets_written = 0;
    bool no_file = false;
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
  void progress_state();
  float elapsed() const;

  static uint8_t s_u8g2_esp32_i2c_byte_cb(u8x8_struct *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
  uint8_t u8g2_esp32_i2c_byte_cb(u8x8_struct *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

  I2CHost& _bus;
  std::unique_ptr<u8g2_struct> _u8g2;
  std::vector<uint8_t> _i2c_buffer;

  display_state_e _state = START;
  start_info_t _start_info;
  wifi_info_t _wifi_info;
  sdcard_info_t _sdcard_info;

  int64_t _state_switch_timestamp;
};
