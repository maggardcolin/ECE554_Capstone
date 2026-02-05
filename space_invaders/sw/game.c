#include "game.h"
#include "../hw_contract.h"
#include "font.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

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

static int digit_count(uint32_t v)
{
    int d = 1;
    while (v >= 10) {
        v /= 10;
        d++;
    }
    return d;
}

/// draw_sprite1r_scaled: Render a 1-bit sprite scaled by an integer factor
/// Parameters:
///   lfb   - Logical framebuffer to render to
///   s     - Pointer to sprite1r_t containing width, height, stride, and packed bits
///   x0    - Top-left x-coordinate in logical pixels
///   y0    - Top-left y-coordinate in logical pixels
///   color - 32-bit ARGB color to render sprite pixels in
///   scale - Integer scale factor (minimum 1)
static void draw_sprite1r_scaled(lfb_t *lfb, const sprite1r_t *s, int x0, int y0, uint32_t color, int scale) {
    if (scale < 1) scale = 1;
    for (int y = 0; y < s->h; y++) {
        int yy = y0 + y * scale;
        if ((unsigned)yy >= (unsigned)LH) continue;
        const uint8_t *row = s->bits + y * s->stride;
        for (int x = 0; x < s->w; x++) {
            if (!((row[x >> 3] >> (7 - (x & 7))) & 1)) continue;
            int xx = x0 + x * scale;
            if ((unsigned)xx >= (unsigned)LW) continue;
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

static void draw_filled_circle(lfb_t *lfb, int x0, int y0, int r, uint32_t color) {
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x * x + y * y <= r * r) l_putpix(lfb, x0 + x, y0 + y, color);
        }
    }
}

static void draw_bar(lfb_t *lfb, int x, int y, int w, int h, int fill_w, uint32_t fill_color) {
    if (fill_w < 0) fill_w = 0;
    if (fill_w > w) fill_w = w;

    // White border
    for (int i = 0; i <= w; i++) {
        l_putpix(lfb, x + i, y - 1, 0xFFFFFFFF);
        l_putpix(lfb, x + i, y + h, 0xFFFFFFFF);
    }
    for (int j = 0; j <= h; j++) {
        l_putpix(lfb, x - 1, y + j, 0xFFFFFFFF);
        l_putpix(lfb, x + w, y + j, 0xFFFFFFFF);
    }
    
    // Top-left corner pixel
    l_putpix(lfb, x - 1, y - 1, 0xFFFFFFFF);

    for (int i = 0; i < fill_w; i++) {
        for (int j = 0; j < h; j++) {
            l_putpix(lfb, x + i, y + j, fill_color);
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
        // Yellow circle with "2X" text
        int r = 6;
        draw_filled_circle(lfb, x0, y0, r, 0xFFFFFF00);
        l_draw_text(lfb, x0 - 4, y0 - 3, "2X", 1, 0xFF000000);
    } else if (type == POWERUP_TRIPLE_SHOT) {
        // Blue circle with line
        int r = 6;
        draw_filled_circle(lfb, x0, y0, r, 0xFF0000FF);
        // Draw vertical line (white, center)
        for (int i = -4; i <= 4; i++) l_putpix(lfb, x0, y0 + i, 0xFFFFFFFF);
    } else {
        // Rapid-fire: orange circle with bullet icon
        int r = 6;
        draw_filled_circle(lfb, x0, y0, r, 0xFFFA500);
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

/// double_shot_active: Check if double points powerup is currently active
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

static void clear_player_shots(game_t *g) {
    for (int i = 0; i < MAX_PSHOTS; i++) {
        g->pshot[i].alive = 0;
        g->pshot_left[i].alive = 0;
        g->pshot_right[i].alive = 0;
    }
}

static void try_spawn_bullet(bullet_t *shots, int x, int y) {
    for (int i = 0; i < MAX_PSHOTS; i++) {
        if (!shots[i].alive) {
            shots[i].alive = 1;
            shots[i].x = x;
            shots[i].y = y;
            break;
        }
    }
}

static void fire_player_shots(game_t *g, int center_x, int center_y) {
    try_spawn_bullet(g->pshot, center_x, center_y);
    if (triple_shot_active(g)) {
        try_spawn_bullet(g->pshot_left, center_x - 2, center_y);
        try_spawn_bullet(g->pshot_right, center_x + 2, center_y);
    }
}

static int compute_fire_cooldown(const game_t *g) {
    int base = 20 - g->fire_speed_bonus * 2;
    if (base < 5) base = 5;
    int cooldown = rapid_fire_active(g) ? (base / 2) : base;
    if (cooldown < 3) cooldown = 3;
    return cooldown;
}

static void update_player_shots(game_t *g) {
    for (int i = 0; i < MAX_PSHOTS; i++) {
        if (g->pshot[i].alive) {
            g->pshot[i].y -= 4;
            if (g->pshot[i].y < 0) g->pshot[i].alive = 0;
        }
        if (g->pshot_left[i].alive) {
            g->pshot_left[i].x -= 1;
            g->pshot_left[i].y -= 4;
            if (g->pshot_left[i].y < 0 || g->pshot_left[i].x < 0) g->pshot_left[i].alive = 0;
        }
        if (g->pshot_right[i].alive) {
            g->pshot_right[i].x += 1;
            g->pshot_right[i].y -= 4;
            if (g->pshot_right[i].y < 0 || g->pshot_right[i].x >= LW) g->pshot_right[i].alive = 0;
        }
    }
}

static void render_player_shots(const game_t *g, lfb_t *lfb) {
    for (int s = 0; s < MAX_PSHOTS; s++) {
        if (g->pshot[s].alive) {
            uint32_t bullet_color = rapid_fire_active(g) ? 0xFFFFA500 : 0xFFFFFFFF;
            for (int i = 0; i < 5; i++) l_putpix(lfb, g->pshot[s].x, g->pshot[s].y - i, bullet_color);
        }
        if (g->pshot_left[s].alive) {
            for (int i = 0; i < 5; i++) l_putpix(lfb, g->pshot_left[s].x - i/2, g->pshot_left[s].y - i, 0xFF0000FF);
        }
        if (g->pshot_right[s].alive) {
            for (int i = 0; i < 5; i++) l_putpix(lfb, g->pshot_right[s].x + i/2, g->pshot_right[s].y - i, 0xFF0000FF);
        }
    }
}

static int count_aliens_remaining(const game_t *g) {
    int remaining = 0;
    for (int r = 0; r < AROWS; r++) {
        for (int c = 0; c < ACOLS; c++) {
            if (g->alien_alive[r][c]) remaining++;
        }
    }
    return remaining;
}

static int bullet_x_with_spread(const bullet_t *shot, int bi, int spread_dir) {
    return shot->x + (spread_dir * (bi / 2));
}

static void handle_bunker_bullet_collisions(game_t *g, bullet_t *shot, int damage_radius);

static void handle_player_shot_collisions(game_t *g, bullet_t *shots, int spread_dir, int can_collect_powerup) {
    for (int si = 0; si < MAX_PSHOTS; si++) {
        if (!shots[si].alive) continue;

        if (can_collect_powerup && g->powerup_active) {
            int dx = shots[si].x - g->powerup_x;
            int dy = shots[si].y - g->powerup_y;
            if ((dx*dx + dy*dy) <= 36) {
                for (int i = 0; i < 5; i++) {
                    if (g->powerup_slot_timer[i] == 0) {
                        g->powerup_slot_timer[i] = 600;
                        g->powerup_type_slot[i] = g->powerup_type;
                        break;
                    }
                }
                g->powerup_active = 0;
                shots[si].alive = 0;
            }
        }
        if (!shots[si].alive) continue;

        if (g->boss_alive) {
            int boss_hit = 0;
            for (int bi = 0; bi < 5 && !boss_hit; bi++) {
                if (bullet_hits_sprite(g->boss_frame ? &g->BOSS_B : &g->BOSS_A,
                                       g->boss_x, g->boss_y,
                                       bullet_x_with_spread(&shots[si], bi, spread_dir), shots[si].y - bi)) {
                    boss_hit = 1;
                }
            }
            if (boss_hit) {
                g->boss_health -= g->player_damage;
                if (g->boss_health <= 0) {
                    g->boss_health = 0;
                    g->boss_alive = 0;
                    g->boss_laser.alive = 0;  // Clear laser when boss dies
                    g->score += double_shot_active(g) ? 1000 : 500;
                }
                shots[si].alive = 0;
            }
        }
        if (!shots[si].alive) continue;

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
                    if (bullet_hits_sprite(AS, ax, ay,
                                           bullet_x_with_spread(&shots[si], bi, spread_dir), shots[si].y - bi)) {
                        bullet_hit = 1;
                    }
                }
                if (bullet_hit) {
                    g->alien_health[r][c] -= g->player_damage;
                    if (g->alien_health[r][c] <= 0) {
                        g->alien_alive[r][c] = 0;
                        int remaining = count_aliens_remaining(g);
                        int points;
                        if (remaining == 0) points = double_shot_active(g) ? 200 : 100;
                        else points = double_shot_active(g) ? 20 : 10;
                        g->score += points;
                    }
                    shots[si].alive = 0;
                    hit = 1;
                }
            }
        }
        if (!hit && shots[si].alive) {
            handle_bunker_bullet_collisions(g, &shots[si], 3);
        }
    }
}

