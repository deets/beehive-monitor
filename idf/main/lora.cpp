// Copyright: 2022, Diez B. Roggisch, Berlin, all rights reserved
#ifdef USE_LORA
#include "lora.hpp"
#include "pins.hpp"
#include "beehive_events.hpp"
#include "sht3xdis.hpp"
#include "mqtt.hpp"
#include "appstate.hpp"

#include "esp_mac.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

#define TAG "lora"

namespace {

std::array<std::array<uint8_t, 6>, 2> LORA_SENDER_MACS = {
  {
    {0x30, 0x83, 0x98, 0xdc, 0xca, 0xfc}, // Heiko's
    {0x4c, 0x75, 0x25, 0xc3, 0x5e, 0xbc} // Diez
  }
};


}

namespace beehive::lora {

bool is_field_device()
{
  std::array<uint8_t, 6> mac;
  esp_read_mac(mac.data(), ESP_MAC_WIFI_STA);
  for(const auto& field_mac : LORA_SENDER_MACS)
  {
    if(field_mac == mac)
    {
      return true;
    }
  }
  return false;
}


LoRaLink::LoRaLink()
  : _lora(VSPI_HOST, LORA_CS, LORA_SCLK, LORA_MOSI, LORA_MISO, LORA_SPI_SPEED, LORA_DI0, beehive::appstate::lora_dbm())
{
  // Set our own custom syncword (BEeehive)
  _lora.sync_word(0xBE);
  ESP_ERROR_CHECK(esp_event_handler_instance_register(CONFIG_EVENTS, esp_mqtt_event_id_t(ESP_EVENT_ANY_ID), LoRaLink::s_config_event_handler, this, NULL));
}


LoRaLink::~LoRaLink()
{
}


void LoRaLink::setup_field_work(size_t sequence_num)
{
  _sequence_num = sequence_num;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
                    SENSOR_EVENTS,
                    beehive::events::sensors::SHT3XDIS_READINGS,
                    LoRaLink::s_sensor_event_handler,
                    this, NULL
                    )
    );
}


void LoRaLink::s_sensor_event_handler(void *handler_args,
                                        esp_event_base_t base, int32_t id,
                                        void *event_data) {

  static_cast<LoRaLink*>(handler_args)->sensor_event_handler(base, beehive::events::sensors::sensor_events_t(id), event_data);
}

void LoRaLink::sensor_event_handler(
    esp_event_base_t base, beehive::events::sensors::sensor_events_t id,
    void *event_data)
{
  const auto readings = beehive::events::sensors::receive_readings(id, event_data);
  if (readings) {
    ESP_LOGD(TAG, "Received sensor message, creating LoRa Message");
    ++_sequence_num;
    size_t offset = 0;
    // We have a current maximum of 8 sensors with 6 bytes of information
    // in total. So we should be safe to carry these + some protocol overhead.
    std::array<uint8_t, 128> data; // Currently a hard limit instead of FIFO size
    std::copy(&_sequence_num, &_sequence_num + sizeof(_sequence_num), data.data() + offset);
    offset += sizeof(_sequence_num);
    data[offset++] = 1; // Running number
    data[offset++] = readings->size();
    for(const auto& reading : *readings)
    {
      data[offset++] = reading.busno;
      data[offset++] = reading.address;
      data[offset++] = reading.raw_humidity & 0xff;
      data[offset++] = reading.raw_humidity >> 8;
      data[offset++] = reading.raw_temperature & 0xff;
      data[offset++] = reading.raw_temperature >> 8;
    }
    _lora.send(data.data(), offset, 10000);
    ++_package_count;
    beehive::events::lora::send_stats(_package_count, _malformed_package_count);
  }
}

void LoRaLink::run_base_work()
{
  using namespace beehive::events::sensors;
  using namespace sht3xdis;

  while(true)
  {
    std::array<uint8_t, RF95::FIFO_SIZE> data;
    const auto bytes_received = _lora.recv(data);

    // No bytes means stray package or something similar
    if(bytes_received == 0)
    {
      continue;
    }
    ++_package_count;
    // Sanity check our packet
    auto valid = false;
    if(bytes_received >= 6)
    {
      const auto readings_count = data[5];
      // 6 bytes preamble with seq_no, running no and then
      // 6 bytes per reading
      valid = bytes_received == 6 + 6 * readings_count;
    }

    if(valid)
    {
      // I can't use the following, but have to do this using the
      // awkward for loop. If not the CPU pukes with some memory
      // alignment issue.
      //std::copy(data.data(), data.data() + sizeof(_sequence_num), &_sequence_num);
      size_t sno = 0;
      for(size_t i=0; i < 4; ++i)
      {
        sno |= data[i] << i;
      }
      _sequence_num = sno;
      //const auto running_number = data[4];
      const auto readings_count = data[5];
      size_t base_offset = 6;
      std::vector<sht3xdis_value_t> readings(readings_count);
      for(size_t i=0; i < readings_count; ++i)
      {
        const auto offset = base_offset + i * 6;
        sht3xdis_value_t reading;
        reading.busno = data[offset];
        reading.address = data[offset + 1];
        reading.raw_humidity = data[offset + 2] | (data[offset + 3] << 8);
        reading.raw_temperature = data[offset + 4] | (data[offset + 5] << 8);
        reading.humidity = SHT3XDIS::raw2humidity(reading.raw_humidity);
        reading.temperature = SHT3XDIS::raw2humidity(reading.raw_temperature);
        readings[i] = reading;
      }
      ESP_LOGI(TAG, "Received sensor readings, s-no: %d, readings: %d", _sequence_num, readings_count);
      if(!_mqtt)
      {
        _mqtt = std::unique_ptr<beehive::mqtt::MQTTClient>(new beehive::mqtt::MQTTClient(_sequence_num));
      }
      beehive::events::sensors::send_readings(readings);
    }
    else
    {
      ++_malformed_package_count;
      ESP_LOGE(TAG, "Received malformed package no %d, bytes received: %d", _malformed_package_count, bytes_received);
    }
    beehive::events::lora::send_stats(_package_count, _malformed_package_count);
  }
}

void LoRaLink::s_config_event_handler(void *handler_args,
                                        esp_event_base_t event_base, int32_t event_id,
                                        void *event_data)
{
  static_cast<LoRaLink*>(handler_args)->config_event_handler(event_base, beehive::events::config::config_events_t(event_id), event_data);
}


void LoRaLink::config_event_handler(esp_event_base_t base, beehive::events::config::config_events_t id, void* event_data)
{
  switch(id)
  {
  case beehive::events::config::LORA_DBM:
    _lora.tx_power((int)*(uint32_t*)event_data);
    break;
    // All ignored
  case beehive::events::config::SLEEPTIME:
  case beehive::events::config::MQTT_HOST:
  case beehive::events::config::SYSTEM_NAME:
    break;
  }
}

} // namespace beehive::lora


#endif // USE_LORA
