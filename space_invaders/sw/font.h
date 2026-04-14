#pragma once
#include <stdint.h>
#include "gfx.h"

void l_draw_digit(lfb_t *lfb, int x0, int y0, int d, uint32_t c);
void l_draw_score(lfb_t *lfb, int x, int y, int score, uint32_t c);
void l_draw_text(lfb_t *lfb, int x, int y, const char *text, int scale, uint32_t c);
void l_draw_text_clipped_y(lfb_t *lfb, int x, int y, const char *text, int scale, uint32_t c, int y_min, int y_max);
int l_text_width_compact(const char *text);
void l_draw_text_compact(lfb_t *lfb, int x, int y, const char *text, uint32_t c);
