all: run build

run: build
	sudo ./music

build:	
	gcc music.c music_alsa.c -o music -lpigpio -lasound -lpthread
