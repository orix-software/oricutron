#!/bin/bash
gcc -g -I ../../ -fPIC -shared  $(PKG_CONFIG_PATH=/usr/lib64/pkgconfig pkg-config sdl --cflags) -o libdebug.so debug.c

