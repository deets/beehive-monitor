#!/bin/bash
. $(dirname $0)/common.sh

idf.py build -DTTGO:BOOL=ON
