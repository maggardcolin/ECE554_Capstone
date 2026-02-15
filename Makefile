ifeq ($(DISPLAY),)
	$(error Must be run in a display enabled environment)
endif

.PHONY: help run run-sim clean-sim clean 

OS := $(shell uname -s)

all: help

help:

run:

ifeq ($(OS),Darwin)

run-sim: 
	$(MAKE) -C space_invaders software
	dd if=/dev/zero of=usb_sim.raw bs=512M count=1
	disk=$$(hdiutil attach -imagekey diskimage-class=CRawDiskImage -nomount usb_sim.raw); \
	diskutil eraseDisk ExFAT GAMESIM $$disk; 
	cp space_invaders/sw_game /Volumes/GAMESIM/
	echo "Space Invaders" > /Volumes/GAMESIM/title

clean-sim:
	-disk=$$(df | grep "GAMESIM" | head -c10); \
	hdiutil detach $$disk;
	-rm -rf usb_sim.raw

else

USER := $(shell whoami)

run-sim:
	$(MAKE) -C space_invaders software
	dd if=/dev/zero of=usb_sim.raw bs=512M count=1
	mkfs.ext4 -L GAMESIM usb_sim.raw
	sudo mkdir /game-mnt
	sudo mount -o loop,rw -t ext4 usb_sim.raw /game-mnt
	sudo chown -R $(USER):$(USER) /game-mnt
	mkdir /game-mnt/GAMESIM
	cp space_invaders/sw_game /game-mnt/GAMESIM
	echo "Space Invaders" > /game-mnt/GAMESIM/title

clean-sim:
	-sudo umount /game-mnt
	-sudo rm -rf usb_sim.raw /game-mnt

endif

clean: clean-sim
