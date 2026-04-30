#include "../hw_contract.h"
#include "gfx.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#ifndef FB

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

void l_clear_2() {}

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

#else

#define MMIO_DEVICE "/dev/uio4"
#define MMIO_MAP_SIZE_PATH "/sys/class/uio/uio4/maps/map0/size"
void* MMIO_MAP = NULL;

#define UIO_DEVICE "/dev/uio5"
#define UIO_MAP_SIZE_PATH "/sys/class/uio/uio5/maps/map0/size"
void* DMA_MAP = NULL;

#define DMA_DEVICE "/dev/udmabuf0"
#define DMA_ADDR_PATH "/sys/class/u-dma-buf/udmabuf0/phys_addr"
#define DMA_SIZE_PATH "/sys/class/u-dma-buf/udmabuf0/size"

// uint64_t **instruction = NULL;
// volatile uint64_t *instruction;
volatile uint64_t *put_rect = NULL;
volatile uint64_t put_pixel[320][180] = {0};
volatile bool     ins_changed[320][180] = {false};

int lfb_init(lfb_t *lfb) {
    lfb->px = (uint32_t*)malloc((size_t)LW * (size_t)LH * sizeof(uint32_t));
    return lfb->px ? 0 : -1;
}

void lfb_free(lfb_t *lfb) {
    if (lfb->px) free(lfb->px);
    lfb->px = NULL;
}

void upscale_to_physical(uint32_t *dst, const lfb_t *src) {}

int initialized = 0;

bool clr_screen = false;

void init() {
	if (MMIO_MAP == NULL) MMIO_MAP=initialize_uio(MMIO_DEVICE, MMIO_MAP_SIZE_PATH); 
        if (DMA_MAP == NULL) DMA_MAP=initialize_uio(UIO_DEVICE, UIO_MAP_SIZE_PATH); 
 
        if (initialized == 0) { 
                for (int y = 0; y < 4; y++) { 
                        for (int x = 0; x < 5; x++) { 
                                send_instruction(MMIO_MAP, 1, x, y, 1, 1, 0, 1 << (y*5+x)); 
                        } 
                } 
                send_instruction(MMIO_MAP, 2, 0, 0, 0, 0, 0, 0xFFFFFFFFu); 

                for(int i = 0; i < 3; i++) poll_dma(DMA_MAP, 0);

                initialized = 1; 
        }

	if (put_rect == NULL) {
		put_rect = calloc(100, sizeof(uint64_t));
	}
}

void l_clear(lfb_t *lfb, uint32_t argb) {
	if (!initialized) init();

	// clr_screen = true;

	//if (instruction != NULL) memset(instruction, 0, 320*240*sizeof(uint64_t));
}

void l_clear_2() {
	if (!initialized) init();

	clr_screen = true;

	for (int x = 0; x < 320; x++) {
		for (int y = 0; y < 180; y++) {
			ins_changed[x][y] = false;
			put_pixel[x][y] = 0;
		}
	}
}

// int cached_ins = 0;

void commit_ins(void) {
	if (!initialized) init();

	// if (instruction[0] == 0) return;

	volatile int index = 0;
	if (clr_screen) {
		send_instruction(MMIO_MAP, 0x2ULL, 0, 0, 0, 0, 0, 0xFFFFFFFFu);
		clr_screen = false;
	}
	// usleep(50);
	
//	while (instruction[index] != 0) {
//		send_instruction_2(MMIO_MAP, instruction[index]);
//		//usleep(5);
//		instruction[index] = 0;
//		index++;
//	}

	while (put_rect[index] != 0) {
		send_instruction_2(MMIO_MAP, put_rect[index]);
		put_rect[index] = 0;
		index++;
	}
	
	for (int x = 0; x < 320; x++) {
		for (int y = 0; y < 180; y++) {
			if (ins_changed[x][y]) {
				send_instruction_2(MMIO_MAP, put_pixel[x][y]);
				ins_changed[x][y] = false;
			}
		}
	}

	// printf("Cached instructions: %d\tLast index: %d\tDifference: %d\n", cached_ins, index - 1, cached_ins - (index - 1));
	// cached_ins = 0;

	// usleep(20000);
	
	poll_dma(DMA_MAP, 0);
	
	// usleep(12000);

	write_to_fb();
}

void cache_ins(uint64_t inst, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
	uint64_t word = 0;
        word |= (inst & 0x1F)  	<< 59;
        word |= (a1 & 0xFFF)    << 47;
        word |= (a2 & 0xFFF)    << 35;
        word |= (a3 & 0xFFF)	<< 23;
        word |= (a4 & 0xFFF) 	<< 11;
        word |= (a5 & 0x7FF);

	// cached_ins++;
	if (inst == 0x3ULL) {
		if (put_pixel[(int) a1][(int) a2] != word) {
			put_pixel[(int) a1][(int) a2] = word;
			ins_changed[(int) a1][(int) a2] = true;
		} // else cached_ins--;
	} else {
		int fifo_index = 0;
		for (fifo_index = 0; fifo_index < 100; fifo_index++) {
			if (put_rect[fifo_index] == 0) break;
		}
		put_rect[fifo_index] = word;
	}
}

uint8_t map_color(uint32_t argb) {
	if ((argb & 0x00FFFFFF) != 0x000000) {
                return 15;
        } else {
                return 0;
        }

}

void l_putpix(lfb_t *lfb, int x, int y, uint32_t argb) {
	if(x<0||x>=320||y<0||y>=180)
		return;
	
	if (!initialized) init();

        uint8_t color = map_color(argb);
        
	cache_ins(0x3ULL, x, y, color, 0x0ULL, 0x0ULL);
}

void l_putrect(int x, int y, int w, int h, uint32_t argb) {
	if (!initialized) init();
	if (x < 0) x = 0;
	if (y < 0) y = 0;

        uint8_t color = map_color(argb);

	cache_ins(0x5ULL, x, y, w, h, color);
}

void toggle_music(uint32_t music_mask) {
	if (!initialized) init();

	set_music_gpio(MMIO_MAP, music_mask);
}

#endif
