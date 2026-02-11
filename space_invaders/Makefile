ifeq ($(DISPLAY),)
	$(error Must be run in a display enabled environment)
endif

.PHONY: run hwardware software clean

OS := $(shell uname -s)

SDLFLAGS := $(shell sdl2-config --cflags --libs)
RTLIBRARY := $(if $(filter $(OS),Linux),-lrt,)

GCCFLAGS := -O3 $(SDLFLAGS) $(RTLIBRARY)

all: run

run: hardware software
	./hw_sim & sleep 0.5
	./sw_game

hardware:
	gcc $(GCCFLAGS) hw_sim.c -o hw_sim

software:
	gcc $(GCCFLAGS) sw/main.c sw/shm_map.c sw/gfx.c sw/sprite.c sw/font.c sw/game.c -o sw_game

clean:
	rm -f hw_sim sw_game
