#include "wifi.hh"
#include "mqtt.hpp"
#include "sensors.hpp"


#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <memory>

extern "C" void app_main();

#define TAG "main"

namespace {


void main_task(void*)
{
  setup_wifi();
  auto mqtt_client = std::unique_ptr<MQTTClient>(new MQTTClient("192.168.1.108"));
  while(true)
  {
    vTaskDelay(200 / portTICK_PERIOD_MS);
    mqtt_client->publish("foobar", "baz");
  }
}

void sensor_task(void*)
{
  Sensors sensors;
  sensors.sensor_values.connect([](sht3xdis_value_t v) {
    ESP_LOGI(TAG, "%i:%i -> %i, %i", v.busno, v.address, v.values.humidity, v.values.temperature);
  });
  while(true)
  {
    ESP_LOGI("main", "here I am");
    sensors.work();
    vTaskDelay(200 / portTICK_PERIOD_MS);
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
  xTaskCreatePinnedToCore(sensor_task, "sensor", 8192, NULL, uxTaskPriorityGet(NULL), NULL, 0);
}
