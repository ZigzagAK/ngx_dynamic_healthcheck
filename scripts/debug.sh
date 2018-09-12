#!/bin/bash

ulimit -c unlimited
DIR=$(pwd)
export LD_LIBRARY_PATH=$DIR/lib
./sbin/nginx.debug -p $DIR
