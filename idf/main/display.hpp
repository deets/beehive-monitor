#include "beehive_events.hpp"
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
    #ifdef USE_LORA
    LORA,
    #endif
    SDCARD,
    SYSTEM,
    SENSORS,
    MQTT
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

#ifdef USE_LORA
  struct lora_info_t : event_listener_base_t
  {
    lora_info_t();
    void event_handler(esp_event_base_t event_base,
                       int32_t event_id, void* event_data) override;

    void show(Display&);

    size_t package_count = 0;
    size_t malformed_package_count = 0;
  };
#endif

  struct sdcard_info_t : event_listener_base_t {

    sdcard_info_t();
    void event_handler(esp_event_base_t event_base,
                       int32_t event_id, void* event_data) override;

    void show(Display&);

    bool mounted = false;
    size_t datasets_written = 0;
    size_t file_count = 0;
    bool no_file = false;
  };

  struct ota_info_t : event_listener_base_t {

    ota_info_t();
    void event_handler(esp_event_base_t event_base,
                       int32_t event_id, void* event_data) override;

    void show(Display&);

    beehive::events::ota::ota_events_t state = beehive::events::ota::NONE;
    int64_t until = 0;

    bool ongoing();
  };

  struct smartconfig_info_t : event_listener_base_t {

    smartconfig_info_t();
    void event_handler(esp_event_base_t event_base,
                       int32_t event_id, void* event_data) override;

    void show(Display&);

    bool started = false;
    bool ongoing();
  };

  struct system_info_t
  {
    void show(Display&);
  };

  struct sensor_info_t : event_listener_base_t {

    sensor_info_t();
    void event_handler(esp_event_base_t event_base,
                       int32_t event_id, void* event_data) override;

    void show(Display&);

    size_t sensor_count = 0;
    size_t sensor_readings = 0;
  };

  struct mqtt_info_t : event_listener_base_t {

    mqtt_info_t();
    void event_handler(esp_event_base_t event_base,
                       int32_t event_id, void* event_data) override;

    void show(Display&);

    size_t message_backlog;
  };

public:
  Display(I2CHost&);
  ~Display();

  void update();
  void clear();
  int font_render(const font_info_t& font, auto renderable, int x, int y);
  int font_text_width(const font_info_t& font, auto renderable);
  void hline(int x, int x2, int y);
  void start_task();
  // should be called every 100ms
  void work();

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
  #ifdef USE_LORA
  lora_info_t _lora_info;
  #endif
  wifi_info_t _wifi_info;
  sdcard_info_t _sdcard_info;
  system_info_t _system_info;
  sensor_info_t _sensor_info;
  mqtt_info_t _mqtt_info;
  ota_info_t _ota_info;
  smartconfig_info_t _smartconfig_info;

  int64_t _state_switch_timestamp;
};
