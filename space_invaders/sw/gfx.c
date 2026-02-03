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
