#include "game_player.h"

#include "../hw_contract.h"
#include "font.h"
#include "game_helpers.h"

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

void clear_player_shots(game_t *g) {
    for (int i = 0; i < MAX_PSHOTS; i++) {
        g->pshot[i].alive = 0;
        g->pshot_left[i].alive = 0;
        g->pshot_right[i].alive = 0;
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

    uint32_t life_color = 0xFF00FF00;
    if (health_pct <= 33) life_color = 0xFFFF0000;
    else if (health_pct <= 67) life_color = 0xFFFFFF00;

    int fill_w = (max_lives > 0) ? (g->lives * bar_w) / max_lives : 0;
    draw_bar(lfb, lx, bar_y, bar_w, bar_h, fill_w, life_color);
}

void render_player_shots(const game_t *g, lfb_t *lfb) {
    for (int s = 0; s < MAX_PSHOTS; s++) {
        if (g->pshot[s].alive) {
            uint32_t bullet_color = rapid_fire_active(g) ? 0xFFFFA500 : 0xFFFFFFFF;
            for (int i = 0; i < 5; i++) l_putpix(lfb, g->pshot[s].x, g->pshot[s].y - i, bullet_color);
        }
        if (g->pshot_left[s].alive) {
            for (int i = 0; i < 5; i++) l_putpix(lfb, g->pshot_left[s].x - i / 2, g->pshot_left[s].y - i, 0xFF0000FF);
        }
        if (g->pshot_right[s].alive) {
            for (int i = 0; i < 5; i++) l_putpix(lfb, g->pshot_right[s].x + i / 2, g->pshot_right[s].y - i, 0xFF0000FF);
        }
    }
}
