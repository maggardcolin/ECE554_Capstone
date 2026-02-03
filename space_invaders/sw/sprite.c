#include "sprite.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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

void free_sprite(sprite1r_t *s) {
    if (s->bits) free(s->bits);
    s->bits = NULL;
    s->w = s->h = s->stride = 0;
}

int sprite_get(const sprite1r_t *s, int x, int y) {
    const uint8_t *row = s->bits + y * s->stride;
    return (row[x >> 3] >> (7 - (x & 7))) & 1;
}

void sprite_clear_bit(sprite1r_t *s, int x, int y) {
    uint8_t *row = s->bits + y * s->stride;
    row[x >> 3] &= (uint8_t)~(1u << (7 - (x & 7)));
}

int bullet_hits_sprite(const sprite1r_t *s, int sx, int sy, int bx, int by) {
    if (bx < sx || by < sy || bx >= sx + s->w || by >= sy + s->h) return 0;
    return sprite_get(s, bx - sx, by - sy);
}

void bunker_damage(sprite1r_t *b, int hitx, int hity, int radius) {
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx*dx + dy*dy > radius*radius) continue;
            int x = hitx + dx;
            int y = hity + dy;
            if ((unsigned)x < (unsigned)b->w && (unsigned)y < (unsigned)b->h) {
                sprite_clear_bit(b, x, y);
            }
        }
    }
}
