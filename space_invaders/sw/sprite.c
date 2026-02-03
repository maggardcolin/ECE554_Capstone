#include "sprite.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/// make_sprite_from_ascii: Create a 1-bit sprite from ASCII art representation
/// Parameters:
///   rows - Array of string pointers, each row is a string of '.' (off) or 'X' (on)
///   h    - Height in rows
/// Returns: sprite1r_t structure containing width, height, stride, and bit-packed pixel data
/// Notes: All rows must be same width. Exits with error if width mismatch detected.
///        Uses packed bit representation (8 pixels per byte, MSB first).
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

/// free_sprite: Free pixel memory allocated by make_sprite_from_ascii
/// Parameters:
///   s - Pointer to sprite1r_t to deallocate
/// Returns: void
/// Notes: Safe to call multiple times (checks for NULL). Zeros out sprite dimensions.
void free_sprite(sprite1r_t *s) {
    if (s->bits) free(s->bits);
    s->bits = NULL;
    s->w = s->h = s->stride = 0;
}

/// sprite_get: Read a single pixel bit from a sprite at coordinates
/// Parameters:
///   s - Pointer to sprite1r_t
///   x - X-coordinate (0 to w-1)
///   y - Y-coordinate (0 to h-1)
/// Returns: 1 if pixel is set, 0 otherwise
/// Notes: No bounds checking; caller must ensure x and y are valid.
int sprite_get(const sprite1r_t *s, int x, int y) {
    const uint8_t *row = s->bits + y * s->stride;
    return (row[x >> 3] >> (7 - (x & 7))) & 1;
}

/// sprite_clear_bit: Erase a single pixel bit from a sprite at coordinates
/// Parameters:
///   s - Pointer to sprite1r_t
///   x - X-coordinate (0 to w-1)
///   y - Y-coordinate (0 to h-1)
/// Returns: void
/// Notes: Used for bunker damage. No bounds checking; caller must ensure x and y are valid.
void sprite_clear_bit(sprite1r_t *s, int x, int y) {
    uint8_t *row = s->bits + y * s->stride;
    row[x >> 3] &= (uint8_t)~(1u << (7 - (x & 7)));
}

/// bullet_hits_sprite: Determine if a bullet collides with a sprite
/// Parameters:
///   s  - Pointer to sprite1r_t to test against
///   sx - Sprite origin x-coordinate
///   sy - Sprite origin y-coordinate
///   bx - Bullet x-coordinate
///   by - Bullet y-coordinate
/// Returns: 1 if bullet is within sprite bounds and on a set pixel, 0 otherwise
/// Notes: Used for all collision detection (aliens, bunkers, player).
int bullet_hits_sprite(const sprite1r_t *s, int sx, int sy, int bx, int by) {
    if (bx < sx || by < sy || bx >= sx + s->w || by >= sy + s->h) return 0;
    return sprite_get(s, bx - sx, by - sy);
}

/// bunker_damage: Apply circular damage to bunker sprite at impact point
/// Parameters:
///   b      - Pointer to bunker sprite1r_t
///   hitx   - Damage center x-coordinate (relative to bunker origin)
///   hity   - Damage center y-coordinate (relative to bunker origin)
///   radius - Damage radius in pixels (circular area of effect)
/// Returns: void
/// Notes: Clears all sprite bits within radius circle. Used when bullets hit bunkers.
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
