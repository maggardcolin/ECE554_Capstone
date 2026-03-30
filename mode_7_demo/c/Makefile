.PHONY: all build run hardware software assets clean

CC ?= gcc
OS := $(shell uname -s)
SDLFLAGS := $(shell sdl2-config --cflags --libs)
RTLIBRARY := $(if $(filter $(OS),Linux),-lrt,)
IM_CONVERT := $(shell command -v magick >/dev/null 2>&1 && echo "magick" || echo "convert")

CFLAGS ?= -O3 -Wall -Wextra
INCLUDES := -I.
HW_LIBS := $(SDLFLAGS) $(RTLIBRARY)
SW_LIBS := -lm $(RTLIBRARY)

all: run

build: assets hardware software

run: build
	./hw_sim & sleep 0.5
	./mode7_demo

assets: map3.rgba map2.rgba sky.rgba

map3.rgba: map3.png
	$(IM_CONVERT) $< -resize 1024x1024\! -alpha on rgba:$@

map2.rgba: map2.png
	$(IM_CONVERT) $< -resize 1024x1024\! -alpha on rgba:$@

sky.rgba: sky.jpg
	$(IM_CONVERT) $< -resize 1024x1024\! -alpha on rgba:$@

hardware: hw_sim

software: mode7_demo

kill:
	pkill -9 hw_sim || true

hw_sim: hw_sim.c hw_contract.h
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ hw_sim.c $(HW_LIBS)

mode7_demo: mode7_demo.c shm_map.c shm_map.h hw_contract.h
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ mode7_demo.c shm_map.c $(SW_LIBS)

clean:
	rm -f hw_sim mode7_demo map3.rgba map2.rgba sky.rgba
