#include "game.h"
#include "boss.h"
#include "game_helpers.h"
#include "game_player.h"
#include "../hw_contract.h"
#include "font.h"
#include "music_alsa.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define EXIT_BLINK_INTERVAL_FRAMES 8
#define EXIT_BLINK_TOGGLES 6
#define PLAYER_DEATH_DELAY_FRAMES 60
#define PLAYER_IFRAMES 60
#define ALIEN_EXPLOSION_FRAMES 18
#define BOSS_BOMB_EXPLOSION_FRAMES 60
#define BOSS_INTRO_FRAMES 180
#define YELLOW_BOSS_ALIENS 5
#define YELLOW_BOSS_ATTACK_SHUFFLE 3
#define WIN_LEVEL 8
#define WIN_RING_ALIENS 6
#define WIN_EXPLOSION_DELAY_FRAMES 10

static const sprite1r_t *boss_sprite_for_frame(const game_t *g, int frame) {
    if (g->boss_type == BOSS_TYPE_BLUE) {
        return frame ? &g->BOSS2_B : &g->BOSS2_A;
    }
    return frame ? &g->BOSS_B : &g->BOSS_A;
}

static const sprite1r_t *active_boss_sprite(const game_t *g) {
    return boss_sprite_for_frame(g, g->boss_frame);
}

static const char *boss_intro_type_text(const game_t *g) {
    if (g->boss_type == BOSS_TYPE_BLUE) return "BLUE";
    if (g->boss_type == BOSS_TYPE_YELLOW) return "YELLOW";
    return "MULTICOLOR";
}

static const char *boss_intro_main_attack_text(const game_t *g) {
    if (g->boss_type == BOSS_TYPE_BLUE) return "TRIPLE SHOT";
    if (g->boss_type == BOSS_TYPE_YELLOW) return "STANDARD LASER";
    return "STANDARD LASER";
}

static int blue_black_hole_enabled(const game_t *g) {
    return (g->boss_type == BOSS_TYPE_BLUE) && (g->level >= 3);
}

static const char *boss_intro_special_attack_text(const game_t *g) {
    if (g->boss_type == BOSS_TYPE_BLUE) return blue_black_hole_enabled(g) ? "BLACK HOLE" : "BOMB";
    if (g->boss_type == BOSS_TYPE_YELLOW) return "SHUFFLE";
    if (g->level >= 3) return "PLASMA BEAM HEAL BEAM";
    return "PLASMA BEAM";
}

static int yellow_boss_hp_per_alien(const game_t *g) {
    int hp = (g->boss_max_health + (YELLOW_BOSS_ALIENS - 1)) / YELLOW_BOSS_ALIENS;
    if (hp < 1) hp = 1;
    return hp;
}

static void clear_yellow_boss_state(game_t *g) {
    memset(g->yellow_boss_marked, 0, sizeof(g->yellow_boss_marked));
    for (int i = 0; i < YELLOW_BOSS_ALIENS; i++) {
        g->yellow_beam_shot[i].alive = 0;
    }
}

static void update_yellow_boss_health(game_t *g) {
    int total = 0;
    for (int r = 0; r < AROWS; r++) {
        for (int c = 0; c < ACOLS; c++) {
            if (g->yellow_boss_marked[r][c] && g->alien_alive[r][c]) {
                total += g->alien_health[r][c];
            }
        }
    }

    g->boss_health = total;
    if (g->boss_health < 0) g->boss_health = 0;

    if (g->boss_type == BOSS_TYPE_YELLOW && g->boss_alive && g->boss_health <= 0) {
        g->boss_alive = 0;
        g->boss_power_active = 0;
        g->boss_power_timer = 0;
        g->boss_power_cooldown = 0;
        for (int i = 0; i < YELLOW_BOSS_ALIENS; i++) {
            g->yellow_beam_shot[i].alive = 0;
        }
    }
}

static void select_yellow_boss_aliens(game_t *g) {
    int rr[AROWS * ACOLS];
    int cc[AROWS * ACOLS];
    int alive_count = 0;

    clear_yellow_boss_state(g);

    for (int r = 0; r < AROWS; r++) {
        for (int c = 0; c < ACOLS; c++) {
            if (!g->alien_alive[r][c]) continue;
            rr[alive_count] = r;
            cc[alive_count] = c;
            alive_count++;
        }
    }

    for (int i = alive_count - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int tr = rr[i], tc = cc[i];
        rr[i] = rr[j]; cc[i] = cc[j];
        rr[j] = tr; cc[j] = tc;
    }

    int pick_count = (alive_count < YELLOW_BOSS_ALIENS) ? alive_count : YELLOW_BOSS_ALIENS;
    int hp_each = yellow_boss_hp_per_alien(g);

    for (int i = 0; i < pick_count; i++) {
        int r = rr[i];
        int c = cc[i];
        g->yellow_boss_marked[r][c] = 1;
        g->alien_health[r][c] = hp_each;
    }

    g->boss_max_health = hp_each * pick_count;
    update_yellow_boss_health(g);
}

static void shuffle_yellow_boss_aliens(game_t *g) {
    int mr[YELLOW_BOSS_ALIENS], mc[YELLOW_BOSS_ALIENS], mcount = 0;
    int cr[AROWS * ACOLS], cc[AROWS * ACOLS], ccount = 0;

    for (int r = 0; r < AROWS; r++) {
        for (int c = 0; c < ACOLS; c++) {
            if (!g->alien_alive[r][c]) continue;
            if (g->yellow_boss_marked[r][c]) {
                if (mcount < YELLOW_BOSS_ALIENS) {
                    mr[mcount] = r;
                    mc[mcount] = c;
                    mcount++;
                }
            } else {
                cr[ccount] = r;
                cc[ccount] = c;
                ccount++;
            }
        }
    }

    for (int i = ccount - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int tr = cr[i], tc = cc[i];
        cr[i] = cr[j]; cc[i] = cc[j];
        cr[j] = tr; cc[j] = tc;
    }

    int swaps = (mcount < ccount) ? mcount : ccount;
    for (int i = 0; i < swaps; i++) {
        int r1 = mr[i], c1 = mc[i];
        int r2 = cr[i], c2 = cc[i];

        int tmp_hp = g->alien_health[r1][c1];
        g->alien_health[r1][c1] = g->alien_health[r2][c2];
        g->alien_health[r2][c2] = tmp_hp;

        g->yellow_boss_marked[r1][c1] = 0;
        g->yellow_boss_marked[r2][c2] = 1;
    }

    update_yellow_boss_health(g);
}

static void clear_boss_projectiles(game_t *g) {
    g->boss_shot.alive = 0;
    g->boss_laser.alive = 0;
    for (int i = 0; i < 3; i++) {
        g->boss_triple_shot[i].alive = 0;
    }
    for (int i = 0; i < YELLOW_BOSS_ALIENS; i++) {
        g->yellow_beam_shot[i].alive = 0;
    }
    g->boss_bomb.alive = 0;
    g->boss_bomb.exploding = 0;
    g->boss_bomb.explode_timer = 0;
    g->boss_bomb.hit_player = 0;
}

static int boss_bomb_explosion_radius(const game_t *g) {
    (void)g;
    int age = BOSS_BOMB_EXPLOSION_FRAMES - g->boss_bomb.explode_timer;
    int frame = age / 10;
    int base_r = 6 + frame * 3;
    return base_r + 4;
}

static void render_intro_regular_shot(lfb_t *lfb, int x, int y, uint32_t color) {
    for (int i = 0; i < 5; i++) l_putpix(lfb, x, y + i, color);
}

static void render_intro_plasma_beam(lfb_t *lfb, int cx, int y, int beam_w, uint32_t color) {
    int left = cx - beam_w / 2;
    const int trail_offsets[] = {0, 5, 10, 20};
    const int trail_intensity[] = {100, 65, 40, 22};

    for (int t = 0; t < 4; t++) {
        int ty = y - trail_offsets[t];
        if (ty < 0 || ty >= LH) continue;
        uint32_t c = color_with_intensity(color, trail_intensity[t]);
        for (int px = left; px < left + beam_w; px++) {
            if (px >= 0 && px < LW) l_putpix(lfb, px, ty, c);
        }
    }
}

