
set(srcs
  main.cpp
  mqtt.hpp
  mqtt.cpp
  sensors.hpp
  sensors.cpp
  roland.hpp
  roland.cpp
  sdcard.hpp
  sdcard.cpp
  beehive_events.hpp
  beehive_events.cpp
  beehive_http.hpp
  beehive_http.cpp
  )

idf_component_register(
  SRCS ${srcs}
  INCLUDE_DIRS "."
  REQUIRES esp_http_server nvs_flash mqtt fatfs esp32deets
  )
