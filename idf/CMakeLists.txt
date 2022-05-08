# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
set (CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fconcepts")

set(TTGO OFF CACHE BOOL "Use TTGO LoRa32")
set(NODEMCU OFF CACHE BOOL "Use NodeMCU")
set(LORA OFF CACHE BOOL "Use LORA (Only on LoRa32)")

if(TTGO)
  add_definitions(-DBOARD_TTGO)
  if(LORA)
    add_definitions(-DUSE_LORA)
  endif()
endif()

if(NODEMCU)
  add_definitions(-DBOARD_NODEMCU)
  if(LORA)
    message(FATAL_ERROR "Can't use LoRa on NodeMCU")
  endif()
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(COMPILE_OPTIONS "-DU8X8_WITH_USER_PTR" APPEND)
project(beehive)
