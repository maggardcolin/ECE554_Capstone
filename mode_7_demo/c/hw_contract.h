#pragma once

#include <stdint.h>

#define W 1280
#define H 720
#define BYTES_PER_PIXEL 4
#define FB_SIZE (W * H * BYTES_PER_PIXEL)
#define FB_COUNT 2

#define BTN_LEFT  (1u << 0)
#define BTN_RIGHT (1u << 1)
#define BTN_FIRE  (1u << 2)
#define BTN_QUIT  (1u << 3)
#define BTN_PAUSE (1u << 4)
#define BTN_RESET (1u << 5)
#define BTN_DOWN  (1u << 6)
#define BTN_UP    (1u << 7)
#define BTN_SEL   (1u << 8)

typedef struct {
    volatile uint32_t buttons;
    volatile uint32_t vsync_counter;
    volatile uint32_t front_idx;
    volatile uint32_t back_idx;
    volatile uint32_t swap_request;
    volatile uint32_t swap_ack;
    volatile uint32_t prop_quit;
    volatile uint32_t reserved[10];
} mmio_regs_t;