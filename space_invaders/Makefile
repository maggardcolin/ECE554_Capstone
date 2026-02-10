ifeq ($(DISPLAY),)
	$(error Must be run in a display enabled environment)
endif

.PHONY: run hardware software clean

run: hardware software
	./hw_sim & sleep 0.1
	./sw_game

hardware:
	gcc -O3 hw_sim.c -o hw_sim `sdl2-config --cflags --libs` -lrt

software:
	gcc -O3 sw/main.c sw/shm_map.c sw/gfx.c sw/sprite.c sw/font.c sw/game.c -o sw_game -lrt

clean:
	rm -f hw_sim sw_game