static void handle_enemy_shot_collisions(game_t *g, bullet_t *shot) {
    if (!shot->alive) return;
    if (bullet_hits_sprite(&g->PLAYER, g->player_x, g->player_y, shot->x, shot->y)) {
        g->lives--;
        clear_player_shots(g);
        shot->alive = 0;
        if (g->lives <= 0) {
            g->game_over = 1;
            g->game_over_score = g->score;
            g->game_over_delay_timer = 90;
        }
    } else {
        handle_bunker_bullet_collisions(g, shot, 3);
    }
}

static void handle_bunker_bullet_collisions(game_t *g, bullet_t *shot, int damage_radius) {
    if (!shot->alive) return;
    // No bunker collisions on level 0 (tutorial)
    if (g->level == 0) return;
    for (int i = 0; i < 4 && shot->alive; i++) {
        sprite1r_t *b = g->bunkers[i];
        int bx = g->bunker_x[i], by = g->bunker_y;
        if (bullet_hits_sprite(b, bx, by, shot->x, shot->y)) {
            bunker_damage(b, shot->x - bx, shot->y - by, damage_radius);
            shot->alive = 0;
        }
    }
}

static void reset_powerup_slots(game_t *g, powerup_type_t default_type) {
    for (int i = 0; i < 5; i++) {
        g->powerup_slot_timer[i] = 0;
        g->powerup_type_slot[i] = default_type;
    }
}

static void reset_shop_state(game_t *g) {
    g->in_shop = 0;
    g->shop_count = 0;
    g->shop_next_level = 1;
    g->shop_anim_timer = 0;
    g->shopkeeper_frame = 0;
    for (int i = 0; i < MAX_SHOP_ITEMS; i++) g->shop_items[i].active = 0;
}

static void reset_player_progression(game_t *g) {
    g->lives = PLAYER_LIVES;
    g->player_speed = 2;
    g->fire_speed_bonus = 0;
    g->player_damage = 1;
    reset_shop_state(g);
    reset_powerup_slots(g, POWERUP_DOUBLE_SHOT);
}

static void destroy_bunkers_on_overlap(game_t *g, int x, int y, int w, int h) {
    for (int i = 0; i < 4; i++) {
        sprite1r_t *b = g->bunkers[i];
        int bx = g->bunker_x[i], by = g->bunker_y;
        if (x < bx + b->w && x + w > bx && y < by + b->h && y + h > by) {
            memset(b->bits, 0, (size_t)b->stride * (size_t)b->h);
        }
    }
}

static void setup_level(game_t *g, int level, int reset_score);

