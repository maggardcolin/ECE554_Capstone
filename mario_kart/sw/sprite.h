#pragma once
#include <stdint.h>
#include "gfx.h"

// ─── Kart sprite (12x8) - viewed from behind ───────────────────────────────
// Rear spoiler, body, wheels
static const char *kart_rows[] = {
    "....XXXX....",   // rear spoiler
    "..XXXXXXXX..",   // upper body / roll bar
    ".XXXXXXXXXX.",   // windshield + body
    "XXXXXXXXXXXX",   // main body (widest)
    "XXXXXXXXXXXX",   // lower body
    "XXX......XXX",   // undercarriage
    "X.X......X.X",   // rear wheels
    "............",   // ground clearance
};
static const int kart_h = 8;

// ─── Item box sprite (8x8) ─────────────────────────────────────────────────
static const char *itembox_rows[] = {
    "XXXXXXXX",
    "X......X",
    "X.XXXX.X",
    "X.X..X.X",
    "X.X..X.X",
    "X.XXXX.X",
    "X......X",
    "XXXXXXXX",
};
static const int itembox_h = 8;

// ─── Banana sprite (5x5) ──────────────────────────────────────────────────
static const char *banana_rows[] = {
    ".XXX.",
    "X....",
    "X....",
    ".XXX.",
    "....X",
};
static const int banana_h = 5;

// ─── Red shell sprite (8x6) ───────────────────────────────────────────────
static const char *shell_rows[] = {
    "..XXXX..",
    ".XXXXXX.",
    "XXXXXXXX",
    "XXXXXXXX",
    ".XXXXXX.",
    "..XXXX..",
};
static const int shell_h = 6;

// ─── Mushroom sprite (7x7) ────────────────────────────────────────────────
static const char *mushroom_rows[] = {
    "..XXX..",
    ".XXXXX.",
    "XXXXXXX",
    "X.X.X.X",
    ".XXXXX.",
    ".X...X.",
    ".XXXXX.",
};
static const int mushroom_h = 7;

// ─── Finish flag sprite (12x8) ────────────────────────────────────────────
static const char *flag_rows[] = {
    "XXXX........",
    "XXXXXX......",
    "XXXXXXXXXX..",
    "XXXXXXXXXXXX",
    "XXXXXXXXXX..",
    "XXXXXX......",
    "XXXX........",
    "............",
};
static const int flag_h = 8;

// ─── Sprite structure ──────────────────────────────────────────────────────
typedef struct {
    int w, h;
    int stride;    // bytes per row
    uint8_t *bits; // mutable row-packed bits (1 bit per pixel)
} sprite1r_t;

sprite1r_t make_sprite_from_ascii(const char **rows, int h);
void       free_sprite(sprite1r_t *s);

int  sprite_get(const sprite1r_t *s, int x, int y);

// Drawing functions
void draw_sprite1r(lfb_t *lfb, const sprite1r_t *s, int x0, int y0, uint32_t color);
void draw_sprite1r_scaled(lfb_t *lfb, const sprite1r_t *s, int x0, int y0, uint32_t color, int scale);

// Draw sprite with a shadow beneath it for 3D depth illusion
void draw_sprite1r_shadow(lfb_t *lfb, const sprite1r_t *s, int x0, int y0, uint32_t color, int scale);
