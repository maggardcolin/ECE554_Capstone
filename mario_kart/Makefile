ifeq ($(DISPLAY),)
	$(error Must be run in a display enabled environment)
endif

.PHONY: run hardware software clean

OS := $(shell uname -s)

# Try sdl2-config first, fall back to direct flags
SDL2_CONFIG := $(shell which sdl2-config 2>/dev/null)
ifdef SDL2_CONFIG
  SDLFLAGS := $(shell sdl2-config --cflags --libs)
else
  SDLFLAGS := -I/usr/include/SDL2 -lSDL2
endif

RTLIBRARY := $(if $(filter $(OS),Linux),-lrt,)

GCCFLAGS := -O3 -lm $(SDLFLAGS) $(RTLIBRARY)

all: run

run: hardware software
	./hw_sim & sleep 0.5
	./sw_game

hardware:
	gcc hw_sim.c -o hw_sim $(GCCFLAGS)

software:
	gcc sw/main.c sw/shm_map.c sw/gfx.c sw/sprite.c sw/font.c sw/game.c -o sw_game $(GCCFLAGS)

clean:
	rm -f hw_sim sw_game
