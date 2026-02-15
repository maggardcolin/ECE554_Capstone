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
	-rm usb_sim.raw
else
run-sim:

clean-sim:
endif

clean: clean-sim
