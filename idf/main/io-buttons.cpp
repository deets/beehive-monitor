// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#include "io-buttons.hpp"
#include "beehive_events.hpp"
#include "pins.hpp"

#include <soc/gpio_struct.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_timer.h>

#include <unordered_map>
#include <stdint.h>
#include <list>
#include <map>

namespace beehive::iobuttons {

namespace {

#define TAG "io"

const int DEBOUNCE = (200 * 1000);

std::unordered_map<int, uint64_t> s_debounces = {
  { PIN_NUM_OTA, 200 * 1000}
};

std::map<button_events_t, std::list<std::function<void(button_events_t)>>> s_button_callbacks;
std::unordered_map<int, uint64_t> s_last;

void IRAM_ATTR gpio_isr_handler(void* arg)
{
  int pin = (int)arg;
  int64_t ts = esp_timer_get_time();
  if(s_last.count(pin) && s_last[pin] + s_debounces[pin] > ts)
  {
    return;
  }
  s_last[pin] = ts;
  const auto event = static_cast<button_events_t>(pin);

  esp_event_post(
    BUTTON_EVENTS, event, nullptr, 0, 0);

}

void s_button_event_handler(void *event_handler_arg,
                            esp_event_base_t event_base, int32_t event_id,
                            void *event_data)
{
  ESP_LOGE(TAG, "s_button_event_handler");
  const auto event = static_cast<button_events_t>(event_id);
  for(auto& cb : s_button_callbacks[event])
  {
    cb(event);
  }
}

} // end ns anonymous


void setup()
{
  gpio_config_t io_conf;
  //interrupt of rising edge
  io_conf.intr_type = (gpio_int_type_t)GPIO_PIN_INTR_POSEDGE;
  io_conf.pin_bit_mask = \
  (1ULL<< PIN_NUM_OTA);

  //set as input mode
  io_conf.mode = GPIO_MODE_INPUT;
  //enable pull-up mode
  io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&io_conf);

  // install global GPIO ISR handler
  gpio_install_isr_service(0);
  // install individual interrupts
  gpio_isr_handler_add(PIN_NUM_OTA, gpio_isr_handler, (void*)PIN_NUM_OTA);
  // fill map to avoid allocates in ISR
  int64_t ts = esp_timer_get_time();
  for(const auto& item : s_debounces)
  {
    s_last[item.first] = ts;
  }

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
		    BUTTON_EVENTS,
		    ESP_EVENT_ANY_ID,
		    s_button_event_handler, nullptr, nullptr));

}

void register_button_callback(button_events_t e,
                              std::function<void(button_events_t)> cb)
{
  s_button_callbacks[e].push_back(cb);
}

} // namespace beehive::iobuttons
