.PHONY: run hwardware software clean

OS := $(shell uname -s)

SDLFLAGS := $(shell sdl2-config --cflags --libs)
RTLIBRARY := $(if $(filter $(OS),Linux),-lrt,)

GCCFLAGS := -O3 $(SDLFLAGS) $(RTLIBRARY)

all: run

ifeq ($(FB),)

run: hardware software
	./hw_sim & sleep 0.5 && ./sw_game

hardware:
	gcc hw_sim.c -o hw_sim $(GCCFLAGS)

else

run: hardware software
	./fb_sim & sleep 0.5
	./sw_game
	pkill -9 -f fb_sim

hardware:
	gcc fb_sim.c -o fb_sim $(GCCFLAGS) -levdev

endif

software:
	gcc sw/main.c sw/shm_map.c sw/gfx.c sw/sprite.c sw/font.c sw/game.c -o sw_game $(GCCFLAGS)

clean:
	rm -f hw_sim sw_game
