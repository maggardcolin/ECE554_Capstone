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

static int text_width_5x5(const char *text, int scale) {
    if (scale < 1) scale = 1;
    int count = 0;
    for (const char *p = text; *p; p++) count++;
    if (count == 0) return 0;
    return (count * 6 - 1) * scale; // (w=5 + spacing=1) * n - spacing
}

static void draw_powerup_icon(lfb_t *lfb, int x0, int y0) {
    int r = 6;
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x*x + y*y <= r*r) l_putpix(lfb, x0 + x, y0 + y, 0xFFFF0000);
        }
    }
    l_draw_text(lfb, x0 - 4, y0 - 3, "2X", 1, 0xFFFFFFFF);
}

static int double_shot_active(const game_t *g) {
    for (int i = 0; i < 5; i++) if (g->powerup_slot_timer[i] > 0) return 1;
    return 0;
}

static int aliens_remaining(const game_t *g) {
    for (int r = 0; r < AROWS; r++) for (int c = 0; c < ACOLS; c++) if (g->alien_alive[r][c]) return 1;
    return 0;
}

static void bunkers_rebuild(game_t *g);

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

    g->alien_origin_x = 50;
    g->alien_origin_y = 30;
    g->alien_dx = 1;
    g->alien_step_px = 2;
    g->alien_drop_px = 6;
    g->alien_frame = 0;
    g->alien_timer = 0;
    g->alien_period = 20;

    g->pshot.alive = 0;
    g->ashot.alive = 0;
    g->fire_cooldown = 0;

    bunkers_rebuild(g);
}

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

void game_init(game_t *g) {
    memset(g, 0, sizeof(*g));
    g->PLAYER  = make_sprite_from_ascii(player_rows, 8);
    g->ALIEN_A = make_sprite_from_ascii(alienA_rows, 8);
    g->ALIEN_B = make_sprite_from_ascii(alienB_rows, 8);

    bunkers_rebuild(g);

    g->bunker_x[0] = 55;  g->bunker_x[1] = 115; g->bunker_x[2] = 175; g->bunker_x[3] = 235;
    g->bunker_y = LH - 65;

    g->game_over = 0;
    g->game_over_score = 0;
    g->start_screen = 1;
    g->lives = 2;
    for (int i = 0; i < 5; i++) g->powerup_slot_timer[i] = 0;

    setup_level(g, 1, 1);
}

void game_reset(game_t *g) {
    g->game_over = 0;
    g->game_over_score = 0;
    g->start_screen = 0;
    g->lives = 2;
    for (int i = 0; i < 5; i++) g->powerup_slot_timer[i] = 0;

    setup_level(g, 1, 1);
}

