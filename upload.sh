#!/bin/bash
PORT=/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0
SRC=src/*.py

for f in $SRC
do
    echo $f
    pipenv run ampy --port $PORT put $f
done
