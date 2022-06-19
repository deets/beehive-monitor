// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#include "beehive_events.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_ota_ops.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_log.h>
#include <freertos/event_groups.h>

#include <sstream>
#include <cstring>

namespace {

#define TAG "ota"

const auto UPDATE_URL_BASE = "https://roggisch.de/beehive/beehive-";

extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

EventGroupHandle_t s_ota_event_group;

void ota_task(void *pvParameter)
{
  while(true)
  {
    const auto bits = xEventGroupWaitBits(s_ota_event_group, 1, pdTRUE, pdFALSE, 100000);
    if(bits)
    {
      beehive::events::ota::started();
      const auto app_desc = esp_ota_get_app_description();
      std::stringstream url_stream;
      url_stream << UPDATE_URL_BASE << app_desc->version;
      #ifdef BOARD_TTGO
      url_stream << "-ttgo";
      #ifdef USE_LORA
      url_stream << "-lora";
      #endif
      #endif
      url_stream << ".bin";
      const auto url = url_stream.str();
      ESP_LOGI(TAG, "Starting OTA example, fetching: %s", url.c_str());

      esp_http_client_config_t config;

      std::memset(&config, 0, sizeof(esp_http_client_config_t));
      config.url = url.c_str(),
      config.cert_pem = (char *)server_cert_pem_start,
      config.keep_alive_enable = true;

      esp_err_t ret = esp_https_ota(&config);
      if (ret == ESP_OK) {
        beehive::events::ota::found();
        esp_restart();
      } else {
        beehive::events::ota::none();
      }
    }
  }
}

} // namespace

void start_ota_task()
{
  static auto s_task_created = false;
  if(!s_task_created)
  {
    s_ota_event_group = xEventGroupCreate();
    xTaskCreate(&ota_task, "ota_task", 8192, NULL, 5, NULL);
    s_task_created = true;
  }
  xEventGroupSetBits(s_ota_event_group, 1);
}
