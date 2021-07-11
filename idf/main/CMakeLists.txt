set(srcs
  main.cpp
  mqtt.hpp
  mqtt.cpp
  sensors.hpp
  sensors.cpp
  beehive_events.hpp
  beehive_events.cpp
  )

idf_component_register(
  SRCS ${srcs}
  INCLUDE_DIRS "."
  REQUIRES esp32deets nvs_flash mqtt
  )
