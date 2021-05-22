#!/bin/bash
set -e
PORT=/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0

utemplate=$(realpath modules/utemplate/utemplate_util.py)
(cd src; rm -f *_tpl.py; export PYTHONPATH=`pwd`; for t in "*.tpl"; do echo compiling $t; python3 $utemplate compile $t; done)

src="src/*.py src/*.json"

for f in $src
do
    hash=$(cat $f | sha1sum | awk '{ print $ 1}')
    hashfile="$(dirname $f)/.$(basename $f).hash"
    if [ ! -e "$hashfile" ] || [ "$(<$hashfile)" != $hash ]
    then
	echo "$f"
	pipenv run ampy --port $PORT put $f
	echo $hash > $hashfile
    fi
done
