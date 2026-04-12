#include "boss.h"

#include <stdio.h>
#include "font.h"
#include "game_helpers.h"
#include "music_alsa.h"

#define BOSS_DEATH_DELAY_FRAMES 60
#define ALIEN_EXPLOSION_FRAMES 18

static void render_explosion_points(lfb_t *lfb, int cx, int cy, int points) {
    if (points <= 0) return;

    char points_text[16];
    snprintf(points_text, sizeof(points_text), "%d", points);
    int text_w = text_width_5x5(points_text, 1);
    l_draw_text(lfb, cx - (text_w / 2), cy - 3, points_text, 1, 0xFFFFFFFF);
}

static const sprite1r_t *boss_sprite_for_frame(const game_t *g, int frame) {
    if (g->boss_type == BOSS_TYPE_BLUE) {
        return frame ? &g->BOSS2_B : &g->BOSS2_A;
    }
    return frame ? &g->BOSS_B : &g->BOSS_A;
}

void boss_trigger_death(game_t *g, int points_awarded) {
    if (g->boss_dying || !g->boss_alive) return;
    g->boss_dying = 1;
    g->boss_death_timer = BOSS_DEATH_DELAY_FRAMES;
    g->boss_explode_points = points_awarded;
    if (points_awarded > 0) {
        g->score += points_awarded;
    }
    g->boss_laser.alive = 0;
    g->boss_shot.alive = 0;
    for (int i = 0; i < 3; i++) {
        g->boss_triple_shot[i].alive = 0;
    }
    g->boss_bomb.alive = 0;
    g->boss_bomb.exploding = 0;
    g->boss_bomb.hit_player = 0;
    g->boss_shield_active = 0;
    g->ashot.alive = 0;
    music_play_boom_long();
}

void boss_apply_explosion_to_aliens(game_t *g) {
    if (!g->boss_dying) return;

    const sprite1r_t *AS = g->alien_frame ? &g->ALIEN_B : &g->ALIEN_A;
    const sprite1r_t *BS = boss_sprite_for_frame(g, g->boss_frame);
    int spacing_x = 6, spacing_y = 5;
    int cx = g->boss_x + BS->w / 2;
    int cy = g->boss_y + BS->h / 2;
    int radius = boss_explosion_radius(g);

    for (int r = 0; r < AROWS; r++) {
        for (int c = 0; c < ACOLS; c++) {
            if (!g->alien_alive[r][c]) continue;
            int ax = g->alien_origin_x + c * (AS->w + spacing_x);
            int ay = g->alien_origin_y + r * (AS->h + spacing_y);
            if (circle_intersects_rect(cx, cy, radius, ax, ay, AS->w, AS->h)) {
                g->alien_alive[r][c] = 0;
            }
        }
    }
}

void boss_apply_alien_explosions_to_boss(game_t *g) {
    if (!g->boss_alive || g->boss_dying) return;

    const sprite1r_t *AS = g->alien_frame ? &g->ALIEN_B : &g->ALIEN_A;
    const sprite1r_t *BS = boss_sprite_for_frame(g, g->boss_frame);
    int spacing_x = 6, spacing_y = 5;

    for (int r = 0; r < AROWS; r++) {
        for (int c = 0; c < ACOLS; c++) {
            int timer = g->alien_explode_timer[r][c];
            if (timer <= 0) continue;
            if (g->alien_explode_hit_boss[r][c]) continue;

            int cx = g->alien_origin_x + c * (AS->w + spacing_x) + AS->w / 2;
            int cy = g->alien_origin_y + r * (AS->h + spacing_y) + AS->h / 2;
            int radius = alien_explosion_radius(timer);

            if (circle_intersects_rect(cx, cy, radius, g->boss_x, g->boss_y, BS->w, BS->h)) {
                g->alien_explode_hit_boss[r][c] = 1;
                g->boss_health -= 1;
                if (g->boss_health <= 0) {
                    g->boss_health = 0;
                    boss_trigger_death(g, 0);
                    return;
                }
            }
        }
    }
}

void boss_render_explosion(const game_t *g, lfb_t *lfb) {
    const sprite1r_t *BS = boss_sprite_for_frame(g, g->boss_frame);
    int cx = g->boss_x + BS->w / 2;
    int cy = g->boss_y + BS->h / 2;
    int age = BOSS_DEATH_DELAY_FRAMES - g->boss_death_timer;
    int frame = age / 10;
    int base_r = 6 + frame * 3;

    draw_filled_circle(lfb, cx, cy, base_r + 4, 0xFFFF4500);
    draw_filled_circle(lfb, cx, cy, base_r + 1, 0xFFFF8C00);
    if ((age & 1) == 0) {
        draw_filled_circle(lfb, cx, cy, (base_r > 3) ? (base_r - 3) : 1, 0xFFFFFF00);
    }

    int spark_r = base_r + 6;
    for (int i = 0; i < 14; i++) {
        int dx = ((i * 13 + age * 4) % (spark_r * 2 + 1)) - spark_r;
        int dy = ((i * 17 + age * 5) % (spark_r * 2 + 1)) - spark_r;
        uint32_t c = (i & 1) ? 0xFFFFA500 : 0xFFFFFF00;
        l_putpix(lfb, cx + dx, cy + dy, c);
    }

    if (g->boss_death_timer > 0) {
        render_explosion_points(lfb, cx, cy, g->boss_explode_points);
    }
}
