#!/bin/bash
gcc -g -I ../../ -fPIC -shared  $(PKG_CONFIG_PATH=/usr/lib64/pkgconfig pkg-config sdl --cflags) -o libds1501.so ds1501.c

