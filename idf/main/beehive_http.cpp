// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#include "beehive_http.hpp"
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
    "/json", HTTP_GET,
    [](const json&) -> json {
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
      return j2;
    });
  _server.start();
}

}