void game_update(game_t *g, uint32_t buttons, uint32_t vsync_counter) {
    if (g->start_screen) {
        if (buttons & BTN_FIRE) game_reset(g);
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
        if (g->game_over_delay_timer == 0 && (buttons & BTN_FIRE)) game_reset(g);
        return;
    }

    for (int i = 0; i < 5; i++) {
        if (g->powerup_slot_timer[i] > 0) g->powerup_slot_timer[i]--;
    }

    if (!g->powerup_active) {
        g->powerup_spawn_timer++;
        if (g->powerup_spawn_timer >= 900) {
            int tile = 8;
            int cols = LW / tile;
            int rows = LH / tile;
            int tx = rand() % cols;
            int ty = rand() % rows;
            g->powerup_x = tx * tile + tile / 2;
            g->powerup_y = ty * tile + tile / 2;
            g->powerup_active = 1;
            g->powerup_timer = 300;
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
    if ((buttons & BTN_FIRE) && !g->pshot.alive && g->fire_cooldown == 0) {
        g->pshot.alive = 1;
        g->pshot.x = g->player_x + g->PLAYER.w/2;
        g->pshot.y = g->player_y - 1;
        g->fire_cooldown = 6;
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

    if (g->pshot.alive) {
        g->pshot.y -= 4;
        if (g->pshot.y < 0) g->pshot.alive = 0;
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

    // collisions: player shot
    if (g->pshot.alive) {
        if (g->powerup_active) {
            int dx = g->pshot.x - g->powerup_x;
            int dy = g->pshot.y - g->powerup_y;
            if ((dx*dx + dy*dy) <= 36) {
                for (int i = 0; i < 5; i++) {
                    if (g->powerup_slot_timer[i] == 0) {
                        g->powerup_slot_timer[i] = 600;
                        break;
                    }
                }
                g->powerup_active = 0;
                g->pshot.alive = 0;
            }
        }
        if (!g->pshot.alive) {
            // consumed by powerup
        } else {
            const sprite1r_t *AS = g->alien_frame ? &g->ALIEN_B : &g->ALIEN_A;
            int spacing_x = 6, spacing_y = 5;
            int hit = 0;
            for (int r = 0; r < AROWS && !hit; r++) {
                for (int c = 0; c < ACOLS && !hit; c++) {
                    if (!g->alien_alive[r][c]) continue;
                    int ax = g->alien_origin_x + c * (AS->w + spacing_x);
                    int ay = g->alien_origin_y + r * (AS->h + spacing_y);
                    if (bullet_hits_sprite(AS, ax, ay, g->pshot.x, g->pshot.y)) {
                        g->alien_alive[r][c] = 0;
                        g->pshot.alive = 0;
                        g->score += double_shot_active(g) ? 20 : 10;
                        hit = 1;
                    }
                }
            }
            if (!hit && g->pshot.alive) {
                for (int i = 0; i < 4 && g->pshot.alive; i++) {
                    sprite1r_t *b = g->bunkers[i];
                    int bx = g->bunker_x[i], by = g->bunker_y;
                    if (bullet_hits_sprite(b, bx, by, g->pshot.x, g->pshot.y)) {
                        bunker_damage(b, g->pshot.x - bx, g->pshot.y - by, 3);
                        g->pshot.alive = 0;
                    }
                }
            }
        }
    }

    // collisions: alien shot
    if (g->ashot.alive) {
        if (bullet_hits_sprite(&g->PLAYER, g->player_x, g->player_y, g->ashot.x, g->ashot.y)) {
            g->lives--;
            g->pshot.alive = 0;
            g->ashot.alive = 0;
            g->player_x = LW/2 - g->PLAYER.w/2;
            g->player_y = LH - 30;
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

    if (!g->game_over && !aliens_remaining(g)) {
        g->level_complete = 1;
        g->level_complete_timer = 60;
        g->level_just_completed = g->level;
    }
}

void game_render(game_t *g, lfb_t *lfb) {
    if (g->start_screen) {
        l_clear(lfb, 0xFF000000);
        const char *title = "SPACE INVADERS";
        const char *prompt = "PRESS SPACE TO START";

        int title_scale = 3;
        int title_w = text_width_5x5(title, title_scale);
        int title_x = (LW - title_w) / 2;
        int title_y = LH / 2 - 40;
        l_draw_text(lfb, title_x, title_y, title, title_scale, 0xFF00FF00);

        int prompt_scale = 2;
        int prompt_w = text_width_5x5(prompt, prompt_scale);
        int prompt_x = (LW - prompt_w) / 2;
        int prompt_y = title_y + 40;
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

        draw_sprite1r(lfb, &g->PLAYER, cx - p_w / 2, cy - p_h / 2, 0xFF00FF00);

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

        const char *prompt = "PRESS SPACE TO CONTINUE";
        int prompt_scale = 2;
        int prompt_w = text_width_5x5(prompt, prompt_scale);
        int prompt_x = (LW - prompt_w) / 2;
        int prompt_y = label_y + 25;
        l_draw_text(lfb, prompt_x, prompt_y, prompt, prompt_scale, 0xFFFFFFFF);
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
        for (int i = 0; i < g->lives; i++) {
            draw_sprite1r(lfb, &g->PLAYER, lx + i * (g->PLAYER.w + 2), y - 2, 0xFF00FF00);
        }
    }

    // bunkers
    for (int i = 0; i < 4; i++) {
        // blit sprite bits onto lfb
        sprite1r_t *b = g->bunkers[i];
        int x0 = g->bunker_x[i], y0 = g->bunker_y;
        draw_sprite1r(lfb, b, x0, y0, 0xFF00FF00);
    }

    // aliens
    const sprite1r_t *AS = g->alien_frame ? &g->ALIEN_B : &g->ALIEN_A;
    int spacing_x = 6, spacing_y = 5;
    for (int r = 0; r < AROWS; r++) {
        for (int c = 0; c < ACOLS; c++) {
            if (!g->alien_alive[r][c]) continue;
            int ax = g->alien_origin_x + c * (AS->w + spacing_x);
            int ay = g->alien_origin_y + r * (AS->h + spacing_y);

            draw_sprite1r(lfb, AS, ax, ay, 0xFFFFFFFF);
        }
    }

    // player
    draw_sprite1r(lfb, &g->PLAYER, g->player_x, g->player_y, 0xFF00FF00);

    if (g->powerup_active) {
        draw_powerup_icon(lfb, g->powerup_x, g->powerup_y);
    }

    // bullets
    if (g->pshot.alive) for (int i = 0; i < 5; i++) l_putpix(lfb, g->pshot.x, g->pshot.y - i, 0xFFFFFFFF);
    if (g->ashot.alive) for (int i = 0; i < 5; i++) l_putpix(lfb, g->ashot.x, g->ashot.y + i, 0xFFFF0000);

    {
        int base_y = LH - 12;
        int spacing = 14;
        int right_x = LW - 6;
        for (int i = 0; i < 5; i++) {
            int x = right_x - (4 - i) * spacing;
            if (g->powerup_slot_timer[i] > 0) draw_powerup_icon(lfb, x, base_y + 4);
        }
    }
}
