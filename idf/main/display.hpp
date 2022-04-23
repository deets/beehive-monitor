#include "i2c.hh"
#include <memory>
#include <vector>

struct u8g2_struct;
struct u8x8_struct;

class Display {
public:
  Display(I2CHost&);
  ~Display();

private:
  static uint8_t s_u8g2_esp32_i2c_byte_cb(u8x8_struct *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
  uint8_t u8g2_esp32_i2c_byte_cb(u8x8_struct *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

  I2CHost& _bus;
  std::unique_ptr<u8g2_struct> _u8g2;
  std::vector<uint8_t> _i2c_buffer;
};
