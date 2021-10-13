#!/bin/bash

files="beehive.bin bootloader/bootloader.bin ota_data_initial.bin partition_table/partition-table.bin"

dest=$1

if [ ! -d "$dest" ]
then
    echo "No such directory: '$dest'"
    exit 1
fi

base=$(dirname "$0")

for f in $files
do
    fp="$base/build/$f"
    if [ ! -e "$fp" ]
    then
        echo "No such file '$f'"
        exit 1
    fi
done

for f in $files
do
    fp="$base/build/$f"
    cp "$fp" "$dest"
done
