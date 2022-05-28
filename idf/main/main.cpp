#include "freertos/projdefs.h"
#include "i2c.hh"
#include "display.hpp"
#include "buttons.hpp"
#include "wifi.hpp"
#include "mqtt.hpp"
#include "sensors.hpp"
#include "sdcard.hpp"
#include "beehive_events.hpp"
#include "beehive_http.hpp"
#include "ota.hpp"
#include "wifi-provisioning.hpp"
#include "smartconfig.hpp"
#include "appstate.hpp"
#ifdef USE_LORA
#include "lora.hpp"
#endif

#include "mqtt_client.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <esp_sntp.h>
#include <mdns.h>
#include <chrono>
#include <atomic>
#include <sstream>

extern "C" void app_main();

#define TAG "main"

using namespace beehive;
using namespace std::chrono_literals;

namespace {

const int SDCARD_BIT = BIT0;
const int MQTT_PUBLISHED_BIT = BIT1;
const auto SLEEP_CONDITION_TIMEOUT = 30s;
const auto NTP_TIMEOUT = 15;

bool s_caffeine = false;

void sensor_task(void* user_pointer)
{
  I2CHost* bus = static_cast<I2CHost*>(user_pointer);
  Sensors sensors(*bus);
  vTaskDelay(2000 / portTICK_PERIOD_MS);

  while(true)
  {
    ESP_LOGE(TAG, "sensors work");
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
  size_t message_backlog = *static_cast<size_t*>(event_data);
  // if message_backlog is empty, we set the bit to indicated
  // all messages have been served for now!
  if(message_backlog == 0)
  {
    xEventGroupSetBits(event_group, MQTT_PUBLISHED_BIT);
  }
}


void print_time()
{
  time_t now;
  char strftime_buf[64];
  struct tm timeinfo;
  time(&now);

  // Set timezone to China Standard Time
  setenv("TZ", "UTC", 1);
  tzset();
  localtime_r(&now, &timeinfo);

  strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
  ESP_LOGI(TAG, "The current date/time in UTC is: %s", strftime_buf);
  switch(sntp_get_sync_status())
  {
  case SNTP_SYNC_STATUS_RESET:
    ESP_LOGI(TAG, "SNTP_SYNC_STATUS_RESET");
    break;
  case SNTP_SYNC_STATUS_COMPLETED:
    ESP_LOGI(TAG, "SNTP_SYNC_STATUS_COMPLETED");
    break;
  case SNTP_SYNC_STATUS_IN_PROGRESS:
    ESP_LOGI(TAG, "SNTP_SYNC_STATUS_IN_PROGRESS");
    break;
  }
}


bool stay_awake()
{
  const auto mode = true; //beehive::iobuttons::mode_on();
  const auto res = s_caffeine || mode;
  print_time();
  ESP_LOGI(
    TAG,
    "stay_awake mode: %s, %s -> %s",
    (mode ? "LED is ON" : "LED is OFF"),
    (s_caffeine ? "BOOT has been pressed" : "BOOT has not been pressed"),
    (res ? "we stay awake" : "we go to sleep"));
  return res;
}


void start_ntp_service()
{
  ESP_LOGI(TAG, "Acquire time using NTP");
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, beehive::appstate::ntp_server());
  sntp_init();
  for(int i=0; i < NTP_TIMEOUT; ++i)
  {
    if(sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED)
    {
      return;
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  ESP_LOGE(TAG, "Couldn't obtain NTP date!");
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


void setup_sensor_task(I2CHost& i2c_bus)
{
  // it seems if I don't bind this to core 0, the i2c
  // subsystem fails randomly.
  xTaskCreatePinnedToCore(sensor_task, "sensor", 8192, &i2c_bus, uxTaskPriorityGet(NULL), NULL, 0);
}


void wait_or_sleep()
{
  beehive::appstate::promote_configuration();

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


void setup_buttons()
{
  deets::buttons::setup_pin(
    {
      PIN_NUM_OTA,
      deets::buttons::pull_e::UP,
      deets::buttons::irq_e::NEG,
      2000,
      std::monostate{}
    }
    );

  beehive::events::buttons::register_button_callback(
    beehive::events::buttons::OTA,
    [](beehive::events::buttons::button_events_t) {
      s_caffeine = true;
      if(deets::wifi::connected())
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
}

#ifdef USE_LORA
void run_over_lora(I2CHost &i2c_bus)
{
  beehive::lora::LoRaLink lora;
  if(beehive::lora::is_field_device())
  {
    sdcard::SDCardWriter sdcard_writer;
    beehive::http::HTTPServer http_server([&sdcard_writer]() { return sdcard_writer.file_count();});
    setup_sensor_task(i2c_bus);
    lora.setup_field_work(sdcard_writer.total_datasets_written());
    // The whole system is now event driven, whenever the
    // sensor task does anything, we produce a LoRa message.
    wait_or_sleep();
  }
  else
  {
    lora.run_base_work();
  }
}

#else // USE_LORA

void run_over_wifi(I2CHost& i2c_bus)
{
  sdcard::SDCardWriter sdcard_writer;
  // We pass the total_datasets_written as sequence number to start
  // from
  mqtt::MQTTClient mqtt_client(sdcard_writer.total_datasets_written());

  beehive::http::HTTPServer http_server([&sdcard_writer]() { return sdcard_writer.file_count();});

  setup_sensor_task(i2c_bus);
  wait_or_sleep();
}

#endif // USE_LORA

} // end ns anon

void app_main()
{
  esp_log_level_set("mqtt", ESP_LOG_DEBUG);
  esp_log_level_set("lora", ESP_LOG_DEBUG);
  esp_log_level_set("sensors", ESP_LOG_DEBUG);
  esp_log_level_set("buttons", ESP_LOG_DEBUG);
  esp_log_level_set("rf95", ESP_LOG_DEBUG);

  // Must be the first, because we heavily rely
  // on the event system!
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  I2CHost i2c_bus{0, SDA, SCL};

  #ifdef BOARD_TTGO
  Display display(i2c_bus);
  #endif

  // must be early because it initialises NVS
  beehive::appstate::init();
  setup_buttons();

  // we first need to setup wifi, because otherwise
  // MQTT fails due to missing network stack initialisation!
  deets::wifi::setup();
  start_mdns_service();
  start_ntp_service();

  #ifdef USE_LORA
  run_over_lora(i2c_bus);
  #else
  run_over_wifi(i2c_bus);
  #endif
}
