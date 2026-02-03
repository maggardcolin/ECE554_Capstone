#include "game.h"
#include "../hw_contract.h"
#include "font.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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

/// draw_sprite1r: Render a 1-bit sprite to the framebuffer with a specified color
/// Parameters:
///   lfb   - Logical framebuffer to render to
///   s     - Pointer to sprite1r_t containing width, height, stride, and packed bits
///   x0    - Top-left x-coordinate in logical pixels
///   y0    - Top-left y-coordinate in logical pixels
///   color - 32-bit ARGB color to render sprite pixels in
/// Returns: void
/// Notes: Clips to framebuffer boundaries. Only renders pixels where sprite bit is 1.
static void draw_sprite1r(lfb_t *lfb, const sprite1r_t *s, int x0, int y0, uint32_t color) {
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

/// text_width_5x5: Calculate the width in logical pixels of text rendered at given scale
/// Parameters:
///   text  - Null-terminated string to measure
///   scale - Scaling factor (clamped to minimum 1)
/// Returns: Width in logical pixels; 0 if text is empty
/// Notes: Uses formula (count*6-1)*scale where 6 = glyph_width(5) + spacing(1)
static int text_width_5x5(const char *text, int scale) {
    if (scale < 1) scale = 1;
    int count = 0;
    for (const char *p = text; *p; p++) count++;
    if (count == 0) return 0;
    return (count * 6 - 1) * scale; // (w=5 + spacing=1) * n - spacing
}

/// draw_powerup_icon: Render a powerup icon based on its type
/// Parameters:
///   lfb  - Logical framebuffer to render to
///   x0   - Center x-coordinate in logical pixels
///   y0   - Center y-coordinate in logical pixels
///   type - Powerup type to render icon for
/// Returns: void
/// Notes: Draws colored circle with text overlay. Double-shot = red "2X", Triple-shot = blue lines,
///        Rapid-fire = red circle with bullet icon
///        Used for powerup pickups and inventory display on HUD.
static void draw_powerup_icon(lfb_t *lfb, int x0, int y0, powerup_type_t type) {
    if (type == POWERUP_DOUBLE_SHOT) {
        // Red circle with "2X" text
        int r = 6;
        for (int y = -r; y <= r; y++) {
            for (int x = -r; x <= r; x++) {
                if (x*x + y*y <= r*r) l_putpix(lfb, x0 + x, y0 + y, 0xFFFF0000);
            }
        }
        l_draw_text(lfb, x0 - 4, y0 - 3, "2X", 1, 0xFFFFFFFF);
    } else if (type == POWERUP_TRIPLE_SHOT) {
        // Blue circle with line
        int r = 6;
        for (int y = -r; y <= r; y++) {
            for (int x = -r; x <= r; x++) {
                if (x*x + y*y <= r*r) l_putpix(lfb, x0 + x, y0 + y, 0xFF0000FF);
            }
        }
        // Draw vertical line (white, center)
        for (int i = -4; i <= 4; i++) l_putpix(lfb, x0, y0 + i, 0xFFFFFFFF);
    } else {
        // Rapid-fire: red circle with bullet icon
        int r = 6;
        for (int y = -r; y <= r; y++) {
            for (int x = -r; x <= r; x++) {
                if (x*x + y*y <= r*r) l_putpix(lfb, x0 + x, y0 + y, 0xFFFF0000);
            }
        }
        // Bullet body (white)
        for (int i = -3; i <= 3; i++) l_putpix(lfb, x0, y0 + i, 0xFFFFFFFF);
        // Bullet tip (white)
        l_putpix(lfb, x0, y0 - 4, 0xFFFFFFFF);
    }
}

/// is_powerup_active: Check if a specific powerup type is currently active
/// Parameters:
///   g    - Pointer to game state
///   type - Powerup type to check for
/// Returns: 1 if powerup type is active in any slot, 0 otherwise
/// Notes: Checks all 5 powerup slots for the specified type.
static int is_powerup_active(const game_t *g, powerup_type_t type) {
    for (int i = 0; i < 5; i++) {
        if (g->powerup_slot_timer[i] > 0 && g->powerup_type_slot[i] == type) return 1;
    }
    return 0;
}

/// double_shot_active: Check if double damage powerup is currently active
/// Parameters:
///   g - Pointer to game state
/// Returns: 1 if POWERUP_DOUBLE_SHOT is active in any slot, 0 otherwise
/// Notes: When active, alien kills grant 20 points instead of 10.
static int double_shot_active(const game_t *g) {
    return is_powerup_active(g, POWERUP_DOUBLE_SHOT);
}

/// triple_shot_active: Check if triple shot powerup is currently active
/// Parameters:
///   g - Pointer to game state
/// Returns: 1 if POWERUP_TRIPLE_SHOT is active in any slot, 0 otherwise
/// Notes: When active, firing creates 3 bullets spread at angles.
static int triple_shot_active(const game_t *g) {
    return is_powerup_active(g, POWERUP_TRIPLE_SHOT);
}

/// rapid_fire_active: Check if rapid fire powerup is currently active
/// Parameters:
///   g - Pointer to game state
/// Returns: 1 if POWERUP_RAPID_FIRE is active in any slot, 0 otherwise
/// Notes: When active, firing uses a different cooldown.
static int rapid_fire_active(const game_t *g) {
    return is_powerup_active(g, POWERUP_RAPID_FIRE);
}

/// aliens_remaining: Check if any aliens are still alive on the grid
/// Parameters:
///   g - Pointer to game state
/// Returns: 1 if at least one alien_alive[r][c] is true, 0 if all defeated
/// Notes: Used to detect level completion. Scans entire AROWS x ACOLS grid.
static int aliens_remaining(const game_t *g) {
    for (int r = 0; r < AROWS; r++) for (int c = 0; c < ACOLS; c++) if (g->alien_alive[r][c]) return 1;
    return 0;
}

static void bunkers_rebuild(game_t *g);

/// setup_level: Initialize or reinitialize all level parameters
/// Parameters:
///   g            - Pointer to game state
///   level        - Level number to initialize (1 = first level)
///   reset_score  - If true, reset score to 0; if false, keep current score
/// Returns: void
/// Notes: Sets player position, alien grid state, animation timers, powerup state, bullets.
///        Called by game_reset() and during level progression.
static void setup_level(game_t *g, int level, int reset_score) {
    if (reset_score) g->score = 0;
    g->level = level;
    g->level_complete = 0;
    g->level_complete_timer = 0;
    g->level_just_completed = 0;

    g->powerup_active = 0;
    g->powerup_timer = 0;
    g->powerup_spawn_timer = 0;
    for (int i = 0; i < 5; i++) g->powerup_slot_timer[i] = 0;

    g->player_x = LW/2 - g->PLAYER.w/2;
    g->player_y = LH - 30;

    memset(g->alien_alive, 1, sizeof(g->alien_alive));
    // Level 1: aliens have 1 HP, Level 2+: aliens have 2 HP (green -> white -> dead)
    int hp = (level >= 2) ? 2 : 1;
    for (int r = 0; r < AROWS; r++) {
        for (int c = 0; c < ACOLS; c++) {
            g->alien_health[r][c] = hp;
        }
    }

    g->alien_origin_x = 50;
    g->alien_origin_y = 30;
    g->alien_dx = 1;
    g->alien_step_px = 2;
    g->alien_drop_px = 6;
    g->alien_frame = 0;
    g->alien_timer = 0;
    g->alien_period = 20 - (level - 1) * 2; // Faster aliens at higher levels
    if (g->alien_period < 5) g->alien_period = 5; // Minimum period

    // Boss alien setup (separate from regular aliens)
    g->boss_alive = 1;
    g->boss_health = 20 + (level - 1) * 5; // More HP at higher levels
    g->boss_x = (LW - g->BOSS_A.w) / 2;
    g->boss_y = 15;
    g->boss_dx = 1;
    g->boss_frame = 0;
    g->boss_timer = 0;
    g->boss_period = 15 - (level - 1) * 2; // Faster boss at higher levels
    if (g->boss_period < 5) g->boss_period = 5; // Minimum period

    for (int i = 0; i < MAX_PSHOTS; i++) {
        g->pshot[i].alive = 0;
        g->pshot_left[i].alive = 0;
        g->pshot_right[i].alive = 0;
    }
    g->ashot.alive = 0;
    g->boss_shot.alive = 0;
    g->fire_cooldown = 0;

    bunkers_rebuild(g);
}

/// bunkers_rebuild: Recreate all 4 bunker sprites from template, clearing previous damage
/// Parameters:
///   g - Pointer to game state
/// Returns: void
/// Notes: Called during level initialization and setup_level(). Frees old bunker sprites first.
///        Each bunker is 16 pixels wide x 20 pixels tall.
static void bunkers_rebuild(game_t *g) {
    free_sprite(&g->BUNKER0);
    free_sprite(&g->BUNKER1);
    free_sprite(&g->BUNKER2);
    free_sprite(&g->BUNKER3);

    g->BUNKER0 = make_sprite_from_ascii(bunker_rows, 16);
    g->BUNKER1 = make_sprite_from_ascii(bunker_rows, 16);
    g->BUNKER2 = make_sprite_from_ascii(bunker_rows, 16);
    g->BUNKER3 = make_sprite_from_ascii(bunker_rows, 16);

    g->bunkers[0] = &g->BUNKER0;
    g->bunkers[1] = &g->BUNKER1;
    g->bunkers[2] = &g->BUNKER2;
    g->bunkers[3] = &g->BUNKER3;
}

/// game_init: Initialize game state and all sprites for a new game
/// Parameters:
///   g - Pointer to game_t structure to initialize
/// Returns: void
/// Notes: Allocates and initializes sprites (player, aliens, bunkers). Sets up initial state.
///        Displays start screen. Must be called once before game_update/game_render.
void game_init(game_t *g) {
    memset(g, 0, sizeof(*g));
    g->PLAYER  = make_sprite_from_ascii(player_rows, 8);
    g->ALIEN_A = make_sprite_from_ascii(alienA_rows, 8);
    g->ALIEN_B = make_sprite_from_ascii(alienB_rows, 8);
    g->BOSS_A  = make_sprite_from_ascii(bossA_rows, 10);
    g->BOSS_B  = make_sprite_from_ascii(bossB_rows, 10);

    bunkers_rebuild(g);

    g->bunker_x[0] = 55;  g->bunker_x[1] = 115; g->bunker_x[2] = 175; g->bunker_x[3] = 235;
    g->bunker_y = LH - 65;

    g->game_over = 0;
    g->game_over_score = 0;
    g->start_screen = 1;
    g->start_screen_delay_timer = 0;
    g->lives = PLAYER_LIVES;
    for (int i = 0; i < 5; i++) {
        g->powerup_slot_timer[i] = 0;
        g->powerup_type_slot[i] = POWERUP_DOUBLE_SHOT;
    }

    setup_level(g, 1, 1);
}

/// game_reset: Reset game to initial state after game-over or level completion
/// Parameters:
///   g - Pointer to game state
/// Returns: void
/// Notes: Resets lives to 2, clears powerup inventory, initializes level 1.
///        Called when player presses SPACE after game-over screen.
void game_reset(game_t *g) {
    g->game_over = 0;
    g->game_over_score = 0;
    g->start_screen = 0;
    g->start_screen_delay_timer = 0;
    g->lives = PLAYER_LIVES;
    for (int i = 0; i < 5; i++) {
        g->powerup_slot_timer[i] = 0;
        g->powerup_type_slot[i] = POWERUP_DOUBLE_SHOT;
    }

    setup_level(g, 1, 1);
}

/// game_update: Main game logic loop - process input, collisions, animations, and state changes
/// Parameters:
///   g              - Pointer to game state
///   buttons        - Bitmask of current input: BTN_LEFT, BTN_RIGHT, BTN_FIRE, BTN_QUIT
///   vsync_counter  - Monotonically increasing counter (increments ~60 times/second)
/// Returns: void
/// Notes: Called every frame. Handles startup screen, level completion, game-over delays,
///        player movement, firing, alien movement, collision detection, powerup spawning.
void game_update(game_t *g, uint32_t buttons, uint32_t vsync_counter) {
    if (g->start_screen) {
        if (g->start_screen_delay_timer > 0) g->start_screen_delay_timer--;
        if (g->start_screen_delay_timer == 0 && (buttons & BTN_FIRE)) game_reset(g);
        return;
    }
    if (g->level_complete) {
        if (g->level_complete_timer > 0) g->level_complete_timer--;
        if (g->level_complete_timer == 0) {
            setup_level(g, g->level + 1, 0);
        }
        return;
    }
    if (g->game_over) {
        g->alien_frame = (int)((vsync_counter / 20u) & 1u);
        if (g->game_over_delay_timer > 0) g->game_over_delay_timer--;
        if (g->game_over_delay_timer == 0 && (buttons & BTN_FIRE)) {
            g->game_over = 0;
            g->game_over_score = 0;
            g->start_screen = 1;
            g->start_screen_delay_timer = 30;
            g->lives = PLAYER_LIVES;
            for (int i = 0; i < 5; i++) {
                g->powerup_slot_timer[i] = 0;
                g->powerup_type_slot[i] = POWERUP_DOUBLE_SHOT;
            }
            setup_level(g, 1, 1);
        }
        return;
    }

    // Decrement score every 0.33 seconds (20 ticks at 60 FPS)
    static int score_decrement_timer = 0;
    score_decrement_timer++;
    if (score_decrement_timer >= 20) {
        if (g->score > 0) g->score--;
        score_decrement_timer = 0;
    }

    for (int i = 0; i < 5; i++) {
        if (g->powerup_slot_timer[i] > 0) g->powerup_slot_timer[i]--;
    }

    if (!g->powerup_active) {
        g->powerup_spawn_timer++;
        if (g->powerup_spawn_timer >= 900) {
            int tile = 8;
            // Constrain spawning to center area and upper half to ensure hittable
            int cols = (LW - 80) / tile;  // Leave 40px margin on each side
            int rows = (LH - 100) / tile;  // Leave space at bottom for player
            int tx = 40/tile + (rand() % cols);  // Start from left margin
            int ty = 30/tile + (rand() % rows);  // Start from top, avoid bunkers
            g->powerup_x = tx * tile + tile / 2;
            g->powerup_y = ty * tile + tile / 2;
            g->powerup_type = (powerup_type_t)(g->score % POWERUP_COUNT);
            g->powerup_active = 1;
            g->powerup_timer = 300;  // 5 seconds at ~60 FPS
            g->powerup_spawn_timer = 0;
        }
    } else {
        if (g->powerup_timer > 0) g->powerup_timer--;
        if (g->powerup_timer == 0) g->powerup_active = 0;
    }
    if (buttons & BTN_LEFT)  g->player_x -= 2;
    if (buttons & BTN_RIGHT) g->player_x += 2;

    if (g->player_x < 0) g->player_x = 0;
    if (g->player_x > LW - g->PLAYER.w) g->player_x = LW - g->PLAYER.w;

    if (g->fire_cooldown > 0) g->fire_cooldown--;
    if ((buttons & BTN_FIRE) && g->fire_cooldown == 0) {
        int center_x = g->player_x + g->PLAYER.w/2;
        int center_y = g->player_y - 1;

        // Fire main center bullet into first free slot
        for (int i = 0; i < MAX_PSHOTS; i++) {
            if (!g->pshot[i].alive) {
                g->pshot[i].alive = 1;
                g->pshot[i].x = center_x;
                g->pshot[i].y = center_y;
                break;
            }
        }

        // Fire angled bullets if triple-shot is active
        if (triple_shot_active(g)) {
            for (int i = 0; i < MAX_PSHOTS; i++) {
                if (!g->pshot_left[i].alive) {
                    g->pshot_left[i].alive = 1;
                    g->pshot_left[i].x = center_x - 2;
                    g->pshot_left[i].y = center_y;
                    break;
                }
            }
            for (int i = 0; i < MAX_PSHOTS; i++) {
                if (!g->pshot_right[i].alive) {
                    g->pshot_right[i].alive = 1;
                    g->pshot_right[i].x = center_x + 2;
                    g->pshot_right[i].y = center_y;
                    break;
                }
            }
        }

        g->fire_cooldown = rapid_fire_active(g) ? 10 : 20;
    }

    g->alien_timer++;
    if (g->alien_timer >= g->alien_period) {
        g->alien_timer = 0;
        g->alien_frame ^= 1;

        int minc = ACOLS, maxc = -1;
        for (int r = 0; r < AROWS; r++) for (int c = 0; c < ACOLS; c++) {
            if (g->alien_alive[r][c]) { if (c < minc) minc = c; if (c > maxc) maxc = c; }
        }

        if (maxc >= 0) {
            const sprite1r_t *AS = g->alien_frame ? &g->ALIEN_B : &g->ALIEN_A;
            int spacing_x = 6;
            int grid_w = (maxc - minc + 1) * (AS->w + spacing_x) - spacing_x;

            int next_origin_x = g->alien_origin_x + g->alien_dx * g->alien_step_px;
            int next_left  = next_origin_x + minc * (AS->w + spacing_x);
            int next_right = next_left + grid_w;

            if (next_left < 5 || next_right > LW - 5) {
                g->alien_dx = -g->alien_dx;
                g->alien_origin_y += g->alien_drop_px;
                // After boss is killed, speed up aliens on each wall hit
                if (!g->boss_alive && g->boss_health == 0) {
                    g->alien_period -= 2;
                    if (g->alien_period < 5) g->alien_period = 5;
                }
            } else {
                g->alien_origin_x = next_origin_x;
            }

            int alive_count = 0;
            for (int r = 0; r < AROWS; r++) for (int c = 0; c < ACOLS; c++) alive_count += g->alien_alive[r][c] ? 1 : 0;
            if (alive_count > 0) {
                int target = 10 + alive_count / 6;
                if (target < g->alien_period) g->alien_period = target;
            }
        }
    }

    // Boss movement and animation (separate from grid)
    if (g->boss_alive) {
        g->boss_timer++;
        if (g->boss_timer >= g->boss_period) {
            g->boss_timer = 0;
            g->boss_frame ^= 1;
        }

        int next_bx = g->boss_x + g->boss_dx;
        int boss_w = g->BOSS_A.w;
        if (next_bx < 0 || next_bx > LW - boss_w) {
            g->boss_dx = -g->boss_dx;
            next_bx = g->boss_x + g->boss_dx;
        }
        g->boss_x = next_bx;

        // Descend once more than half of regular aliens are dead
        int alive_count = 0;
        for (int r = 0; r < AROWS; r++) {
            for (int c = 0; c < ACOLS; c++) {
                alive_count += g->alien_alive[r][c] ? 1 : 0;
            }
        }
        int total_aliens = AROWS * ACOLS;
        if (alive_count <= total_aliens / 2) {
            if (g->boss_timer == 0) g->boss_y += 1;
        }

        // Game over if boss reaches player's y position
        int boss_h = g->BOSS_A.h;
        if (g->boss_y + boss_h >= g->player_y) {
            g->game_over = 1;
            g->game_over_score = g->score;
            g->game_over_delay_timer = 120;
        }

        // Boss destroys bunkers on collision
        for (int i = 0; i < 4; i++) {
            sprite1r_t *b = g->bunkers[i];
            int bx = g->bunker_x[i], by = g->bunker_y;
            if (g->boss_x < bx + b->w && g->boss_x + boss_w > bx &&
                g->boss_y < by + b->h && g->boss_y + boss_h > by) {
                memset(b->bits, 0, (size_t)b->stride * (size_t)b->h);
            }
        }
    }

    // Aliens destroy bunkers on collision
    {
        const sprite1r_t *AS = g->alien_frame ? &g->ALIEN_B : &g->ALIEN_A;
        int spacing_x = 6, spacing_y = 5;
        for (int r = 0; r < AROWS; r++) {
            for (int c = 0; c < ACOLS; c++) {
                if (!g->alien_alive[r][c]) continue;
                int ax = g->alien_origin_x + c * (AS->w + spacing_x);
                int ay = g->alien_origin_y + r * (AS->h + spacing_y);
                for (int i = 0; i < 4; i++) {
                    sprite1r_t *b = g->bunkers[i];
                    int bx = g->bunker_x[i], by = g->bunker_y;
                    if (ax < bx + b->w && ax + AS->w > bx &&
                        ay < by + b->h && ay + AS->h > by) {
                        memset(b->bits, 0, (size_t)b->stride * (size_t)b->h);
                    }
                }
            }
        }
    }

    for (int i = 0; i < MAX_PSHOTS; i++) {
        if (g->pshot[i].alive) {
            g->pshot[i].y -= 4;
            if (g->pshot[i].y < 0) g->pshot[i].alive = 0;
        }
        if (g->pshot_left[i].alive) {
            g->pshot_left[i].x -= 1;  // Move left at -15 degree angle
            g->pshot_left[i].y -= 4;
            if (g->pshot_left[i].y < 0 || g->pshot_left[i].x < 0) g->pshot_left[i].alive = 0;
        }
        if (g->pshot_right[i].alive) {
            g->pshot_right[i].x += 1;  // Move right at +15 degree angle
            g->pshot_right[i].y -= 4;
            if (g->pshot_right[i].y < 0 || g->pshot_right[i].x >= LW) g->pshot_right[i].alive = 0;
        }
    }

    if (!g->ashot.alive) {
        static int col_cursor = 0;
        col_cursor = (col_cursor + 1) % ACOLS;
        int c = col_cursor;
        int r_fire = -1;
        for (int r = AROWS - 1; r >= 0; r--) if (g->alien_alive[r][c]) { r_fire = r; break; }

        if (r_fire >= 0 && (vsync_counter % 30u) == 0u) {
            const sprite1r_t *AS = g->alien_frame ? &g->ALIEN_B : &g->ALIEN_A;
            int spacing_x = 6, spacing_y = 5;
            int ax = g->alien_origin_x + c * (AS->w + spacing_x);
            int ay = g->alien_origin_y + r_fire * (AS->h + spacing_y);
            g->ashot.alive = 1;
            g->ashot.x = ax + AS->w/2;
            g->ashot.y = ay + AS->h + 1;
        }
    } else {
        g->ashot.y += 3;
        if (g->ashot.y > LH) g->ashot.alive = 0;
    }

    // Boss shooting logic (rate depends on health)
    if (g->boss_alive) {
        int health_pct = (g->boss_health * 100) / 20;
        int period = 90; // slow when green
        if (health_pct <= 10) period = 15;       // fast when red
        else if (health_pct <= 50) period = 30;  // medium when yellow

        if (!g->boss_shot.alive) {
            if ((vsync_counter % (uint32_t)period) == 0u) {
                const sprite1r_t *BS = g->boss_frame ? &g->BOSS_B : &g->BOSS_A;
                g->boss_shot.alive = 1;
                g->boss_shot.x = g->boss_x + BS->w / 2;
                g->boss_shot.y = g->boss_y + BS->h + 1;
            }
        } else {
            g->boss_shot.y += 2;
            if (g->boss_shot.y > LH) g->boss_shot.alive = 0;
        }
    } else {
        g->boss_shot.alive = 0;
    }

    // collisions: player shot (center)
    for (int si = 0; si < MAX_PSHOTS; si++) {
        if (!g->pshot[si].alive) continue;

        if (g->powerup_active) {
            int dx = g->pshot[si].x - g->powerup_x;
            int dy = g->pshot[si].y - g->powerup_y;
            if ((dx*dx + dy*dy) <= 36) {
                for (int i = 0; i < 5; i++) {
                    if (g->powerup_slot_timer[i] == 0) {
                        g->powerup_slot_timer[i] = 600;  // 10 seconds duration
                        g->powerup_type_slot[i] = g->powerup_type;
                        break;
                    }
                }
                g->powerup_active = 0;
                g->pshot[si].alive = 0;
            }
        }
        if (!g->pshot[si].alive) continue;

        if (g->boss_alive) {
            int boss_hit = 0;
            for (int bi = 0; bi < 5 && !boss_hit; bi++) {
                if (bullet_hits_sprite(g->boss_frame ? &g->BOSS_B : &g->BOSS_A,
                                       g->boss_x, g->boss_y,
                                       g->pshot[si].x, g->pshot[si].y - bi)) {
                    boss_hit = 1;
                }
            }
            if (boss_hit) {
                g->boss_health--;
                if (g->boss_health <= 0) {
                    g->boss_health = 0;
                    g->boss_alive = 0;
                    g->score += double_shot_active(g) ? 1000 : 500;
                }
                g->pshot[si].alive = 0;
            }
        }
        if (!g->pshot[si].alive) continue;

        const sprite1r_t *AS = g->alien_frame ? &g->ALIEN_B : &g->ALIEN_A;
        int spacing_x = 6, spacing_y = 5;
        int hit = 0;
        for (int r = 0; r < AROWS && !hit; r++) {
            for (int c = 0; c < ACOLS && !hit; c++) {
                if (!g->alien_alive[r][c]) continue;
                int ax = g->alien_origin_x + c * (AS->w + spacing_x);
                int ay = g->alien_origin_y + r * (AS->h + spacing_y);
                int bullet_hit = 0;
                for (int bi = 0; bi < 5 && !bullet_hit; bi++) {
                    if (bullet_hits_sprite(AS, ax, ay, g->pshot[si].x, g->pshot[si].y - bi)) {
                        bullet_hit = 1;
                    }
                }
                if (bullet_hit) {
                    g->alien_health[r][c]--;
                    if (g->alien_health[r][c] <= 0) {
                        g->alien_alive[r][c] = 0;
                        g->score += double_shot_active(g) ? 20 : 10;
                    }
                    g->pshot[si].alive = 0;
                    hit = 1;
                }
            }
        }
        if (!hit && g->pshot[si].alive) {
            for (int i = 0; i < 4 && g->pshot[si].alive; i++) {
                sprite1r_t *b = g->bunkers[i];
                int bx = g->bunker_x[i], by = g->bunker_y;
                if (bullet_hits_sprite(b, bx, by, g->pshot[si].x, g->pshot[si].y)) {
                    bunker_damage(b, g->pshot[si].x - bx, g->pshot[si].y - by, 3);
                    g->pshot[si].alive = 0;
                }
            }
        }
    }

    // collisions: player left shot (triple-shot)
    for (int si = 0; si < MAX_PSHOTS; si++) {
        if (!g->pshot_left[si].alive) continue;

        if (g->boss_alive) {
            int boss_hit = 0;
            for (int bi = 0; bi < 5 && !boss_hit; bi++) {
                if (bullet_hits_sprite(g->boss_frame ? &g->BOSS_B : &g->BOSS_A,
                                       g->boss_x, g->boss_y,
                                       g->pshot_left[si].x - bi / 2, g->pshot_left[si].y - bi)) {
                    boss_hit = 1;
                }
            }
            if (boss_hit) {
                g->boss_health--;
                if (g->boss_health <= 0) {
                    g->boss_health = 0;
                    g->boss_alive = 0;
                    g->score += double_shot_active(g) ? 1000 : 500;
                }
                g->pshot_left[si].alive = 0;
            }
        }
        if (!g->pshot_left[si].alive) continue;

        int hit = 0;
        const sprite1r_t *AS = g->alien_frame ? &g->ALIEN_B : &g->ALIEN_A;
        int spacing_x = 6, spacing_y = 5;
        for (int r = 0; r < AROWS && !hit; r++) {
            for (int c = 0; c < ACOLS && !hit; c++) {
                if (!g->alien_alive[r][c]) continue;
                int ax = g->alien_origin_x + c * (AS->w + spacing_x);
                int ay = g->alien_origin_y + r * (AS->h + spacing_y);
                int bullet_hit = 0;
                for (int bi = 0; bi < 5 && !bullet_hit; bi++) {
                    if (bullet_hits_sprite(AS, ax, ay, g->pshot_left[si].x - bi/2, g->pshot_left[si].y - bi)) {
                        bullet_hit = 1;
                    }
                }
                if (bullet_hit) {
                    g->alien_health[r][c]--;
                    if (g->alien_health[r][c] <= 0) {
                        g->alien_alive[r][c] = 0;
                        g->score += double_shot_active(g) ? 20 : 10;
                    }
                    g->pshot_left[si].alive = 0;
                    hit = 1;
                }
            }
        }
        if (!hit && g->pshot_left[si].alive) {
            for (int i = 0; i < 4 && g->pshot_left[si].alive; i++) {
                sprite1r_t *b = g->bunkers[i];
                int bx = g->bunker_x[i], by = g->bunker_y;
                if (bullet_hits_sprite(b, bx, by, g->pshot_left[si].x, g->pshot_left[si].y)) {
                    bunker_damage(b, g->pshot_left[si].x - bx, g->pshot_left[si].y - by, 3);
                    g->pshot_left[si].alive = 0;
                }
            }
        }
    }

    // collisions: player right shot (triple-shot)
    for (int si = 0; si < MAX_PSHOTS; si++) {
        if (!g->pshot_right[si].alive) continue;

        if (g->boss_alive) {
            int boss_hit = 0;
            for (int bi = 0; bi < 5 && !boss_hit; bi++) {
                if (bullet_hits_sprite(g->boss_frame ? &g->BOSS_B : &g->BOSS_A,
                                       g->boss_x, g->boss_y,
                                       g->pshot_right[si].x + bi / 2, g->pshot_right[si].y - bi)) {
                    boss_hit = 1;
                }
            }
            if (boss_hit) {
                g->boss_health--;
                if (g->boss_health <= 0) {
                    g->boss_health = 0;
                    g->boss_alive = 0;
                    g->score += double_shot_active(g) ? 1000 : 500;
                }
                g->pshot_right[si].alive = 0;
            }
        }
        if (!g->pshot_right[si].alive) continue;

        int hit = 0;
        const sprite1r_t *AS = g->alien_frame ? &g->ALIEN_B : &g->ALIEN_A;
        int spacing_x = 6, spacing_y = 5;
        for (int r = 0; r < AROWS && !hit; r++) {
            for (int c = 0; c < ACOLS && !hit; c++) {
                if (!g->alien_alive[r][c]) continue;
                int ax = g->alien_origin_x + c * (AS->w + spacing_x);
                int ay = g->alien_origin_y + r * (AS->h + spacing_y);
                int bullet_hit = 0;
                for (int bi = 0; bi < 5 && !bullet_hit; bi++) {
                    if (bullet_hits_sprite(AS, ax, ay, g->pshot_right[si].x + bi/2, g->pshot_right[si].y - bi)) {
                        bullet_hit = 1;
                    }
                }
                if (bullet_hit) {
                    g->alien_health[r][c]--;
                    if (g->alien_health[r][c] <= 0) {
                        g->alien_alive[r][c] = 0;
                        g->score += double_shot_active(g) ? 20 : 10;
                    }
                    g->pshot_right[si].alive = 0;
                    hit = 1;
                }
            }
        }
        if (!hit && g->pshot_right[si].alive) {
            for (int i = 0; i < 4 && g->pshot_right[si].alive; i++) {
                sprite1r_t *b = g->bunkers[i];
                int bx = g->bunker_x[i], by = g->bunker_y;
                if (bullet_hits_sprite(b, bx, by, g->pshot_right[si].x, g->pshot_right[si].y)) {
                    bunker_damage(b, g->pshot_right[si].x - bx, g->pshot_right[si].y - by, 3);
                    g->pshot_right[si].alive = 0;
                }
            }
        }
    }

    // collisions: alien shot
    if (g->ashot.alive) {
        if (bullet_hits_sprite(&g->PLAYER, g->player_x, g->player_y, g->ashot.x, g->ashot.y)) {
            g->lives--;
            for (int i = 0; i < MAX_PSHOTS; i++) {
                g->pshot[i].alive = 0;
                g->pshot_left[i].alive = 0;
                g->pshot_right[i].alive = 0;
            }
            g->ashot.alive = 0;
            if (g->lives <= 0) {
                g->game_over = 1;
                g->game_over_score = g->score;
                g->game_over_delay_timer = 120;
            }
        } else {
            for (int i = 0; i < 4 && g->ashot.alive; i++) {
                sprite1r_t *b = g->bunkers[i];
                int bx = g->bunker_x[i], by = g->bunker_y;
                if (bullet_hits_sprite(b, bx, by, g->ashot.x, g->ashot.y)) {
                    bunker_damage(b, g->ashot.x - bx, g->ashot.y - by, 3);
                    g->ashot.alive = 0;
                }
            }
        }
    }

    // collisions: boss shot
    if (g->boss_shot.alive) {
        if (bullet_hits_sprite(&g->PLAYER, g->player_x, g->player_y, g->boss_shot.x, g->boss_shot.y)) {
            g->lives--;
            for (int i = 0; i < MAX_PSHOTS; i++) {
                g->pshot[i].alive = 0;
                g->pshot_left[i].alive = 0;
                g->pshot_right[i].alive = 0;
            }
            g->boss_shot.alive = 0;
            if (g->lives <= 0) {
                g->game_over = 1;
                g->game_over_score = g->score;
                g->game_over_delay_timer = 120;
            }
        } else {
            for (int i = 0; i < 4 && g->boss_shot.alive; i++) {
                sprite1r_t *b = g->bunkers[i];
                int bx = g->bunker_x[i], by = g->bunker_y;
                if (bullet_hits_sprite(b, bx, by, g->boss_shot.x, g->boss_shot.y)) {
                    bunker_damage(b, g->boss_shot.x - bx, g->boss_shot.y - by, 3);
                    g->boss_shot.alive = 0;
                }
            }
        }
    }

    // Check if aliens have reached the player level
    if (!g->game_over) {
        const sprite1r_t *AS = g->alien_frame ? &g->ALIEN_B : &g->ALIEN_A;
        int spacing_x = 6, spacing_y = 5;
        for (int r = 0; r < AROWS && !g->game_over; r++) {
            for (int c = 0; c < ACOLS && !g->game_over; c++) {
                if (!g->alien_alive[r][c]) continue;
                int ay = g->alien_origin_y + r * (AS->h + spacing_y);
                if (ay >= g->player_y) {
                    g->game_over = 1;
                    g->game_over_score = g->score;
                    g->game_over_delay_timer = 120;
                }
            }
        }
    }

    if (!g->game_over && !aliens_remaining(g) && !g->boss_alive) {
        g->level_complete = 1;
        g->level_complete_timer = 60;
        g->level_just_completed = g->level;
    }
}

/// game_render: Render the entire game frame to the logical framebuffer
/// Parameters:
///   g   - Pointer to game state containing all visual data
///   lfb - Logical framebuffer to draw to
/// Returns: void
/// Notes: Draws different screens based on game state: startup, gameplay, level-complete, game-over.
///        Renders sprites (player, aliens, bunkers), bullets, HUD (score, level, lives), powerups.
void game_render(game_t *g, lfb_t *lfb) {
    if (g->start_screen) {
        l_clear(lfb, 0xFF000000);
        const char *title = "BULLET HELL";
        const char *title2 = "SPACE INVADERS";
        const char *prompt = "PRESS SPACE TO START";

        int title_scale = 3;
        int title_w = text_width_5x5(title, title_scale);
        int title_x = (LW - title_w) / 2;
        int title_y = LH / 2 - 40;
        l_draw_text(lfb, title_x, title_y, title, title_scale, 0xFF00FF00);

        int title2_scale = 3;
        int title2_w = text_width_5x5(title2, title2_scale);
        int title2_x = (LW - title2_w) / 2;
        int title2_y = LH / 2 - 10;
        l_draw_text(lfb, title2_x, title2_y, title2, title2_scale, 0xFF00FF00);

        int prompt_scale = 2;
        int prompt_w = text_width_5x5(prompt, prompt_scale);
        int prompt_x = (LW - prompt_w) / 2;
        int prompt_y = title2_y + 40;
        l_draw_text(lfb, prompt_x, prompt_y, prompt, prompt_scale, 0xFFFFFFFF);
        return;
    }

    if (g->level_complete) {
        l_clear(lfb, 0xFF000000);
        char msg[32];
        int lvl = g->level_just_completed;
        if (lvl < 0) lvl = 0;
        snprintf(msg, sizeof(msg), "LEVEL %d COMPLETE", lvl);
        int scale = 3;
        int w = text_width_5x5(msg, scale);
        int x = (LW - w) / 2;
        int y = LH / 2 - 10;
        l_draw_text(lfb, x, y, msg, scale, 0xFFFFFFFF);
        return;
    }

    if (g->game_over) {
        l_clear(lfb, 0xFF000000);

        const char *title = "GAME OVER";
        int title_scale = 4;
        int title_w = text_width_5x5(title, title_scale);
        int title_x = (LW - title_w) / 2;
        int title_y = 10;
        l_draw_text(lfb, title_x, title_y, title, title_scale, 0xFFFF0000);

        int cx = LW / 2;
        int cy = LH / 2 - 10;

        const sprite1r_t *AS = g->alien_frame ? &g->ALIEN_B : &g->ALIEN_A;
        int a_w = AS->w;
        int a_h = AS->h;
        int p_w = g->PLAYER.w;
        int p_h = g->PLAYER.h;

        uint32_t player_color = triple_shot_active(g) ? 0xFF0000FF : 0xFF00FF00;
        draw_sprite1r(lfb, &g->PLAYER, cx - p_w / 2, cy - p_h / 2, player_color);

        int r = 22;
        int hx[6] = { cx, cx + r, cx + r, cx, cx - r, cx - r };
        int hy[6] = { cy - r, cy - r/2, cy + r/2, cy + r, cy + r/2, cy - r/2 };
        for (int i = 0; i < 6; i++) {
            draw_sprite1r(lfb, AS, hx[i] - a_w / 2, hy[i] - a_h / 2, 0xFFFFFFFF);
        }

        const char *score_label = "SCORE:";
        int label_scale = 2;
        int label_w = text_width_5x5(score_label, label_scale);
        int label_x = (LW - label_w) / 2 - 12;
        int label_y = cy + r + 15;
        l_draw_text(lfb, label_x, label_y, score_label, label_scale, 0xFFFFFFFF);
        l_draw_score(lfb, label_x + label_w + 6, label_y + 2, g->game_over_score, 0xFFFFFFFF);

        if (g->game_over_delay_timer == 0) {
            const char *prompt = "PRESS SPACE TO CONTINUE";
            int prompt_scale = 2;
            int prompt_w = text_width_5x5(prompt, prompt_scale);
            int prompt_x = (LW - prompt_w) / 2;
            int prompt_y = label_y + 25;
            l_draw_text(lfb, prompt_x, prompt_y, prompt, prompt_scale, 0xFFFFFFFF);
        }
        return;
    }

    l_clear(lfb, 0xFF000000);
    {
        const char *label = "SCORE:";
        int label_scale = 1;
        int label_w = text_width_5x5(label, label_scale);
        int x = 5;
        l_draw_text(lfb, x, 5, label, label_scale, 0xFFFFFFFF);
        l_draw_score(lfb, x + label_w + 6, 5, g->score, 0xFFFFFFFF);
    }

    // Boss health bar to the right of the score
    if (g->boss_alive) {
        uint32_t boss_color = 0xFF00FF00; // Green 50-100%
        int health_pct = (g->boss_health * 100) / 20;
        if (health_pct <= 10) boss_color = 0xFFFF0000; // Red < 10%
        else if (health_pct <= 50) boss_color = 0xFFFFFF00; // Yellow 10-50%

        const char *boss_label = "BOSS";
        int boss_label_w = text_width_5x5(boss_label, 1);
        int score_label_w = text_width_5x5("SCORE:", 1);
        int score_max_w = 8 * 4; // 8 digits max, 4px per digit
        int bx = 5 + score_label_w + 6 + score_max_w + 8;
        int by = 5;

        l_draw_text(lfb, bx, by, boss_label, 1, boss_color);

        int bar_x = bx + boss_label_w + 6;
        int bar_y = by + 1;
        int bar_w = 50;
        int bar_h = 6;

        // White border
        for (int i = 0; i <= bar_w; i++) {
            l_putpix(lfb, bar_x + i, bar_y - 1, 0xFFFFFFFF);
            l_putpix(lfb, bar_x + i, bar_y + bar_h, 0xFFFFFFFF);
        }
        for (int j = 0; j <= bar_h; j++) {
            l_putpix(lfb, bar_x - 1, bar_y + j, 0xFFFFFFFF);
            l_putpix(lfb, bar_x + bar_w, bar_y + j, 0xFFFFFFFF);
        }

        int fill_w = (g->boss_health * bar_w) / 20;
        for (int i = 0; i < fill_w; i++) {
            for (int j = 0; j < bar_h; j++) {
                l_putpix(lfb, bar_x + i, bar_y + j, boss_color);
            }
        }
    }

    {
        const char *label = "LEVEL:";
        int label_scale = 1;
        int label_w = text_width_5x5(label, label_scale);
        int x = LW - (label_w + 6 + 3) - 5;
        l_draw_text(lfb, x, 5, label, label_scale, 0xFFFFFFFF);
        l_draw_score(lfb, x + label_w + 6, 5, g->level, 0xFFFFFFFF);
    }

    {
        const char *p1 = "PLAYER 1";
        int scale = 1;
        int x = 5;
        int y = LH - 12;
        l_draw_text(lfb, x, y, p1, scale, 0xFFFFFFFF);

        int lx = x + text_width_5x5(p1, scale) + 6;
        int bar_w = 40;
        int bar_h = 6;
        int bar_y = y - 1;

        int max_lives = PLAYER_LIVES;
        if (g->lives > max_lives) max_lives = g->lives;
        int health_pct = (max_lives > 0) ? (g->lives * 100) / max_lives : 0;

        uint32_t life_color = 0xFF00FF00; // green >= 50%
        if (health_pct <= 33) life_color = 0xFFFF0000;
        else if (health_pct <= 67) life_color = 0xFFFFFF00;

        // White border
        for (int i = 0; i <= bar_w; i++) {
            l_putpix(lfb, lx + i, bar_y - 1, 0xFFFFFFFF);
            l_putpix(lfb, lx + i, bar_y + bar_h, 0xFFFFFFFF);
        }
        for (int j = 0; j <= bar_h; j++) {
            l_putpix(lfb, lx - 1, bar_y + j, 0xFFFFFFFF);
            l_putpix(lfb, lx + bar_w, bar_y + j, 0xFFFFFFFF);
        }

        int fill_w = (max_lives > 0) ? (g->lives * bar_w) / max_lives : 0;
        for (int i = 0; i < fill_w; i++) {
            for (int j = 0; j < bar_h; j++) {
                l_putpix(lfb, lx + i, bar_y + j, life_color);
            }
        }
    }

    // bunkers
    for (int i = 0; i < 4; i++) {
        // blit sprite bits onto lfb
        sprite1r_t *b = g->bunkers[i];
        int x0 = g->bunker_x[i], y0 = g->bunker_y;
        draw_sprite1r(lfb, b, x0, y0, 0xFF00FF00);
    }

    // boss (drawn behind regular aliens)
    if (g->boss_alive) {
        int health_pct = (g->boss_health * 100) / 20;
        uint32_t boss_color = 0xFF00FF00;
        if (health_pct <= 10) boss_color = 0xFFFF0000;
        else if (health_pct <= 50) boss_color = 0xFFFFFF00;

        const sprite1r_t *BS = g->boss_frame ? &g->BOSS_B : &g->BOSS_A;
        draw_sprite1r(lfb, BS, g->boss_x, g->boss_y, boss_color);
    }

    // aliens
    const sprite1r_t *AS = g->alien_frame ? &g->ALIEN_B : &g->ALIEN_A;
    int spacing_x = 6, spacing_y = 5;
    for (int r = 0; r < AROWS; r++) {
        for (int c = 0; c < ACOLS; c++) {
            if (!g->alien_alive[r][c]) continue;
            int ax = g->alien_origin_x + c * (AS->w + spacing_x);
            int ay = g->alien_origin_y + r * (AS->h + spacing_y);

            // Green if full health, white if damaged
            uint32_t alien_color = (g->alien_health[r][c] > 1) ? 0xFF00FF00 : 0xFFFFFFFF;
            draw_sprite1r(lfb, AS, ax, ay, alien_color);
        }
    }

    uint32_t player_color = triple_shot_active(g) ? 0xFF0000FF : 0xFF00FF00;
    draw_sprite1r(lfb, &g->PLAYER, g->player_x, g->player_y, player_color);

    if (g->powerup_active) {
        draw_powerup_icon(lfb, g->powerup_x, g->powerup_y, g->powerup_type);
    }

    // bullets
    for (int s = 0; s < MAX_PSHOTS; s++) {
        if (g->pshot[s].alive) {
            for (int i = 0; i < 5; i++) l_putpix(lfb, g->pshot[s].x, g->pshot[s].y - i, 0xFFFFFFFF);
        }
        if (g->pshot_left[s].alive) {
            for (int i = 0; i < 5; i++) l_putpix(lfb, g->pshot_left[s].x - i/2, g->pshot_left[s].y - i, 0xFF0000FF);
        }
        if (g->pshot_right[s].alive) {
            for (int i = 0; i < 5; i++) l_putpix(lfb, g->pshot_right[s].x + i/2, g->pshot_right[s].y - i, 0xFF0000FF);
        }
    }
    if (g->ashot.alive) for (int i = 0; i < 5; i++) l_putpix(lfb, g->ashot.x, g->ashot.y + i, 0xFFFF0000);
    if (g->boss_shot.alive) {
        for (int i = 0; i < 5; i++) l_putpix(lfb, g->boss_shot.x, g->boss_shot.y + i, 0xFFFF0000);
    }

    {
        int base_y = LH - 12;
        int spacing = 14;
        int right_x = LW - 6;
        for (int i = 0; i < 5; i++) {
            int x = right_x - (4 - i) * spacing;
            if (g->powerup_slot_timer[i] > 0) draw_powerup_icon(lfb, x, base_y + 4, g->powerup_type_slot[i]);
        }
    }
}
