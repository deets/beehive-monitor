#!/bin/bash

release="$1"
files="beehive.bin bootloader.bin ota_data_initial.bin partition-table.bin"

if [ "$release" == "-h" ]
then
    echo "Usage: $0 <relative-path-to-release>"
    exit 0
fi

if esptool.py -h >> /dev/null
then
    esptool.py -b460800 --before default_reset --after hard_reset --chip esp32  write_flash --flash_mode dio --flash_size detect --flash_freq 40m \
               0x1000 "$release/bootloader.bin" \
               0x8000 "$release/partition-table.bin" \
               0xf000 "$release/ota_data_initial.bin" \
               0x20000 "$release/beehive.bin"
else
    echo "No esptool.py found, have you setup the IDF?"
    exit 1
fi

#/home/deets/.espressif/python_env/idf4.2_py3.8_env/bin/python ../../../../../esp/v4.2/components/esptool_py/esptool/esptool.py -p (PORT) -b 460800 --before default_reset --after hard_reset --chip esp32  write_flash --flash_mode dio --flash_size detect --flash_freq 40m 0x1000 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0xf000 build/ota_data_initial.bin 0x20000 build/beehive.bin
