# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
set (CMAKE_CXX_STANDARD 17)

set(TTGO OFF CACHE BOOL "Use TTGO LoRa32")
set(NODEMCU OFF CACHE BOOL "Use NodeMCU")

if(TTGO)
  add_definitions(-DBOARD_TTGO)
endif()
if(NODEMCU)
  add_definitions(-DBOARD_NODEMCU)
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(beehive)
