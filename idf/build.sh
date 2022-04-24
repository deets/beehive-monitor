#!/bin/bash
. ~esp/v4.4/export.sh
idf.py build -DTTGO:BOOL=ON
