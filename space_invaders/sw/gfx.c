#include "../hw_contract.h"
#include "gfx.h"
#include <string.h>

int lfb_init(lfb_t *lfb) {
    lfb->px = (uint32_t*)malloc((size_t)LW * (size_t)LH * sizeof(uint32_t));
    return lfb->px ? 0 : -1;
}

void lfb_free(lfb_t *lfb) {
    if (lfb->px) free(lfb->px);
    lfb->px = NULL;
}

void l_clear(lfb_t *lfb, uint32_t argb) {
    for (int i = 0; i < LW * LH; i++) lfb->px[i] = argb;
}

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
