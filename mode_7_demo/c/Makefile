ifeq ($(DISPLAY),)
	$(error Must be run in a display enabled environment)
endif

.PHONY: all run hardware software assets clean

OS := $(shell uname -s)
SDLFLAGS := $(shell sdl2-config --cflags --libs)
RTLIBRARY := $(if $(filter $(OS),Linux),-lrt,)
MATHLIB := -lm

GCCFLAGS := -O3 $(SDLFLAGS) $(RTLIBRARY) $(MATHLIB)

all: run

run: assets hardware software
	./hw_sim & sleep 0.5
	./mode7_demo

assets: map3.rgba map2.rgba sky.rgba

map3.rgba: map3.png
	convert $< -resize 1024x1024\! -alpha on rgba:$@

map2.rgba: map2.png
	convert $< -resize 1024x1024\! -alpha on rgba:$@

sky.rgba: sky.jpg
	convert $< -resize 1024x1024\! -alpha on rgba:$@

hardware:
	gcc ../space_invaders/hw_sim.c -I../space_invaders -o hw_sim $(GCCFLAGS)

software:
	gcc mode7_demo.c ../space_invaders/sw/shm_map.c -I../space_invaders/sw -I../space_invaders -o mode7_demo $(GCCFLAGS)

clean:
	rm -f hw_sim mode7_demo map3.rgba map2.rgba sky.rgba
