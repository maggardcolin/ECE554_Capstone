#pragma once

#include <stdint.h>
#include "game.h"

int digit_count(uint32_t v);
void draw_filled_circle(lfb_t *lfb, int x0, int y0, int r, uint32_t color);
void draw_bar(lfb_t *lfb, int x, int y, int w, int h, int fill_w, uint32_t fill_color);
int text_width_5x5(const char *text, int scale);
uint32_t color_with_intensity(uint32_t color, int intensity_pct);
void render_fps_counter(lfb_t *lfb);

int exit_sign_visible(const game_t *g);
void draw_powerup_icon(lfb_t *lfb, int x0, int y0, powerup_type_t type);
void draw_points_upgrade_icon(lfb_t *lfb, int x0, int y0);
int is_powerup_active(const game_t *g, powerup_type_t type);
int double_shot_active(const game_t *g);
int triple_shot_active(const game_t *g);
int rapid_fire_active(const game_t *g);
int explosive_shot_active(const game_t *g);

int boss_shield_x(const game_t *g);
int boss_shield_y(const game_t *g);
const sprite1r_t *boss_sprite_for_frame(const game_t *g, int frame);
void render_explosion_points(lfb_t *lfb, int cx, int cy, int points);
int circle_intersects_rect(int cx, int cy, int r, int rx, int ry, int rw, int rh);
int boss_explosion_radius(const game_t *g);
int alien_explosion_radius(int timer);
