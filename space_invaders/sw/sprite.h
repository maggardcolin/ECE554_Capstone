#pragma once
#include <stdint.h>
#include "gfx.h"

// Sprite ASCII art definitions (row-packed)
static const char *player_rows[] = {
    "....XX....",
    "...XXXX...",
    "..XXXXXX..",
    ".XXXXXXXX.",
    "XXXXXXXXXX",
    "XX..XX..XX",
    "XX..XX..XX",
    "..........",
};
static const char *alienA_rows[] = {
    "..XX...XX..",
    "...XXXXX...",
    "..XXXXXXX..",
    ".XX.XXX.XX.",
    "XXXXXXXXXXX",
    "..X.....X..",
    ".X.......X.",
    "X.........X",
};
static const char *alienB_rows[] = {
    "..XX...XX..",
    "X..XXXXX..X",
    "X.XXXXXXX.X",
    "XX.XXX.XXX.",
    "XXXXXXXXXXX",
    ".X..X.X..X.",
    "X..X...X..X",
    ".XX.....XX.",
};
static const char *bossA_rows[] = {
    "....XXXX....XXXX....XXXX....",
    "..XXXXXXXX..XXXX..XXXXXXXX..",
    ".XXXX..XXXXXXXXXXXX..XXXX...",
    "XXXX..XXXXXXXXXXXXXX..XXXX..",
    "XXXXXX..XXXXXXXXXX..XXXXXX..",
    "XXXXXXXX..XXXXXX..XXXXXXXX..",
    "XXXX..XXXXXXXXXXXXXX..XXXX..",
    "..XXXX..XXXXXXXXXX..XXXX....",
    "....XXXXXXXX..XXXXXXXX......",
    "......XX..XX..XX..XX........",
};
static const char *bossB_rows[] = {
    "....XXXX....XXXX....XXXX....",
    "..XXXXXXXX..XXXX..XXXXXXXX..",
    ".XXXX..XXXXXXXXXXXX..XXXX...",
    "XXXX..XXXXXXXXXXXXXX..XXXX..",
    "XXXXXX..XXXXXXXXXX..XXXXXX..",
    "XXXXXXXX..XXXXXX..XXXXXXXX..",
    "XXXX..XXXXXXXXXXXXXX..XXXX..",
    "..XXXX..XXXXXXXXXX..XXXX....",
    "....XXXXXXXX..XXXXXXXX......",
    "........XX..XX..XX..XX......",
};
static const char *boss2A_rows[] = {
    "....XX......XXXX......XX....",
    "..XXXXXX..XXXXXXXX..XXXXXX..",
    ".XXXX..XXXXXXXXXXXXXX..XXXX.",
    "XXXX....XXXXXXXXXXXX....XXXX",
    "XXXXXX..XXXXXXXXXXXX..XXXXXX",
    "XXXXXXXX..XXXXXXXX..XXXXXXXX",
    "XXXX..XXXX..XXXX..XXXX..XXXX",
    "..XXXX..XXXXXXXXXXXX..XXXX..",
    "....XXXXXXXX....XXXXXXXX....",
    "......XX..XX....XX..XX......",
};
static const char *boss2B_rows[] = {
    "....XX......XXXX......XX....",
    "..XXXXXX..XXXXXXXX..XXXXXX..",
    ".XXXX..XXXXXXXXXXXXXX..XXXX.",
    "XXXX....XXXXXXXXXXXX....XXXX",
    "XXXXXX..XXXXXXXXXXXX..XXXXXX",
    "XXXXXXXX..XXXXXXXX..XXXXXXXX",
    "XXXX..XXXX..XXXX..XXXX..XXXX",
    "..XXXX..XXXXXXXXXXXX..XXXX..",
    "....XXXXXXXX....XXXXXXXX....",
    "........XX..XX..XX..XX......",
};
static const char *boss3A_rows[] = {
    "......XXXXXX..XXXXXX......",
    "....XXXXXXXX..XXXXXXXX....",
    "..XXXX..XXXXXXXX..XXXX....",
    ".XXXX....XXXXXX....XXXX...",
    "XXXXXX..XXXXXXXX..XXXXXX..",
    "XXXX..XXXXXXXXXXXX..XXXX..",
    ".XX..XXXXXX..XXXXXX..XX...",
    "..XXXX..XX....XX..XXXX....",
    "....XXXXXXXX..XXXXXXXX....",
    "......XX..XX..XX..XX......",
};
static const char *boss3B_rows[] = {
    "......XXXXXX..XXXXXX......",
    "....XXXXXXXX..XXXXXXXX....",
    "..XXXX..XXXXXXXX..XXXX....",
    ".XXXX....XXXXXX....XXXX...",
    "XXXXXX..XXXXXXXX..XXXXXX..",
    "XXXX..XXXXXXXXXXXX..XXXX..",
    ".XX..XXXXXX..XXXXXX..XX...",
    "..XXXX..XX....XX..XXXX....",
    "....XXXXXXXX..XXXXXXXX....",
    "........XX..XX..XX..XX....",
};
static const char *boss4A_rows[] = {
    ".....XXX......XXXX......XXX.....",
    "...XXXXX....XXXXXXXX....XXXXX...",
    "..XXX..XX..XXXXXXXXXX..XX..XXX..",
    ".XXX....XXXXXXXX..XXXXXXXX...XXX",
    "XXX..XXXXXX..XXXXXX..XXXXXX..XXX",
    "XXX..XXXX..XXXXXXXXXX..XXXX..XXX",
    ".XXX..XX..XXXXXXXXXXXX..XX..XXX.",
    "..XXX..XXXXXX....XXXXXX..XXX....",
    "....XXXXXXXX..XX..XXXXXXXX......",
    "......XX..XX..XX..XX..XX........",
};
static const char *boss4B_rows[] = {
    ".....XXX......XXXX......XXX.....",
    "...XXXXX....XXXXXXXX....XXXXX...",
    "..XXX..XX..XXXXXXXXXX..XX..XXX..",
    ".XXX....XXXXXXXX..XXXXXXXX...XXX",
    "XXX..XXXXXX..XXXXXX..XXXXXX..XXX",
    "XXX..XXXX..XXXXXXXXXX..XXXX..XXX",
    ".XXX..XX..XXXXXXXXXXXX..XX..XXX.",
    "..XXX..XXXXXX....XXXXXX..XXX....",
    "....XXXXXXXX..XX..XXXXXXXX......",
    "........XX..XX..XX..XX..XX......",
};
static const char *bunker_rows[] = {
    "....XXXXXXXXXXXX....",
    "...XXXXXXXXXXXXXX...",
    "..XXXXXXXXXXXXXXXX..",
    ".XXXXXXXXXXXXXXXXXX.",
    "XXXXXXXXXXXXXXXXXXXX",
    "XXXXXXX......XXXXXXX",
    "XXXXXX........XXXXXX",
    "XXXXX..........XXXXX",
    "XXXXX..........XXXXX",
    "XXXXX..........XXXXX",
    "XXXXX..........XXXXX",
    "XXXXX..........XXXXX",
    "XXXXX..........XXXXX",
    "XXXXX..........XXXXX",
    "....................",
    "....................",
};

// Shop item icons (7x7)
static const char *shop_life_rows[] = {
    "...X...",
    "...X...",
    "XXXXXXX",
    "...X...",
    "...X...",
    ".......",
    ".......",
};
static const char *shop_fire_rows[] = {
    "...X...",
    "..XXX..",
    ".XXXXX.",
    "...X...",
    "...X...",
    "...X...",
    ".......",
};
static const char *shop_move_rows[] = {
    ".......",
    "..X....",
    "...X...",
    "XXXXX..",
    "...X...",
    "..X....",
    ".......",
};
static const char *shop_dmg_rows[] = {
    "...X...",
    "...X...",
    "XXXXXXX",
    "...X...",
    "...X...",
    ".......",
    ".......",
};

static const char *shop_pierce_rows[] = {
    "...X...",
    "..XXX..",
    "..XXX..",
    "..XXX..",
    "..XXX..",
    "...X...",
    ".......",
};

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

// Drawing functions
void draw_sprite1r(lfb_t *lfb, const sprite1r_t *s, int x0, int y0, uint32_t color);
void draw_sprite1r_scaled(lfb_t *lfb, const sprite1r_t *s, int x0, int y0, uint32_t color, int scale);
