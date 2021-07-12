#include "roland.hpp"

namespace beehive::mqtt::roland {

namespace {

const auto QOS = 1;
const auto RETAIN = 0;
const auto START = "/bienen/start";
const auto VALUE = "/bienen/value";
const auto STOP = "/bienen/stop";

struct sht3xdis_message_t
{
  struct   __attribute__ ((packed))  payload_t
  {
    uint8_t busno;
    uint8_t address;
    // The ESP32 is little endian, so that's what we are transmitting
    uint16_t humidity;
    uint16_t temperature;
  };


  sht3xdis_message_t(const beehive::events::sensors::sht3xdis_value_t& v)
    : _payload{v.busno, v.address, v.humidity, v.temperature }
  {
  }

  const char* payload() const
  {
    return (const char*)&_payload;
  }

  int payload_size() const
  {
    return sizeof(payload_t);
  }

  payload_t _payload;
};

} // namespace

void publish(const std::vector<events::sensors::sht3xdis_value_t> &readings,
             std::function < void(const char *topic, const char *data, int len,
                                  int qos, int retain)> publish)
{
  publish(START, nullptr, 0, QOS, RETAIN);
  for(const auto& entry : readings)
  {
    const auto payload = sht3xdis_message_t{entry};
    publish(VALUE, payload.payload(), payload.payload_size(), QOS, RETAIN);
  }
  publish(STOP, nullptr, 0, QOS, RETAIN);
}

} // namespace beehive::mqtt::roland
