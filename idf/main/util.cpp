// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#include "util.hpp"

#include <iomanip>
#include <chrono>

namespace beehive::util {

std::string isoformat()
{
  std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::stringstream ss;
  ss << std::put_time( std::localtime( &t ), "%FT%T%z" );
  return ss.str();
}

}
