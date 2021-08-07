// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_ota_ops.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_log.h>

#include <sstream>
#include <cstring>

namespace {

#define TAG "ota"

const auto UPDATE_URL_BASE = "https://roggisch.de/beehive/beehive-";

extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");


void ota_task(void *pvParameter)
{
  const auto app_desc = esp_ota_get_app_description();
  std::stringstream url_stream;
  url_stream << UPDATE_URL_BASE << app_desc->version << ".bin";
  const auto url = url_stream.str();
  ESP_LOGI(TAG, "Starting OTA example, fetching: %s", url.c_str());

  esp_http_client_config_t config;

  std::memset(&config, 0, sizeof(esp_http_client_config_t));
  config.url = url.c_str(),
  config.cert_pem = (char *)server_cert_pem_start,
  config.keep_alive_enable = true;

  esp_err_t ret = esp_https_ota(&config);
  if (ret == ESP_OK) {
    esp_restart();
  } else {
    ESP_LOGE(TAG, "No OTA update found");
  }
  // When I do not put this here, the system crashes
  while(true)
  {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
} // namespace

void start_ota_task()
{
  xTaskCreate(&ota_task, "ota_example_task", 8192, NULL, 5, NULL);
}
