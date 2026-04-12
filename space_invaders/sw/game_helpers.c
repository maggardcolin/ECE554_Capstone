#include "game_helpers.h"

#include "font.h"
#include <stdio.h>
#include <time.h>

int digit_count(uint32_t v)
{
    int d = 1;
    while (v >= 10) {
        v /= 10;
        d++;
    }
    return d;
}

void draw_filled_circle(lfb_t *lfb, int x0, int y0, int r, uint32_t color) {
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x * x + y * y <= r * r) l_putpix(lfb, x0 + x, y0 + y, color);
        }
    }
}

void draw_bar(lfb_t *lfb, int x, int y, int w, int h, int fill_w, uint32_t fill_color) {
    if (fill_w < 0) fill_w = 0;
    if (fill_w > w) fill_w = w;

    for (int i = 0; i <= w; i++) {
        l_putpix(lfb, x + i, y - 1, 0xFFFFFFFF);
        l_putpix(lfb, x + i, y + h, 0xFFFFFFFF);
    }
    for (int j = 0; j <= h; j++) {
        l_putpix(lfb, x - 1, y + j, 0xFFFFFFFF);
        l_putpix(lfb, x + w, y + j, 0xFFFFFFFF);
    }

    l_putpix(lfb, x - 1, y - 1, 0xFFFFFFFF);

    for (int i = 0; i < fill_w; i++) {
        for (int j = 0; j < h; j++) {
            l_putpix(lfb, x + i, y + j, fill_color);
        }
    }
}

int text_width_5x5(const char *text, int scale) {
    if (scale < 1) scale = 1;
    int count = 0;
    for (const char *p = text; *p; p++) count++;
    if (count == 0) return 0;
    return (count * 6 - 1) * scale;
}

uint32_t color_with_intensity(uint32_t color, int intensity_pct) {
    if (intensity_pct < 0) intensity_pct = 0;
    if (intensity_pct > 100) intensity_pct = 100;

    uint32_t a = color & 0xFF000000u;
    uint32_t r = (color >> 16) & 0xFFu;
    uint32_t g = (color >> 8) & 0xFFu;
    uint32_t b = color & 0xFFu;

    r = (r * (uint32_t)intensity_pct) / 100u;
    g = (g * (uint32_t)intensity_pct) / 100u;
    b = (b * (uint32_t)intensity_pct) / 100u;

    return a | (r << 16) | (g << 8) | b;
}

void render_fps_counter(lfb_t *lfb) {
    static struct timespec last_ts = {0, 0};
    static int frame_count = 0;
    static int fps = 0;

    struct timespec now;
    timespec_get(&now, TIME_UTC);
    frame_count++;

    if (last_ts.tv_sec == 0 && last_ts.tv_nsec == 0) {
        last_ts = now;
    }

    double elapsed = (double)(now.tv_sec - last_ts.tv_sec) +
                     (double)(now.tv_nsec - last_ts.tv_nsec) / 1000000000.0;
    if (elapsed >= 1.0) {
        fps = (int)(frame_count / elapsed);
        frame_count = 0;
        last_ts = now;
    }

    char fps_text[16];
    snprintf(fps_text, sizeof(fps_text), "FPS:%d", fps);
    int x = LW - text_width_5x5(fps_text, 1) - 80;
    int y = LH - 12;
    l_draw_text(lfb, x, y, fps_text, 1, 0xFFFFFFFF);
}

int exit_sign_visible(const game_t *g) {
    if (!g->exit_available) return 0;
    if (g->exit_blink_toggles_remaining <= 0) return 1;
    return (g->exit_blink_toggles_remaining % 2) == 0;
}

