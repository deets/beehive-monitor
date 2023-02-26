#!/bin/bash
base=$(dirname "$0")
. $base/common.sh

files="beehive.bin beehive.elf bootloader/bootloader.bin ota_data_initial.bin partition_table/partition-table.bin"

dest="${1}${SUFFIX}"

if [ ! -d "$dest" ]
then
    echo "No such directory: '$dest'"
    exit 1
fi


for f in $files
do
    fp="${BUILD_DIR}/${f}"
    if [ ! -e "$fp" ]
    then
        echo "No such file '$fp'"
        exit 1
    fi
done

for f in $files
do
    fp="$BUILD_DIR/$f"
    if [ "$f" == "beehive.bin" ]
    then
          cp -v "$fp" "$dest/beehive${SUFFIX}.bin"
    elif [ "$f" == "beehive.elf" ]
    then
          cp -v "$fp" "$dest/beehive${SUFFIX}.elf"
    else
        cp -v "$fp" "$dest"
    fi
done
