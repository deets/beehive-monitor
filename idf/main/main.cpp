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

#include "mqtt_client.h"
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

const int SDCARD_BIT = BIT0;
const int MQTT_PUBLISHED_BIT = BIT1;
const auto SLEEP_CONDITION_TIMEOUT = 30s;

bool s_caffeine = false;

void sensor_task(void*)
{
  Sensors sensors;
  vTaskDelay(2000 / portTICK_PERIOD_MS);

  while(true)
  {
    sensors.work();
    vTaskDelay((std::chrono::seconds(beehive::appstate::sleeptime()) / 1ms) / portTICK_PERIOD_MS);
  }
}


void sdcard_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  EventGroupHandle_t event_group = static_cast<EventGroupHandle_t>(event_handler_arg);
  xEventGroupSetBits(event_group, SDCARD_BIT);
}

void mqtt_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  EventGroupHandle_t event_group = static_cast<EventGroupHandle_t>(event_handler_arg);
  xEventGroupSetBits(event_group, MQTT_PUBLISHED_BIT);
}


bool stay_awake()
{
  const auto mode = beehive::iobuttons::mode_on();
  const auto res = s_caffeine || mode;
  ESP_LOGI(
    TAG,
    "stay_awake mode: %s, s_caffeine: %s -> %s",
    (mode ? "LED is ON" : "LED is OFF"),
    (s_caffeine ? "BOOT has been pressed" : "BOOT has not been pressed"),
    (res ? "we stay awake" : "we go to sleep"));
  return res;
}

void mainloop()
{
  while(true)
  {
    while(stay_awake())
    {
      vTaskDelay(20000 / portTICK_PERIOD_MS);
    }
    auto event_group = xEventGroupCreate();
    // Any SDCard event means either we could successfully
    // write *or* there was an unrecoverable error - so we
    // don't discriminate and just set the bit always
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
		      SDCARD_EVENTS,
		      ESP_EVENT_ANY_ID,
		      sdcard_event_handler, event_group, nullptr));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
		      BEEHIVE_MQTT_EVENTS,
		      beehive::events::mqtt::PUBLISHED,
		      mqtt_event_handler, event_group, nullptr));


    const auto bits = xEventGroupWaitBits(event_group, SDCARD_BIT | MQTT_PUBLISHED_BIT, pdTRUE, pdTRUE, (SLEEP_CONDITION_TIMEOUT / 1ms) / portTICK_PERIOD_MS);
    // the bits that arrived are still set! So even if we timeout,
    // some bits are set.
    if((bits & SDCARD_BIT) && (bits & MQTT_PUBLISHED_BIT))
    {
      ESP_LOGI(TAG, "SDCard & MQTT woke us up");
    }
    else
    {
      ESP_LOGI(TAG, "Timeout woke us up");
    }
    if(!stay_awake())
    {
      ESP_LOGI(TAG, "Sleeping for %i seconds", beehive::appstate::sleeptime());
      esp_sleep_enable_timer_wakeup(std::chrono::seconds(beehive::appstate::sleeptime()) / 1us);
      esp_deep_sleep_start();
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
  beehive::iobuttons::setup();

  beehive::events::buttons::register_button_callback(
    beehive::events::buttons::OTA,
    [](beehive::events::buttons::button_events_t) {
      s_caffeine = true;
      if(wifi_connected())
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

  beehive::events::buttons::register_button_callback(
    beehive::events::buttons::MODE,
    [](beehive::events::buttons::button_events_t) {
      ESP_LOGI(TAG, "MODE switch: %s", (beehive::iobuttons::mode_on() ? "ON" : "OFF"));
    }
    );

  setup_wifi();
  start_mdns_service();
  // we first need to setup wifi, because otherwise
  // MQTT fails due to missing network stack initialisation!
  sdcard::SDCardWriter sdcard_writer;
  mqtt::MQTTClient mqtt_client(sdcard_writer.total_datasets_written());

  beehive::http::HTTPServer http_server;

  // right before we go into our mainloop
  // we need to broadcast our configured state.
  beehive::appstate::promote_configuration();

  // it seems if I don't bind this to core 0, the i2c
  // subsystem fails randomly.
  xTaskCreatePinnedToCore(sensor_task, "sensor", 8192, NULL, uxTaskPriorityGet(NULL), NULL, 0);

  mainloop();
}
