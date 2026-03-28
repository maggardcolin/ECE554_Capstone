#include "sprite.h"
#include "gfx.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/// make_sprite_from_ascii: Create a 1-bit sprite from ASCII art representation
/// Parameters:
///   rows - Array of string pointers, each row is a string of '.' (off) or 'X' (on)
///   h    - Height in rows
/// Returns: sprite1r_t with packed 1-bit pixel data (MSB first per byte)
sprite1r_t make_sprite_from_ascii(const char **rows, int h) {
    int w = (int)strlen(rows[0]);
    for (int i = 1; i < h; i++) {
        int wi = (int)strlen(rows[i]);
        if (wi != w) { fprintf(stderr, "Sprite row width mismatch\n"); exit(1); }
    }
    int stride = (w + 7) / 8;
    uint8_t *bits = (uint8_t*)calloc((size_t)h * (size_t)stride, 1);
    if (!bits) { fprintf(stderr, "calloc failed\n"); exit(1); }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            char c = rows[y][x];
            if (c != '.' && c != ' ') {
                bits[y * stride + (x >> 3)] |= (uint8_t)(1u << (7 - (x & 7)));
            }
        }
    }
    sprite1r_t s = { .w = w, .h = h, .stride = stride, .bits = bits };
    return s;
}

/// free_sprite: Free memory allocated by make_sprite_from_ascii
void free_sprite(sprite1r_t *s) {
    if (s->bits) free(s->bits);
    s->bits = NULL;
    s->w = s->h = s->stride = 0;
}

/// sprite_get: Read a single pixel bit from a sprite
int sprite_get(const sprite1r_t *s, int x, int y) {
    const uint8_t *row = s->bits + y * s->stride;
    return (row[x >> 3] >> (7 - (x & 7))) & 1;
}

/// draw_sprite1r: Draw a 1-bit sprite at (x0, y0) with a solid color
void draw_sprite1r(lfb_t *lfb, const sprite1r_t *s, int x0, int y0, uint32_t color) {
    for (int y = 0; y < s->h; y++) {
        int yy = y0 + y;
        if ((unsigned)yy >= (unsigned)LH) continue;
        const uint8_t *row = s->bits + y * s->stride;
        for (int x = 0; x < s->w; x++) {
            int xx = x0 + x;
            if ((unsigned)xx >= (unsigned)LW) continue;
            if ((row[x >> 3] >> (7 - (x & 7))) & 1) lfb->px[yy * LW + xx] = color;
        }
    }
}

/// draw_sprite1r_scaled: Draw a 1-bit sprite scaled by an integer factor
void draw_sprite1r_scaled(lfb_t *lfb, const sprite1r_t *s, int x0, int y0, uint32_t color, int scale) {
    if (scale < 1) scale = 1;
    for (int y = 0; y < s->h; y++) {
        const uint8_t *row = s->bits + y * s->stride;
        for (int x = 0; x < s->w; x++) {
            if (!((row[x >> 3] >> (7 - (x & 7))) & 1)) continue;
            int xx = x0 + x * scale;
            int yy = y0 + y * scale;
            for (int dy = 0; dy < scale; dy++) {
                int yyy = yy + dy;
                if ((unsigned)yyy >= (unsigned)LH) continue;
                for (int dx = 0; dx < scale; dx++) {
                    int xxx = xx + dx;
                    if ((unsigned)xxx >= (unsigned)LW) continue;
                    lfb->px[yyy * LW + xxx] = color;
                }
            }
        }
    }
}

/// draw_sprite1r_shadow: Draw a sprite with a dark elliptical shadow beneath it.
/// Shadow provides a visual depth cue for the pseudo-3D perspective.
/// The shadow is created by darkening the road pixels beneath the sprite
/// (multiplying each channel by 2/3) rather than by alpha blending.
void draw_sprite1r_shadow(lfb_t *lfb, const sprite1r_t *s, int x0, int y0, uint32_t color, int scale) {
    // Draw shadow ellipse at bottom of sprite
    int spr_w = s->w * scale;
    int spr_cx = x0 + spr_w / 2;
    int spr_bottom = y0 + s->h * scale;
    int sh_rx = spr_w / 2;
    int sh_ry = (sh_rx > 2) ? sh_rx / 3 : 1;

    for (int dy = -sh_ry; dy <= sh_ry; dy++) {
        for (int dx = -sh_rx; dx <= sh_rx; dx++) {
            // Ellipse equation: (dx/rx)^2 + (dy/ry)^2 <= 1
            int rx2 = sh_rx * sh_rx;
            int ry2 = sh_ry * sh_ry;
            if ((long)dx * dx * ry2 + (long)dy * dy * rx2 <= (long)rx2 * ry2) {
                int px = spr_cx + dx;
                int py = spr_bottom + dy;
                if ((unsigned)px < (unsigned)LW && (unsigned)py < (unsigned)LH) {
                    // Darken existing pixel to create shadow
                    uint32_t orig = lfb->px[py * LW + px];
                    uint32_t r = ((orig >> 16) & 0xFF) * 2 / 3;
                    uint32_t g = ((orig >>  8) & 0xFF) * 2 / 3;
                    uint32_t b = ((orig      ) & 0xFF) * 2 / 3;
                    lfb->px[py * LW + px] = 0xFF000000u | (r << 16) | (g << 8) | b;
                }
            }
        }
    }

    // Draw the sprite on top
    draw_sprite1r_scaled(lfb, s, x0, y0, color, scale);
}