static void render_intro_ring_points(lfb_t *lfb, int cx, int cy, int radius, uint32_t color) {
    static const int ring_dx[20] = {16, 15, 13, 10, 5, 0, -5, -10, -13, -15,
                                    -16, -15, -13, -10, -5, 0, 5, 10, 13, 15};
    static const int ring_dy[20] = {0, 5, 10, 13, 15, 16, 15, 13, 10, 5,
                                    0, -5, -10, -13, -15, -16, -15, -13, -10, -5};

    for (int i = 0; i < 20; i++) {
        int px = cx + (ring_dx[i] * radius) / 16;
        int py = cy + (ring_dy[i] * radius) / 16;
        l_putpix(lfb, px, py, color);
    }
}

static void render_black_hole_explosion(lfb_t *lfb, int cx, int cy, int r) {
    draw_filled_circle(lfb, cx, cy, r, 0xFF1A1A28);
    draw_filled_circle(lfb, cx, cy, r - 3, 0xFF06070D);
    int ring_r = r - 1;
    if (ring_r > 1) {
        render_intro_ring_points(lfb, cx, cy, ring_r, 0xFF66CCFF);
    }
}

static void render_intro_boss_attack_preview(const game_t *g, lfb_t *lfb, int boss_x, int boss_y, const sprite1r_t *BS) {
    int elapsed = BOSS_INTRO_FRAMES - g->boss_intro_timer;
    int start_y = boss_y + BS->h + 2;
    int center_x = boss_x + BS->w / 2;

    const int intro_hold = 10;
    const int regular_dur = 24;
    const int gap_dur = 8;
    const int special_dur = 44;

    int t0 = intro_hold;
    int t1 = t0 + regular_dur;
    int t2 = t1 + gap_dur;
    int t3 = t2 + regular_dur;
    int t4 = t3 + gap_dur;
    int t5 = t4 + special_dur;
    int t6 = t5 + special_dur;

    if (g->boss_type == BOSS_TYPE_YELLOW) {
        const sprite1r_t *AS = g->alien_frame ? &g->ALIEN_B : &g->ALIEN_A;
        int spacing = 8;
        int total_w = YELLOW_BOSS_ALIENS * AS->w + (YELLOW_BOSS_ALIENS - 1) * spacing;
        int start_x = (LW - total_w) / 2;
        int row_y = boss_y;

        for (int i = 0; i < YELLOW_BOSS_ALIENS; i++) {
            int x = start_x + i * (AS->w + spacing);
            if (elapsed >= t4 && elapsed < t5) {
                int local = elapsed - t4;
                int wave = (local / 4 + i * 2) % 4;
                x += wave - 2;
            }
            draw_sprite1r(lfb, AS, x, row_y, 0xFFFFFF00);
        }

        if ((elapsed >= t0 && elapsed < t1) || (elapsed >= t2 && elapsed < t3)) {
            int local = (elapsed >= t2) ? (elapsed - t2) : (elapsed - t0);
            int beam_y = start_y + local * 2;
            for (int i = 0; i < YELLOW_BOSS_ALIENS; i++) {
                int beam_x = start_x + i * (AS->w + spacing) + AS->w / 2;
                render_intro_regular_shot(lfb, beam_x, beam_y, 0xFFFFFF00);
            }
        }
        return;
    }

    if (g->boss_type == BOSS_TYPE_BLUE) {
        if (elapsed >= t0 && elapsed < t1) {
            int local = elapsed - t0;
            int y = start_y + local * 2;
            render_intro_regular_shot(lfb, center_x - 4 - local / 2, y, 0xFF3399FF);
            render_intro_regular_shot(lfb, center_x, y, 0xFF3399FF);
            render_intro_regular_shot(lfb, center_x + 4 + local / 2, y, 0xFF3399FF);
        } else if (elapsed >= t2 && elapsed < t3) {
            int local = elapsed - t2;
            int y = start_y + local * 2;
            render_intro_regular_shot(lfb, center_x - 4 - local / 2, y, 0xFF3399FF);
            render_intro_regular_shot(lfb, center_x, y, 0xFF3399FF);
            render_intro_regular_shot(lfb, center_x + 4 + local / 2, y, 0xFF3399FF);
        } else if (elapsed >= t4 && elapsed < t5) {
            int local = elapsed - t4;
            int fall_frames = 18;
            if (local < fall_frames) {
                int by = start_y + local * 3;
                draw_filled_circle(lfb, center_x, by, 4, 0xFF66CCFF);
                draw_filled_circle(lfb, center_x, by, 2, 0xFFB8ECFF);
            } else {
                int ex = center_x;
                int ey = start_y + fall_frames * 3;
                int exp_local = local - fall_frames;
                int r = 10 + (exp_local / 3) * 2;
                if (r > 22) r = 22;
                if (blue_black_hole_enabled(g)) {
                    render_black_hole_explosion(lfb, ex, ey, r);
                } else {
                    draw_filled_circle(lfb, ex, ey, r, 0xFF0B3A8F);
                    draw_filled_circle(lfb, ex, ey, r - 3, 0xFF1E6AD6);
                    if ((exp_local & 1) == 0) {
                        int core = r - 7;
                        if (core < 1) core = 1;
                        draw_filled_circle(lfb, ex, ey, core, 0xFF66CCFF);
                    }
                }
            }
        }
        return;
    }

    // Multicolor boss preview.
    if (elapsed >= t0 && elapsed < t1) {
        int local = elapsed - t0;
        render_intro_regular_shot(lfb, center_x, start_y + local * 2, 0xFFFF0000);
    } else if (elapsed >= t2 && elapsed < t3) {
        int local = elapsed - t2;
        render_intro_regular_shot(lfb, center_x, start_y + local * 2, 0xFFFF0000);
    } else if (elapsed >= t4 && elapsed < t5) {
        int local = elapsed - t4;
        render_intro_plasma_beam(lfb, center_x, start_y + local * 3, BS->w, 0xFF8000FF);
    } else if (g->level >= 3 && elapsed >= t5 && elapsed < t6) {
        int local = elapsed - t5;
        render_intro_plasma_beam(lfb, center_x, start_y + local * 3, BS->w, 0xFF00FF00);
    }
}

static sprite1r_t make_filled_sprite(int w, int h) {
    int stride = (w + 7) / 8;
    uint8_t *bits = (uint8_t *)calloc((size_t)h * (size_t)stride, 1);
    if (!bits) {
        fprintf(stderr, "calloc failed\n");
        exit(1);
    }
    for (int y = 0; y < h; y++) {
        memset(bits + y * stride, 0xFF, (size_t)stride);
    }
    if ((w & 7) != 0) {
        uint8_t mask = (uint8_t)(0xFFu << (8 - (w & 7)));
        for (int y = 0; y < h; y++) {
            bits[y * stride + (stride - 1)] &= mask;
        }
    }
    sprite1r_t s = { .w = w, .h = h, .stride = stride, .bits = bits };
    return s;
}

static void refill_sprite_full(sprite1r_t *s) {
    for (int y = 0; y < s->h; y++) {
        memset(s->bits + y * s->stride, 0xFF, (size_t)s->stride);
    }
    if ((s->w & 7) != 0) {
        uint8_t mask = (uint8_t)(0xFFu << (8 - (s->w & 7)));
        for (int y = 0; y < s->h; y++) {
            s->bits[y * s->stride + (s->stride - 1)] &= mask;
        }
    }
}

static int sprite_has_any_bits(const sprite1r_t *s) {
    for (int y = 0; y < s->h; y++) {
        for (int b = 0; b < s->stride; b++) {
            if (s->bits[y * s->stride + b] != 0) return 1;
        }
    }
    return 0;
}

