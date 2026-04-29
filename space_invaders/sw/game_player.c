#include "game_player.h"

#include "../hw_contract.h"
#include "font.h"
#include "game_helpers.h"

#define MAGICIAN_SIDE_WALL_THICKNESS 4
#define MAGICIAN_SIDE_DEFLECT_DY -2
#define MAGICIAN_SIDE_DEFLECT_DX 3
static int magician_side_wall_active(const game_t *g) {
	return g->boss_alive && !g->boss_dying && (g->boss_type == BOSS_TYPE_MAGICIAN);
}

static int magician_side_wall_left_bound(void) {
	return MAGICIAN_SIDE_WALL_THICKNESS;
}

static int magician_side_wall_right_bound(void) {
	return LW - MAGICIAN_SIDE_WALL_THICKNESS - 1;
}

static void try_spawn_bullet(game_t *g, bullet_t *shots, int x, int y, int pierce_active, int critical) {
	for (int i = 0; i < MAX_PSHOTS; i++) {
		if (!shots[i].alive) {
			int base_damage = pierce_active ? (g->player_damage + 1) : g->player_damage;
			shots[i].alive = 1;
			shots[i].x = x;
			shots[i].y = y;
			shots[i].prev_x = x;
			shots[i].prev_y = y;
			shots[i].dy = -4;
			shots[i].reflected = 0;
			shots[i].critical = critical ? 1 : 0;
			shots[i].last_hit_r = -1;
			shots[i].last_hit_c = -1;
			shots[i].pierce_active = pierce_active ? 1 : 0;
			// Critical piercing shots carry a doubled damage pool while still piercing.
			shots[i].damage_remaining = (critical ? (base_damage * 2) : base_damage);
			break;
		}
	}
}

static void fire_player_shots(game_t *g, int center_x, int center_y) {
	int triple_active = triple_shot_active(g);
	int center_critical = 0;
	if (g->crit_unlocked) {
		g->shots_fired_counter++;
		if (g->shots_fired_counter >= 10) {
			center_critical = 1;
			g->shots_fired_counter = 0;
		}
	} else {
		g->shots_fired_counter = 0;
	}
	try_spawn_bullet(g, g->pshot, center_x, center_y, g->pierce_unlocked, center_critical);
	if (triple_active) {
		try_spawn_bullet(g, g->pshot_left, center_x - 2, center_y, 0, 0);
		try_spawn_bullet(g, g->pshot_right, center_x + 2, center_y, 0, 0);
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
		g->pshot[i].reflected = 0;
		g->pshot_left[i].reflected = 0;
		g->pshot_right[i].reflected = 0;
		g->pshot[i].dy = 0;
		g->pshot_left[i].dy = 0;
		g->pshot_right[i].dy = 0;
		g->pshot[i].damage_remaining = 0;
		g->pshot_left[i].damage_remaining = 0;
		g->pshot_right[i].damage_remaining = 0;
		g->pshot[i].critical = 0;
		g->pshot_left[i].critical = 0;
		g->pshot_right[i].critical = 0;
	}
}

void update_player_shots(game_t *g) {
	int top_limit = TOP_HUD_SEPARATOR_Y + 1;

	for (int i = 0; i < MAX_PSHOTS; i++) {
		if (g->pshot[i].alive) {
			int dy = (g->pshot[i].dy != 0) ? g->pshot[i].dy : -4;
			g->pshot[i].y += dy;
			if (g->pshot[i].y < top_limit || g->pshot[i].y >= LH) g->pshot[i].alive = 0;
		}
		if (g->pshot_left[i].alive) {
			int dx = -1;
			if (g->pshot_left[i].reflected == 1) dx = 0;
			else if (g->pshot_left[i].reflected == 2) dx = MAGICIAN_SIDE_DEFLECT_DX;
			int dy = (g->pshot_left[i].dy != 0) ? g->pshot_left[i].dy : -4;
			g->pshot_left[i].x += dx;
			g->pshot_left[i].y += dy;

			if (magician_side_wall_active(g) && g->pshot_left[i].reflected == 0) {
				int left_bound = magician_side_wall_left_bound();
				if (g->pshot_left[i].x < left_bound) {
					int overshoot = left_bound - g->pshot_left[i].x;
					g->pshot_left[i].x = left_bound + overshoot;
					g->pshot_left[i].reflected = 2;
					g->pshot_left[i].dy = MAGICIAN_SIDE_DEFLECT_DY;
					continue;
				}
			}

			if (g->pshot_left[i].y < top_limit || g->pshot_left[i].y >= LH || g->pshot_left[i].x < 0 || g->pshot_left[i].x >= LW) g->pshot_left[i].alive = 0;
		}
		if (g->pshot_right[i].alive) {
			int dx = 1;
			if (g->pshot_right[i].reflected == 1) dx = 0;
			else if (g->pshot_right[i].reflected == 3) dx = -MAGICIAN_SIDE_DEFLECT_DX;
			int dy = (g->pshot_right[i].dy != 0) ? g->pshot_right[i].dy : -4;
			g->pshot_right[i].x += dx;
			g->pshot_right[i].y += dy;

			if (magician_side_wall_active(g) && g->pshot_right[i].reflected == 0) {
				int right_bound = magician_side_wall_right_bound();
				if (g->pshot_right[i].x > right_bound) {
					int overshoot = g->pshot_right[i].x - right_bound;
					g->pshot_right[i].x = right_bound - overshoot;
					g->pshot_right[i].reflected = 3;
					g->pshot_right[i].dy = MAGICIAN_SIDE_DEFLECT_DY;
					continue;
				}
			}

			if (g->pshot_right[i].y < top_limit || g->pshot_right[i].y >= LH || g->pshot_right[i].x < 0 || g->pshot_right[i].x >= LW) g->pshot_right[i].alive = 0;
		}
	}
}

