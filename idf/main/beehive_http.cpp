// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#include "beehive_http.hpp"
#include "appstate.hpp"
#include "beehive_events.hpp"

#include "http.hpp"
#include "nlohmann/json.hpp"

namespace beehive::http {

namespace {

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

} // namespace


using json = nlohmann::json;

HTTPServer::HTTPServer(std::function<size_t()> file_count)
  : _file_count(file_count)
{
  _server.set_cors("*", 600);
  _server.register_handler(
    "/",
    const_cast<uint8_t*>(index_html_start),
    const_cast<uint8_t*>(index_html_end),
    deets::http::ContentType::text_html
    );

  _server.register_handler(
    "/configuration", HTTP_POST,
    [](const json& body) -> json {
      if(body.contains("mqtt_hostname") && body["mqtt_hostname"].is_string())
      {
	const auto hostname = body["mqtt_hostname"].get<std::string>();
	beehive::appstate::set_mqtt_host(hostname);
      }
      if(body.contains("system_name") && body["system_name"].is_string())
      {
	const auto system_name = body["system_name"].get<std::string>();
	beehive::appstate::set_system_name(system_name);
      }
      if(body.contains("sleeptime") && body["sleeptime"].is_number())
      {
	const auto sleeptime = body["sleeptime"].get<uint32_t>();
	beehive::appstate::set_sleeptime(sleeptime);
      }
#ifdef USE_LORA
      if(body.contains("lora_dbm") && body["lora_dbm"].is_number())
      {
	const auto lora_dbm = body["lora_dbm"].get<uint32_t>();
	beehive::appstate::set_lora_dbm(lora_dbm);
      }
#endif // USE_LORA

      json j2 = {
	{"status", "ok"}
      };
      return j2;
    });

  _server.register_handler(
    "/configuration", HTTP_GET,
    [](const json& body) -> json {
      json j2 = {
#ifdef USE_LORA
	{"lora_dbm", beehive::appstate::lora_dbm()},
#endif // USE_LORA
	{"sleeptime", beehive::appstate::sleeptime()},
	{"system_name", beehive::appstate::system_name()},
	{"app_version", beehive::appstate::version()},
	{"mqtt_hostname", beehive::appstate::mqtt_host()}
      };
      return j2;
    });

  _server.register_handler(
    "/status", HTTP_GET,
    [this](const json& body) -> json {
      json j2 = {
	{"file-count", _file_count() }
      };
      return j2;
    });

  _server.start();
}

}