static void render_player_explosion_at(lfb_t *lfb, int cx, int cy, int age) {
    if (age < 0) age = 0;
    if (age > PLAYER_DEATH_DELAY_FRAMES) age = PLAYER_DEATH_DELAY_FRAMES;

    int frame = age / 12;
    int base_r = 3 + frame * 2;

    draw_filled_circle(lfb, cx, cy, base_r + 2, 0xFFFF4500);
    draw_filled_circle(lfb, cx, cy, base_r, 0xFFFF8C00);
    if ((age & 1) == 0) {
        draw_filled_circle(lfb, cx, cy, (base_r > 2) ? (base_r - 2) : 1, 0xFFFFFF00);
    }

    int spark_r = base_r + 3;
    for (int i = 0; i < 8; i++) {
        int dx = ((i * 7 + age * 3) % (spark_r * 2 + 1)) - spark_r;
        int dy = ((i * 11 + age * 5) % (spark_r * 2 + 1)) - spark_r;
        uint32_t c = (i & 1) ? 0xFFFFA500 : 0xFFFFFF00;
        l_putpix(lfb, cx + dx, cy + dy, c);
    }
}

static void render_player_explosion(const game_t *g, lfb_t *lfb) {
    int cx = g->player_x + g->PLAYER.w / 2;
    int cy = g->player_y + g->PLAYER.h / 2;
    int age = PLAYER_DEATH_DELAY_FRAMES - g->player_death_timer;
    render_player_explosion_at(lfb, cx, cy, age);
}

static void render_explosion_points(lfb_t *lfb, int cx, int cy, int points) {
    if (points <= 0) return;

    char points_text[16];
    snprintf(points_text, sizeof(points_text), "%d", points);
    int text_w = text_width_5x5(points_text, 1);
    l_draw_text(lfb, cx - (text_w / 2), cy - 3, points_text, 1, 0xFFFFFFFF);
}

static void render_exit_indicator(lfb_t *lfb, int exit_y) {
    const char *exit_label = "EXIT";
    int exit_w = text_width_5x5(exit_label, 1);
    int arrow_gap = 3;
    int arrow_shaft_w = 5;
    int arrow_head_w = 4;
    int arrow_w = arrow_shaft_w + arrow_head_w;
    int right_margin = 5;

    int arrow_x = LW - right_margin - arrow_w;
    int exit_x = arrow_x - arrow_gap - exit_w;
    l_draw_text(lfb, exit_x, exit_y, exit_label, 1, 0xFFFF0000);

    int arrow_mid_y = exit_y + 2;
    for (int y = -1; y <= 1; y++) {
        for (int x = 0; x < arrow_shaft_w; x++) {
            l_putpix(lfb, arrow_x + x, arrow_mid_y + y, 0xFFFF0000);
        }
    }
    for (int i = 0; i < arrow_head_w; i++) {
        int hx = arrow_x + arrow_shaft_w + i;
        int half_h = arrow_head_w - 1 - i;
        for (int y = -half_h; y <= half_h; y++) {
            l_putpix(lfb, hx, arrow_mid_y + y, 0xFFFF0000);
        }
    }
}

static void render_alien_explosion(lfb_t *lfb, int cx, int cy, int timer, int points) {
    int age = ALIEN_EXPLOSION_FRAMES - timer;
    int base_r = 2 + (age / 3);

    draw_filled_circle(lfb, cx, cy, base_r + 2, 0xFFFF4500);
    draw_filled_circle(lfb, cx, cy, base_r, 0xFFFF8C00);
    if ((age & 1) == 0) {
        draw_filled_circle(lfb, cx, cy, (base_r > 1) ? (base_r - 1) : 1, 0xFFFFFF00);
    }

    if (timer > 0) {
        render_explosion_points(lfb, cx, cy, points);
    }
}

static void reset_win_state(game_t *g) {
    g->win_screen = 0;
    g->win_explosion_index = 0;
    g->win_explosion_delay_timer = WIN_EXPLOSION_DELAY_FRAMES;
    g->win_prompt_ready = 0;
    for (int i = 0; i < WIN_RING_ALIENS; i++) {
        g->win_alien_explode_timer[i] = 0;
    }
}

static int player_invulnerable(const game_t *g) {
    return (g->player_iframe_timer > 0) && !g->player_dying;
}

static int player_visible_with_iframes(const game_t *g) {
    if (!player_invulnerable(g)) return 1;
    return ((g->player_iframe_timer / 4) & 1) == 0;
}

static void trigger_player_death(game_t *g);

static void apply_player_damage(game_t *g, int damage) {
    if (damage <= 0) return;
    if (player_invulnerable(g)) return;

    g->lives -= damage;
    if (g->lives < 0) g->lives = 0;
    if (g->lives > 0) {
        g->player_iframe_timer = PLAYER_IFRAMES;
        music_play_boom();
    }
    clear_player_shots(g);
    if (g->lives <= 0) {
        trigger_player_death(g);
    }
}

