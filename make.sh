#!/bin/sh
automake
autoconf
./configure "$@"
make
