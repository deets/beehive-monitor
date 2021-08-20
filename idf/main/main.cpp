#include "freertos/projdefs.h"
#include "io-buttons.hpp"
#include "wifi.hh"
#include "mqtt.hpp"
#include "sensors.hpp"
#include "sdcard.hpp"
#include "beehive_events.hpp"
#include "beehive_http.hpp"
#include "ota.hpp"
#include "wifi-provisioning.hpp"
#include "smartconfig.hpp"
#include "appstate.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <mdns.h>
#include <chrono>
#include <sstream>

extern "C" void app_main();

#define TAG "main"

using namespace beehive;
using namespace std::chrono_literals;

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

const int SDCARD_BIT = BIT0;

void sdcard_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  EventGroupHandle_t event_group = static_cast<EventGroupHandle_t>(event_handler_arg);
  xEventGroupSetBits(event_group, SDCARD_BIT);
}

void mainloop(bool wait_for_events)
{
  if(wait_for_events)
  {
    auto event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
		      SDCARD_EVENTS,
		      ESP_EVENT_ANY_ID,
		      sdcard_event_handler, event_group, nullptr));


    const auto bits = xEventGroupWaitBits(event_group, SDCARD_BIT, pdTRUE, pdFALSE, (10s / 1ms) / portTICK_PERIOD_MS);
    if(bits & SDCARD_BIT)
    {
      ESP_LOGI(TAG, "SDCard woke us up");
    }
    else
    {
      ESP_LOGI(TAG, "Timeout woke us up");
    }

    ESP_LOGI(TAG, "Sleeping for %i seconds", beehive::appstate::sleeptime());
    esp_sleep_enable_timer_wakeup(std::chrono::seconds(beehive::appstate::sleeptime()) / 1us);
    esp_deep_sleep_start();
  }
  else
  {
    while(true)
    {
      vTaskDelay(20000 / portTICK_PERIOD_MS);
    }
  }
}

void start_mdns_service()
{
    //initialize mDNS service
    esp_err_t err = mdns_init();
    if (err) {
        printf("MDNS Init failed: %d\n", err);
        return;
    }

    //set hostname
    mdns_hostname_set(beehive::appstate::system_name().c_str());
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    std::stringstream ss;
    ss << beehive::appstate::system_name() << " webserver";
    mdns_service_instance_name_set("_http", "_tcp", ss.str().c_str());
}

} // end ns anon

void app_main()
{
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  // must be early because it initialises NVS
  beehive::appstate::init();
  setup_wifi();

  mqtt::MQTTClient mqtt_client;
  sdcard::SDCardWriter sdcard_writer;
  beehive::http::HTTPServer http_server;

  start_mdns_service();

  // it seems if I don't bind this to core 0, the i2c
  // subsystem fails randomly.
  xTaskCreatePinnedToCore(sensor_task, "sensor", 8192, NULL, uxTaskPriorityGet(NULL), NULL, 0);

  beehive::iobuttons::setup();

  beehive::events::buttons::register_button_callback(
    beehive::events::buttons::BUTTON_EVENT_OTA,
    [](beehive::events::buttons::button_events_t) {
      if(false && wifi_connected())
      {
	ESP_LOGI(TAG, "Connected to WIFI, run OTA");
	start_ota_task();
      }

      else
      {
	ESP_LOGI(TAG, "Not connected to WIFI, running smartconfig");
	beehive::smartconfig::run();
      }
    }
    );

  // right before we go into our mainloop
  // we need to broadcast our configured state.
  beehive::appstate::promote_configuration();
  mainloop(false);
}
