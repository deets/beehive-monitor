#include <http.hpp>

namespace beehive::http {

class HTTPServer {
public:
  HTTPServer();
private:
  deets::http::HTTPServer _server;
};

}
