#!/bin/bash
. $(dirname $0)/common.sh

idf.py -B "$build_dir" flash
