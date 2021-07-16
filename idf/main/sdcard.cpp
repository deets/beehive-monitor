// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved
#include "sdcard.hpp"
#include "pins.hpp"

#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "hal/gpio_types.h"
#include "hal/spi_types.h"
#include "sdkconfig.h"
#include <iostream>
#include <sstream>
#include <iomanip>

//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

namespace beehive::sdcard {

namespace {

static const char *TAG = "sdcard";

#define MOUNT_POINT "/sdcard"
static const char* s_mount_point  = "/sdcard";

#define FILE_PREFIX "BEE" // must be upper-case
#define DATAPOINTS_PER_DAY (12 * 24) // every 5 minutes, 24h a day

// DMA channel to be used by the SPI peripheral
#ifndef SPI_DMA_CHAN
#define SPI_DMA_CHAN    1
#endif //SPI_DMA_CHAN



} // namespace

SDCardWriter::SDCardWriter()
  : _file(nullptr)
  , _filename_index(0)
  , _datapoint_index(0)
  , _datapoints_written(0)
{
    esp_err_t ret;
    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    ESP_LOGI(TAG, "Initializing SD card");

    _host = sdmmc_host_t SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
	.flags = 0,
	.intr_flags = 0,
    };
    ret = spi_bus_initialize(spi_host_device_t(_host.slot), &bus_cfg, SPI_DMA_CHAN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return;
    }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = spi_host_device_t(_host.slot);

    ret = esp_vfs_fat_sdspi_mount(s_mount_point, &_host, &slot_config, &mount_config, &_card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return;
    }
    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, _card);

    ESP_ERROR_CHECK(esp_event_handler_instance_register(SENSOR_EVENTS, beehive::events::sensors::SENSOR_EVENT_SHT3XDIS_READINGS, SDCardWriter::s_sensor_event_handler, this, NULL));
    setup_file_info();
}

SDCardWriter::~SDCardWriter()
{
    // All done, unmount partition and disable SDMMC or SPI peripheral
    esp_vfs_fat_sdcard_unmount(s_mount_point, _card);
    ESP_LOGI(TAG, "Card unmounted");
    spi_bus_free(spi_host_device_t(_host.slot));
}

void SDCardWriter::setup_file_info()
{
  const auto prefix_len = strlen(FILE_PREFIX);

  struct dirent *ep;
  auto dp = opendir(s_mount_point);

  if (dp != NULL)
  {
    while ((ep = readdir(dp)))
    {
      const auto entry_len = strnlen(ep->d_name, 256);
      const auto compare_len = std::min(entry_len, prefix_len);
      ESP_LOGD(TAG, "Found file: '%s', compare length: %i, entry_len: %i", ep->d_name, compare_len, entry_len);
      if(strncmp(FILE_PREFIX, ep->d_name, compare_len) == 0)
      {
	ESP_LOGD(TAG, "Found beehive file %s", ep->d_name);
	_filename_index = std::max(
	  _filename_index,
	  size_t(std::stoul(std::string(&ep->d_name[prefix_len]), nullptr, 16) + 1)
	);
	// In a first approximation, we derive the datapoint index from the
	// number of entries per day
	_datapoint_index = _filename_index * DATAPOINTS_PER_DAY;
      }
    }
    closedir(dp);
  }
  else
  {
    ESP_LOGE(TAG, "Couldn't open %s in setup_file_info", s_mount_point);
  }
}

void SDCardWriter::report_file_size()
{
  struct stat statbuf;
  stat(_filename.c_str(), &statbuf);
  ESP_LOGD(TAG, "File %s size %li", _filename.c_str(), statbuf.st_size);
}

void SDCardWriter::file_rotation() {
  if(_file)
  {
    fclose(_file);
    _file = nullptr;

    report_file_size();

    if(_datapoints_written > DATAPOINTS_PER_DAY)
    {
      _datapoints_written = 0;
    }
  }
  if(!_file)
  {
    if(_datapoints_written == 0)
    {
      std::stringstream ss;
      const auto digits = 8 - strlen(FILE_PREFIX);
      const auto mask = (1 << (4 * digits)) - 1;
      ss << MOUNT_POINT << "/" << std::string(FILE_PREFIX) << std::hex << std::setw(digits) << std::setfill('0') << (++_filename_index & mask)  << ".txt";
      _filename = ss.str();
    }

    const auto mode = _datapoints_written == 0 ? "w" : "a";
    if(_datapoints_written == 0)
    {
      ESP_LOGI(TAG, "Creating SD card log file '%s'", _filename.c_str());
    }
    ESP_LOGD(TAG, "log file mode: '%s'", mode);

    _file = fopen(_filename.c_str(), mode);

    if(!_file)
    {
      ESP_LOGE(TAG, "error opening file %s: %i, %s", _filename.c_str(), errno, strerror(errno));
    }

    report_file_size();

    if(_datapoints_written == 0)
    {
      fprintf(_file, "SEQUENCE,BN,SA,HUMI,TEMP\r\n");
    }
  }
}


void SDCardWriter::s_sensor_event_handler(void *handler_args,
                                        esp_event_base_t base, int32_t id,
                                        void *event_data) {

  static_cast<SDCardWriter*>(handler_args)->sensor_event_handler(base, beehive::events::sensors::sensor_events_t(id), event_data);
}

void SDCardWriter::sensor_event_handler(esp_event_base_t base, beehive::events::sensors::sensor_events_t id, void* event_data)
{
  const auto readings = beehive::events::sensors::receive_readings(id, event_data);
  file_rotation();
  if(readings)
  {
    if(_file)
    {
      ESP_LOGD(TAG, "Writing data to the sdcard");
      std::stringstream ss;

      for(const auto& reading : *readings)
      {
	++_datapoints_written;
	ss << std::hex << std::setw(8) << std::setfill('0') << _datapoint_index << ",";
	ss << std::hex << std::setw(2) << std::setfill('0') << int(reading.busno) << ",";
	ss << std::hex << std::setw(2) << std::setfill('0') << int(reading.address) << ",";
	ss << std::hex << std::setw(4) << std::setfill('0') << int(reading.humidity) << ",";
	ss << std::hex << std::setw(4) << std::setfill('0') << int(reading.temperature) << "\r\n";
      }
      fprintf(_file, ss.str().c_str());
    }
  }
}

} // namespace beehive::sdcard
