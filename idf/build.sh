#!/bin/bash
. $(dirname $0)/common.sh

build_dir=""
if [ "$BOARD" == "TTGO" ]
then
    board_option=-DTTGO:BOOL=ON
    lora_option=-DLORA:BOOL=ON
    idf.py -B "$BUILD_DIR" build "$board_option" "$lora_option"
else
    board_option=-DNODEMCU:BOOL=ON
    idf.py -B "$BUILD_DIR" build "$board_option"
fi
