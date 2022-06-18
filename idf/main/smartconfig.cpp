// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#include "smartconfig.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <esp_netif.h>
#include <esp_smartconfig.h>

#include <string.h>
#include <stdlib.h>

namespace beehive::smartconfig {

namespace {

#define TAG "smartc"

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t s_wifi_event_group = nullptr;

static const int START_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;

void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
  if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
    ESP_LOGI(TAG, "Scan done");
  } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
    ESP_LOGI(TAG, "Found channel");
  } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
    ESP_LOGI(TAG, "Got SSID and password");

    smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
    wifi_config_t wifi_config;
    uint8_t ssid[33] = { 0 };
    uint8_t password[65] = { 0 };

    bzero(&wifi_config, sizeof(wifi_config_t));
    memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
    memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
    wifi_config.sta.bssid_set = evt->bssid_set;
    if (wifi_config.sta.bssid_set == true) {
      memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
    }

    memcpy(ssid, evt->ssid, sizeof(evt->ssid));
    memcpy(password, evt->password, sizeof(evt->password));
    ESP_LOGI(TAG, "SSID:%s", ssid);
    ESP_LOGI(TAG, "PASSWORD:%s", password);

    ESP_ERROR_CHECK( esp_wifi_disconnect() );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    esp_wifi_connect();
    ESP_LOGI(TAG, "after connect");
    // Not sure why, but the SC_EVENT_SEND_ACK_DONE doesn't
    // seem to happen. But being here means we can go &
    // try again.
    xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
  } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
    xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
  }
}


void smartconfig_task(void * parm)
{
  while(true)
  {
    auto uxBits = xEventGroupWaitBits(s_wifi_event_group, START_BIT, true, false, portMAX_DELAY);
    if(uxBits)
    {
      // We need this first so that an ongoing attempt
      // to connect doesn't interfere.
      esp_wifi_disconnect();
      ESP_LOGI(TAG, "Start SmartConfig");
      ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );
      smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
      ESP_ERROR_CHECK( esp_smartconfig_start(&cfg) );

      uxBits = xEventGroupWaitBits(s_wifi_event_group, ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
      if(uxBits & ESPTOUCH_DONE_BIT) {
        ESP_LOGI(TAG, "SmartConfig over");
        esp_smartconfig_stop();
      }
    }
    xEventGroupClearBits(s_wifi_event_group, ESPTOUCH_DONE_BIT | START_BIT);
  }
}

}

void run()
{
  if(!s_wifi_event_group)
  {
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
    xTaskCreate(smartconfig_task, "smartconfig_task", 4096, NULL, 3, NULL);
  }
  xEventGroupSetBits(s_wifi_event_group, START_BIT);
}

} // namespace beehive::smartconfig
