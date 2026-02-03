/// hw_contract.h: Hardware/Software interface specification
/// Defines shared memory layout, constants, and MMIO register structure
/// for communication between hardware simulator (hw_sim) and game software
// hw_contract.h
#pragma once
#include <stdint.h>

#define W 1280
#define H 720
#define BYTES_PER_PIXEL 4
#define FB_SIZE (W * H * BYTES_PER_PIXEL)
#define FB_COUNT 2

// Button bits
#define BTN_LEFT  (1u << 0)
#define BTN_RIGHT (1u << 1)
#define BTN_FIRE  (1u << 2)
#define BTN_QUIT  (1u << 3)

/// mmio_regs_t: Memory-mapped I/O register structure shared via mmap
/// Memory layout: software and hardware share this via /pynq_fbmmio shared memory
/// Fields:
///   buttons        - Bitmask set by hw_sim: BTN_LEFT | BTN_RIGHT | BTN_FIRE | BTN_QUIT
///   vsync_counter  - Incremented by hw_sim every frame (~60 Hz), read-only to software
///   front_idx      - Index of framebuffer shown on display (controlled by hw_sim)
///   back_idx       - Index of framebuffer software writes to
///   swap_request   - Software sets to 1 to request buffer swap, hw_sim sets to 0 when done
///   swap_ack       - hw_sim sets to current vsync_counter after swap (software detects change)
///   reserved       - Padding for cache-line alignment
typedef struct {
    volatile uint32_t buttons;        // set by hw_sim
    volatile uint32_t vsync_counter;  // increments every present
    volatile uint32_t front_idx;      // selected by hw_sim
    volatile uint32_t back_idx;       // selected by game
    volatile uint32_t swap_request;   // game sets to 1
    volatile uint32_t swap_ack;       // hw_sim sets = vsync_counter after swap
    volatile uint32_t reserved[10];
} mmio_regs_t;
