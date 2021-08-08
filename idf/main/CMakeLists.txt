
set(srcs
  main.cpp
  mqtt.hpp
  mqtt.cpp
  ota.hpp
  ota.cpp
  io-buttons.hpp
  io-buttons.cpp
  sensors.hpp
  sensors.cpp
  roland.hpp
  roland.cpp
  sdcard.hpp
  sdcard.cpp
  smartconfig.hpp
  smartconfig.cpp
  #wifi-provisioning.hpp
  #wifi-provisioning.cpp
  beehive_events.hpp
  beehive_events.cpp
  beehive_http.hpp
  beehive_http.cpp
  )

idf_component_register(
  SRCS ${srcs}
  INCLUDE_DIRS "."
  REQUIRES esp_http_server nvs_flash mqtt fatfs esp32deets app_update esp_http_client esp_https_ota
  EMBED_TXTFILES ${project_dir}/server_certs/ca_cert.pem
  )
