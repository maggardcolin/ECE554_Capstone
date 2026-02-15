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
	sudo mkdir /mnt/GAMESIM
	sudo mount -o loop,rw -t ext4 usb_sim.raw /mnt/GAMESIM
	sudo chown -R $(USER):$(USER) /mnt/GAMESIM
	cp space_invaders/sw_game /mnt/GAMESIM
	echo "Space Invaders" > /mnt/GAMESIM/title

clean-sim:
	-sudo umount /mnt/GAMESIM
	-sudo rm -rf usb_sim.raw /mnt/GAMESIM

hard-setup:
	@read -r -p "WARNING! This will modify your fstab. Use only if you know what you are doing... (press anything to continue)" v;
	-sudo mkdir /mnt/GAMESIM
	grep "GAMESIM" /etc/fstab || echo "LABEL=GAMESIM /mnt/GAMESIM ext4 noauto,x-systemd.automount 0 0" | sudo tee -a /etc/fstab
	sudo systemctl daemon-reload
	sudo systemctl start mnt-GAMESIM.automount

endif

clean: clean-sim
	$(MAKE) -C space_invaders clean
	$(MAKE) -C console_menu clean
	@read -r -p "The next commands will undo the hard setup. Feel free to ctrl+c if you didn't do it or don't want to undo. (press anything)" v;
	-grep "GAMESIM" /etc/fstab && sudo sed -i '/GAMESIM/d' /etc/fstab
	sudo systemctl daemon-reload