static const char *shop_item_label(shop_item_type_t type) {
    switch (type) {
        case SHOP_ITEM_FIRE_SPEED: return "FIRE";
        case SHOP_ITEM_MOVE_SPEED: return "MOVE";
        case SHOP_ITEM_LIFE:       return "LIFE";
        case SHOP_ITEM_DAMAGE:     return "DMG";
        default:                   return "ITEM";
    }
}

static void enter_shop(game_t *g) {
    g->in_shop = 1;
    g->shop_next_level = g->level + 1;
    g->shop_count++;

    int price_multiplier = g->shop_count; // Base * shops seen
    int spacing = 60;
    int start_x = LW / 2 - spacing;
    int y = LH / 2;
    for (int i = 0; i < MAX_SHOP_ITEMS; i++) {
        g->shop_items[i].active = 1;
        g->shop_items[i].type = (shop_item_type_t)(rand() % SHOP_ITEM_COUNT);
        g->shop_items[i].price = 500 * price_multiplier;
        g->shop_items[i].x = start_x + i * spacing;
        g->shop_items[i].y = y;
    }

    clear_player_shots(g);
    g->ashot.alive = 0;
    g->boss_shot.alive = 0;
    g->fire_cooldown = 0;

    // Reset powerups
    for (int i = 0; i < 5; i++) {
        g->powerup_slot_timer[i] = 0;
    }

    // Position player on left side, keep y position constant
    g->player_x = 10;

    g->shop_anim_timer = 0;
    g->shopkeeper_frame = 0;
}

static void shop_update(game_t *g, uint32_t buttons, uint32_t vsync_counter) {
    (void)vsync_counter;
    g->shop_anim_timer++;
    if (g->shop_anim_timer >= 20) {
        g->shop_anim_timer = 0;
        g->shopkeeper_frame ^= 1;
    }

    if (buttons & BTN_LEFT)  g->player_x -= g->player_speed;
    if (buttons & BTN_RIGHT) g->player_x += g->player_speed;

    if (g->player_x < 0) g->player_x = 0;
    if (g->player_x > LW - g->PLAYER.w) g->player_x = LW - g->PLAYER.w;

    if (g->fire_cooldown > 0) g->fire_cooldown--;
    if ((buttons & BTN_FIRE) && g->fire_cooldown == 0) {
        int center_x = g->player_x + g->PLAYER.w/2;
        int center_y = g->player_y - 1;
        fire_player_shots(g, center_x, center_y);
        g->fire_cooldown = compute_fire_cooldown(g);
    }

    update_player_shots(g);

    int item_w = 18;
    int item_h = 12;
    for (int si = 0; si < MAX_PSHOTS; si++) {
        int bx[3] = { g->pshot[si].x, g->pshot_left[si].x, g->pshot_right[si].x };
        int by[3] = { g->pshot[si].y, g->pshot_left[si].y, g->pshot_right[si].y };
        int alive[3] = { g->pshot[si].alive, g->pshot_left[si].alive, g->pshot_right[si].alive };

        for (int bi = 0; bi < 3; bi++) {
            if (!alive[bi]) continue;
            for (int it = 0; it < MAX_SHOP_ITEMS; it++) {
                if (!g->shop_items[it].active) continue;
                int ix = g->shop_items[it].x - item_w / 2;
                int iy = g->shop_items[it].y - item_h / 2;
                if (bx[bi] >= ix && bx[bi] <= ix + item_w && by[bi] >= iy && by[bi] <= iy + item_h) {
                    if (g->score >= g->shop_items[it].price) {
                        g->score -= g->shop_items[it].price;
                        if (g->shop_items[it].type == SHOP_ITEM_FIRE_SPEED) g->fire_speed_bonus++;
                        else if (g->shop_items[it].type == SHOP_ITEM_MOVE_SPEED) g->player_speed++;
                        else if (g->shop_items[it].type == SHOP_ITEM_LIFE) g->lives++;
                        else if (g->shop_items[it].type == SHOP_ITEM_DAMAGE) g->player_damage++;
                        g->shop_items[it].active = 0;
                    }
                    if (bi == 0) g->pshot[si].alive = 0;
                    if (bi == 1) g->pshot_left[si].alive = 0;
                    if (bi == 2) g->pshot_right[si].alive = 0;
                    break;
                }
            }
        }
    }

    // Exit shop if player moves to far right edge
    if (g->player_x >= LW - g->PLAYER.w) {
        g->in_shop = 0;
        setup_level(g, g->shop_next_level, 0);
    }
}

