ifeq ($(FB),)

OS := $(shell uname -s)

RTLIBRARY := $(if $(filter $(OS),Linux),-lrt,)
SDLFLAGS := $(shell sdl2-config --cflags --libs)

ALSAFLAGS := $(shell pkg-config --cflags --libs alsa 2>/dev/null)

ifeq ($(strip $(ALSAFLAGS)),)
AUDIODEFS := -DNO_ALSA=1
AUDIOLIBS :=
else
AUDIODEFS :=
AUDIOLIBS := $(ALSAFLAGS) -lpthread
endif


GCCFLAGS := -O3 $(AUDIODEFS) $(SDLFLAGS) $(RTLIBRARY) $(AUDIOLIBS) -g

all: run

run: hardware software
	-pkill -9 hw_sim
	-pkill -9 sw_game
	./hw_sim & HW_PID=$$!; \
	trap 'kill -9 $$HW_PID 2>/dev/null || true' EXIT INT TERM; \
	sleep 0.5; \
	./sw_game

hardware:
	gcc hw_sim.c -o hw_sim $(GCCFLAGS)

software:
	gcc -D__HW_SIM__ sw/main.c sw/shm_map.c sw/gfx.c sw/sprite.c sw/font.c sw/game.c sw/boss.c sw/game_helpers.c sw/game_player.c sw/music_alsa.c -o sw_game $(GCCFLAGS)

.PHONY: clean
clean:
	-rm -f hw_sim sw_game

else

APP = space-invaders

CFLAGS += $(shell pkg-config --cflags libevdev)
LDFLAGS += $(shell pkg-config --libs libevdev)

CFLAGS += $(shell pkg-config --cflags alsa)
LDFLAGS += $(shell pkg-config --libs alsa)

CFLAGS += -O0 -lrt -lpthread -DNO_ALSA -g3

SW_SRC := sw/petalinux.c sw/font.c sw/gfx.c sw/shm_map.c sw/sprite.c sw/boss.c sw/game.c sw/game_helpers.c sw/game_player.c sw/music_alsa.c sw/main.c 
HW_SRC := fb_sim.c
SRC := $(HW_SRC) $(SW_SRC) $(APP).c
APP_OBJS = $(SRC:.c=.o)

all: build

build: $(APP)

$(APP): $(APP_OBJS)
	$(CC) -DFB $^ -o $@ $(CFLAGS) $(LDFLAGS) $(LDLIBS) -lpthread

%.o: %.c
	$(CC) -DFB $(CFLAGS) -c $< -o $@

clean:
	-rm -f $(APP) $(APP_OBJS)

endif

