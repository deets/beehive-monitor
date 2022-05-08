#!/bin/bash
. $(dirname $0)/common.sh

build_dir=""
if [ "$BOARD" == "TTGO" ]
then
    board_option=-DTTGO:BOOL=ON
    build_dir="build-ttgo"
else
    board_option=-DNODEMCU:BOOL=ON
    build_dir="build-nodemcu"
fi

idf.py -B "$build_dir" build "$board_option"