void update_player_movement(game_t *g, uint32_t buttons) {
	if (buttons & BTN_LEFT) g->player_x -= g->player_speed;
	if (buttons & BTN_RIGHT) g->player_x += g->player_speed;

	int left_limit = g->tower_wall_active ? g->tower_wall_left : 0;
	int right_limit = g->tower_wall_active ? (g->tower_wall_right - g->PLAYER.w) : (LW - g->PLAYER.w);
	if (magician_side_wall_active(g)) {
		int magician_left = magician_side_wall_left_bound();
		int magician_right = magician_side_wall_right_bound() - g->PLAYER.w + 1;
		if (magician_left > left_limit) left_limit = magician_left;
		if (magician_right < right_limit) right_limit = magician_right;
	}
	if (right_limit < left_limit) right_limit = left_limit;

	if (g->player_x < left_limit) g->player_x = left_limit;
	if (g->player_x > right_limit) g->player_x = right_limit;
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

static uint32_t get_player_color(game_t *g) {
	if (explosive_shot_active(g)) return 0xFFFF0000;
	if (rapid_fire_active(g)) return 0xFFFFA500;
	if (triple_shot_active(g)) return 0xFF0000FF;
	return 0xFF00FF00;
}

static uint32_t powerup_type_color(powerup_type_t type) {
	if (type == POWERUP_DOUBLE_SHOT) return 0xFFFFFF00;
	if (type == POWERUP_TRIPLE_SHOT) return 0xFF0000FF;
	if (type == POWERUP_RAPID_FIRE) return 0xFFFFA500;
	if (type == POWERUP_EXPLOSIVE) return 0xFFFF0000;
	if (type == POWERUP_SHIELD) return 0xFF66CCFF;
	return 0xFFFFFFFF;
}

void render_player(game_t *g, lfb_t *lfb) {
	uint32_t player_color = get_player_color(g);

	if (g->player_prev_x != g->player_x || g->player_prev_y != g->player_y) {
		draw_sprite1r(lfb, &g->PLAYER, g->player_prev_x, g->player_prev_y, 0);
		g->player_prev_x = g->player_x; g->player_prev_y = g->player_y;
	}

	draw_sprite1r(lfb, &g->PLAYER, g->player_x, g->player_y, player_color);
}

void render_score_display(game_t *g, lfb_t *lfb) {
	const char *score_label = "SCORE:";
	const char *power_label = "POWER:";
	int label_scale = 1;
	int digit_w = 4;
	int y = LH - 12;

	int score_label_w = text_width_5x5(score_label, label_scale);
	int score_digits = digit_count(g->score);
	int score_w = score_digits * digit_w;
	int right_edge = LW - 5;
	int score_x = right_edge - score_w;
	int score_label_x = right_edge - score_label_w - 30;

	uint32_t score_color = double_shot_active(g) ? 0xFFFFFF00 : 0xFFFFFFFF;
	l_draw_text(lfb, score_label_x, y, score_label, label_scale, score_color);
	l_draw_score(lfb, score_x, y, g->score, score_color);

	int best_timer = 0;
	powerup_type_t best_type = POWERUP_DOUBLE_SHOT;
	for (int i = 0; i < 5; i++) {
		if (g->powerup_slot_timer[i] > best_timer) {
			best_timer = g->powerup_slot_timer[i];
			best_type = (powerup_type_t)g->powerup_type_slot[i];
		}
	}
	uint32_t power_countdown_color = (best_timer > 0) ? powerup_type_color(best_type) : 0xFFFFFFFF;
	int power_gap = 4;

	int power_label_w = text_width_5x5(power_label, label_scale);
	int none_w = text_width_5x5("NONE", label_scale);
	int power_value_w = none_w;
	int power_total_w = power_label_w + power_gap + power_value_w;
	int power_x = score_label_x - 6 - power_total_w;

	l_draw_text(lfb, power_x, y, power_label, label_scale, 0xFFFFFFFF);
	int value_x = power_x + power_label_w + power_gap;

	if (best_timer <= 0) {
		l_draw_text(lfb, value_x, y, "NONE", label_scale, 0xFFFFFFFF);
		return;
	}

	int seconds_w = 2 * digit_w;

	int tenths_total = (best_timer * 10 + 30) / 60;
	if (tenths_total < 0) tenths_total = 0;
	if (tenths_total > 999) tenths_total = 999;
	int seconds = tenths_total / 10;
	int tenths = tenths_total % 10;
	int tens = seconds / 10;
	int ones = seconds % 10;

	if (tens > 0) {
		l_draw_digit(lfb, value_x, y, tens, 0xFFFFFFFF);
	}
	int ones_x = value_x + digit_w;
	int dot_x = ones_x + digit_w - 1;
	int tenths_x = dot_x + 5;
	l_draw_digit(lfb, ones_x, y, ones, power_countdown_color);
	l_draw_text(lfb, dot_x, y, ".", label_scale, power_countdown_color);
	l_draw_score(lfb, tenths_x, y, tenths, power_countdown_color);
}

void render_player_health_bar(game_t *g, lfb_t *lfb) {
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
	uint32_t lives_color = g->hard_mode ? 0xFFFF0000 : 0xFFFFFFFF;
	l_draw_score(lfb, lives_x, text_y, lives, lives_color);

	int status_x = lives_x + digit_count(lives) * 4 + 8;
	int unlocked_icon_type[3];
	int unlocked_count = 0;
	for (int i = 0; i < g->unlock_order_count && i < 3; i++) {
		unlocked_icon_type[unlocked_count++] = g->unlock_order[i];
	}

	int box_x = status_x;
	int box_y = text_y - 5;
	int box_w = 12;
	int box_h = 12;
	int box_gap = 2;

	for (int i = 0; i < 3; i++) {
		int x0 = box_x + i * (box_w + box_gap);
		uint32_t border = 0xFFFFFFFF;

		for (int x = 0; x < box_w; x++) {
			l_putpix(lfb, x0 + x, box_y, border);
			l_putpix(lfb, x0 + x, box_y + box_h - 1, border);
		}
		for (int y = 0; y < box_h; y++) {
			l_putpix(lfb, x0, box_y + y, border);
			l_putpix(lfb, x0 + box_w - 1, box_y + y, border);
		}

		if (i < unlocked_count) {
			if (unlocked_icon_type[i] == SHOP_ITEM_PIERCE) {
				int px = x0 + (box_w - g->SHOP_PIERCE.w) / 2;
				int py = box_y + (box_h - g->SHOP_PIERCE.h) / 2;
				draw_sprite1r(lfb, &g->SHOP_PIERCE, px, py, 0xFF66CCFF);
			} else if (unlocked_icon_type[i] == SHOP_ITEM_POINTS) {
				int cx = x0 + box_w / 2;
				int cy = box_y + box_h / 2;
				draw_points_upgrade_icon(lfb, cx, cy);
			} else if (unlocked_icon_type[i] == SHOP_ITEM_CRIT) {
				int cx = x0 + box_w / 2;
				int cy = box_y + box_h / 2;
				draw_crit_upgrade_icon(lfb, cx, cy);
			}
		}
	}

}

void render_player_shots(game_t *g, lfb_t *lfb) {
	uint32_t side_bullet_color = 0xFFFFFFFF;
	if (rapid_fire_active(g)) {
		side_bullet_color = 0xFFFFA500;
	} else if (triple_shot_active(g)) {
		side_bullet_color = 0xFF0000FF;
	}

	for (int s = 0; s < MAX_PSHOTS; s++) {
		if (g->pshot[s].alive) {
			if (g->pshot[s].reflected == 1) {
				for (int i = 0; i < 5; i++) l_putpix(lfb, g->pshot[s].x, g->pshot[s].y + i, 0xFF00E5FF);
				continue;
			}
			uint32_t bullet_color = g->pshot[s].critical ? 0xFF00FF00 : (rapid_fire_active(g) ? 0xFFFFA500 : 0xFFFFFFFF);
			if (g->pshot[s].pierce_active) {
				for (int i = 0; i < 5; i++) {
					int py = g->pshot[s].y - i;
					int pyy = g->pshot[s].prev_y - i;

					if (g->pshot[s].prev_x != g->pshot[s].x || g->pshot[s].prev_y != g->pshot[s].y) {
						l_putpix(lfb, g->pshot[s].prev_x, pyy, 0xFF000000);
						l_putpix(lfb, g->pshot[s].prev_x - 1, pyy, 0xFF000000);
						l_putpix(lfb, g->pshot[s].prev_x + 1, pyy, 0xFF000000);
						
						g->pshot[s].prev_x = g->pshot[s].x;
						g->pshot[s].prev_y = g->pshot[s].y;
					}

					l_putpix(lfb, g->pshot[s].x, py, bullet_color);
					l_putpix(lfb, g->pshot[s].x - 1, py, bullet_color);
					l_putpix(lfb, g->pshot[s].x + 1, py, bullet_color);
				}
			} else {
				for (int i = 0; i < 5; i++) {
					int py = g->pshot[s].y - i;
					int pyy = g->pshot[s].prev_y - i;

					if (g->pshot[s].prev_x != g->pshot[s].x || g->pshot[s].prev_y != g->pshot[s].y) {
						l_putpix(lfb, g->pshot[s].prev_x, pyy, 0xFF000000);
						if (g->pshot[s].critical) {
							l_putpix(lfb, g->pshot[s].prev_x - 1, pyy, 0xFF000000);
							l_putpix(lfb, g->pshot[s].prev_x + 1, pyy, 0xFF000000);
						}
						
						g->pshot[s].prev_x = g->pshot[s].x;
						g->pshot[s].prev_y = g->pshot[s].y;
					}

					l_putpix(lfb, g->pshot[s].x, py, bullet_color);
					if (g->pshot[s].critical) {
						l_putpix(lfb, g->pshot[s].x - 1, py, bullet_color);
						l_putpix(lfb, g->pshot[s].x + 1, py, bullet_color);
					}
				}
			}
		}
		if (g->pshot_left[s].alive) {
			if (g->pshot_left[s].reflected == 1) {
				if (g->pshot_left[s].prev_x != g->pshot_left[s].x || g->pshot_left[s].prev_y != g->pshot_left[s].y) {
					for (int i = 0; i < 5; i++) l_putpix(lfb, g->pshot_left[s].prev_x, g->pshot_left[s].prev_y + i, 0xFF000000);

					g->pshot_left[s].prev_x = g->pshot_left[s].x;
					g->pshot_left[s].prev_y = g->pshot_left[s].y;
				}

				for (int i = 0; i < 5; i++) l_putpix(lfb, g->pshot_left[s].x, g->pshot_left[s].y + i, 0xFF00E5FF);
				continue;
			}
			if (g->pshot_left[s].pierce_active) {
				for (int i = 0; i < 5; i++) {
					int py = g->pshot_left[s].y - i;
					int pyy = g->pshot_left[s].prev_y - i;
					if (g->pshot_left[s].prev_x != g->pshot_left[s].x || g->pshot_left[s].prev_y != g->pshot_left[s].y) {
						l_putpix(lfb, g->pshot_left[s].prev_x, pyy, 0xFF000000);
						l_putpix(lfb, g->pshot_left[s].prev_x - 1, pyy, 0xFF000000);
						l_putpix(lfb, g->pshot_left[s].prev_x + 1, pyy, 0xFF000000);

						g->pshot_left[s].prev_x = g->pshot_left[s].x;
						g->pshot_left[s].prev_y = g->pshot_left[s].y;
					}

					l_putpix(lfb, g->pshot_left[s].x, py, 0xFF66CCFF);
					l_putpix(lfb, g->pshot_left[s].x - 1, py, 0xFF66CCFF);
					l_putpix(lfb, g->pshot_left[s].x + 1, py, 0xFF66CCFF);
				}
				continue;
			}
			if (g->pshot_left[s].reflected == 2) {
				for (int i = 0; i < 5; i++) {
					int x = g->pshot_left[s].x + ((3 * i) / 2);
					int xx = g->pshot_left[s].x + ((3 * i) / 2);
					int y = g->pshot_left[s].y - i;
					int yy = g->pshot_left[s].y - i;

					if (g->pshot_left[s].prev_x != g->pshot_left[s].x || g->pshot_left[s].prev_y != g->pshot_left[s].y) {
						l_putpix(lfb, xx, yy, 0xFF000000);

						g->pshot_left[s].prev_x = g->pshot_left[s].x;
						g->pshot_left[s].prev_y = g->pshot_left[s].y;
					}
					l_putpix(lfb, x, y, side_bullet_color);
				}
				continue;
			}

			if (g->pshot_left[s].prev_x != g->pshot_left[s].x || g->pshot_left[s].prev_y != g->pshot_left[s].y) {
				for (int i = 0; i < 5; i++)
					l_putpix(lfb, g->pshot_left[s].prev_x - (i / 4), g->pshot_left[s].prev_y - i, 0xFF000000);

				g->pshot_left[s].prev_x = g->pshot_left[s].x;
				g->pshot_left[s].prev_y = g->pshot_left[s].y;
			}

			for (int i = 0; i < 5; i++) l_putpix(lfb, g->pshot_left[s].x - (i / 4), g->pshot_left[s].y - i, side_bullet_color);
		}
		if (g->pshot_right[s].alive) {
			if (g->pshot_right[s].reflected == 1) {
				if (g->pshot_right[s].prev_x != g->pshot_right[s].x || g->pshot_right[s].prev_y != g->pshot_right[s].y) {
					for (int i = 0; i < 5; i++)
						l_putpix(lfb, g->pshot_right[s].prev_x, g->pshot_right[s].prev_y + i, 0xFF000000);

					g->pshot_right[s].prev_x = g->pshot_right[s].x;
					g->pshot_right[s].prev_y = g->pshot_right[s].y;
				}

				for (int i = 0; i < 5; i++) l_putpix(lfb, g->pshot_right[s].x, g->pshot_right[s].y + i, 0xFF00E5FF);
				continue;
			}
			if (g->pshot_right[s].pierce_active) {
				for (int i = 0; i < 5; i++) {
					int py = g->pshot_right[s].y - i;
					int pyy = g->pshot_right[s].prev_y - i;

					if (g->pshot_right[s].prev_x != g->pshot_right[s].x || g->pshot_right[s].prev_y != g->pshot_right[s].y) {
						l_putpix(lfb, g->pshot_right[s].prev_x, pyy, 0xFF000000);
						l_putpix(lfb, g->pshot_right[s].prev_x - 1, pyy, 0xFF000000);
						l_putpix(lfb, g->pshot_right[s].prev_x + 1, pyy, 0xFF000000);

						g->pshot_right[s].prev_x = g->pshot_right[s].x;
						g->pshot_right[s].prev_y = g->pshot_right[s].y;
					}

					l_putpix(lfb, g->pshot_right[s].x, py, 0xFF66CCFF);
					l_putpix(lfb, g->pshot_right[s].x - 1, py, 0xFF66CCFF);
					l_putpix(lfb, g->pshot_right[s].x + 1, py, 0xFF66CCFF);
				}
				continue;
			}
			if (g->pshot_right[s].reflected == 3) {
				for (int i = 0; i < 5; i++) {
					int x = g->pshot_right[s].x - ((3 * i) / 2);
					int xx = g->pshot_right[s].prev_x - ((3 * i) / 2);
					int y = g->pshot_right[s].y - i;
					int yy = g->pshot_right[s].prev_y - i;

					if (g->pshot_right[s].prev_x != g->pshot_right[s].x || g->pshot_right[s].prev_y != g->pshot_right[s].y) {
						l_putpix(lfb, xx, yy, 0xFF000000);

						g->pshot_right[s].prev_x = g->pshot_right[s].x;
						g->pshot_right[s].prev_y = g->pshot_right[s].y;
					}

					l_putpix(lfb, x, y, side_bullet_color);
				}
				continue;
			}

			if (g->pshot_right[s].prev_x != g->pshot_right[s].x || g->pshot_right[s].prev_y != g->pshot_right[s].y) {
				for (int i = 0; i < 5; i++) l_putpix(lfb, g->pshot_right[s].prev_x + (i / 4), g->pshot_right[s].prev_y - i, 0xFF000000);

				g->pshot_right[s].prev_x = g->pshot_right[s].x;
				g->pshot_right[s].prev_y = g->pshot_right[s].y;
			}

			for (int i = 0; i < 5; i++) l_putpix(lfb, g->pshot_right[s].x + (i / 4), g->pshot_right[s].y - i, side_bullet_color);
		}
	}
}
