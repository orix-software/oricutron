SDL_LIB ?= sdl
INCLUDE_DIR = ../..

CC=gcc
CFLAGS=-g -fPIC -W -Wall -pedantic -I $(INCLUDE_DIR) $(shell PKG_CONFIG_PATH=/usr/lib64/pkgconfig pkg-config $(SDL_LIB) --cflags)
LDFLAGS=-shared

PLUGIN :=  lib$(notdir $(shell pwd)).so

SRC= $(wildcard *.c)
OBJ= $(SRC:.c=.o)

all: $(PLUGIN)

$(PLUGIN): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

.PHONY: clean mrporper

clean:
	rm -rf *.o

mrproper: clean
	rm -rf $(PLUGIN)



