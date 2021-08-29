#include "roland.hpp"
#include "appstate.hpp"

#include <sstream>
#include <iomanip>

namespace beehive::mqtt::roland {

namespace {

const auto QOS = 1;
const auto RETAIN = 0;
const auto VALUE = "B-value";

} // namespace

void publish(const size_t counter, const std::vector<events::sensors::sht3xdis_value_t> &readings,
             std::function < void(const char *topic, const char *data, int len,
                                  int qos, int retain)> publish)
{
  std::stringstream ss;
  size_t readings_count = 0;

  ss << beehive::appstate::system_name() << "," << counter << ":";

  for(const auto& entry : readings)
  {
    ss << std::hex << std::setw(2) << std::setfill('0') << int(entry.busno);
    ss << std::hex << std::setw(2) << std::setfill('0') << int(entry.address) << ",";
    ss << entry.temperature << "," << entry.humidity;
    if(++readings_count < readings.size())
    {
      ss << ":";
    }
  }
  const auto payload = ss.str();
  publish(VALUE, payload.c_str(), payload.size(), QOS, RETAIN);
}

} // namespace beehive::mqtt::roland
