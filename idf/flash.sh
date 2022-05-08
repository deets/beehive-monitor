#!/bin/bash
. $(dirname $0)/common.sh

idf.py -B "$BUILD_DIR" flash
