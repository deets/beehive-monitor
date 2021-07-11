#include "wifi.hh"
#include "mqtt.hpp"
#include "sensors.hpp"
#include "beehive_events.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <memory>

extern "C" void app_main();

#define TAG "main"

namespace {

void sensor_task(void*)
{
  Sensors sensors;
  while(true)
  {
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    sensors.work();
  }
}
} // end ns anon

void app_main()
{
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  //Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  setup_wifi();

  MQTTClient mqtt_client;

  // it seems if I don't bind this to core 0, the i2c
  // subsystem fails randomly.
  xTaskCreatePinnedToCore(sensor_task, "sensor", 8192, NULL, uxTaskPriorityGet(NULL), NULL, 0)
;
  beehive::events::config::mqtt::hostname("192.168.1.108");
  // keep this task alive so we retain
  // the stack-frame.
  while(true)
  {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    mqtt_client.publish("foobar", "baz");
  }
}
