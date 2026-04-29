#include "../hw_contract.h"
#include "gfx.h"
#include <string.h>

/// lfb_init: Initialize a logical framebuffer structure and allocate pixel memory
/// Parameters:
///   lfb - Pointer to lfb_t structure to initialize
/// Returns: 0 on success, -1 if memory allocation fails
/// Notes: Allocates LW*LH uint32_t pixels. Call lfb_free() to deallocate.
int lfb_init(lfb_t *lfb) {
    lfb->px = (uint32_t*)malloc((size_t)LW * (size_t)LH * sizeof(uint32_t));
    return lfb->px ? 0 : -1;
}

/// lfb_free: Free pixel memory allocated by lfb_init and clear framebuffer pointer
/// Parameters:
///   lfb - Pointer to lfb_t structure to clean up
/// Returns: void
/// Notes: Safe to call multiple times (checks for NULL). Sets px to NULL after freeing.
void lfb_free(lfb_t *lfb) {
    if (lfb->px) free(lfb->px);
    lfb->px = NULL;
}

/// l_clear: Fill the entire logical framebuffer with a single color
/// Parameters:
///   lfb  - Pointer to logical framebuffer to clear
///   argb - 32-bit ARGB color value to fill with
/// Returns: void
/// Notes: Fills all LW*LH pixels. Commonly used to set background before rendering.
void l_clear(lfb_t *lfb, uint32_t argb) {
    for (int i = 0; i < LW * LH; i++) lfb->px[i] = argb;
}

/// upscale_to_physical: Scale logical framebuffer to physical screen resolution
/// Parameters:
///   dst - Pointer to physical (W x H) framebuffer to write to
///   src - Pointer to logical (LW x LH) framebuffer to read from
/// Returns: void
/// Notes: Each logical pixel is scaled uniformly by SCALE factor (typically 4x).
///        Used before buffer swap to display logical framebuffer on hardware.
void upscale_to_physical(uint32_t *dst, const lfb_t *src) {
    for (int y = 0; y < LH; y++) {
        const uint32_t *srow = src->px + y * LW;
        for (int sy = 0; sy < SCALE; sy++) {
            uint32_t *drow = dst + (y * SCALE + sy) * W;
            for (int x = 0; x < LW; x++) {
                uint32_t p = srow[x];
                int dx0 = x * SCALE;
                for (int sx = 0; sx < SCALE; sx++) drow[dx0 + sx] = p;
            }
        }
    }
}

#ifdef FB

#define MMIO_DEVICE "/dev/uio4"
#define MMIO_MAP_SIZE_PATH "/sys/class/uio/uio4/maps/map0/size"
void* MMIO_MAP = NULL;

#define UIO_DEVICE "/dev/uio5"
#define UIO_MAP_SIZE_PATH "/sys/class/uio/uio5/maps/map0/size"
void* DMA_MAP = NULL;

#define DMA_DEVICE "/dev/udmabuf0"
#define DMA_ADDR_PATH "/sys/class/u-dma-buf/udmabuf0/phys_addr"
#define DMA_SIZE_PATH "/sys/class/u-dma-buf/udmabuf0/size"

uint64_t **instruction = NULL;


void commit_ins(void) {
	if (instruction == NULL) return;

	for (int i = 0; i < 12; i++) {
		send_instruction(MMIO_MAP, 1, 0, i, 0, 1, 0);

		int index = 0;
		while (instruction[i][index] != 0) {
			send_instruction_2(MMIO_MAP, instruction[i][index]);
			// instruction[i][index] = 0;
			index++;
		}

		poll_dma(DMA_MAP, i);
	}

	write_to_fb();
}

void cache_ins(uint64_t x, uint64_t y, uint64_t color) {
	uint64_t word = 0;
        word |= (uint64_t) ((0x3u & 0x1F))  	<< 59;
        word |= (x & 0xFFF)    	<< 47;
        word |= (y & 0xFFF)    	<< 35;
        word |= (color & 0xFFF) << 23;
        word |= (0x0u & 0xFFF)  << 11;
        word |= (0x0u & 0x7FF);

	int index = ((int)(y / 64));

	int fifo_index = 0;
	for (fifo_index = 0; fifo_index < 1280*64-1; fifo_index++) {
		if (instruction[index][fifo_index] == 0) break;
	}

	instruction[index][fifo_index] = word;
}

void l_putpix(lfb_t *lfb, int x, int y, uint32_t argb) {
	if (MMIO_MAP == NULL) initialize_uio(MMIO_DEVICE, MMIO_MAP_SIZE_PATH);
	if (DMA_MAP == NULL) initialize_uio(UIO_DEVICE, UIO_MAP_SIZE_PATH);
	if (instruction == NULL) {
		instruction = malloc(sizeof(uint64_t*) * 12);
		for (int i = 0; i < 12; i++) {
			instruction[i] = calloc(1280*64, sizeof(uint64_t));
		}
	}

        uint8_t color;
        if ((argb & 0x00FFFFFF) != 0x000000) {
                color = 15;
        } else {
                color = 0;
        }

	cache_ins(x, y, color);
}

#endif
