.PHONY: all run hardware software clean kill hw_sim sw_game

OS       := $(shell uname -s)
SDLFLAGS := $(shell sdl2-config --cflags --libs)
RTLIB    := $(if $(filter $(OS),Linux),-lrt,)
LOGICAL_W ?=
LOGICAL_H ?=
PIXEL_SCALE ?=

RES_DEFS :=
ifneq ($(strip $(LOGICAL_W)),)
RES_DEFS += -DLOGICAL_W=$(LOGICAL_W)
endif
ifneq ($(strip $(LOGICAL_H)),)
RES_DEFS += -DLOGICAL_H=$(LOGICAL_H)
endif
ifneq ($(strip $(PIXEL_SCALE)),)
RES_DEFS += -DPIXEL_SCALE=$(PIXEL_SCALE)
endif

CFLAGS   := -O3 -Wall -Wextra $(RES_DEFS)

all: run

run: hardware software
	./hw_sim & sleep 0.5
	./sw_game

hardware: hw_sim

software: sw_game

hw_sim: hw_sim.c hw_contract.h
	gcc $(CFLAGS) -o $@ hw_sim.c $(SDLFLAGS) $(RTLIB)

SW_SRCS := sw/main.c sw/shm_map.c sw/gfx.c sw/font.c sw/game.c

sw_game: $(SW_SRCS) sw/game.h sw/gfx.h sw/font.h sw/shm_map.h hw_contract.h
	gcc $(CFLAGS) -o $@ $(SW_SRCS) $(SDLFLAGS) $(RTLIB)

kill:
	pkill -9 hw_sim || true

clean:
	rm -f hw_sim sw_game
