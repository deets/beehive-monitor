#include "wifi.hh"
#include "mqtt.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <memory>

extern "C" void app_main();

namespace {

// const auto SDA = gpio_num_t(19);
// const auto SCL = gpio_num_t(20);


void main_task(void*)
{
  setup_wifi();
  auto mqtt_client = std::unique_ptr<MQTTClient>(new MQTTClient("192.168.1.108"));
  while(true)
  {
    ESP_LOGI("main", "here I am");
    vTaskDelay(200 / portTICK_PERIOD_MS);
    mqtt_client->publish("foobar", "baz");
  }
}

} // end ns anon

void app_main()
{
  //Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  // it seems if I don't bind this to core 0, the i2c
  // subsystem fails randomly.
  xTaskCreatePinnedToCore(main_task, "main", 8192, NULL, uxTaskPriorityGet(NULL), NULL, 0);
}
