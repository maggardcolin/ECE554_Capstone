#pragma once
#include <stdint.h>
#include <stdlib.h>
#include "../hw_contract.h"

#ifndef SCALE
#define SCALE 4
#endif

#define LW (W / SCALE)
#define LH (H / SCALE)

#if (W % SCALE) != 0 || (H % SCALE) != 0
#error "W and H must be divisible by SCALE"
#endif

typedef struct {
    uint32_t *px; // LW*LH ARGB
} lfb_t;

int  lfb_init(lfb_t *lfb);
void lfb_free(lfb_t *lfb);

#ifndef FB

static inline void l_putpix(lfb_t *lfb, int x, int y, uint32_t argb) {
    if ((unsigned)x < (unsigned)LW && (unsigned)y < (unsigned)LH) lfb->px[y * LW + x] = argb;
}

void commit_ins(void);

#else

#include "petalinux.h"

void commit_ins(void);

void l_putpix(lfb_t *lfb, int x, int y, uint32_t argb);

#endif


void l_clear(lfb_t *lfb, uint32_t argb);
void upscale_to_physical(uint32_t *dst_argb, const lfb_t *src);
