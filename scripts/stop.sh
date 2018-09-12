#!/bin/bash

DIR=$(pwd)
export LD_LIBRARY_PATH=$DIR/lib
./sbin/nginx -s stop -p $DIR