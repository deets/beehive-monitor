#include "roland.hpp"
#include "appstate.hpp"

#include <iterator>
#include <sstream>
#include <iomanip>
#include <type_traits>
#include <algorithm>

namespace beehive::mqtt::roland {

namespace {

const auto QOS = 1;
const auto RETAIN = 0;
const auto TOPIC = "B-value";

void publish_one_message(
    const size_t counter,
    const std::vector<events::sensors::sht3xdis_value_t> &readings,
    std::function<void(const char *topic, const char *data, int len, int qos,
                       int retain)>
    publish,
    const std::string& column_suffix
  ) {
  std::stringstream ss;
  size_t readings_count = 0;

  ss << beehive::appstate::system_name() << column_suffix << "," << counter << ":";

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
  publish(TOPIC, payload.c_str(), payload.size(), QOS, RETAIN);
}

} // namespace

void publish(const size_t counter, const std::vector<events::sensors::sht3xdis_value_t> &readings,
             std::function < void(const char *topic, const char *data, int len,
                                  int qos, int retain)> publish)
{
  publish_one_message(counter, readings, publish, "");
}

} // namespace beehive::mqtt::roland
