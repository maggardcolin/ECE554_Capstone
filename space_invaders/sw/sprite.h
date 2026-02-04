#pragma once
#include <stdint.h>

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
