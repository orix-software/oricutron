#!/bin/bash
gcc -g -I ../../ -fPIC -shared  $(PKG_CONFIG_PATH=/usr/lib64/pkgconfig pkg-config sdl --cflags) -o libch376.so plugin.c ch376.c

