#pragma once
#include <stdint.h>

typedef struct {
    int w, h;
    int stride;    // bytes per row
    uint8_t *bits; // mutable row-packed bits
} sprite1r_t;

sprite1r_t make_sprite_from_ascii(const char **rows, int h);
void       free_sprite(sprite1r_t *s);

int  sprite_get(const sprite1r_t *s, int x, int y);
void sprite_clear_bit(sprite1r_t *s, int x, int y);

int  bullet_hits_sprite(const sprite1r_t *s, int sx, int sy, int bx, int by);
void bunker_damage(sprite1r_t *b, int hitx, int hity, int radius);
