// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#include "beehive_http.hpp"
#include "appstate.hpp"
#include "beehive_events.hpp"

#include "nlohmann/json.hpp"

namespace beehive::http {

using json = nlohmann::json;

HTTPServer::HTTPServer()
{
  _server.set_cors("*", 600);
  _server.register_handler(
    "/", HTTP_GET,
    [](httpd_req_t* req) -> esp_err_t {
      json j2 = {
	{"pi", 3.141},
	{"happy", true},
	{"name", "Niels"},
	{"nothing", nullptr},
	{"answer", {
	    {"everything", 42}
	  }},
	{"list", {1, 0, 2}},
	{"object", {
	    {"currency", "USD"},
	    {"value", 42.99}
	  }}
      };
      const auto s = j2.dump();
      httpd_resp_send(req, s.c_str(), s.size());
      return ESP_OK;
    });
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

      json j2 = {
	{"status", "ok"}
      };
      return j2;
    });

  _server.start();
}

}
