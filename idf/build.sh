#!/bin/bash
. ~esp/v4.2/export.sh
idf.py build -DTTGO:BOOL=ON
