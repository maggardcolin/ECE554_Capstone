#include "game.h"
#include "../hw_contract.h"
#include "font.h"
#include <string.h>
#include <stdlib.h>

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

    game_reset(g);
}

void game_reset(game_t *g) {
    g->score = 0;
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

void game_update(game_t *g, uint32_t buttons, uint32_t vsync_counter) {
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
                    g->score += 10;
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

    // collisions: alien shot
    if (g->ashot.alive) {
        if (bullet_hits_sprite(&g->PLAYER, g->player_x, g->player_y, g->ashot.x, g->ashot.y)) {
            game_reset(g);
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
}

void game_render(game_t *g, lfb_t *lfb) {
    l_clear(lfb, 0xFF000000);
    l_draw_score(lfb, 5, 5, g->score, 0xFFFFFFFF);

    // bunkers
    for (int i = 0; i < 4; i++) {
        // blit sprite bits onto lfb
        sprite1r_t *b = g->bunkers[i];
        int x0 = g->bunker_x[i], y0 = g->bunker_y;
        for (int y = 0; y < b->h; y++) {
            int yy = y0 + y;
            if ((unsigned)yy >= (unsigned)LH) continue;
            const uint8_t *row = b->bits + y * b->stride;
            for (int x = 0; x < b->w; x++) {
                int xx = x0 + x;
                if ((unsigned)xx >= (unsigned)LW) continue;
                if ((row[x >> 3] >> (7 - (x & 7))) & 1) lfb->px[yy * LW + xx] = 0xFF00FF00;
            }
        }
    }

    // aliens
    const sprite1r_t *AS = g->alien_frame ? &g->ALIEN_B : &g->ALIEN_A;
    int spacing_x = 6, spacing_y = 5;
    for (int r = 0; r < AROWS; r++) {
        for (int c = 0; c < ACOLS; c++) {
            if (!g->alien_alive[r][c]) continue;
            int ax = g->alien_origin_x + c * (AS->w + spacing_x);
            int ay = g->alien_origin_y + r * (AS->h + spacing_y);

            for (int y = 0; y < AS->h; y++) {
                int yy = ay + y;
                if ((unsigned)yy >= (unsigned)LH) continue;
                const uint8_t *row = AS->bits + y * AS->stride;
                for (int x = 0; x < AS->w; x++) {
                    int xx = ax + x;
                    if ((unsigned)xx >= (unsigned)LW) continue;
                    if ((row[x >> 3] >> (7 - (x & 7))) & 1) lfb->px[yy * LW + xx] = 0xFFFFFFFF;
                }
            }
        }
    }

    // player
    {
        sprite1r_t *P = &g->PLAYER;
        int x0 = g->player_x, y0 = g->player_y;
        for (int y = 0; y < P->h; y++) {
            int yy = y0 + y;
            if ((unsigned)yy >= (unsigned)LH) continue;
            const uint8_t *row = P->bits + y * P->stride;
            for (int x = 0; x < P->w; x++) {
                int xx = x0 + x;
                if ((unsigned)xx >= (unsigned)LW) continue;
                if ((row[x >> 3] >> (7 - (x & 7))) & 1) lfb->px[yy * LW + xx] = 0xFF00FF00;
            }
        }
    }

    // bullets
    if (g->pshot.alive) for (int i = 0; i < 5; i++) l_putpix(lfb, g->pshot.x, g->pshot.y - i, 0xFFFFFFFF);
    if (g->ashot.alive) for (int i = 0; i < 5; i++) l_putpix(lfb, g->ashot.x, g->ashot.y + i, 0xFFFF0000);
}