static void trigger_player_death(game_t *g) {
    if (g->player_dying || g->game_over) return;
    g->player_dying = 1;
    g->player_death_timer = PLAYER_DEATH_DELAY_FRAMES;
    g->game_over_score = g->score;
    g->ashot.alive = 0;
    clear_boss_projectiles(g);
    clear_player_shots(g);
    music_play_boom_long();
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

static int award_alien_kill_points(game_t *g, int killed_yellow_boss_alien) {
    int remaining = count_aliens_remaining(g);
    int base_points;

    if (killed_yellow_boss_alien) {
        base_points = (remaining == 0) ? 200 : 50;
    } else {
        base_points = (remaining == 0) ? 100 : 10;
    }

    int points = double_shot_active(g) ? (base_points * 2) : base_points;
    g->score += points;
    return points;
}

static void kill_alien_with_explosion(game_t *g, int r, int c) {
    if (!g->alien_alive[r][c]) return;
    int killed_yellow_boss_alien = g->yellow_boss_marked[r][c];
    g->alien_alive[r][c] = 0;
    g->yellow_boss_marked[r][c] = 0;
    g->alien_health[r][c] = 0;
    g->alien_explode_timer[r][c] = ALIEN_EXPLOSION_FRAMES;
    g->alien_explode_hit_boss[r][c] = 0;
    g->alien_explode_points[r][c] = award_alien_kill_points(g, killed_yellow_boss_alien);
}

static void trigger_explosive_chain(game_t *g, int seed_r, int seed_c) {
    const int dr[4] = {-1, 1, 0, 0};
    const int dc[4] = {0, 0, -1, 1};
    for (int i = 0; i < 4; i++) {
        int r = seed_r + dr[i];
        int c = seed_c + dc[i];
        if (r < 0 || r >= AROWS || c < 0 || c >= ACOLS) continue;
        if (!g->alien_alive[r][c]) continue;
        kill_alien_with_explosion(g, r, c);
    }
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
                        music_play_powerup();
                        break;
                    }
                }
                g->powerup_active = 0;
                shots[si].alive = 0;
            }
        }
        if (!shots[si].alive) continue;

        if (g->boss_shield_active && g->boss_alive && !g->boss_dying) {
            int shield_hit = 0;
            int sx = boss_shield_x(g);
            int sy = boss_shield_y(g);
            for (int bi = 0; bi < 5 && !shield_hit; bi++) {
                int bx = bullet_x_with_spread(&shots[si], bi, spread_dir);
                int by = shots[si].y - bi;
                if (bullet_hits_sprite(&g->BOSS_SHIELD, sx, sy, bx, by)) {
                    bunker_damage(&g->BOSS_SHIELD, bx - sx, by - sy, 3);
                    shield_hit = 1;
                }
            }
            if (shield_hit) {
                if (!sprite_has_any_bits(&g->BOSS_SHIELD)) {
                    g->boss_shield_active = 0;
                }
                shots[si].alive = 0;
            }
        }
        if (!shots[si].alive) continue;

        if (g->boss_alive && !g->boss_dying && g->boss_type != BOSS_TYPE_YELLOW) {
            const sprite1r_t *BS = active_boss_sprite(g);
            int boss_hit = 0;
            for (int bi = 0; bi < 5 && !boss_hit; bi++) {
                if (bullet_hits_sprite(BS,
                                       g->boss_x, g->boss_y,
                                       bullet_x_with_spread(&shots[si], bi, spread_dir), shots[si].y - bi)) {
                    boss_hit = 1;
                }
            }
            if (boss_hit) {
                g->boss_health -= g->player_damage;
                if (g->boss_health <= 0) {
                    g->boss_health = 0;
                    boss_trigger_death(g, double_shot_active(g) ? 1000 : 500);
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
                        kill_alien_with_explosion(g, r, c);
                        if (explosive_shot_active(g)) {
                            trigger_explosive_chain(g, r, c);
                        }
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
    if (g->player_dying) return;
    if (bullet_hits_sprite(&g->PLAYER, g->player_x, g->player_y, shot->x, shot->y)) {
        if (player_invulnerable(g)) {
            shot->alive = 0;
            return;
        }
        apply_player_damage(g, 1);
        shot->alive = 0;
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
    g->player_speed = PLAYER_BASE_SPEED;
    g->fire_speed_bonus = 0;
    g->player_damage = PLAYER_BASE_DAMAGE;
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
    clear_boss_projectiles(g);
    g->fire_cooldown = 0;

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

    update_player_movement(g, buttons);
    update_player_firing(g, buttons);
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
    int exit_y = g->player_y - 10;
    render_exit_indicator(lfb, exit_y);

    // Player
    render_player(g, lfb);

    // Bullets
    render_player_shots(g, lfb);

    // Score (bottom-right)
    render_score_display(g, lfb);

    // SHOP sign on top-left
    {
        const char *shop_label = "SHOP";
        l_draw_text(lfb, 5, 5, shop_label, 1, 0xFF8000FF);
    }

    // PLAYER 1 label and health bar
    render_player_health_bar(g, lfb);
    render_fps_counter(lfb);
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
    g->boss_intro_active = 0;
    g->boss_intro_timer = 0;

    g->powerup_active = 0;
    g->powerup_timer = 0;
    g->powerup_spawn_timer = 0;

    g->player_x = 5;  // Spawn on left side
    g->player_y = LH - 30;
    g->exit_available = 0;
    g->exit_was_available = 0;
    g->exit_blink_timer = 0;
    g->exit_blink_toggles_remaining = 0;
    reset_win_state(g);

    if (level == WIN_LEVEL) {
        g->boss_alive = 0;
        g->boss_dying = 0;
        clear_boss_projectiles(g);
        g->ashot.alive = 0;
        clear_player_shots(g);
        memset(g->alien_alive, 0, sizeof(g->alien_alive));
        memset(g->alien_health, 0, sizeof(g->alien_health));
        memset(g->alien_explode_timer, 0, sizeof(g->alien_explode_timer));
        memset(g->alien_explode_points, 0, sizeof(g->alien_explode_points));
        memset(g->alien_explode_hit_boss, 0, sizeof(g->alien_explode_hit_boss));
        g->boss_explode_points = 0;
        g->win_screen = 1;
        g->player_x = (LW - g->PLAYER.w) / 2;
        g->player_y = LH / 2 - g->PLAYER.h / 2;
        return;
    }

    // Level 0: tutorial level - only one alien in center
    if (level == 0) {
        memset(g->alien_alive, 0, sizeof(g->alien_alive));
        memset(g->alien_health, 0, sizeof(g->alien_health));
        memset(g->alien_explode_timer, 0, sizeof(g->alien_explode_timer));
        memset(g->alien_explode_points, 0, sizeof(g->alien_explode_points));
        memset(g->alien_explode_hit_boss, 0, sizeof(g->alien_explode_hit_boss));
        // Put one alien in center (row 4, column 5)
        g->alien_alive[4][5] = 1;
        g->alien_health[4][5] = 1;
    } else {
        memset(g->alien_alive, 1, sizeof(g->alien_alive));
        memset(g->alien_explode_timer, 0, sizeof(g->alien_explode_timer));
        memset(g->alien_explode_points, 0, sizeof(g->alien_explode_points));
        memset(g->alien_explode_hit_boss, 0, sizeof(g->alien_explode_hit_boss));
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
    g->boss_alive = (level > 0) ? 1 : 0;
    g->boss_type = (level > 0) ? (rand() % BOSS_TYPE_COUNT) : BOSS_TYPE_CLASSIC;
    g->boss_max_health = BOSS_MAX_HEALTH(level);
    g->boss_health = g->boss_max_health;
    clear_yellow_boss_state(g);
    if (g->boss_type == BOSS_TYPE_YELLOW) {
        select_yellow_boss_aliens(g);
    }
    const sprite1r_t *BS = active_boss_sprite(g);
    g->boss_x = (LW - BS->w) / 2;
    g->boss_y = 15;
    g->boss_dx = 1;
    g->boss_dying = 0;
    g->boss_death_timer = 0;
    g->boss_explode_points = 0;
    g->boss_shield_active = 0;
    g->boss_frame = 0;
    g->boss_timer = 0;
    g->boss_period = BOSS_PERIOD(level); // Faster boss at higher levels

    clear_player_shots(g);
    g->ashot.alive = 0;
    clear_boss_projectiles(g);
    g->boss_shield_active = 0;
    g->fire_cooldown = 0;

    // Initialize boss power system
    g->boss_power_timer = 0;
    g->boss_power_max = 600;  // 10 seconds at 60 FPS
    g->boss_power_active = 0;
    g->boss_power_cooldown = 0;
    g->boss_laser_last_hit_y = -1000;  // Far off screen
    g->boss_attack_type = (g->boss_type == BOSS_TYPE_BLUE) ? 2 :
                          (g->boss_type == BOSS_TYPE_YELLOW) ? YELLOW_BOSS_ATTACK_SHUFFLE : 0;  // Current attack being executed
    g->next_boss_attack_type = (g->boss_type == BOSS_TYPE_BLUE) ? 2 :
                               (g->boss_type == BOSS_TYPE_YELLOW) ? YELLOW_BOSS_ATTACK_SHUFFLE : 0;  // Next attack to be charged
    g->boss_green_laser_last_hit_y = -1000;  // Far off screen
    refill_sprite_full(&g->BOSS_SHIELD);

    if (level > 0) {
        g->boss_intro_active = 1;
        g->boss_intro_timer = BOSS_INTRO_FRAMES;
    }

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
    g->BOSS_A  = make_sprite_from_ascii(bossA_rows, 10);
    g->BOSS_B  = make_sprite_from_ascii(bossB_rows, 10);
    g->BOSS2_A = make_sprite_from_ascii(boss2A_rows, 10);
    g->BOSS2_B = make_sprite_from_ascii(boss2B_rows, 10);
    g->BOSS_SHIELD = make_filled_sprite(g->BOSS_A.w, 3);
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

void game_reset(game_t *g) {
    g->game_over = 0;
    g->game_over_score = 0;
    g->start_screen = 0;
    g->start_screen_delay_timer = 0;
    g->paused = 0;
    reset_win_state(g);
    reset_player_progression(g);

    setup_level(g, 0, 1);
}

void game_update(game_t *g, uint32_t buttons, uint32_t vsync_counter) {
    // Handle reset button (anytime during gameplay)
    static uint32_t prev_buttons = 0;
    if ((buttons & BTN_RESET) && !(prev_buttons & BTN_RESET)) {
        g->start_screen = 1;
        g->start_screen_delay_timer = 30;
    }
    
    // Handle pause toggle (only in active gameplay)
    if (!g->start_screen && !g->game_over && !g->level_complete && !g->in_shop && !g->win_screen && !g->boss_intro_active) {
        if ((buttons & BTN_PAUSE) && !(prev_buttons & BTN_PAUSE)) {
            g->paused = !g->paused;
        }
    }
    prev_buttons = buttons;

    // If paused, skip all game logic
    if (g->paused) {
        return;
    }

    if (g->player_iframe_timer > 0) {
        g->player_iframe_timer--;
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

    if (g->win_screen) {
        g->alien_frame = (int)((vsync_counter / 20u) & 1u);

        for (int i = 0; i < WIN_RING_ALIENS; i++) {
            if (g->win_alien_explode_timer[i] > 0) {
                g->win_alien_explode_timer[i]--;
            }
        }

        if (g->win_explosion_index < WIN_RING_ALIENS) {
            if (g->win_explosion_delay_timer > 0) {
                g->win_explosion_delay_timer--;
            }
            if (g->win_explosion_delay_timer == 0) {
                g->win_alien_explode_timer[g->win_explosion_index] = ALIEN_EXPLOSION_FRAMES;
                g->win_explosion_index++;
                g->win_explosion_delay_timer = WIN_EXPLOSION_DELAY_FRAMES;
            }
        } else {
            int any_active = 0;
            for (int i = 0; i < WIN_RING_ALIENS; i++) {
                if (g->win_alien_explode_timer[i] > 0) {
                    any_active = 1;
                    break;
                }
            }
            if (!any_active) {
                g->win_prompt_ready = 1;
            }
        }

        if (g->win_prompt_ready && (buttons & BTN_FIRE)) {
            g->start_screen = 1;
            g->start_screen_delay_timer = 30;
            reset_player_progression(g);
            setup_level(g, 0, 1);
        }
        return;
    }

    if (g->player_dying) {
        if (g->player_death_timer > 0) {
            g->player_death_timer--;
        }
        if (g->player_death_timer <= 0) {
            g->player_dying = 0;
            g->game_over = 1;
            g->game_over_delay_timer = 90;
        }
        return;
    }

    if (g->in_shop) {
        shop_update(g, buttons, vsync_counter);
        return;
    }

    if (g->boss_intro_active) {
        if (g->boss_intro_timer > 0) {
            g->boss_intro_timer--;
        }
        if (g->boss_intro_timer <= 0) {
            g->boss_intro_active = 0;
        }
        return;
    }

    if (g->boss_dying) {
        clear_boss_projectiles(g);
        boss_apply_explosion_to_aliens(g);
        if (g->boss_death_timer > 0) {
            g->boss_death_timer--;
        }
        if (g->boss_death_timer <= 0) {
            g->boss_dying = 0;
            g->boss_alive = 0;
        }
    }

    if (g->exit_blink_toggles_remaining > 0) {
        g->exit_blink_timer--;
        if (g->exit_blink_timer <= 0) {
            g->exit_blink_timer = EXIT_BLINK_INTERVAL_FRAMES;
            g->exit_blink_toggles_remaining--;
        }
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

    for (int r = 0; r < AROWS; r++) {
        for (int c = 0; c < ACOLS; c++) {
            if (g->alien_explode_timer[r][c] > 0) {
                g->alien_explode_timer[r][c]--;
                if (g->alien_explode_timer[r][c] == 0) {
                    g->alien_explode_points[r][c] = 0;
                }
            }
        }
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
    
    update_player_movement(g, buttons);
    update_player_firing(g, buttons);

    g->alien_timer++;
    if (g->alien_timer >= g->alien_period) {
        g->alien_timer = 0;
        g->alien_frame ^= 1;

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

    if (g->boss_alive && !g->boss_dying && g->boss_type == BOSS_TYPE_YELLOW) {
        // Yellow boss is a swarm of marked aliens, so no standalone boss movement.
        int charge_step = 4 + (g->level / 4);
        g->boss_power_timer += charge_step;
        if (g->boss_power_timer > g->boss_power_max) {
            g->boss_power_timer = g->boss_power_max;
        }

        g->next_boss_attack_type = YELLOW_BOSS_ATTACK_SHUFFLE;
        if (g->boss_power_timer >= g->boss_power_max) {
            g->boss_power_active = 1;
            g->boss_attack_type = YELLOW_BOSS_ATTACK_SHUFFLE;
            shuffle_yellow_boss_aliens(g);
            g->boss_power_timer = 0;
            g->boss_power_active = 0;
            music_play_ding();
        }

        update_yellow_boss_health(g);
    }

    // Boss movement and animation (separate from grid)
    if (g->boss_alive && !g->boss_dying && g->boss_type != BOSS_TYPE_YELLOW) {
        const sprite1r_t *BS = active_boss_sprite(g);
        int boss_w = BS->w;
        int boss_h = BS->h;

        // Boss power charging system
        int prev_power_timer = g->boss_power_timer;
        int health_pct = (g->boss_health * 100) / g->boss_max_health;
        if (health_pct <= 33) g->boss_power_timer += 3 + (g->level / 6);
        else if (health_pct <= 67) g->boss_power_timer += 2 + (g->level / 6);
        else g->boss_power_timer += 1 + (g->level / 6);
        if (g->boss_power_timer > g->boss_power_max) {
            g->boss_power_timer = g->boss_power_max;
        }
        
        // Determine next attack type when a new charging cycle starts.
        // Level 7 can increment by >1 per frame, so checking ==1 can be skipped.
        if (prev_power_timer == 0 && g->boss_power_timer > 0) {
            if (g->boss_type == BOSS_TYPE_BLUE) {
                g->next_boss_attack_type = 2;
            } else {
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
        }

        // When power reaches max, trigger special attack
        if (g->boss_power_timer >= g->boss_power_max && g->boss_power_active == 0) {
            g->boss_power_active = 1;
            g->boss_power_cooldown = 30;
            
            // Use the next attack type that was determined during charging
            g->boss_attack_type = g->next_boss_attack_type;

            g->boss_laser.alive = 0;
            g->boss_bomb.alive = 0;
            g->boss_bomb.exploding = 0;
            g->boss_bomb.explode_timer = 0;
            g->boss_bomb.hit_player = 0;

            if (g->boss_attack_type == 2) {
                g->boss_bomb.alive = 1;
                g->boss_bomb.exploding = 0;
                g->boss_bomb.explode_timer = 0;
                g->boss_bomb.hit_player = 0;
                g->boss_bomb.x = g->boss_x + boss_w / 2;
                g->boss_bomb.y = g->boss_y + boss_h + 2;
            } else {
                g->boss_laser.alive = 1;
                g->boss_laser.x = g->boss_x + boss_w / 2;
                g->boss_laser.y = g->boss_y + boss_h + 1;
            }

            g->boss_laser_last_hit_y = -1000;  // Reset hit tracking
            g->boss_green_laser_last_hit_y = -1000;  // Reset green laser tracking
            g->boss_power_timer = 0;  // Reset for next charge
            music_play_laser();
            
            // Boss recovers 10% health if using green laser
            if (g->boss_attack_type == 1) {
                g->boss_health += g->boss_max_health / 10;
                if (g->boss_health > g->boss_max_health) {
                    g->boss_health = g->boss_max_health;
                }
                if (g->level >= 5) {
                    g->boss_shield_active = 1;
                    refill_sprite_full(&g->BOSS_SHIELD);
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

        // Blue boss charged bomb: falls, then explodes.
        if (g->boss_attack_type == 2) {
            if (g->boss_bomb.alive) {
                if (!g->boss_bomb.exploding) {
                    g->boss_bomb.y += 3;
                    if (g->boss_bomb.y >= g->player_y || g->boss_bomb.y >= LH - 6) {
                        g->boss_bomb.exploding = 1;
                        g->boss_bomb.explode_timer = BOSS_BOMB_EXPLOSION_FRAMES;
                    }
                } else {
                    if (g->boss_bomb.explode_timer > 0) {
                        g->boss_bomb.explode_timer--;
                    }
                    if (g->boss_bomb.explode_timer <= 0) {
                        g->boss_bomb.alive = 0;
                        g->boss_bomb.exploding = 0;
                        g->boss_power_active = 0;
                    }
                }
            } else {
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
    if (g->boss_alive && !g->boss_dying) {
        if (g->boss_type == BOSS_TYPE_YELLOW) {
            int period = 20;
            const sprite1r_t *AS = g->alien_frame ? &g->ALIEN_B : &g->ALIEN_A;
            int spacing_x = 6, spacing_y = 5;

            if ((vsync_counter % (uint32_t)period) == 0u) {
                for (int r = 0; r < AROWS; r++) {
                    for (int c = 0; c < ACOLS; c++) {
                        if (!g->yellow_boss_marked[r][c] || !g->alien_alive[r][c]) continue;
                        if ((rand() % 5) != 0) continue; // 1/5 chance per yellow alien to fire

                        int slot = -1;
                        for (int i = 0; i < YELLOW_BOSS_ALIENS; i++) {
                            if (!g->yellow_beam_shot[i].alive) {
                                slot = i;
                                break;
                            }
                        }
                        if (slot < 0) continue;

                        int ax = g->alien_origin_x + c * (AS->w + spacing_x);
                        int ay = g->alien_origin_y + r * (AS->h + spacing_y);
                        g->yellow_beam_shot[slot].alive = 1;
                        g->yellow_beam_shot[slot].x = ax + AS->w / 2;
                        g->yellow_beam_shot[slot].y = ay + AS->h + 1;
                    }
                }
            }

            for (int i = 0; i < YELLOW_BOSS_ALIENS; i++) {
                if (!g->yellow_beam_shot[i].alive) continue;
                g->yellow_beam_shot[i].y += 3;
                if (g->yellow_beam_shot[i].y > LH) g->yellow_beam_shot[i].alive = 0;
            }

            g->boss_shot.alive = 0;
            g->boss_laser.alive = 0;
            for (int i = 0; i < 3; i++) {
                g->boss_triple_shot[i].alive = 0;
            }
        } else {
            int health_pct = (g->boss_health * 100) / 20;
            int period = 90; // slow when green
            if (health_pct <= 10) period = 15;       // fast when red
            else if (health_pct <= 50) period = 30;  // medium when yellow

            if (g->boss_type == BOSS_TYPE_BLUE) {
            int any_triple_alive = 0;
            for (int i = 0; i < 3; i++) {
                if (g->boss_triple_shot[i].alive) {
                    any_triple_alive = 1;
                    break;
                }
            }

            if (!any_triple_alive) {
                if ((vsync_counter % (uint32_t)period) == 0u) {
                    const sprite1r_t *BS = active_boss_sprite(g);
                    int cx = g->boss_x + BS->w / 2;
                    int cy = g->boss_y + BS->h + 1;

                    for (int i = 0; i < 3; i++) {
                        g->boss_triple_shot[i].alive = 1;
                        g->boss_triple_shot[i].x = cx;
                        g->boss_triple_shot[i].y = cy;
                    }
                    g->boss_triple_shot[0].x -= 4;
                    g->boss_triple_shot[2].x += 4;
                    g->boss_triple_shot_dx[0] = -1;
                    g->boss_triple_shot_dx[1] = 0;
                    g->boss_triple_shot_dx[2] = 1;
                }
            } else {
                for (int i = 0; i < 3; i++) {
                    if (!g->boss_triple_shot[i].alive) continue;
                    g->boss_triple_shot[i].x += g->boss_triple_shot_dx[i];
                    g->boss_triple_shot[i].y += 2;
                    if (g->boss_triple_shot[i].y > LH || g->boss_triple_shot[i].x < 0 || g->boss_triple_shot[i].x >= LW) {
                        g->boss_triple_shot[i].alive = 0;
                    }
                }
            }
                g->boss_shot.alive = 0;
            } else if (!g->boss_shot.alive) {
                if ((vsync_counter % (uint32_t)period) == 0u) {
                    const sprite1r_t *BS = active_boss_sprite(g);
                    g->boss_shot.alive = 1;
                    g->boss_shot.x = g->boss_x + BS->w / 2;
                    g->boss_shot.y = g->boss_y + BS->h + 1;
                }
            } else {
                g->boss_shot.y += 2;
                if (g->boss_shot.y > LH) g->boss_shot.alive = 0;
            }
        }
    } else {
        clear_boss_projectiles(g);
    }

    // collisions: player shots
    handle_player_shot_collisions(g, g->pshot, 0, 1);
    handle_player_shot_collisions(g, g->pshot_left, -1, 0);
    handle_player_shot_collisions(g, g->pshot_right, 1, 0);

    if (g->boss_type != BOSS_TYPE_YELLOW) {
        boss_apply_alien_explosions_to_boss(g);
    }

    // collisions: alien and boss shots
    handle_enemy_shot_collisions(g, &g->ashot);
    handle_enemy_shot_collisions(g, &g->boss_shot);
    for (int i = 0; i < 3; i++) {
        handle_enemy_shot_collisions(g, &g->boss_triple_shot[i]);
    }
    for (int i = 0; i < YELLOW_BOSS_ALIENS; i++) {
        handle_enemy_shot_collisions(g, &g->yellow_beam_shot[i]);
    }

    // Handle boss laser collision (only with player, phases through)
    if (g->boss_laser.alive) {
        const sprite1r_t *BS = active_boss_sprite(g);
        const sprite1r_t *p = &g->PLAYER;
        int laser_w = BS->w;  // Laser width matches boss width
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
                if (player_invulnerable(g)) {
                    g->boss_laser_last_hit_y = g->boss_laser.y;
                } else {
                    g->boss_laser_last_hit_y = g->boss_laser.y;  // Track where we hit
                    apply_player_damage(g, 1);
                }
                // Laser continues through player (not destroyed)
            }
        }
        
        // Handle green laser collision with aliens (adds HP)
        if (g->boss_attack_type == 1) {
            int laser_w = BS->w;
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

    // Blue charged bomb explosion: same radius progression as boss death explosion and 2 damage to player.
    if (g->boss_attack_type == 2 && g->boss_bomb.alive && g->boss_bomb.exploding) {
        int radius = boss_bomb_explosion_radius(g);
        int pcx = g->player_x + g->PLAYER.w / 2;
        int pcy = g->player_y + g->PLAYER.h / 2;
        int dx = pcx - g->boss_bomb.x;
        int dy = pcy - g->boss_bomb.y;

        if (blue_black_hole_enabled(g) && !g->player_dying) {
            int pull_step = g->player_speed / 2;
            if (pull_step < 1) pull_step = 1;

            int delta_x = g->boss_bomb.x - pcx;
            if (delta_x > 0) {
                int step = (delta_x < pull_step) ? delta_x : pull_step;
                g->player_x += step;
            } else if (delta_x < 0) {
                int step = ((-delta_x) < pull_step) ? (-delta_x) : pull_step;
                g->player_x -= step;
            }

            if (g->player_x < 0) g->player_x = 0;
            if (g->player_x > LW - g->PLAYER.w) g->player_x = LW - g->PLAYER.w;

            // Recompute center after movement for hit test.
            pcx = g->player_x + g->PLAYER.w / 2;
            dx = pcx - g->boss_bomb.x;
        }

        if (!g->boss_bomb.hit_player && (dx * dx + dy * dy) <= (radius * radius)) {
            g->boss_bomb.hit_player = 1;
            apply_player_damage(g, 2);
        }
    }

    if (g->boss_type == BOSS_TYPE_YELLOW) {
        update_yellow_boss_health(g);
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

    if (g->exit_available && !g->exit_was_available) {
        g->exit_blink_timer = EXIT_BLINK_INTERVAL_FRAMES;
        g->exit_blink_toggles_remaining = EXIT_BLINK_TOGGLES;
        music_play_ding();
    }
    g->exit_was_available = g->exit_available;
}

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

        const int game_over_ship_hold_frames = 30;
        int game_over_age = 90 - g->game_over_delay_timer;
        if (game_over_age < game_over_ship_hold_frames) {
            uint32_t player_color = triple_shot_active(g) ? 0xFF0000FF : 0xFF00FF00;
            draw_sprite1r(lfb, &g->PLAYER, cx - g->PLAYER.w / 2, cy - g->PLAYER.h / 2, player_color);
        } else if (game_over_age < game_over_ship_hold_frames + PLAYER_DEATH_DELAY_FRAMES) {
            render_player_explosion_at(lfb, cx, cy, game_over_age - game_over_ship_hold_frames);
        }

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

    if (g->win_screen) {
        l_clear(lfb, 0xFF000000);

        const char *title = "YOU WIN";
        int title_scale = 4;
        int title_w = text_width_5x5(title, title_scale);
        int title_x = (LW - title_w) / 2;
        int title_y = 10;
        l_draw_text(lfb, title_x, title_y, title, title_scale, 0xFF00FF00);

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
        int hx[WIN_RING_ALIENS] = { cx, cx + r, cx + r, cx, cx - r, cx - r };
        int hy[WIN_RING_ALIENS] = { cy - r, cy - r/2, cy + r/2, cy + r, cy + r/2, cy - r/2 };
        for (int i = 0; i < WIN_RING_ALIENS; i++) {
            if (g->win_alien_explode_timer[i] > 0) {
                render_alien_explosion(lfb, hx[i], hy[i], g->win_alien_explode_timer[i], 0);
            } else if (i >= g->win_explosion_index) {
                draw_sprite1r(lfb, AS, hx[i] - a_w / 2, hy[i] - a_h / 2, 0xFFFFFFFF);
            }
        }

        const char *score_label = "FINAL SCORE:";
        int label_scale = 1;
        int label_w = text_width_5x5(score_label, label_scale);
        int label_x = (LW - label_w) / 2 - 12;
        int label_y = cy + r + 15;
        l_draw_text(lfb, label_x, label_y, score_label, label_scale, 0xFFFFFFFF);
        l_draw_score(lfb, label_x + label_w + 6, label_y, g->score, 0xFFFFFFFF);

        if (g->win_prompt_ready) {
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

    if (g->boss_intro_active) {
        l_clear(lfb, 0xFF000000);

        const char *type_prefix = "ALIEN TYPE:";
        const char *type_value = boss_intro_type_text(g);
        const char *main_prefix = "MAIN ATTACK:";
        const char *main_value = boss_intro_main_attack_text(g);
        const char *special_prefix = "SPECIAL ATTACK:";
        const char *special_value = boss_intro_special_attack_text(g);

        int scale = 1;
        int y = 18;

        uint32_t type_color = 0xFFFF55AA;
        if (g->boss_type == BOSS_TYPE_BLUE) {
            uint32_t blue_flicker_palette[4] = {0xFF1E6AD6, 0xFF3399FF, 0xFF66CCFF, 0xFF3399FF};
            int flicker_idx = ((BOSS_INTRO_FRAMES - g->boss_intro_timer) / 8) & 3;
            type_color = blue_flicker_palette[flicker_idx];
        } else if (g->boss_type == BOSS_TYPE_YELLOW) {
            uint32_t yellow_flicker_palette[4] = {0xFFFFFF66, 0xFFFFFF00, 0xFFFFD700, 0xFFFFFF00};
            int flicker_idx = ((BOSS_INTRO_FRAMES - g->boss_intro_timer) / 8) & 3;
            type_color = yellow_flicker_palette[flicker_idx];
        } else {
            uint32_t flicker_palette[4] = {0xFF00FF00, 0xFFFFFF00, 0xFFFF0000, 0xFF8000FF};
            int flicker_idx = ((BOSS_INTRO_FRAMES - g->boss_intro_timer) / 8) & 3;
            type_color = flicker_palette[flicker_idx];
        }

        int type_prefix_w = text_width_5x5(type_prefix, scale);
        int type_value_w = text_width_5x5(type_value, scale);
        int type_x = (LW - (type_prefix_w + 6 + type_value_w)) / 2;
        l_draw_text(lfb, type_x, y, type_prefix, scale, 0xFFFFFFFF);
        l_draw_text(lfb, type_x + type_prefix_w + 6, y, type_value, scale, type_color);

        y += 14;
        int main_prefix_w = text_width_5x5(main_prefix, scale);
        int main_value_w = text_width_5x5(main_value, scale);
        int main_x = (LW - (main_prefix_w + 6 + main_value_w)) / 2;
        uint32_t main_color = (g->boss_type == BOSS_TYPE_BLUE) ? 0xFF3399FF :
                      (g->boss_type == BOSS_TYPE_YELLOW) ? 0xFFFFFF00 : 0xFFFF0000;
        l_draw_text(lfb, main_x, y, main_prefix, scale, 0xFFFFFFFF);
        l_draw_text(lfb, main_x + main_prefix_w + 6, y, main_value, scale, main_color);

        y += 14;
        int special_prefix_w = text_width_5x5(special_prefix, scale);
        int special_x = 0;
        int special_text_x = 0;
        if (g->boss_type == BOSS_TYPE_BLUE) {
            int special_value_w = text_width_5x5(special_value, scale);
            special_x = (LW - (special_prefix_w + 6 + special_value_w)) / 2;
            special_text_x = special_x + special_prefix_w + 6;
            l_draw_text(lfb, special_x, y, special_prefix, scale, 0xFFFFFFFF);
            l_draw_text(lfb, special_text_x, y, special_value, scale, 0xFF3399FF);
        } else if (g->boss_type == BOSS_TYPE_YELLOW) {
            int special_value_w = text_width_5x5(special_value, scale);
            special_x = (LW - (special_prefix_w + 6 + special_value_w)) / 2;
            special_text_x = special_x + special_prefix_w + 6;
            l_draw_text(lfb, special_x, y, special_prefix, scale, 0xFFFFFFFF);
            l_draw_text(lfb, special_text_x, y, special_value, scale, 0xFFFFFF00);
        } else if (g->level >= 3) {
            const char *spec1 = "PLASMA BEAM";
            const char *spec2 = "HEAL BEAM";
            int spec1_w = text_width_5x5(spec1, scale);
            int spec2_w = text_width_5x5(spec2, scale);
            int gap_w = 4;
            int value_total_w = spec1_w + gap_w + spec2_w;

            special_x = (LW - (special_prefix_w + 6 + value_total_w)) / 2;
            special_text_x = special_x + special_prefix_w + 6;
            l_draw_text(lfb, special_x, y, special_prefix, scale, 0xFFFFFFFF);
            l_draw_text(lfb, special_text_x, y, spec1, scale, 0xFF8000FF);

            l_draw_text(lfb, special_text_x + spec1_w + gap_w, y, spec2, scale, 0xFF00FF00);
        } else {
            int special_value_w = text_width_5x5(special_value, scale);
            special_x = (LW - (special_prefix_w + 6 + special_value_w)) / 2;
            special_text_x = special_x + special_prefix_w + 6;
            l_draw_text(lfb, special_x, y, special_prefix, scale, 0xFFFFFFFF);
            l_draw_text(lfb, special_text_x, y, special_value, scale, 0xFF8000FF);
        }

        const sprite1r_t *BS = active_boss_sprite(g);
        int boss_x = (LW - BS->w) / 2;
        int boss_y = 68;
        if (g->boss_type != BOSS_TYPE_YELLOW) {
            uint32_t boss_color = (g->boss_type == BOSS_TYPE_BLUE) ? 0xFF3399FF : 0xFF00FF00;
            draw_sprite1r(lfb, BS, boss_x, boss_y, boss_color);
        }
        render_intro_boss_attack_preview(g, lfb, boss_x, boss_y, BS);
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
    
    render_score_display(g, lfb);

    // Boss health bar on the left side of screen
    if (g->boss_alive) {
        uint32_t boss_color = (g->boss_type == BOSS_TYPE_BLUE) ? 0xFF3399FF :
                              (g->boss_type == BOSS_TYPE_YELLOW) ? 0xFFFFFF00 : 0xFF00FF00;
        int health_pct = (g->boss_max_health > 0) ? (g->boss_health * 100) / g->boss_max_health : 0;
        if (g->boss_type == BOSS_TYPE_BLUE) {
            if (health_pct <= 33) boss_color = 0xFF114488;
            else if (health_pct <= 67) boss_color = 0xFF1E6AD6;
        } else if (g->boss_type == BOSS_TYPE_YELLOW) {
            if (health_pct <= 33) boss_color = 0xFFFFC000;
            else if (health_pct <= 67) boss_color = 0xFFFFD84D;
        } else {
            if (health_pct <= 33) boss_color = 0xFFFF0000;
            else if (health_pct <= 67) boss_color = 0xFFFFFF00;
        }

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
        else if (g->next_boss_attack_type == 2) power_color = 0xFF3399FF;  // Blue bomb
        else if (g->next_boss_attack_type == YELLOW_BOSS_ATTACK_SHUFFLE) power_color = 0xFFFFFF00;  // Yellow shuffle
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

    render_player_health_bar(g, lfb);
        
    // Powerup progress bars
    {
        const char *p1 = "PLAYER 1";
        int scale = 1;
        int x = 5;
        int y = LH - 12;
        int lx = x + text_width_5x5(p1, scale) + 6;
        int bar_w = 40;
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
                    } else if (g->powerup_type_slot[slot] == POWERUP_EXPLOSIVE) {
                        icon_color = 0xFFFF0000;  // Red
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
    if (g->boss_alive && !g->boss_dying && g->boss_type != BOSS_TYPE_YELLOW) {
        int health_pct = (g->boss_max_health > 0) ? (g->boss_health * 100) / g->boss_max_health : 0;
        uint32_t boss_color = (g->boss_type == BOSS_TYPE_BLUE) ? 0xFF3399FF : 0xFF00FF00;
        if (g->boss_type == BOSS_TYPE_BLUE) {
            if (health_pct <= 33) boss_color = 0xFF114488;
            else if (health_pct <= 67) boss_color = 0xFF1E6AD6;
        } else {
            if (health_pct <= 33) boss_color = 0xFFFF0000;
            else if (health_pct <= 67) boss_color = 0xFFFFFF00;
        }

        // Turn purple when stunned/using purple laser attack
        if (g->boss_power_cooldown > 0) {
            if (g->boss_attack_type == 0) {
                boss_color = 0xFF8000FF;  // Purple for purple laser
            } else if (g->boss_attack_type == 2) {
                boss_color = 0xFF66B2FF;
            }
        }

        const sprite1r_t *BS = active_boss_sprite(g);
        draw_sprite1r(lfb, BS, g->boss_x, g->boss_y, boss_color);

        if (g->boss_shield_active) {
            draw_sprite1r(lfb, &g->BOSS_SHIELD, boss_shield_x(g), boss_shield_y(g), 0xFF00FF00);
        }
    } else if (g->boss_dying) {
        boss_render_explosion(g, lfb);
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
            uint32_t alien_color;
            if (g->yellow_boss_marked[r][c]) {
                alien_color = 0xFFFFFF00;
            } else {
                alien_color = (g->alien_health[r][c] >= 3) ? 0xFFFF0000 : (g->alien_health[r][c] == 2) ? 0xFF00FF00 : 0xFFFFFFFF;
            }
            draw_sprite1r(lfb, AS, ax, ay, alien_color);
        }
    }

    for (int r = 0; r < AROWS; r++) {
        for (int c = 0; c < ACOLS; c++) {
            if (g->alien_explode_timer[r][c] <= 0) continue;
            int ax = g->alien_origin_x + c * (AS->w + spacing_x) + AS->w / 2;
            int ay = g->alien_origin_y + r * (AS->h + spacing_y) + AS->h / 2;
            render_alien_explosion(lfb, ax, ay, g->alien_explode_timer[r][c], g->alien_explode_points[r][c]);
        }
    }

    if (g->player_dying) {
        render_player_explosion(g, lfb);
    } else if (player_visible_with_iframes(g)) {
        render_player(g, lfb);
    }

    if (g->powerup_active) {
        draw_powerup_icon(lfb, g->powerup_x, g->powerup_y, g->powerup_type);
    }

    // bullets
    render_player_shots(g, lfb);
    if (g->ashot.alive) for (int i = 0; i < 5; i++) l_putpix(lfb, g->ashot.x, g->ashot.y + i, 0xFFFF0000);
    if (g->boss_shot.alive) {
        for (int i = 0; i < 5; i++) l_putpix(lfb, g->boss_shot.x, g->boss_shot.y + i, 0xFFFF0000);
    }
    for (int s = 0; s < 3; s++) {
        if (!g->boss_triple_shot[s].alive) continue;
        int x_dir = g->boss_triple_shot_dx[s];
        for (int i = 0; i < 5; i++) {
            int x = g->boss_triple_shot[s].x + ((x_dir * i) / 2);
            int y = g->boss_triple_shot[s].y + i;
            l_putpix(lfb, x, y, 0xFF3399FF);
        }
    }
    for (int s = 0; s < YELLOW_BOSS_ALIENS; s++) {
        if (!g->yellow_beam_shot[s].alive) continue;
        for (int i = 0; i < 5; i++) {
            l_putpix(lfb, g->yellow_beam_shot[s].x, g->yellow_beam_shot[s].y + i, 0xFFFFFF00);
        }
    }

    // Boss laser beam and trailing after-image copies.
    // Only the leading beam is used for collision in update logic.
    if (g->boss_laser.alive) {
        const sprite1r_t *BS = active_boss_sprite(g);
        int laser_w = BS->w;  // Width matches boss width
        int laser_left = g->boss_laser.x - laser_w / 2;
        uint32_t laser_color = (g->boss_attack_type == 1) ? 0xFF00FF00 : 0xFF8000FF;  // Green or purple
        const int trail_offsets[] = {0, 5, 10, 20};
        const int trail_intensity[] = {100, 65, 40, 22};

        for (int t = 0; t < 4; t++) {
            int trail_y = g->boss_laser.y - trail_offsets[t];
            if (trail_y < 0 || trail_y >= LH) continue;
            uint32_t trail_color = color_with_intensity(laser_color, trail_intensity[t]);
            for (int px = laser_left; px < laser_left + laser_w; px++) {
                if (px >= 0 && px < LW) {
                    l_putpix(lfb, px, trail_y, trail_color);
                }
            }
        }
    }

    // Blue boss charged bomb and explosion visual.
    if (g->boss_bomb.alive) {
        if (!g->boss_bomb.exploding) {
            draw_filled_circle(lfb, g->boss_bomb.x, g->boss_bomb.y, 4, 0xFF66CCFF);
            draw_filled_circle(lfb, g->boss_bomb.x, g->boss_bomb.y, 2, 0xFFB8ECFF);
        } else {
            int r = boss_bomb_explosion_radius(g);
            if (blue_black_hole_enabled(g)) {
                render_black_hole_explosion(lfb, g->boss_bomb.x, g->boss_bomb.y, r);
            } else {
                draw_filled_circle(lfb, g->boss_bomb.x, g->boss_bomb.y, r, 0xFF0B3A8F);
                draw_filled_circle(lfb, g->boss_bomb.x, g->boss_bomb.y, r - 3, 0xFF1E6AD6);
                if ((g->boss_bomb.explode_timer & 1) == 0) {
                    int core_r = r - 7;
                    if (core_r < 1) core_r = 1;
                    draw_filled_circle(lfb, g->boss_bomb.x, g->boss_bomb.y, core_r, 0xFF66CCFF);
                }
            }
        }
    }

    // Exit sign when boss is killed
    if (exit_sign_visible(g)) {
        int exit_y = g->player_y - 10;
        render_exit_indicator(lfb, exit_y);
    }

    render_fps_counter(lfb);
}
