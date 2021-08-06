#include "wifi.hh"
#include "mqtt.hpp"
#include "sensors.hpp"
#include "sdcard.hpp"
#include "beehive_events.hpp"
#include "beehive_http.hpp"

#include <esp_ota_ops.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <memory>

extern "C" void app_main();

#define TAG "main"

using namespace beehive;

namespace {

extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

const auto UPDATE_URL = "https://roggisch.de/beehive.bin";

void sensor_task(void*)
{
  Sensors sensors;
  while(true)
  {
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    sensors.work();
  }
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    }
    return ESP_OK;
}

void simple_ota_example_task(void *pvParameter)
{
  ESP_LOGI(TAG, "Starting OTA example, fetching: %s", UPDATE_URL);
  esp_http_client_config_t config = {
    .url = UPDATE_URL,
    .cert_pem = (char *)server_cert_pem_start,
    .event_handler = _http_event_handler,
    .keep_alive_enable = true,
  };


  esp_err_t ret = esp_https_ota(&config);
  if (ret == ESP_OK) {
    esp_restart();
  } else {
    ESP_LOGE(TAG, "Firmware upgrade failed");
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

  mqtt::MQTTClient mqtt_client;
  sdcard::SDCardWriter sdcard_writer;

  // it seems if I don't bind this to core 0, the i2c
  // subsystem fails randomly.
  xTaskCreatePinnedToCore(sensor_task, "sensor", 8192, NULL, uxTaskPriorityGet(NULL), NULL, 0);
  beehive::events::config::mqtt::hostname("192.168.1.108");

  beehive::http::HTTPServer http_server;

  xTaskCreate(&simple_ota_example_task, "ota_example_task", 8192, NULL, 5, NULL);
  // keep this task alive so we retain
  // the stack-frame.
  while(true)
  {
    ESP_LOGE(TAG, "THIS IS THE OLD IMAGE");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
