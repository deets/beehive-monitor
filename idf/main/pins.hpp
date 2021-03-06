// Copyright: 2021, Diez B. Roggisch, Berlin, all rights reserved

#pragma once

#include "hal/gpio_types.h"

#if defined(BOARD_NODEMCU) && defined(BOARD_TTGO)
#error "Define only one of NODEMCU or TTGO"
#endif
#if !defined(BOARD_TTGO) && !defined(BOARD_NODEMCU)
#error "Define one of NODEMCU or TTGO"
#endif

// Pin mapping when using SPI mode.
// With this mapping, SD card can be used both in SPI and 1-line SD mode.
// Note that a pull-up on CS line is required in SD mode.
#define SDCARD_MISO gpio_num_t(2)
#define SDCARD_MOSI gpio_num_t(15)
#define SDCARD_CLK  gpio_num_t(14)
#define SDCARD_CS   gpio_num_t(13)

#define PIN_NUM_OTA gpio_num_t(0)

#ifdef BOARD_NODEMCU
#define PIN_NUM_MODE gpio_num_t(25)

#define SDA gpio_num_t(26)
#define SCL gpio_num_t(27)

#endif // BOARD_NODEMCU

#ifdef BOARD_TTGO
#define PIN_NUM_MODE gpio_num_t(34)

#define SDA gpio_num_t(21)
#define SCL gpio_num_t(22)

#ifdef USE_LORA
#define LORA_MOSI gpio_num_t(27)
#define LORA_SCLK gpio_num_t(5)
#define LORA_CS gpio_num_t(18)
#define LORA_DI0 gpio_num_t(26)
#define LORA_RST gpio_num_t(23)
#define LORA_MISO gpio_num_t(19)
#define LORA_SPI_SPEED 2000000
#endif // USE_LORA

#endif // BOARD_TTGO