void draw_powerup_icon(lfb_t *lfb, int x0, int y0, powerup_type_t type) {
    if (type == POWERUP_DOUBLE_SHOT) {
        int r = 6;
        draw_filled_circle(lfb, x0, y0, r, 0xFFFFFF00);
        l_draw_text(lfb, x0 - 4, y0 - 3, "2X", 1, 0xFF000000);
    } else if (type == POWERUP_SHIELD) {
        int r = 6;
        draw_filled_circle(lfb, x0, y0, r, 0xFF1E6AD6);
        for (int y = -r; y <= r; y++) {
            for (int x = -r; x <= r; x++) {
                int d = x * x + y * y;
                if (d <= (r * r) && d >= ((r - 1) * (r - 1))) {
                    l_putpix(lfb, x0 + x, y0 + y, 0xFF66CCFF);
                }
            }
        }
    } else if (type == POWERUP_TRIPLE_SHOT) {
        int r = 6;
        draw_filled_circle(lfb, x0, y0, r, 0xFF0000FF);
        for (int i = -4; i <= 4; i++) l_putpix(lfb, x0, y0 + i, 0xFFFFFFFF);
    } else if (type == POWERUP_EXPLOSIVE) {
        int r = 6;
        draw_filled_circle(lfb, x0, y0, r, 0xFFFF0000);
        l_putpix(lfb, x0, y0, 0xFFFFFF00);
        l_putpix(lfb, x0 - 2, y0, 0xFFFFFF00);
        l_putpix(lfb, x0 + 2, y0, 0xFFFFFF00);
        l_putpix(lfb, x0, y0 - 2, 0xFFFFFF00);
        l_putpix(lfb, x0, y0 + 2, 0xFFFFFF00);
    } else {
        int r = 6;
        draw_filled_circle(lfb, x0, y0, r, 0xFFFA500);
        for (int i = -3; i <= 3; i++) l_putpix(lfb, x0, y0 + i, 0xFFFFFFFF);
        l_putpix(lfb, x0, y0 - 4, 0xFFFFFFFF);
    }
}

void draw_points_upgrade_icon(lfb_t *lfb, int x0, int y0) {
    // Compact single-digit glyph without the circular backdrop.
    l_draw_text(lfb, x0 - 1, y0 - 2, "2", 1, 0xFFFFFF00);
}

int is_powerup_active(const game_t *g, powerup_type_t type) {
    for (int i = 0; i < 5; i++) {
        if (g->powerup_slot_timer[i] > 0 && g->powerup_type_slot[i] == type) return 1;
    }
    return 0;
}

int double_shot_active(const game_t *g) {
    return g->points_unlocked || is_powerup_active(g, POWERUP_DOUBLE_SHOT);
}

int triple_shot_active(const game_t *g) {
    return is_powerup_active(g, POWERUP_TRIPLE_SHOT);
}

int rapid_fire_active(const game_t *g) {
    return is_powerup_active(g, POWERUP_RAPID_FIRE);
}

int explosive_shot_active(const game_t *g) {
    return is_powerup_active(g, POWERUP_EXPLOSIVE);
}

int shield_power_active(const game_t *g) {
    return is_powerup_active(g, POWERUP_SHIELD);
}

int boss_shield_x(const game_t *g) {
    return g->boss_x;
}

int boss_shield_y(const game_t *g) {
    return g->boss_y + g->BOSS_A.h + 2;
}

const sprite1r_t *boss_sprite_for_frame(const game_t *g, int frame) {
    if (g->boss_type == BOSS_TYPE_BLUE) {
        return frame ? &g->BOSS2_B : &g->BOSS2_A;
    }
    if (g->boss_type == BOSS_TYPE_TOWER) {
        return frame ? &g->BOSS3_B : &g->BOSS3_A;
    }
    if (g->boss_type == BOSS_TYPE_HERMIT) {
        return frame ? &g->BOSS4_B : &g->BOSS4_A;
    }
    return frame ? &g->BOSS_B : &g->BOSS_A;
}

void render_explosion_points(lfb_t *lfb, int cx, int cy, int points) {
    if (points <= 0) return;

    char points_text[16];
    snprintf(points_text, sizeof(points_text), "%d", points);
    int text_w = text_width_5x5(points_text, 1);
    l_draw_text(lfb, cx - (text_w / 2), cy - 3, points_text, 1, 0xFFFFFFFF);
}

int circle_intersects_rect(int cx, int cy, int r, int rx, int ry, int rw, int rh) {
    int closest_x = cx;
    int closest_y = cy;

    if (closest_x < rx) closest_x = rx;
    else if (closest_x > rx + rw) closest_x = rx + rw;

    if (closest_y < ry) closest_y = ry;
    else if (closest_y > ry + rh) closest_y = ry + rh;

    int dx = cx - closest_x;
    int dy = cy - closest_y;
    return (dx * dx + dy * dy) <= (r * r);
}

int boss_explosion_radius(const game_t *g) {
    int age = 60 - g->boss_death_timer;
    int frame = age / 10;
    int base_r = 6 + frame * 3;
    return base_r + 4;
}

int alien_explosion_radius(int timer) {
    int age = 18 - timer;
    int base_r = 2 + (age / 3);
    return base_r + 2;
}
