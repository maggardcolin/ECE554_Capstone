#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include "petalinux.h"

#define MMIO_DEVICE "/dev/uio4"
#define MMIO_MAP_SIZE_PATH "/sys/class/uio/uio4/maps/map0/size"

#define UIO_DEVICE "/dev/uio5"
#define UIO_MAP_SIZE_PATH "/sys/class/uio/uio5/maps/map0/size"

#define DMA_DEVICE "/dev/udmabuf0"
#define DMA_ADDR_PATH "/sys/class/u-dma-buf/udmabuf0/phys_addr"
#define DMA_SIZE_PATH "/sys/class/u-dma-buf/udmabuf0/size"

#define REG_LED 0x00
#define REG_INSTR_LOW 0x04
#define REG_INSTR_HI 0x08
#define REG_TILEMASK 0x14

#define CNTRL_OFF  0x30
#define STATUS_OFF 0x34
#define ADDR_OFF   0x48
#define LENGTH_OFF 0x58

long get_size(char *path) {
	FILE *fp;
	long size;

	fp = fopen(path, "r");
	if (fp == NULL) {
		perror("Failed to open size file");
		return -1;
	}

	if (fscanf(fp, "%lx", &size) != 1) {
		fprintf(stderr, "Failed to read size from file.\n");
		fclose(fp);
		return -1;
	}

	fclose(fp);
	return size;
}

void send_instruction_2(void *mmio, uint64_t ins) {
	*((uint32_t *)(mmio + REG_TILEMASK)) = 0xFFFFFFFFu;

	uint32_t hi = (uint32_t)(ins >> 32);
	uint32_t lo = (uint32_t)(ins & 0xFFFFFFFFu);

	*((uint32_t *)(mmio + REG_INSTR_HI))  = hi;
	*((uint32_t *)(mmio + REG_INSTR_LOW)) = lo;
}

void send_instruction(void *mmio, uint64_t instr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
	uint64_t word = 0;
	word |= (instr & 0x1F)  << 59;
	word |= (a1 & 0xFFF)    << 47;
	word |= (a2 & 0xFFF)    << 35;
	word |= (a3 & 0xFFF)    << 23;
	word |= (a4 & 0xFFF)    << 11;
	word |= (a5 & 0x7FF);

	*((uint32_t *)(mmio + REG_TILEMASK)) = 0xFFFFFFFFu;

	uint32_t hi = (uint32_t)(word >> 32);
	uint32_t lo = (uint32_t)(word & 0xFFFFFFFFu);

	*((uint32_t *)(mmio + REG_INSTR_HI))  = hi;
	*((uint32_t *)(mmio + REG_INSTR_LOW)) = lo;
}

void* initialize_uio(char *dev_path, char *size_path) {
	int mmio_fd;
	void *mmio_ptr;
	long map_size;

	map_size = get_size(size_path);
	if (map_size < 0) { fprintf(stderr, "Unable to get %s size\n", size_path); return 0; }

	mmio_fd = open(dev_path, O_RDWR);
	if (mmio_fd < 0) { fprintf(stderr, "Unable to open %s\n", dev_path); return 0; }

	mmio_ptr = mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, mmio_fd, 0);
	if (mmio_ptr == MAP_FAILED) { fprintf(stderr, "Unable to map mmio"); return 0; }

	return mmio_ptr;
}


#define dma_addr get_size(DMA_ADDR_PATH)

void poll_dma(void *DMA, int y) {
	*((uint32_t *)(DMA + ADDR_OFF)) = (uint32_t) (((uint32_t) dma_addr) + (0x28000u * y));
	*((uint32_t *)(DMA + LENGTH_OFF)) = (uint32_t) 0xFFFFFFFFu;
	while ((*((uint32_t *)(DMA + STATUS_OFF)) & 0x4002) == 0);
	if ((*((uint32_t *)(DMA + STATUS_OFF)) & 0x4000) != 0) {
		perror("DMA ERROR");
	}
}

void write_to_fb() {
	int fb_fd;
	int dma_fd;

	fb_fd = open("/dev/fb0", O_RDWR);
	if (fb_fd < 0) { perror("Unable to open fb0"); }

	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;

	ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
	ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo);

	long screensize = finfo.line_length * vinfo.yres;

	dma_fd = open("/dev/udmabuf0", O_RDWR);
	if (dma_fd < 0) { perror("Unable to open dma"); }

	void *buffer = malloc(screensize);
	if (!buffer) { perror("malloc"); }

	lseek(dma_fd, 0, SEEK_SET);
	lseek(fb_fd, 0, SEEK_SET);

	ssize_t n = read(dma_fd, buffer, screensize);
	if (n != screensize) { perror("unable to read dma"); }

	ssize_t w = write(fb_fd, buffer, screensize);
	if (w != screensize) { perror("unable to write to fb"); }
}
