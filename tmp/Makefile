obj-m += phys_mem.o

# Point this to your specific Xilinx kernel build directory
KDIR ?= /lib/modules/6.6.10-xilinx-v2024.1-g0e84683a8258/build

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
