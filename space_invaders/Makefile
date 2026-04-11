.PHONY: run hwardware software clean

OS := $(shell uname -s)

SDLFLAGS := $(shell sdl2-config --cflags --libs)
RTLIBRARY := $(if $(filter $(OS),Linux),-lrt,)
ALSAFLAGS := $(shell pkg-config --cflags --libs alsa 2>/dev/null)

ifeq ($(strip $(ALSAFLAGS)),)
AUDIODEFS := -DNO_ALSA=1
AUDIOLIBS :=
else
AUDIODEFS :=
AUDIOLIBS := $(ALSAFLAGS) -lpthread
endif

GCCFLAGS := -O3 $(AUDIODEFS) $(SDLFLAGS) $(RTLIBRARY) $(AUDIOLIBS)

all: run

ifeq ($(FB),)

run: hardware software
	./hw_sim & sleep 0.5 && ./sw_game

hardware:
	gcc hw_sim.c -o hw_sim $(GCCFLAGS)

software:
	gcc -D__HW_SIM__ sw/main.c sw/shm_map.c sw/gfx.c sw/sprite.c sw/font.c sw/game.c sw/music_alsa.c -o sw_game $(GCCFLAGS)

else

run: hardware software
	./fb_sim & sleep 0.5
	./sw_game
	pkill -9 -f fb_sim

hardware:
	gcc fb_sim.c -o fb_sim $(GCCFLAGS) -levdev

software:
	gcc sw/main.c sw/shm_map.c sw/gfx.c sw/sprite.c sw/font.c sw/game.c sw/music_alsa.c -o sw_game $(GCCFLAGS)

endif

clean:
	rm -f hw_sim sw_game