static void shop_render(game_t *g, lfb_t *lfb) {
    l_clear(lfb, 0xFF000000);

    // Shopkeeper (purple animated alien) at top-left
    const sprite1r_t *SK = g->shopkeeper_frame ? &g->ALIEN_B : &g->ALIEN_A;
    draw_sprite1r_scaled(lfb, SK, 12, 24, 0xFF8000FF, 2);

    // Draw shop items
    int item_w = 18;
    int item_h = 12;
    for (int i = 0; i < MAX_SHOP_ITEMS; i++) {
        if (!g->shop_items[i].active) continue;
        int x0 = g->shop_items[i].x - item_w / 2;
        int y0 = g->shop_items[i].y - item_h / 2;

        // Border
        for (int x = 0; x <= item_w; x++) {
            l_putpix(lfb, x0 + x, y0 - 1, 0xFFFFFFFF);
            l_putpix(lfb, x0 + x, y0 + item_h, 0xFFFFFFFF);
        }
        for (int y = 0; y <= item_h; y++) {
            l_putpix(lfb, x0 - 1, y0 + y, 0xFFFFFFFF);
            l_putpix(lfb, x0 + item_w, y0 + y, 0xFFFFFFFF);
        }

        // Draw icon based on item type (ASCII sprites)
        int center_x = g->shop_items[i].x;
        int center_y = g->shop_items[i].y;
        const sprite1r_t *icon = NULL;
        uint32_t icon_color = 0xFFFFFFFF;
        if (g->shop_items[i].type == SHOP_ITEM_LIFE) {
            icon = &g->SHOP_LIFE;
            icon_color = 0xFF00FF00;
        } else if (g->shop_items[i].type == SHOP_ITEM_FIRE_SPEED) {
            icon = &g->SHOP_FIRE;
            icon_color = 0xFFFFA500;
        } else if (g->shop_items[i].type == SHOP_ITEM_MOVE_SPEED) {
            icon = &g->SHOP_MOVE;
            icon_color = 0xFF00FF00;
        } else if (g->shop_items[i].type == SHOP_ITEM_DAMAGE) {
            icon = &g->SHOP_DMG;
            icon_color = 0xFFFF0000;
        }
        if (icon) {
            int icon_x = center_x - icon->w / 2;
            int icon_y = center_y - icon->h / 2;
            draw_sprite1r(lfb, icon, icon_x, icon_y, icon_color);
        }

        // Label and price
        const char *label = shop_item_label(g->shop_items[i].type);
        int label_w = text_width_5x5(label, 1);
        l_draw_text(lfb, g->shop_items[i].x - label_w / 2, y0 - 10, label, 1, 0xFFFFFFFF);

        char price_text[16];
        snprintf(price_text, sizeof(price_text), "%d", g->shop_items[i].price);
        int price_w = text_width_5x5(price_text, 1);
        l_draw_text(lfb, g->shop_items[i].x - price_w / 2, y0 + item_h + 2, price_text, 1, 0xFFFFFFFF);
    }

    // Exit sign
    const char *exit_label = "EXIT";
    int exit_w = text_width_5x5(exit_label, 1);
    int exit_x = LW - exit_w - 4;
    int exit_y = g->player_y - 10;
    l_draw_text(lfb, exit_x, exit_y, exit_label, 1, 0xFFFF0000);

    // Player
    uint32_t player_color = 0xFF00FF00;  // Default green
    if (rapid_fire_active(g)) player_color = 0xFFFFA500;  // Orange when rapid-fire active
    else if (triple_shot_active(g)) player_color = 0xFF0000FF;  // Blue when triple-shot active
    draw_sprite1r(lfb, &g->PLAYER, g->player_x, g->player_y, player_color);

    // Bullets
    render_player_shots(g, lfb);

    // Score (bottom-right)
    const char *label = "SCORE:";
    int label_scale = 1;
    int label_w = text_width_5x5(label, label_scale);
    int digit_w = 4;
    int digits = digit_count(g->score);
    int score_w = digits * digit_w;
    int right_edge = LW - 5;              // desired right anchor
    int score_x = right_edge - score_w;   // LSB stays fixed
    int label_x = right_edge - label_w - 30;

    int y = LH - 12;
    uint32_t score_color = double_shot_active(g) ? 0xFFFFFF00 : 0xFFFFFFFF;
    l_draw_text(lfb, label_x, y, label, label_scale, score_color);
    l_draw_score(lfb, score_x, y, g->score, score_color);

    // SHOP sign on top-left
    {
        const char *shop_label = "SHOP";
        l_draw_text(lfb, 5, 5, shop_label, 1, 0xFF8000FF);
    }

    // PLAYER 1 label and health bar
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

        int fill_w = (max_lives > 0) ? (g->lives * bar_w) / max_lives : 0;
        draw_bar(lfb, lx, bar_y, bar_w, bar_h, fill_w, life_color);
    }
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

    g->player_x = 5;  // Spawn on left side
    g->player_y = LH - 30;
    g->exit_available = 0;

    // Level 0: tutorial level - only one alien in center
    if (level == 0) {
        memset(g->alien_alive, 0, sizeof(g->alien_alive));
        memset(g->alien_health, 0, sizeof(g->alien_health));
        // Put one alien in center (row 4, column 5)
        g->alien_alive[4][5] = 1;
        g->alien_health[4][5] = 1;
    } else {
        memset(g->alien_alive, 1, sizeof(g->alien_alive));
        // Level 1: aliens have 1 HP, Level 3+: aliens have 2 HP (green -> white -> dead)
        int hp = (level >= 5) ? 3 : (level >= 3) ? 2 : 1;
        for (int r = 0; r < AROWS; r++) {
            for (int c = 0; c < ACOLS; c++) {
                g->alien_health[r][c] = (rand() % hp) + 1;
            }
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
    // No boss on level 0 (tutorial)
    if (level == 0) {
        g->boss_alive = 0;
        g->boss_health = 0;
        g->boss_max_health = 0;
    } else {
        g->boss_alive = 1;
        g->boss_max_health = 20 + (level - 1) * 5; // More HP at higher levels
        g->boss_health = g->boss_max_health;
    }
    g->boss_x = (LW - g->BOSS_A.w) / 2;
    g->boss_y = 15;
    g->boss_dx = 1;
    g->boss_frame = 0;
    g->boss_timer = 0;
    g->boss_period = 15 - (level - 1) * 2; // Faster boss at higher levels
    if (g->boss_period < 5) g->boss_period = 5; // Minimum period

    clear_player_shots(g);
    g->ashot.alive = 0;
    g->boss_shot.alive = 0;
    g->boss_laser.alive = 0;
    g->fire_cooldown = 0;

    // Initialize boss power system
    g->boss_power_timer = 0;
    g->boss_power_max = 600;  // 10 seconds at 60 FPS
    g->boss_power_active = 0;
    g->boss_power_cooldown = 0;
    g->boss_laser_last_hit_y = -1000;  // Far off screen
    g->boss_attack_type = 0;  // Current attack being executed
    g->next_boss_attack_type = 0;  // Next attack to be charged
    g->boss_green_laser_last_hit_y = -1000;  // Far off screen

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
    g->SHOP_LIFE = make_sprite_from_ascii(shop_life_rows, 7);
    g->SHOP_FIRE = make_sprite_from_ascii(shop_fire_rows, 7);
    g->SHOP_MOVE = make_sprite_from_ascii(shop_move_rows, 7);
    g->SHOP_DMG  = make_sprite_from_ascii(shop_dmg_rows, 7);

    bunkers_rebuild(g);

    g->bunker_x[0] = 55;  g->bunker_x[1] = 115; g->bunker_x[2] = 175; g->bunker_x[3] = 235;
    g->bunker_y = LH - 65;

    g->game_over = 0;
    g->game_over_score = 0;
    g->start_screen = 1;
    g->start_screen_delay_timer = 0;
    reset_player_progression(g);

    setup_level(g, 0, 1);
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
    g->paused = 0;
    reset_player_progression(g);

    setup_level(g, 0, 1);
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
    // Handle reset button (anytime during gameplay)
    static uint32_t prev_buttons = 0;
    if ((buttons & BTN_RESET) && !(prev_buttons & BTN_RESET)) {
        g->start_screen = 1;
        g->start_screen_delay_timer = 30;
    }
    
    // Handle pause toggle (only in active gameplay)
    if (!g->start_screen && !g->game_over && !g->level_complete && !g->in_shop) {
        if ((buttons & BTN_PAUSE) && !(prev_buttons & BTN_PAUSE)) {
            g->paused = !g->paused;
        }
    }
    prev_buttons = buttons;

    // If paused, skip all game logic
    if (g->paused) {
        return;
    }

    if (g->start_screen) {
        if (g->start_screen_delay_timer > 0) g->start_screen_delay_timer--;
        if (g->start_screen_delay_timer == 0 && (buttons & BTN_FIRE)) game_reset(g);
        return;
    }
    if (g->level_complete) {
        if (g->level_complete_timer > 0) g->level_complete_timer--;
        if (g->level_complete_timer == 0) {
            g->level_complete = 0;
            g->level_complete_timer = 0;
            // Skip shop after level 0 (tutorial)
            if (g->level == 0) {
                setup_level(g, START_LEVEL, 0); // SET LEVEL
            } else if ((g->level % 2) == 0) {
                enter_shop(g);
            } else {
                setup_level(g, g->level + 1, 0);
            }
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
            reset_player_progression(g);
            setup_level(g, 0, 1);
        }
        return;
    }

    if (g->in_shop) {
        shop_update(g, buttons, vsync_counter);
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

    // No powerups on level 0 (tutorial)
    if (g->level != 0) {
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
    }
    if (buttons & BTN_LEFT)  g->player_x -= g->player_speed;
    if (buttons & BTN_RIGHT) g->player_x += g->player_speed;

    if (g->player_x < 0) g->player_x = 0;
    if (g->player_x > LW - g->PLAYER.w) g->player_x = LW - g->PLAYER.w;

    if (g->fire_cooldown > 0) g->fire_cooldown--;
    if ((buttons & BTN_FIRE) && g->fire_cooldown == 0) {
        int center_x = g->player_x + g->PLAYER.w/2;
        int center_y = g->player_y - 1;
        fire_player_shots(g, center_x, center_y);
        g->fire_cooldown = compute_fire_cooldown(g);
    }

    g->alien_timer++;
    if (g->alien_timer >= g->alien_period) {
        g->alien_timer = 0;
        g->alien_frame ^= 1;

        // Skip alien movement on level 0 (tutorial)
        if (g->level != 0) {
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
    }

    // Boss movement and animation (separate from grid)
    if (g->boss_alive) {
        int boss_w = g->BOSS_A.w;
        int boss_h = g->BOSS_A.h;

        // Boss power charging system
        int health_pct = (g->boss_health * 100) / g->boss_max_health;
        if (health_pct <= 33) g->boss_power_timer += 3 + (g->level / 6);
        else if (health_pct <= 67) g->boss_power_timer += 2 + (g->level / 6);
        else g->boss_power_timer += 1 + (g->level / 6);
        if (g->boss_power_timer > g->boss_power_max) {
            g->boss_power_timer = g->boss_power_max;
        }
        
        // Determine next attack type when charging starts
        if (g->boss_power_timer == 1) {
            int alive_count = 0;
            for (int r = 0; r < AROWS; r++) {
                for (int c = 0; c < ACOLS; c++) {
                    alive_count += g->alien_alive[r][c] ? 1 : 0;
                }
            }
            int total_aliens = AROWS * ACOLS;
            if ((g->level >= 3) && (alive_count > total_aliens / 2)) {
                g->next_boss_attack_type = (g->next_boss_attack_type == 0) ? 1 : 0;
            } else {
                g->next_boss_attack_type = 0;  // Default to purple laser
            }
        }

        // When power reaches max, trigger special attack
        if (g->boss_power_timer >= g->boss_power_max && g->boss_power_active == 0) {
            g->boss_power_active = 1;
            g->boss_power_cooldown = 30;
            
            // Use the next attack type that was determined during charging
            g->boss_attack_type = g->next_boss_attack_type;
            
            g->boss_laser.alive = 1;
            g->boss_laser.x = g->boss_x + boss_w / 2;
            g->boss_laser.y = g->boss_y + boss_h + 1;
            g->boss_laser_last_hit_y = -1000;  // Reset hit tracking
            g->boss_green_laser_last_hit_y = -1000;  // Reset green laser tracking
            g->boss_power_timer = 0;  // Reset for next charge
            
            // Boss recovers 10% health if using green laser
            if (g->boss_attack_type == 1) {
                g->boss_health += g->boss_max_health / 10;
                if (g->boss_health > g->boss_max_health) {
                    g->boss_health = g->boss_max_health;
                }
            }
        }

        // Handle stun/frozen state during special attack
        if (g->boss_power_cooldown > 0) {
            g->boss_power_cooldown--;
            // Boss doesn't move during stun
        } else {
            // Normal movement
            g->boss_timer++;
            if (g->boss_timer >= g->boss_period) {
                g->boss_timer = 0;
                g->boss_frame ^= 1;
            }

            int next_bx = g->boss_x + g->boss_dx;
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
        }

        // Game over if boss reaches player's y position
        if (g->boss_y + boss_h >= g->player_y) {
            g->game_over = 1;
            g->game_over_score = g->score;
            g->game_over_delay_timer = 90;
        }

        // Boss destroys bunkers on collision (not on level 0)
        if (g->level != 0) {
            destroy_bunkers_on_overlap(g, g->boss_x, g->boss_y, boss_w, boss_h);
        }

        // Update laser position (moves straight down)
        if (g->boss_laser.alive) {
            g->boss_laser.y += 3;
            if (g->boss_laser.y > LH) {
                g->boss_laser.alive = 0;
                g->boss_power_active = 0;
            }
        }
    }

    // Aliens destroy bunkers on collision (not on level 0)
    if (g->level != 0) {
        const sprite1r_t *AS = g->alien_frame ? &g->ALIEN_B : &g->ALIEN_A;
        int spacing_x = 6, spacing_y = 5;
        for (int r = 0; r < AROWS; r++) {
            for (int c = 0; c < ACOLS; c++) {
                if (!g->alien_alive[r][c]) continue;
                int ax = g->alien_origin_x + c * (AS->w + spacing_x);
                int ay = g->alien_origin_y + r * (AS->h + spacing_y);
                destroy_bunkers_on_overlap(g, ax, ay, AS->w, AS->h);
            }
        }
    }

    update_player_shots(g);

    // No alien shooting on level 0 (tutorial)
    if (g->level != 0) {
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

    // collisions: player shots
    handle_player_shot_collisions(g, g->pshot, 0, 1);
    handle_player_shot_collisions(g, g->pshot_left, -1, 0);
    handle_player_shot_collisions(g, g->pshot_right, 1, 0);

    // collisions: alien and boss shots
    handle_enemy_shot_collisions(g, &g->ashot);
    handle_enemy_shot_collisions(g, &g->boss_shot);

    // Handle boss laser collision (only with player, phases through)
    if (g->boss_laser.alive) {
        const sprite1r_t *p = &g->PLAYER;
        int laser_w = g->BOSS_A.w;  // Laser width matches boss width
        int laser_left = g->boss_laser.x - laser_w / 2;
        int laser_right = laser_left + laser_w;
        int player_left = g->player_x;
        int player_right = g->player_x + p->w;
        
        // Purple laser hurts player; green laser only heals aliens
        if (g->boss_attack_type == 0) {
            // Check if laser overlaps with player and we haven't already hit at this position
            if (laser_left < player_right && laser_right > player_left &&
                g->boss_laser.y + 1 >= g->player_y && g->boss_laser.y <= g->player_y + p->h &&
                (g->boss_laser.y - g->boss_laser_last_hit_y) > 5) {  // Only hit once per 5 pixels
                g->lives--;
                g->boss_laser_last_hit_y = g->boss_laser.y;  // Track where we hit
                clear_player_shots(g);
                if (g->lives <= 0) {
                    g->game_over = 1;
                    g->game_over_score = g->score;
                    g->game_over_delay_timer = 90;
                }
                // Laser continues through player (not destroyed)
            }
        }
        
        // Handle green laser collision with aliens (adds HP)
        if (g->boss_attack_type == 1) {
            int laser_w = g->BOSS_A.w;
            int laser_left = g->boss_laser.x - laser_w / 2;
            int laser_right = laser_left + laser_w;
            
            const sprite1r_t *AS = g->alien_frame ? &g->ALIEN_B : &g->ALIEN_A;
            int spacing_x = 6, spacing_y = 5;
            
            // Hit each column that overlaps with the laser
            for (int c = 0; c < ACOLS; c++) {
                int ax = g->alien_origin_x + c * (AS->w + spacing_x);
                int a_right = ax + AS->w;
                
                // Check if this column overlaps with laser horizontally
                if (laser_left >= a_right || laser_right <= ax) continue;
                
                // Find the first (topmost) alive alien in this column that overlaps vertically
                for (int r = 0; r < AROWS; r++) {
                    if (!g->alien_alive[r][c]) continue;
                    
                    int ay = g->alien_origin_y + r * (AS->h + spacing_y);
                    
                    // Check if laser is at this alien's y position
                    if (g->boss_laser.y >= ay && g->boss_laser.y < ay + AS->h) {
                        // Add 1 HP to alien
                        int max_hp = (g->level >= 5) ? 3 : (g->level >= 3) ? 2 : 1;
                        g->alien_health[r][c]++;
                        if (g->alien_health[r][c] > max_hp) {
                            g->alien_health[r][c] = max_hp;
                        }
                        break;  // Only hit one alien per column per frame
                    }
                }
            }
        }
    }

    // Check if player reached right side to exit level (when exit is available)
    if (g->exit_available && g->player_x >= LW - g->PLAYER.w) {
        g->level_complete = 1;
        g->level_complete_timer = 0;
        g->level_just_completed = g->level;
        g->exit_available = 0;
    }
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
                    g->game_over_delay_timer = 90;
                }
            }
        }
    }

    if (!g->game_over && !g->boss_alive && !g->exit_available) {
        if (g->level == 0) {
            int all_aliens_dead = 1;
            for (int r = 0; r < AROWS && all_aliens_dead; r++) {
                for (int c = 0; c < ACOLS && all_aliens_dead; c++) {
                    if (g->alien_alive[r][c]) all_aliens_dead = 0;
                }
            }
            if (all_aliens_dead) {
                g->exit_available = 1;
            }
        } else {
            g->exit_available = 1;
        }
        
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
        const char *title = "ALIENS";
        const char *prompt = "PRESS SPACE TO START";

        int title_scale = 3;
        int title_w = text_width_5x5(title, title_scale);
        int title_x = (LW - title_w) / 2;
        int title_y = LH / 2 - 30;
        l_draw_text(lfb, title_x, title_y, title, title_scale, 0xFF00FF00);

        int prompt_scale = 2;
        int prompt_w = text_width_5x5(prompt, prompt_scale);
        int prompt_x = (LW - prompt_w) / 2;
        int prompt_y = title_y + 40;
        l_draw_text(lfb, prompt_x, prompt_y, prompt, prompt_scale, 0xFFFFFFFF);
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

        const char *score_label = "FINAL SCORE:";
        int label_scale = 1;
        int label_w = text_width_5x5(score_label, label_scale);
        int label_x = (LW - label_w) / 2 - 12;
        int label_y = cy + r + 15;
        l_draw_text(lfb, label_x, label_y, score_label, label_scale, 0xFFFFFFFF);
        l_draw_score(lfb, label_x + label_w + 6, label_y, g->game_over_score, 0xFFFFFFFF);

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

    if (g->in_shop) {
        shop_render(g, lfb);
        return;
    }

    l_clear(lfb, 0xFF000000);

    // Draw "PAUSED" text if game is paused
    if (g->paused) {
        const char *paused_text = "PAUSED";
        int scale = 2;
        int text_w = text_width_5x5(paused_text, scale);
        int text_x = (LW - text_w) / 2;
        int text_y = (LH - scale * 5) / 2;
        l_draw_text(lfb, text_x, text_y, paused_text, scale, 0xFFFFFFFF);
    }
    
    // Starfield background (static tiling pattern)
    {
        int star_spacing = 20;
        for (int y = 0; y < LH; y += star_spacing) {
            for (int x = 0; x < LW; x += star_spacing) {
                // Pseudo-random star pattern based on position
                int seed = (x ^ y) * 73856093 + (g->powerup_spawn_timer / 10);
                if ((seed % 7) == 0) {  // ~14% of positions have stars
                    uint32_t star_color = 0xFF333333;  // Dark gray
                    l_putpix(lfb, x, y, star_color);
                    if ((seed % 13) == 0 && x + 1 < LW) {  // Some stars are 2x2
                        l_putpix(lfb, x + 1, y, star_color);
                        if (y + 1 < LH) l_putpix(lfb, x, y + 1, star_color);
                        l_putpix(lfb, x + 1, y + 1, star_color);
                    }
                }
            }
        }
    }
    
    // Level 0 tutorial text
    if (g->level == 0) {
        const char *tutorial1 = "USE A/D OR LEFT/RIGHT TO MOVE";
        const char *tutorial2 = "AND PRESS SPACE TO SHOOT";
        const char *tutorial3 = "DESTROY THE BOSS ON EACH LEVEL TO EXIT";
        const char *tutorial4 = "THEN PROCEED TO THE EXIT ON THE RIGHT";
        int scale = 1;
        int w1 = text_width_5x5(tutorial1, scale);
        int w2 = text_width_5x5(tutorial2, scale);
        int w3 = text_width_5x5(tutorial3, scale);
        int w4 = text_width_5x5(tutorial4, scale);
        int x1 = (LW - w1) / 2;
        int x2 = (LW - w2) / 2;
        int x3 = (LW - w3) / 2;
        int x4 = (LW - w4) / 2;
        int y1 = 20;
        int y2 = y1 + 10;
        int y3 = y2 + 10;
        int y4 = y3 + 10;
        l_draw_text(lfb, x1, y1, tutorial1, scale, 0xFFFFFFFF);
        l_draw_text(lfb, x2, y2, tutorial2, scale, 0xFFFFFFFF);
        l_draw_text(lfb, x3, y3, tutorial3, scale, 0xFFFFFFFF);
        l_draw_text(lfb, x4, y4, tutorial4, scale, 0xFFFFFFFF);
    }
    
    {
        const char *label = "SCORE:";
        int label_scale = 1;
        int label_w = text_width_5x5(label, label_scale);
        int digit_w = 4;
        int digits = digit_count(g->score);
        int score_w = digits * digit_w;
        int right_edge = LW - 5;              // desired right anchor
        int score_x = right_edge - score_w;   // LSB stays fixed
        int label_x = right_edge - label_w - 30;

        int y = LH - 12;
        uint32_t score_color = double_shot_active(g) ? 0xFFFFFF00 : 0xFFFFFFFF;
        l_draw_text(lfb, label_x, y, label, label_scale, score_color);
        l_draw_score(lfb, score_x, y, g->score, score_color);
    }

    // Boss health bar on the left side of screen
    if (g->boss_alive) {
        uint32_t boss_color = 0xFF00FF00; // Green 50-100%
        int health_pct = (g->boss_max_health > 0) ? (g->boss_health * 100) / g->boss_max_health : 0;
        if (health_pct <= 33) boss_color = 0xFFFF0000; // Red < 10%
        else if (health_pct <= 67) boss_color = 0xFFFFFF00; // Yellow 10-50%

        const char *boss_label = "BOSS";
        int boss_label_w = text_width_5x5(boss_label, 1);
        int bx = 5;
        int by = 5;

        l_draw_text(lfb, bx, by, boss_label, 1, 0xFFFFFFFF);

        int bar_x = bx + boss_label_w + 6;
        int bar_y = by - 1;
        int bar_w = 50;
        int bar_h = 6;

        int fill_w = (g->boss_max_health > 0) ? (g->boss_health * bar_w) / g->boss_max_health : 0;
        draw_bar(lfb, bar_x, bar_y, bar_w, bar_h, fill_w, boss_color);

        // Boss power bar (to the right of health bar on same line)
        const char *power_label = "POWER:";
        int power_label_w = text_width_5x5(power_label, 1);
        int power_bar_y = by;
        int power_bar_x_start = bar_x + bar_w + 15;  // 15 pixels to the right of health bar
        l_draw_text(lfb, power_bar_x_start, power_bar_y, power_label, 1, 0xFFFFFFFF);

        int power_bar_x = power_bar_x_start + power_label_w + 6;
        int power_bar_h = 6;
        int power_bar_w = 30;
        
        // Determine power color based on what attack will be used (shows next attack)
        uint32_t power_color;  // Purple (default for purple laser)
        if (g->next_boss_attack_type == 0) power_color = 0xFF8000FF;  // Purple
        else if (g->next_boss_attack_type == 1) power_color = 0xFF00FF00;  // Green (for green laser heal)
        else power_color = 0xFF808080;  // Gray (unknown)

        int power_fill_w = (g->boss_power_max > 0) ? (g->boss_power_timer * power_bar_w) / g->boss_power_max : 0;
        draw_bar(lfb, power_bar_x, power_bar_y, power_bar_w, power_bar_h, power_fill_w, power_color);
    }

    {
        const char *label = "LEVEL:";
        int label_scale = 1;
        int label_w = text_width_5x5(label, label_scale);
        int x = LW - (label_w + 6 + 3) - 5;
        l_draw_text(lfb, x, 5, label, label_scale, 0xFFFFFFFF);
        l_draw_score(lfb, x + label_w + 6, 5, g->level, 0xFFFFFFFF);

        const char *remaining_label = "REMAINING:";
        int remaining_label_scale = 1;
        int remaining_label_w = text_width_5x5(remaining_label, remaining_label_scale);
        int remaining_x = x - remaining_label_w - 20;
        int remaining_count = 0;
        for (int r = 0; r < AROWS; r++) {
            for (int c = 0; c < ACOLS; c++) {
                if (g->alien_alive[r][c]) remaining_count++;
            }
        }
        l_draw_text(lfb, remaining_x, 5, remaining_label, remaining_label_scale, 0xFFFFFFFF);
        l_draw_score(lfb, remaining_x + remaining_label_w + 6, 5, remaining_count, 0xFFFFFFFF);

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

        int fill_w = (max_lives > 0) ? (g->lives * bar_w) / max_lives : 0;
        draw_bar(lfb, lx, bar_y, bar_w, bar_h, fill_w, life_color);
        
        // Powerup progress bars
        {
            int px = lx + bar_w + 10;  // Start after health bar
            int py = y;
            int powerup_bar_h = 6;
            int powerup_bar_w = 30;
            int icon_size = 6;
            int spacing = 45;  // Space between each powerup display
            
            for (int slot = 0; slot < 5; slot++) {
                if (g->powerup_slot_timer[slot] > 0) {
                    // Draw powerup icon
                    uint32_t icon_color = 0xFFFFFFFF;
                    if (g->powerup_type_slot[slot] == POWERUP_DOUBLE_SHOT) {
                        icon_color = 0xFFFFFF00;  // Yellow
                    } else if (g->powerup_type_slot[slot] == POWERUP_TRIPLE_SHOT) {
                        icon_color = 0xFF0000FF;  // Blue
                    } else if (g->powerup_type_slot[slot] == POWERUP_RAPID_FIRE) {
                        icon_color = 0xFFFFA500;  // Orange
                    }
                    
                    // Draw "POWER:" label
                    const char *power_label = "POWER:";
                    l_draw_text(lfb, px, py, power_label, 1, 0xFFFFFFFF);
                    int label_w = text_width_5x5(power_label, 1);
                    
                    // Draw progress bar to the right of label
                    int bar_x = px + label_w + 3;
                    int bar_y = py - 1;
                    
                    // Filled portion (proportional to remaining time)
                    int max_timer = 600;  // 600 frames = 10 seconds at 60 FPS
                    int fill_w = (g->powerup_slot_timer[slot] * powerup_bar_w) / max_timer;
                    draw_bar(lfb, bar_x, bar_y, powerup_bar_w, powerup_bar_h, fill_w, icon_color);
                    
                    // Move to next slot position
                    px += spacing;
                }
            }
        }
    }
    // bunkers (not on level 0)
    if (g->level != 0) {
        for (int i = 0; i < 4; i++) {
            // blit sprite bits onto lfb
            sprite1r_t *b = g->bunkers[i];
            int x0 = g->bunker_x[i], y0 = g->bunker_y;
            draw_sprite1r(lfb, b, x0, y0, 0xFF00FF00);
        }
    }

    // boss (drawn behind regular aliens)
    if (g->boss_alive) {
        int health_pct = (g->boss_max_health > 0) ? (g->boss_health * 100) / g->boss_max_health : 0;
        uint32_t boss_color = 0xFF00FF00;
        if (health_pct <= 33) boss_color = 0xFFFF0000;
        else if (health_pct <= 67) boss_color = 0xFFFFFF00;

        // Turn purple when stunned/using purple laser attack
        if (g->boss_power_cooldown > 0) {
            if (g->boss_attack_type == 0) {
                boss_color = 0xFF8000FF;  // Purple for purple laser
            }
        }

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

            // Red if 3+, Green if 2, white if 1
            uint32_t alien_color = (g->alien_health[r][c] >= 3) ? 0xFFFF0000 : (g->alien_health[r][c] == 2) ? 0xFF00FF00 : 0xFFFFFFFF;
            draw_sprite1r(lfb, AS, ax, ay, alien_color);
        }
    }

    uint32_t player_color = 0xFF00FF00;  // Default green
    if (rapid_fire_active(g)) player_color = 0xFFFFA500;  // Orange when rapid-fire active
    else if (triple_shot_active(g)) player_color = 0xFF0000FF;  // Blue when triple-shot active
    draw_sprite1r(lfb, &g->PLAYER, g->player_x, g->player_y, player_color);

    if (g->powerup_active) {
        draw_powerup_icon(lfb, g->powerup_x, g->powerup_y, g->powerup_type);
    }

    // bullets
    render_player_shots(g, lfb);
    if (g->ashot.alive) for (int i = 0; i < 5; i++) l_putpix(lfb, g->ashot.x, g->ashot.y + i, 0xFFFF0000);
    if (g->boss_shot.alive) {
        for (int i = 0; i < 5; i++) l_putpix(lfb, g->boss_shot.x, g->boss_shot.y + i, 0xFFFF0000);
    }

    // Boss laser beam (straight down) - purple or green depending on attack type
    if (g->boss_laser.alive) {
        int laser_w = g->BOSS_A.w;  // Width matches boss width
        int laser_left = g->boss_laser.x - laser_w / 2;
        uint32_t laser_color = (g->boss_attack_type == 1) ? 0xFF00FF00 : 0xFF8000FF;  // Green or purple
        for (int px = laser_left; px < laser_left + laser_w; px++) {
            if (px >= 0 && px < LW && g->boss_laser.y >= 0 && g->boss_laser.y < LH) {
                l_putpix(lfb, px, g->boss_laser.y, laser_color);
            }
        }
    }

    // Exit sign when boss is killed
    if (g->exit_available) {
        const char *exit_label = "EXIT";
        int exit_w = text_width_5x5(exit_label, 1);
        int exit_x = LW - exit_w - 4;
        int exit_y = g->player_y - 10;
        l_draw_text(lfb, exit_x, exit_y, exit_label, 1, 0xFFFF0000);
    }
}
