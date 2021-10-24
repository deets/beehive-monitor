#include <http.hpp>

namespace beehive::http {

class HTTPServer {
public:
  HTTPServer(std::function<size_t()> file_count);
private:
  deets::http::HTTPServer _server;
  std::function<size_t()> _file_count;
};

}
