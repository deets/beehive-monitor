// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#pragma once

#include "beehive_events.hpp"

#include "sdcard.hpp"
#include "sdmmc_cmd.h"

namespace beehive::sdcard {

class SDCardWriter
{
public:
  SDCardWriter();
  ~SDCardWriter();

  size_t total_datasets_written() const { return _total_datasets_written; }

private:

  static void s_sensor_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
  void sensor_event_handler(esp_event_base_t base, beehive::events::sensors::sensor_events_t id, void* event_data);
  void setup_file_info();
  void file_rotation();
  void report_file_size();
  void count_datasets_written();

  sdmmc_card_t* _card;
  sdmmc_host_t _host;

  FILE* _file;
  std::string _filename;
  // number of the filename to write to
  size_t _filename_index;
  // number of datasets written in one file.
  size_t _datasets_written;
  // a globally running number for all datasets
  size_t _total_datasets_written;
};

} // namespace beehive::sdcard
