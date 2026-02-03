hardware:
	gcc hw_sim.c -o hw_sim `sdl2-config --cflags --libs` -lrt
	./hw_sim
software:
	gcc sw/main.c sw/shm_map.c sw/gfx.c sw/sprite.c sw/font.c sw/game.c -o sw_game -lrt
	./sw_game
clean:
	rm -f hw_sim sw_game