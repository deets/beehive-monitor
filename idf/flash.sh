#!/bin/bash
. $(dirname $0)/common.sh

for port in /dev/serial/by-id/usb-1a86_USB_Single_Serial_*
do
    idf.py -B "$BUILD_DIR" flash -p $port
done
