#include "game_player.h"

#include "../hw_contract.h"
#include "font.h"
#include "game_helpers.h"

static void try_spawn_bullet(game_t *g, bullet_t *shots, int x, int y, int pierce_active) {
    for (int i = 0; i < MAX_PSHOTS; i++) {
        if (!shots[i].alive) {
            shots[i].alive = 1;
            shots[i].x = x;
            shots[i].y = y;
            shots[i].last_hit_r = -1;
            shots[i].last_hit_c = -1;
            shots[i].pierce_active = pierce_active ? 1 : 0;
            shots[i].damage_remaining = pierce_active ? (g->player_damage + 1) : g->player_damage;
            break;
        }
    }
}

static void fire_player_shots(game_t *g, int center_x, int center_y) {
    try_spawn_bullet(g, g->pshot, center_x, center_y, g->pierce_unlocked);
    if (triple_shot_active(g)) {
        try_spawn_bullet(g, g->pshot_left, center_x - 2, center_y, 0);
        try_spawn_bullet(g, g->pshot_right, center_x + 2, center_y, 0);
    }
}

static int compute_fire_cooldown(const game_t *g) {
    int base = 20 - g->fire_speed_bonus * 2;
    if (base < 5) base = 5;
    int cooldown = rapid_fire_active(g) ? (base / 2) : base;
    if (g->pierce_unlocked) cooldown *= 2;
    if (cooldown < 3) cooldown = 3;
    return cooldown;
}

void clear_player_shots(game_t *g) {
    for (int i = 0; i < MAX_PSHOTS; i++) {
        g->pshot[i].alive = 0;
        g->pshot_left[i].alive = 0;
        g->pshot_right[i].alive = 0;
        g->pshot[i].pierce_active = 0;
        g->pshot_left[i].pierce_active = 0;
        g->pshot_right[i].pierce_active = 0;
        g->pshot[i].damage_remaining = 0;
        g->pshot_left[i].damage_remaining = 0;
        g->pshot_right[i].damage_remaining = 0;
    }
}

void update_player_shots(game_t *g) {
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

void update_player_movement(game_t *g, uint32_t buttons) {
    if (buttons & BTN_LEFT) g->player_x -= g->player_speed;
    if (buttons & BTN_RIGHT) g->player_x += g->player_speed;

    if (g->player_x < 0) g->player_x = 0;
    if (g->player_x > LW - g->PLAYER.w) g->player_x = LW - g->PLAYER.w;
}

void update_player_firing(game_t *g, uint32_t buttons) {
    if (g->fire_cooldown > 0) g->fire_cooldown--;
    if ((buttons & BTN_FIRE) && g->fire_cooldown == 0) {
        int center_x = g->player_x + g->PLAYER.w / 2;
        int center_y = g->player_y - 1;
        fire_player_shots(g, center_x, center_y);
        g->fire_cooldown = compute_fire_cooldown(g);
    }
}

static uint32_t get_player_color(const game_t *g) {
    if (rapid_fire_active(g)) return 0xFFFFA500;
    if (triple_shot_active(g)) return 0xFF0000FF;
    return 0xFF00FF00;
}

void render_player(const game_t *g, lfb_t *lfb) {
    uint32_t player_color = get_player_color(g);
    draw_sprite1r(lfb, &g->PLAYER, g->player_x, g->player_y, player_color);
}

void render_score_display(const game_t *g, lfb_t *lfb) {
    const char *label = "SCORE:";
    int label_scale = 1;
    int label_w = text_width_5x5(label, label_scale);
    int digit_w = 4;
    int digits = digit_count(g->score);
    int score_w = digits * digit_w;
    int right_edge = LW - 5;
    int score_x = right_edge - score_w;
    int label_x = right_edge - label_w - 30;

    int y = LH - 12;
    uint32_t score_color = double_shot_active(g) ? 0xFFFFFF00 : 0xFFFFFFFF;
    l_draw_text(lfb, label_x, y, label, label_scale, score_color);
    l_draw_score(lfb, score_x, y, g->score, score_color);
}

void render_player_health_bar(const game_t *g, lfb_t *lfb) {
    int icon_x = 5;
    int icon_y = LH - g->PLAYER.h - 4;
    uint32_t player_color = get_player_color(g);
    draw_sprite1r(lfb, &g->PLAYER, icon_x, icon_y, player_color);

    int x_text_x = icon_x + g->PLAYER.w + 4;
    int text_y = LH - 12;
    l_draw_text(lfb, x_text_x, text_y, "x", 1, 0xFFFFFFFF);

    int lives_x = x_text_x + text_width_5x5("x", 1) + 4;
    int lives = g->lives;
    if (lives < 0) lives = 0;
    l_draw_score(lfb, lives_x, text_y, lives, 0xFFFFFFFF);

    if (g->pierce_unlocked) {
        int pierce_x = lives_x + digit_count(lives) * 4 + 8;
        int pierce_y = LH - g->SHOP_PIERCE.h - 4;
        draw_sprite1r(lfb, &g->SHOP_PIERCE, pierce_x, pierce_y, 0xFF66CCFF);
    }
}

void render_player_shots(const game_t *g, lfb_t *lfb) {
    for (int s = 0; s < MAX_PSHOTS; s++) {
        if (g->pshot[s].alive) {
            uint32_t bullet_color = rapid_fire_active(g) ? 0xFFFFA500 : 0xFFFFFFFF;
            if (g->pshot[s].pierce_active) {
                for (int i = 0; i < 5; i++) {
                    int py = g->pshot[s].y - i;
                    l_putpix(lfb, g->pshot[s].x, py, 0xFF66CCFF);
                    l_putpix(lfb, g->pshot[s].x - 1, py, 0xFF66CCFF);
                    l_putpix(lfb, g->pshot[s].x + 1, py, 0xFF66CCFF);
                }
            } else {
                for (int i = 0; i < 5; i++) l_putpix(lfb, g->pshot[s].x, g->pshot[s].y - i, bullet_color);
            }
        }
        if (g->pshot_left[s].alive) {
            for (int i = 0; i < 5; i++) l_putpix(lfb, g->pshot_left[s].x - i / 2, g->pshot_left[s].y - i, 0xFF0000FF);
        }
        if (g->pshot_right[s].alive) {
            for (int i = 0; i < 5; i++) l_putpix(lfb, g->pshot_right[s].x + i / 2, g->pshot_right[s].y - i, 0xFF0000FF);
        }
    }
}
