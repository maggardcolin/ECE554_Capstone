#pragma once
#include <stdint.h>
#include "gfx.h"

void l_draw_digit(lfb_t *lfb, int x0, int y0, int d, uint32_t c);
void l_draw_score(lfb_t *lfb, int x, int y, int score, uint32_t c);
void l_draw_text(lfb_t *lfb, int x, int y, const char *text, int scale, uint32_t c);
