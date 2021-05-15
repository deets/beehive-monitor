#!/bin/bash
set -e
PORT=/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0

utemplate=$(realpath modules/utemplate/utemplate_util.py)
(cd src; rm *_tpl.py; export PYTHONPATH=`pwd`; for t in "*.tpl"; do echo compiling $t; python3 $utemplate compile $t; done)

src="src/*.py src/*.json"

for f in $src
do
    echo "$f"
    pipenv run ampy --port $PORT put $f
done
