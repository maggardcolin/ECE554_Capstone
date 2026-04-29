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
#include <time.h>

#define EXIT_BLINK_INTERVAL_FRAMES 8
#define EXIT_BLINK_TOGGLES 6
#define PLAYER_EXIT_ASCEND_SPEED 2
#define PLAYER_DEATH_DELAY_FRAMES 60
#define PLAYER_IFRAMES 60
#define CRITICAL_HIT_TEXT_FRAMES 120
#define PLAYER_SHIELD_RADIUS 8
#define ALIEN_EXPLOSION_FRAMES 18
#define BOSS_BOMB_EXPLOSION_FRAMES 60
#define BOSS_BOMB_SPEED 2
#define BOSS_BOMB_HITS_TO_REVERSE 2
#define BOSS_INTRO_FRAMES 225
#define YELLOW_BOSS_ALIENS 5
#define YELLOW_BOSS_ATTACK_SHUFFLE 3
#define TOWER_WALL_ATTACK 4
#define HERMIT_DODGE_ATTACK 5
#define CHARIOT_CHARGE_ATTACK 6
#define MAGICIAN_MIRROR_ATTACK 7
#define CHARIOT_ARC_SHOT_COUNT 9
#define CHARIOT_ARC_MIDDLE_SHOT_COUNT 5
#define CHARIOT_ARC_SHOT_DELAY 8
#define MAGICIAN_ORB_SPEED_X 2
#define MAGICIAN_ORB_SPEED_Y 1
#define MAGICIAN_ORB_HIT_COUNT 3
#define MAGICIAN_ORB_FIRE_PERIOD_MULT 2
#define MAGICIAN_ORB_MOVE_FRAME_DIV 1
#define MAGICIAN_ORB_RADIUS 11
#define MAGICIAN_MIRROR_WIDTH_BOSS 56
#define MAGICIAN_MIRROR_WIDTH_ALIEN 30
#define MAGICIAN_SIDE_WALL_THICKNESS 4
#define MAGICIAN_REFLECT_TAN30_NUM 577
#define MAGICIAN_REFLECT_TAN30_DEN 1000
#define MAGICIAN_MIRROR_NOTCH_HALF_BOSS 9
#define MAGICIAN_MIRROR_ALIEN_OFFSET 3
#define MAGICIAN_MIRROR_BOSS_OFFSET 8
#define MAGICIAN_MIRROR_DURATION 180
#define MAGICIAN_CURSE_TEXT_DURATION_FRAMES 300
#define CHARIOT_CHARGE_SPEED_Y 4
#define CHARIOT_CHARGE_SPEED_X 3
#define CHARIOT_CHARGE_EXPLOSION_FRAMES 30
#define CHARIOT_CHARGE_EXPLOSION_RADIUS 20
#define HERMIT_LIGHTNING_SPEED 1.5f
#define HERMIT_LIGHTNING_YELLOW_FRAMES 30
#define HERMIT_LIGHTNING_MAX_TRAIL 42.0f
#define HERMIT_LIGHTNING_TAIL_SPEED 24.0f
#define HERMIT_LIGHTNING_FADE_SPEED 36.0f
#define TOWER_ASTEROID_RADIUS 7
#define TOWER_ASTEROID_EXPLOSION_FRAMES 28
#define TOWER_WALL_WIDTH 22
#define TOWER_WALL_STEP 6
#define TOWER_WALL_MIN_GAP 0
#define TOWER_WALL_DURATION_FRAMES 150
#define WIN_LEVEL 8
#define WIN_RING_ALIENS 6
#define WIN_EXPLOSION_DELAY_FRAMES 10
#define OVERWORLD_LEVEL_COUNT 10
#define OVERWORLD_FLY_FRAMES 120
#define OVERWORLD_HOLD_FRAMES 36
#define OVERWORLD_PIXELATE_FRAMES 36
#define PRACTICE_LEVEL_MIN 1
#define PRACTICE_LEVEL_MAX (WIN_LEVEL - 1)
#define PRACTICE_EXIT_INDEX BOSS_TYPE_COUNT
#define PRACTICE_MENU_COUNT (BOSS_TYPE_COUNT + 1)
#define DIFFICULTY_MENU_COUNT 4
#define MAIN_MENU_COUNT 3
#define TUTORIAL_POWERUP_COUNT 4
#define TUTORIAL_LEAVE_ROW 1
#define TUTORIAL_LEAVE_COL 0
#define TUTORIAL_SHOOTER_ROW 1
#define TUTORIAL_SHOOTER_COL 1
#define TUTORIAL_CENTER_ROW_START 0
#define TUTORIAL_CENTER_ROW_END 3
#define TUTORIAL_CENTER_COL_START 3
#define TUTORIAL_CENTER_COL_END 7
#define TUTORIAL_RESPAWN_PERIOD_FRAMES 300
#define CREDITS_SCROLL_SPEED 1
#define CREDITS_LINE_SPACING 12
#define CREDITS_ALIEN_GAP 24
#define OVERWORLD_TOTAL_FRAMES (OVERWORLD_FLY_FRAMES + OVERWORLD_HOLD_FRAMES + OVERWORLD_PIXELATE_FRAMES)

int do_clear = 0;

static const sprite1r_t *active_boss_sprite(const game_t *g) {
	return boss_sprite_for_frame(g, g->boss_frame);
}

static const char *boss_intro_type_text(const game_t *g) {
	if (g->boss_type == BOSS_TYPE_BLUE) return "MOON";
	if (g->boss_type == BOSS_TYPE_YELLOW) return "STAR";
	if (g->boss_type == BOSS_TYPE_TOWER) return "TOWER";
	if (g->boss_type == BOSS_TYPE_HERMIT) return "HERMIT";
	if (g->boss_type == BOSS_TYPE_CHARIOT) return "CHARIOT";
	if (g->boss_type == BOSS_TYPE_MAGICIAN) return "MAGICIAN";
	return "EMPEROR";
}

static const char *boss_menu_label(int boss_type) {
	if (boss_type == BOSS_TYPE_BLUE) return "MOON";
	if (boss_type == BOSS_TYPE_YELLOW) return "STAR";
	if (boss_type == BOSS_TYPE_TOWER) return "TOWER";
	if (boss_type == BOSS_TYPE_HERMIT) return "HERMIT";
	if (boss_type == BOSS_TYPE_CHARIOT) return "CHARIOT";
	if (boss_type == BOSS_TYPE_MAGICIAN) return "MAGICIAN";
	return "EMPEROR";
}

static int practice_menu_boss_type_for_index(int menu_index) {
	static const int order[BOSS_TYPE_COUNT] = {
		BOSS_TYPE_MAGICIAN, // I
		BOSS_TYPE_CLASSIC,  // IV (Emperor)
		BOSS_TYPE_CHARIOT,  // VII
		BOSS_TYPE_HERMIT,   // IX
		BOSS_TYPE_TOWER,    // XVI
		BOSS_TYPE_YELLOW,   // XVII (Star)
		BOSS_TYPE_BLUE      // XVIII (Moon)
	};
	if (menu_index < 0 || menu_index >= BOSS_TYPE_COUNT) return BOSS_TYPE_CLASSIC;
	return order[menu_index];
}

static uint32_t practice_menu_boss_color(int boss_type) {
	if (boss_type == BOSS_TYPE_BLUE) return 0xFF3399FF;
	if (boss_type == BOSS_TYPE_YELLOW) return 0xFFFFFF00;
	if (boss_type == BOSS_TYPE_TOWER) return 0xFF8B5A2B;
	if (boss_type == BOSS_TYPE_HERMIT) return 0xFFB266FF;
	if (boss_type == BOSS_TYPE_CHARIOT) return 0xFFFF8C00;
	if (boss_type == BOSS_TYPE_MAGICIAN) return 0xFF00E5FF;
	return 0xFF00FF00;
}

static uint32_t magician_color_for_special_state(int special_active) {
	return special_active ? 0xFF00E5FF : 0xFFA8A8A8;
}

static const char *boss_intro_main_attack_text(const game_t *g) {
	if (g->boss_type == BOSS_TYPE_BLUE) return "TRIPLE SHOT";
	if (g->boss_type == BOSS_TYPE_YELLOW) return "STANDARD LASER";
	if (g->boss_type == BOSS_TYPE_TOWER) return "ASTEROID";
	if (g->boss_type == BOSS_TYPE_HERMIT) return "LIGHTNING";
	if (g->boss_type == BOSS_TYPE_CHARIOT) return "ARC";
	if (g->boss_type == BOSS_TYPE_MAGICIAN) return "ORB";
	return "STANDARD LASER";
}

static int blue_black_hole_enabled(const game_t *g) {
	return (g->boss_type == BOSS_TYPE_BLUE) && (g->level >= 3);
}

static int any_powerup_active(const game_t *g) {
	for (int i = 0; i < 5; i++) {
		if (g->powerup_slot_timer[i] > 0) return 1;
	}
	return 0;
}

static int tutorial_is_leave_alien(int r, int c) {
	return r == TUTORIAL_LEAVE_ROW && c == TUTORIAL_LEAVE_COL;
}

static int tutorial_is_shooter_alien(int r, int c) {
	return r == TUTORIAL_SHOOTER_ROW && c == TUTORIAL_SHOOTER_COL;
}

static int tutorial_is_center_pack_alien(int r, int c) {
	if (r < TUTORIAL_CENTER_ROW_START || r > TUTORIAL_CENTER_ROW_END) return 0;
	if (c < TUTORIAL_CENTER_COL_START || c > TUTORIAL_CENTER_COL_END) return 0;
	return 1;
}

static powerup_type_t tutorial_powerup_type_for_index(int index) {
	static const powerup_type_t tutorial_types[TUTORIAL_POWERUP_COUNT] = {
		POWERUP_TRIPLE_SHOT,
		POWERUP_RAPID_FIRE,
		POWERUP_EXPLOSIVE,
		POWERUP_SHIELD
	};

	if (index < 0 || index >= TUTORIAL_POWERUP_COUNT) return POWERUP_TRIPLE_SHOT;
	return tutorial_types[index];
}

static void tutorial_powerup_icon_position(int index, int *x, int *y) {
	int right_margin = 14;
	int spacing = 22;
	int y_fixed = TOP_HUD_SEPARATOR_Y + 18;
	if (y_fixed > BOTTOM_HUD_SEPARATOR_Y - 12) y_fixed = BOTTOM_HUD_SEPARATOR_Y - 12;

	int start_x = LW - right_margin - (TUTORIAL_POWERUP_COUNT - 1) * spacing;
	*x = start_x + index * spacing;
	*y = y_fixed;
}

static const char *boss_intro_special_attack_text(const game_t *g) {
	if (g->boss_type == BOSS_TYPE_BLUE) return blue_black_hole_enabled(g) ? "BLACK HOLE" : "BOMB";
	if (g->boss_type == BOSS_TYPE_YELLOW) return "SHUFFLE";
	if (g->boss_type == BOSS_TYPE_TOWER) return "WALLS";
	if (g->boss_type == BOSS_TYPE_HERMIT) return "SUMMON";
	if (g->boss_type == BOSS_TYPE_CHARIOT) return "CHARGE";
	if (g->boss_type == BOSS_TYPE_MAGICIAN) return "MIRROR WALL";
	if (g->level >= 3) return "PLASMA HEAL";
	return "PLASMA";
}

static const char *boss_intro_desc_line1(const game_t *g) {
	if (g->boss_type == BOSS_TYPE_BLUE) {
		return (g->level >= 3) ? "SHOOTS A BLACK HOLE WHICH PULLS YOU IN." :
			"FIRES WIDE TRIPLE SHOTS.";
	}
	if (g->boss_type == BOSS_TYPE_YELLOW) return "CONTROLS REGULAR ALIENS.";
	if (g->boss_type == BOSS_TYPE_TOWER) return "CONSTRAINS THE ARENA AND THROWS ASTEROIDS.";
	if (g->boss_type == BOSS_TYPE_HERMIT) {
		return (g->level >= 3) ? "HOMING LIGHTNING AND DODGING." :
			"HOMING LIGHTNING ATTACKS.";
	}
	if (g->boss_type == BOSS_TYPE_CHARIOT) return "SHOOTS IN AN ARC.";
	if (g->boss_type == BOSS_TYPE_MAGICIAN) return "FIRES BOUNCING ORBS AT ANGLES.";
	return (g->level >= 3) ? "EMPEROR ENTERS PHASE TWO PLASMA PATTERNS." :
		"FIGHTS WITH HEAVY PLASMA FIRE.";
}

static const char *boss_intro_desc_line2(const game_t *g) {
	if (g->boss_type == BOSS_TYPE_BLUE) {
		return (g->level >= 3) ? "GETS STRONGER ONCE HALF OF THE ALIENS ARE GONE." :
			"SHOOTING THE BOMB REVERSES ITS COURSE.";
	}
	if (g->boss_type == BOSS_TYPE_YELLOW) return "BEST TO TAKE OUT COLUMNS.";
	if (g->boss_type == BOSS_TYPE_TOWER) return "ASTEROID EXPLOSIONS HURT IT.";
	if (g->boss_type == BOSS_TYPE_HERMIT) {
		return (g->level >= 3) ? "TRY TO BREAK ITS SHIELD. SUMMONS ALIENS." :
			"JUMPS AROUND A BIT. SUMMONS ALIENS.";
	}
	if (g->boss_type == BOSS_TYPE_CHARIOT) return "RUSHES TOWARDS THE PLAYER.";
	if (g->boss_type == BOSS_TYPE_MAGICIAN) return "MIRROR WALLS REFLECT SHOTS.";
	return (g->level >= 3) ? "GREEN PLASMA NOW HEALS DURING THE FIGHT." :
		"DESCENDS ONCE HALF THE ALIENS ARE GONE.";
}

static int magician_side_wall_active(const game_t *g) {
	return g->boss_alive && !g->boss_dying && (g->boss_type == BOSS_TYPE_MAGICIAN);
}

static int magician_side_wall_thickness(const game_t *g) {
	if (!magician_side_wall_active(g)) return 0;
	int thickness = MAGICIAN_SIDE_WALL_THICKNESS;
	int max_thickness = (LW / 2) - 1;
	if (max_thickness < 1) max_thickness = 1;
	if (thickness > max_thickness) thickness = max_thickness;
	return thickness;
}

static int magician_inward_dx_from_vertical_step(int vertical_step) {
	if (vertical_step < 0) vertical_step = -vertical_step;
	int dx = (vertical_step * MAGICIAN_REFLECT_TAN30_NUM + (MAGICIAN_REFLECT_TAN30_DEN / 2)) /
		MAGICIAN_REFLECT_TAN30_DEN;
	if (dx < 1) dx = 1;
	return dx;
}

static void reflect_x_from_magician_side_walls(const game_t *g, int *x, int *dx, int vertical_step) {
	int wall = magician_side_wall_thickness(g);
	if (wall <= 0) return;

	int left_bound = wall;
	int right_bound = LW - wall - 1;
	if (right_bound < left_bound) right_bound = left_bound;
	int inward_dx = magician_inward_dx_from_vertical_step(vertical_step);

	if (*x < left_bound) {
		int overshoot = left_bound - *x;
		*x = left_bound + overshoot;
		*dx = inward_dx;
	} else if (*x > right_bound) {
		int overshoot = *x - right_bound;
		*x = right_bound - overshoot;
		*dx = -inward_dx;
	}

	if (*x < left_bound) *x = left_bound;
	if (*x > right_bound) *x = right_bound;
}

static int movement_left_bound(const game_t *g) {
	int left = g->tower_wall_active ? g->tower_wall_left : 0;
	int mirror_wall = magician_side_wall_thickness(g);
	if (mirror_wall > left) left = mirror_wall;
	return left;
}

static int movement_right_bound(const game_t *g) {
	int right = g->tower_wall_active ? g->tower_wall_right : LW;
	int mirror_wall = magician_side_wall_thickness(g);
	int mirror_right = LW - mirror_wall;
	if (mirror_wall > 0 && mirror_right < right) right = mirror_right;
	return right;
}

static int tower_walls_closed(const game_t *g) {
	return g->tower_wall_active && (g->tower_wall_right <= g->tower_wall_left);
}

static int tower_wall_path_clear(const game_t *g, int proposed_left, int proposed_right) {
	const sprite1r_t *BS = active_boss_sprite(g);
	int boss_left = g->boss_x;
	int boss_right = g->boss_x + BS->w;
	if (boss_left < proposed_left || boss_right > proposed_right) {
		return 0;
	}

	const sprite1r_t *AS = g->alien_frame ? &g->ALIEN_B : &g->ALIEN_A;
	int spacing_x = 6, spacing_y = 5;
	for (int r = 0; r < AROWS; r++) {
		for (int c = 0; c < ACOLS; c++) {
			if (!g->alien_alive[r][c]) continue;
			int ax = g->alien_origin_x + c * (AS->w + spacing_x);
			int ay = g->alien_origin_y + r * (AS->h + spacing_y);
			(void)ay;
			if (ax < proposed_left || (ax + AS->w) > proposed_right) {
				return 0;
			}
		}
	}

	return 1;
}

static int tower_asteroid_explosion_radius(const game_t *g, int ai) {
	int age = TOWER_ASTEROID_EXPLOSION_FRAMES - g->tower_asteroid_explode_timer[ai];
	int radius = TOWER_ASTEROID_RADIUS + (age / 2);
	int max_radius = TOWER_ASTEROID_RADIUS * 2 + 3;
	if (radius > max_radius) radius = max_radius;
	return radius;
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
		if (g->practice_run_active) {
			g->boss_alive = 0;
			g->boss_power_active = 0;
			g->boss_power_timer = 0;
			g->boss_power_cooldown = 0;
			g->practice_return_delay_timer = 60;
			for (int i = 0; i < YELLOW_BOSS_ALIENS; i++) {
				g->yellow_beam_shot[i].alive = 0;
			}
		} else {
			g->boss_alive = 0;
			g->boss_power_active = 0;
			g->boss_power_timer = 0;
			g->boss_power_cooldown = 0;
			for (int i = 0; i < YELLOW_BOSS_ALIENS; i++) {
				g->yellow_beam_shot[i].alive = 0;
			}
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
	for (int i = 0; i < CHARIOT_ARC_SHOT_COUNT; i++) {
		g->boss_arc_shot[i].alive = 0;
		g->boss_arc_shot_dx[i] = 0;
	}
	for (int i = 0; i < YELLOW_BOSS_ALIENS; i++) {
		g->yellow_beam_shot[i].alive = 0;
	}
	for (int i = 0; i < MAX_TOWER_ASTEROIDS; i++) {
		g->tower_asteroid[i].alive = 0;
		g->tower_asteroid_spin[i] = 0;
		g->tower_asteroid_hp[i] = 0;
		g->tower_asteroid_dx[i] = 0;
		g->tower_asteroid_exploding[i] = 0;
		g->tower_asteroid_explode_timer[i] = 0;
		g->tower_asteroid_boss_damage_applied[i] = 0;
	}
	g->boss_bomb.alive = 0;
	g->boss_bomb.dy = BOSS_BOMB_SPEED;
	g->boss_bomb.exploding = 0;
	g->boss_bomb.explode_timer = 0;
	g->boss_bomb.hit_player = 0;
	g->boss_bomb.hits_taken = 0;
	g->boss_bomb.reversed = 0;
	g->boss_bomb.reverse_damage_applied = 0;
	g->boss_special_x = 0;
	g->boss_special_y = 0;
	g->boss_special_timer = 0;
	g->boss_special_hit_applied = 0;
	g->chariot_arc_active = 0;
	g->chariot_arc_next_index = 0;
	g->chariot_arc_step = 1;
	g->chariot_arc_delay = 0;
	g->chariot_charge_damage = 0;
	g->chariot_charge_target_x = 0;
	g->tower_wall_active = 0;
	g->tower_wall_timer = 0;
	g->tower_wall_left = 0;
	g->tower_wall_right = LW;
	g->magician_mirror.active = 0;
	g->magician_mirror.follow_boss = 1;
	g->magician_mirror.alien_r = -1;
	g->magician_mirror.alien_c = -1;
	g->magician_mirror.x = 0;
	g->magician_mirror.y = 0;
	g->magician_mirror.width = MAGICIAN_MIRROR_WIDTH_BOSS;
	g->magician_mirror.timer = 0;
	g->magician_mirror_alt.active = 0;
	g->magician_mirror_alt.follow_boss = 1;
	g->magician_mirror_alt.alien_r = -1;
	g->magician_mirror_alt.alien_c = -1;
	g->magician_mirror_alt.x = 0;
	g->magician_mirror_alt.y = 0;
	g->magician_mirror_alt.width = MAGICIAN_MIRROR_WIDTH_BOSS;
	g->magician_mirror_alt.timer = 0;
	g->magician_wave.active = 0;
	g->magician_wave.x = 0;
	g->magician_wave.y = 0;
	g->magician_wave.vx = 0;
	g->magician_wave.vy = 0;
	g->magician_wave.dir = 1;
	g->magician_wave.hp = 0;
	for (int i = 0; i < HERMIT_MAX_LIGHTNINGS; i++) {
		g->hermit_lightning[i].active = 0;
		g->hermit_lightning[i].flash_timer = 0;
		g->hermit_lightning[i].yellow_hit_applied = 0;
	}
}

static int segment_hits_player(const game_t *g, float x0, float y0, float x1, float y1) {
	const sprite1r_t *p = &g->PLAYER;
	int dx = (int)(x1 - x0);
	if (dx < 0) dx = -dx;
	int dy = (int)(y1 - y0);
	if (dy < 0) dy = -dy;
	int steps = (dx > dy) ? dx : dy;
	if (steps < 1) steps = 1;

	for (int i = 0; i <= steps; i++) {
		float t = (float)i / (float)steps;
		int px = (int)(x0 + (x1 - x0) * t);
		int py = (int)(y0 + (y1 - y0) * t);
		if (px >= g->player_x && px < g->player_x + p->w &&
				py >= g->player_y && py < g->player_y + p->h) {
			return 1;
		}
	}
	return 0;
}

static void destroy_bunkers_along_segment(game_t *g, float x0, float y0, float x1, float y1) {
	if (g->level == 0) return;

	int dx = (int)(x1 - x0);
	if (dx < 0) dx = -dx;
	int dy = (int)(y1 - y0);
	if (dy < 0) dy = -dy;
	int steps = (dx > dy) ? dx : dy;
	if (steps < 1) steps = 1;

	for (int i = 0; i <= steps; i++) {
		float t = (float)i / (float)steps;
		int px = (int)(x0 + (x1 - x0) * t);
		int py = (int)(y0 + (y1 - y0) * t);

		for (int bi = 0; bi < 4; bi++) {
			sprite1r_t *b = g->bunkers[bi];
			int bx = g->bunker_x[bi];
			int by = g->bunker_y;

			if (bullet_hits_sprite(b, bx, by, px, py)) {
				bunker_damage(b, px - bx, py - by, 1);
			}
		}
	}
}

static float hermit_absf(float v) {
	return (v < 0.0f) ? -v : v;
}

static float hermit_lightning_span(const hermit_lightning_t *l) {
	float dx = hermit_absf(l->tip_x - l->start_x);
	float dy = hermit_absf(l->tip_y - l->start_y);
	return (dx > dy) ? dx : dy;
}

static void hermit_move_point_toward(float *x, float *y, float tx, float ty, float step) {
	float dx = tx - *x;
	float dy = ty - *y;
	float adx = hermit_absf(dx);
	float ady = hermit_absf(dy);
	float denom = (adx > ady) ? adx : ady;

	if (denom <= step || denom < 0.001f) {
		*x = tx;
		*y = ty;
		return;
	}

	float s = step / denom;
	*x += dx * s;
	*y += dy * s;
}

static void spawn_hermit_lightning_from(game_t *g, float start_x, float start_y) {
	int slot = -1;
	for (int i = 0; i < HERMIT_MAX_LIGHTNINGS; i++) {
		if (!g->hermit_lightning[i].active) {
			slot = i;
			break;
		}
	}
	if (slot < 0) return;

	hermit_lightning_t *l = &g->hermit_lightning[slot];
	l->active = 1;
	l->flash_timer = 0;
	l->yellow_hit_applied = 0;
	l->start_x = start_x;
	l->start_y = start_y;
	l->tip_x = start_x;
	l->tip_y = start_y;

	float tx = (float)(g->player_x + g->PLAYER.w / 2);
	float ty = (float)LH;
	float dx = tx - l->start_x;
	float dy = ty - l->start_y;
	float adx = (dx < 0.0f) ? -dx : dx;
	float ady = (dy < 0.0f) ? -dy : dy;
	float denom = (adx > ady) ? adx : ady;
	if (denom < 1.0f) denom = 1.0f;
	float scale = HERMIT_LIGHTNING_SPEED / denom;
	l->vx = dx * scale;
	l->vy = dy * scale;
}

static void hermit_spawn_lightning(game_t *g) {
	const sprite1r_t *BS = active_boss_sprite(g);
	spawn_hermit_lightning_from(g,
			(float)(g->boss_x + BS->w / 2),
			(float)(g->boss_y + BS->h + 1));
}

static void start_tower_asteroid_explosion(game_t *g, int ai) {
	if (g->tower_asteroid_exploding[ai]) return;
	g->tower_asteroid[ai].alive = 0;
	g->tower_asteroid_hp[ai] = 0;
	g->tower_asteroid_dx[ai] = 0;
	g->tower_asteroid_exploding[ai] = 1;
	g->tower_asteroid_explode_timer[ai] = TOWER_ASTEROID_EXPLOSION_FRAMES;
	g->tower_asteroid_boss_damage_applied[ai] = 0;
	music_play_tower_asteroid_boom();
}

static int boss_bomb_explosion_radius(const game_t *g) {
	int age = BOSS_BOMB_EXPLOSION_FRAMES - g->boss_bomb.explode_timer;
	int frame = age / 10;
	int base_r = 6 + frame * 3;
	int radius = base_r + 4;

	// Moon boss variant: once half the alien wave is gone, boost bomb/black-hole radius.
	if (g->boss_type == BOSS_TYPE_BLUE) {
		int alive_count = 0;
		for (int r = 0; r < AROWS; r++) {
			for (int c = 0; c < ACOLS; c++) {
				alive_count += g->alien_alive[r][c] ? 1 : 0;
			}
		}
		int total_aliens = AROWS * ACOLS;
		if (alive_count <= total_aliens / 2) {
			radius = (radius * 3) / 2;
		}
	}

	return radius;
}

static void kill_alien_with_explosion(game_t *g, int r, int c);
static void apply_player_damage(game_t *g, int damage);
static int sprite_has_any_bits(const sprite1r_t *s);
static int player_shield_radius(const game_t *g);
static int projectile_hits_player_shield(const game_t *g, int px, int py);
static void render_player_shield_ring(const game_t *g, lfb_t *lfb);
static void handle_enemy_shot_collisions(game_t *g, bullet_t *shot, int x_drift, int shield_blockable);
static void draw_segment(lfb_t *lfb, int x0, int y0, int x1, int y1, uint32_t color);

static void apply_tower_asteroid_explosion_effects(game_t *g, int ai, int radius) {
	const sprite1r_t *AS = g->alien_frame ? &g->ALIEN_B : &g->ALIEN_A;
	int spacing_x = 6, spacing_y = 5;

	for (int r = 0; r < AROWS; r++) {
		for (int c = 0; c < ACOLS; c++) {
			if (!g->alien_alive[r][c]) continue;
			int ax = g->alien_origin_x + c * (AS->w + spacing_x) + AS->w / 2;
			int ay = g->alien_origin_y + r * (AS->h + spacing_y) + AS->h / 2;
			int dx = ax - g->tower_asteroid[ai].x;
			int dy = ay - g->tower_asteroid[ai].y;
			if ((dx * dx + dy * dy) <= (radius * radius)) {
				kill_alien_with_explosion(g, r, c);
			}
		}
	}

	if (!g->tower_asteroid_boss_damage_applied[ai] && g->boss_alive && !g->boss_dying && g->boss_type != BOSS_TYPE_YELLOW) {
		const sprite1r_t *BS = active_boss_sprite(g);
		if (circle_intersects_rect(g->tower_asteroid[ai].x, g->tower_asteroid[ai].y, radius,
					g->boss_x, g->boss_y, BS->w, BS->h)) {
			int boss_damage = g->boss_max_health / 10;
			if (boss_damage < 1) boss_damage = 1;
			g->boss_health -= boss_damage;
			g->tower_asteroid_boss_damage_applied[ai] = 1;

			if (g->boss_health <= 0) {
				g->boss_health = 0;
				boss_trigger_death(g, double_shot_active(g) ? 1000 : 500);
			}
		}
	}

	for (int i = 0; i < 4; i++) {
		sprite1r_t *b = g->bunkers[i];
		if (!sprite_has_any_bits(b)) continue;
		int bx = g->bunker_x[i];
		int by = g->bunker_y;
		if (circle_intersects_rect(g->tower_asteroid[ai].x, g->tower_asteroid[ai].y, radius, bx, by, b->w, b->h)) {
			memset(b->bits, 0, (size_t)b->stride * (size_t)b->h);
		}
	}

	// Chain reaction: explosion can ignite other active asteroids.
	for (int other = 0; other < MAX_TOWER_ASTEROIDS; other++) {
		if (other == ai) continue;
		if (!g->tower_asteroid[other].alive) continue;
		if (g->tower_asteroid_exploding[other]) continue;

		int dx = g->tower_asteroid[other].x - g->tower_asteroid[ai].x;
		int dy = g->tower_asteroid[other].y - g->tower_asteroid[ai].y;
		int trigger_r = radius + TOWER_ASTEROID_RADIUS;
		if ((dx * dx + dy * dy) <= (trigger_r * trigger_r)) {
			start_tower_asteroid_explosion(g, other);
		}
	}

	if (!g->player_dying) {
		const sprite1r_t *p = &g->PLAYER;
		if (circle_intersects_rect(g->tower_asteroid[ai].x, g->tower_asteroid[ai].y, radius,
					g->player_x, g->player_y, p->w, p->h)) {
			apply_player_damage(g, 1);
		}
	}
}

static void apply_reversed_bomb_explosion_effects(game_t *g, int radius) {
	const sprite1r_t *AS = g->alien_frame ? &g->ALIEN_B : &g->ALIEN_A;
	int spacing_x = 6, spacing_y = 5;
	for (int r = 0; r < AROWS; r++) {
		for (int c = 0; c < ACOLS; c++) {
			if (!g->alien_alive[r][c]) continue;
			int ax = g->alien_origin_x + c * (AS->w + spacing_x) + AS->w / 2;
			int ay = g->alien_origin_y + r * (AS->h + spacing_y) + AS->h / 2;
			int dx = ax - g->boss_bomb.x;
			int dy = ay - g->boss_bomb.y;
			if ((dx * dx + dy * dy) <= (radius * radius)) {
				kill_alien_with_explosion(g, r, c);
			}
		}
	}

	if (!g->boss_bomb.reverse_damage_applied && g->boss_alive && !g->boss_dying) {
		const sprite1r_t *BS = active_boss_sprite(g);
		int left = g->boss_x;
		int right = g->boss_x + BS->w - 1;
		int top = g->boss_y;
		int bottom = g->boss_y + BS->h - 1;

		int nearest_x = g->boss_bomb.x;
		if (nearest_x < left) nearest_x = left;
		if (nearest_x > right) nearest_x = right;

		int nearest_y = g->boss_bomb.y;
		if (nearest_y < top) nearest_y = top;
		if (nearest_y > bottom) nearest_y = bottom;

		int dx = g->boss_bomb.x - nearest_x;
		int dy = g->boss_bomb.y - nearest_y;
		int boss_hit_by_explosion = (dx * dx + dy * dy) <= (radius * radius);

		if (boss_hit_by_explosion) {
			int boss_damage = g->boss_max_health / 5;
			if (boss_damage < 1) boss_damage = 1;
			g->boss_health -= boss_damage;
			g->boss_bomb.reverse_damage_applied = 1;

			if (g->boss_health <= 0) {
				g->boss_health = 0;
				boss_trigger_death(g, double_shot_active(g) ? 1000 : 500);
			}
		}
	}
}

static void apply_chariot_charge_explosion_to_aliens(game_t *g, int cx, int cy, int radius) {
	const sprite1r_t *AS = g->alien_frame ? &g->ALIEN_B : &g->ALIEN_A;
	int spacing_x = 6, spacing_y = 5;
	for (int r = 0; r < AROWS; r++) {
		for (int c = 0; c < ACOLS; c++) {
			if (!g->alien_alive[r][c]) continue;
			int ax = g->alien_origin_x + c * (AS->w + spacing_x);
			int ay = g->alien_origin_y + r * (AS->h + spacing_y);
			if (circle_intersects_rect(cx, cy, radius, ax, ay, AS->w, AS->h)) {
				kill_alien_with_explosion(g, r, c);
			}
		}
	}
}

static int rects_overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
	return (ax < bx + bw) && (ax + aw > bx) && (ay < by + bh) && (ay + ah > by);
}

static void spawn_chariot_arc_shot(game_t *g, int index, int x, int y) {
	// Approximate 15-degree spacing from -60 to +60 degrees.
	static const int arc_dx[CHARIOT_ARC_SHOT_COUNT] = {-4, -3, -2, -1, 0, 1, 2, 3, 4};
	if (index < 0 || index >= CHARIOT_ARC_SHOT_COUNT) return;
	g->boss_arc_shot[index].alive = 1;
	g->boss_arc_shot[index].x = x;
	g->boss_arc_shot[index].y = y;
	g->boss_arc_shot_dx[index] = arc_dx[index];
}

static int any_chariot_arc_shot_alive(const game_t *g) {
	for (int i = 0; i < CHARIOT_ARC_SHOT_COUNT; i++) {
		if (g->boss_arc_shot[i].alive) return 1;
	}
	return 0;
}

static int iabs_int(int v) {
	return (v < 0) ? -v : v;
}

static int any_magician_wave_alive(const game_t *g) {
	return g->magician_wave.active;
}

static void reflect_player_shot_downward(bullet_t *shot) {
	shot->reflected = 1;
	shot->dy = 3;
	shot->pierce_active = 0;
	shot->damage_remaining = 0;
	shot->last_hit_r = -1;
	shot->last_hit_c = -1;
}

static int magician_mirror_is_reflective(const magician_mirror_t *m) {
	if (!m->active) return 0;
	if (m->timer <= 0) return 0;
	if (m->y < GAMEPLAY_CLIP_Y_MIN || m->y > GAMEPLAY_CLIP_Y_MAX) return 0;
	return 1;
}

static int magician_mirror_attack_active(const game_t *g) {
	return magician_mirror_is_reflective(&g->magician_mirror) ||
		magician_mirror_is_reflective(&g->magician_mirror_alt);
}

static int magician_mirror_has_segment_at_x(const magician_mirror_t *m, int x) {
	int half_w = m->width / 2;
	if (x < m->x - half_w || x > m->x + half_w) return 0;
	if (m->follow_boss && iabs_int(x - m->x) <= MAGICIAN_MIRROR_NOTCH_HALF_BOSS) return 0;
	return 1;
}

static void reset_magician_mirror_state(magician_mirror_t *m) {
	m->active = 0;
	m->follow_boss = 1;
	m->alien_r = -1;
	m->alien_c = -1;
	m->x = 0;
	m->y = 0;
	m->width = MAGICIAN_MIRROR_WIDTH_BOSS;
	m->timer = 0;
}

static int magician_mirror_hits_point(const magician_mirror_t *m, int x, int y) {
	if (!magician_mirror_is_reflective(m)) return 0;
	if (!magician_mirror_has_segment_at_x(m, x)) return 0;
	return (iabs_int(y - m->y) <= 1);
}

static int magician_wave_hits_point(const game_t *g, int x, int y) {
	const magician_wave_t *w = &g->magician_wave;
	if (!w->active) return 0;
	int dx = x - w->x;
	int dy = y - w->y;
	int r = MAGICIAN_ORB_RADIUS + 1;
	return (dx * dx + dy * dy) <= (r * r);
}

static void spawn_magician_wave(game_t *g) {
	const sprite1r_t *BS = active_boss_sprite(g);
	magician_wave_t *w = &g->magician_wave;
	int boss_cx = g->boss_x + BS->w / 2;
	w->active = 1;
	w->x = boss_cx;
	w->y = g->boss_y + BS->h + 6;
	// Randomly fire either rightward-down or leftward-down at a shallow angle.
	int go_right = rand() & 1;
	w->vx = go_right ? MAGICIAN_ORB_SPEED_X : -MAGICIAN_ORB_SPEED_X;
	w->dir = go_right ? 1 : -1;
	w->vy = MAGICIAN_ORB_SPEED_Y;
	w->hp = MAGICIAN_ORB_HIT_COUNT;
}

static void update_magician_wave(game_t *g, uint32_t vsync_counter) {
	magician_wave_t *w = &g->magician_wave;
	if (!w->active) return;
	if ((vsync_counter % (uint32_t)MAGICIAN_ORB_MOVE_FRAME_DIV) != 0u) return;
	w->x += w->vx;
	w->y += w->vy;

	if (w->x - MAGICIAN_ORB_RADIUS <= 0) {
		w->x = MAGICIAN_ORB_RADIUS;
		if (w->vx < 0) w->vx = -w->vx;
		w->dir = 1;
	} else if (w->x + MAGICIAN_ORB_RADIUS >= LW - 1) {
		w->x = LW - 1 - MAGICIAN_ORB_RADIUS;
		if (w->vx > 0) w->vx = -w->vx;
		w->dir = -1;
	}

	if (w->vy < 0 && g->boss_alive && !g->boss_dying && g->boss_type == BOSS_TYPE_MAGICIAN) {
		const sprite1r_t *BS = active_boss_sprite(g);
		if (circle_intersects_rect(w->x, w->y, MAGICIAN_ORB_RADIUS,
					g->boss_x, g->boss_y, BS->w, BS->h)) {
			int orb_damage = g->boss_max_health / 10;
			if (orb_damage < 1) orb_damage = 1;
			g->boss_health -= orb_damage;
			if (g->boss_health <= 0) {
				g->boss_health = 0;
				boss_trigger_death(g, double_shot_active(g) ? 1000 : 500);
			}
			w->active = 0;
			return;
		}
	}

	if (circle_intersects_rect(w->x, w->y, MAGICIAN_ORB_RADIUS,
				g->player_x, g->player_y, g->PLAYER.w, g->PLAYER.h)) {
		apply_player_damage(g, 1);
	}

	if (w->y - MAGICIAN_ORB_RADIUS > LH || w->y + MAGICIAN_ORB_RADIUS < 0) {
		w->active = 0;
	}
}

static void render_magician_wave(const game_t *g, lfb_t *lfb) {
	const magician_wave_t *w = &g->magician_wave;
	if (!w->active) return;
	draw_filled_circle_clipped_y(lfb, w->x, w->y, MAGICIAN_ORB_RADIUS, 0xFF8E8E8E,
			GAMEPLAY_CLIP_Y_MIN, GAMEPLAY_CLIP_Y_MAX);
	draw_filled_circle_clipped_y(lfb, w->x, w->y, MAGICIAN_ORB_RADIUS - 3, 0xFFC0C0C0,
			GAMEPLAY_CLIP_Y_MIN, GAMEPLAY_CLIP_Y_MAX);
	draw_filled_circle_clipped_y(lfb, w->x - 3, w->y - 3, 2, 0xFFE0E0E0,
			GAMEPLAY_CLIP_Y_MIN, GAMEPLAY_CLIP_Y_MAX);
}

static void spawn_magician_mirror(game_t *g) {
	magician_mirror_t *m = &g->magician_mirror;
	m->active = 1;
	m->timer = MAGICIAN_MIRROR_DURATION;
	m->width = MAGICIAN_MIRROR_WIDTH_BOSS;
	m->follow_boss = 1;
	m->alien_r = -1;
	m->alien_c = -1;

	magician_mirror_t *m_alt = &g->magician_mirror_alt;
	reset_magician_mirror_state(m_alt);
	m_alt->active = 1;
	m_alt->timer = MAGICIAN_MIRROR_DURATION;
	m_alt->follow_boss = 0;
	m_alt->width = MAGICIAN_MIRROR_WIDTH_ALIEN;

	int target_row = -1;
	for (int r = AROWS - 1; r >= 0; r--) {
		for (int c = 0; c < ACOLS; c++) {
			if (g->alien_alive[r][c]) {
				target_row = r;
				break;
			}
		}
		if (target_row >= 0) break;
	}

	if (target_row >= 0) {
		int cols[ACOLS];
		int n = 0;
		for (int c = 0; c < ACOLS; c++) {
			if (!g->alien_alive[target_row][c]) continue;
			cols[n++] = c;
		}

		if (n > 0) {
			int pick = rand() % n;
			m_alt->alien_r = target_row;
			m_alt->alien_c = cols[pick];
		} else {
			reset_magician_mirror_state(m_alt);
		}
	} else {
		reset_magician_mirror_state(m_alt);
	}
}

static void update_magician_mirror_slot(game_t *g, magician_mirror_t *m) {
	if (!m->active) return;

	if (m->timer > 0) m->timer--;
	if (m->timer <= 0) {
		reset_magician_mirror_state(m);
		return;
	}

	if (m->follow_boss) {
		const sprite1r_t *BS = active_boss_sprite(g);
		m->x = g->boss_x + BS->w / 2;
		m->y = g->boss_y + BS->h + MAGICIAN_MIRROR_BOSS_OFFSET;
		m->width = MAGICIAN_MIRROR_WIDTH_BOSS;
		return;
	}

	if (m->alien_r < 0 || m->alien_r >= AROWS || m->alien_c < 0 || m->alien_c >= ACOLS ||
			!g->alien_alive[m->alien_r][m->alien_c]) {
		reset_magician_mirror_state(m);
		return;
	}

	const sprite1r_t *AS = g->alien_frame ? &g->ALIEN_B : &g->ALIEN_A;
	int spacing_x = 6, spacing_y = 5;
	int ax = g->alien_origin_x + m->alien_c * (AS->w + spacing_x);
	int ay = g->alien_origin_y + m->alien_r * (AS->h + spacing_y);
	m->x = ax + AS->w / 2;
	m->y = ay + AS->h + MAGICIAN_MIRROR_ALIEN_OFFSET;
	m->width = MAGICIAN_MIRROR_WIDTH_ALIEN;
}

static void update_magician_mirror(game_t *g) {
	update_magician_mirror_slot(g, &g->magician_mirror);
	update_magician_mirror_slot(g, &g->magician_mirror_alt);
}

static void render_intro_regular_shot(lfb_t *lfb, int x, int y, uint32_t color) {
	for (int i = 0; i < 5; i++) l_putpix(lfb, x, y + i, color);
}

static void draw_segment(lfb_t *lfb, int x0, int y0, int x1, int y1, uint32_t color) {
	int dx = x1 - x0;
	int dy = y1 - y0;
	int steps = dx;
	if (steps < 0) steps = -steps;
	int ay = dy;
	if (ay < 0) ay = -ay;
	if (ay > steps) steps = ay;
	if (steps == 0) {
		l_putpix(lfb, x0, y0, color);
		return;
	}

	for (int i = 0; i <= steps; i++) {
		int px = x0 + (dx * i) / steps;
		int py = y0 + (dy * i) / steps;
		l_putpix(lfb, px, py, color);
	}
}

static void draw_zigzag_segment(lfb_t *lfb, int x0, int y0, int x1, int y1, int amplitude, int step, uint32_t color) {
	int dx = x1 - x0;
	int dy = y1 - y0;
	int len = dx;
	if (len < 0) len = -len;
	int ay = dy;
	if (ay < 0) ay = -ay;
	if (ay > len) len = ay;

	if (len <= step || step < 1) {
		draw_segment(lfb, x0, y0, x1, y1, color);
		return;
	}

	float flen = (float)len;
	int prev_x = x0;
	int prev_y = y0;
	int seg = 0;

	for (int s = step; s < len; s += step) {
		float t = (float)s / flen;
		float bx = (float)x0 + (float)dx * t;
		float by = (float)y0 + (float)dy * t;

		int sign = (seg & 1) ? 1 : -1;
		float ox = ((float)(-dy) / flen) * (float)(amplitude * sign);
		float oy = ((float)dx / flen) * (float)(amplitude * sign);
		int zx = (int)(bx + ox);
		int zy = (int)(by + oy);

		draw_segment(lfb, prev_x, prev_y, zx, zy, color);
		prev_x = zx;
		prev_y = zy;
		seg++;
	}

	draw_segment(lfb, prev_x, prev_y, x1, y1, color);
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

static void render_black_hole_explosion_clipped(lfb_t *lfb, int cx, int cy, int r) {
	draw_filled_circle_clipped_y(lfb, cx, cy, r, 0xFF1A1A28, GAMEPLAY_CLIP_Y_MIN, GAMEPLAY_CLIP_Y_MAX);
	draw_filled_circle_clipped_y(lfb, cx, cy, r - 3, 0xFF06070D, GAMEPLAY_CLIP_Y_MIN, GAMEPLAY_CLIP_Y_MAX);

	int ring_r = r - 1;
	if (ring_r <= 1) return;

	static const int ring_dx[20] = {16, 15, 13, 10, 5, 0, -5, -10, -13, -15,
		-16, -15, -13, -10, -5, 0, 5, 10, 13, 15};
	static const int ring_dy[20] = {0, 5, 10, 13, 15, 16, 15, 13, 10, 5,
		0, -5, -10, -13, -15, -16, -15, -13, -10, -5};
	for (int i = 0; i < 20; i++) {
		int px = cx + (ring_dx[i] * ring_r) / 16;
		int py = cy + (ring_dy[i] * ring_r) / 16;
		putpix_clipped_y(lfb, px, py, 0xFF66CCFF, GAMEPLAY_CLIP_Y_MIN, GAMEPLAY_CLIP_Y_MAX);
	}
}

static void render_tower_asteroid(lfb_t *lfb, int cx, int cy, int spin) {
	draw_filled_circle(lfb, cx, cy, TOWER_ASTEROID_RADIUS, 0xFF8B5A2B);
	draw_filled_circle(lfb, cx, cy, TOWER_ASTEROID_RADIUS - 2, 0xFF6A4421);

	int phase = (spin / 4) & 3;
	if (phase == 0) {
		for (int i = -5; i <= 5; i++) l_putpix(lfb, cx + i, cy + (i / 2), 0xFFC08A4B);
	} else if (phase == 1) {
		for (int i = -5; i <= 5; i++) l_putpix(lfb, cx + (i / 2), cy + i, 0xFFC08A4B);
	} else if (phase == 2) {
		for (int i = -5; i <= 5; i++) l_putpix(lfb, cx + i, cy - (i / 2), 0xFFC08A4B);
	} else {
		for (int i = -5; i <= 5; i++) l_putpix(lfb, cx - (i / 2), cy + i, 0xFFC08A4B);
	}
}

static int circle_hits_sprite(const sprite1r_t *s, int sx, int sy, int cx, int cy, int r) {
	int nearest_x = cx;
	int nearest_y = cy;
	int left = sx;
	int right = sx + s->w - 1;
	int top = sy;
	int bottom = sy + s->h - 1;

	if (nearest_x < left) nearest_x = left;
	if (nearest_x > right) nearest_x = right;
	if (nearest_y < top) nearest_y = top;
	if (nearest_y > bottom) nearest_y = bottom;

	int dx = cx - nearest_x;
	int dy = cy - nearest_y;
	return (dx * dx + dy * dy) <= (r * r);
}

#ifndef FB

static void render_tower_walls(lfb_t *lfb, const game_t *g) {
	if (!g->tower_wall_active) return;
	int top = TOP_HUD_SEPARATOR_Y + 1;
	int bottom = BOTTOM_HUD_SEPARATOR_Y - 1;
	if (bottom < top) return;

	for (int y = top; y <= bottom; y++) {
		for (int x = 0; x < g->tower_wall_left; x++) {
			l_putpix(lfb, x, y, 0xFF4A2E12);
		}
		for (int x = g->tower_wall_right; x < LW; x++) {
			l_putpix(lfb, x, y, 0xFF4A2E12);
		}
	}
}

#else

static void render_tower_walls(lfb_t *lfb, const game_t *g) {
	if (!g->tower_wall_active) return;
	int top = TOP_HUD_SEPARATOR_Y + 1;
	int bottom = BOTTOM_HUD_SEPARATOR_Y - 1;
	if (bottom < top) return;

	int height = bottom - top + 1;
	uint32_t color = 0xFF4A2E12;

	// Left wall
	if (g->tower_wall_left > 0)
		l_putrect(0, top, g->tower_wall_left, height, color);

	// Right wall
	if (g->tower_wall_right < LW)
		l_putrect(g->tower_wall_right, top, LW - g->tower_wall_right, height, color);
}

#endif

static void render_magician_side_walls(lfb_t *lfb, const game_t *g) {
	int wall = magician_side_wall_thickness(g);
	if (wall <= 0) return;

	int top = TOP_HUD_SEPARATOR_Y + 1;
	int bottom = BOTTOM_HUD_SEPARATOR_Y - 1;
	if (bottom < top) return;

	for (int y = top; y <= bottom; y++) {
		for (int x = 0; x < wall; x++) {
			uint32_t c = 0xFFB0B0B0;
			if (x == wall - 1 && ((y & 3) == 0)) c = 0xFFE0E0E0;
			l_putpix(lfb, x, y, c);
		}
		for (int x = LW - wall; x < LW; x++) {
			uint32_t c = 0xFFB0B0B0;
			if (x == LW - wall && ((y & 3) == 0)) c = 0xFFE0E0E0;
			l_putpix(lfb, x, y, c);
		}
	}
}

static int chariot_intro_charge_anim_active(const game_t *g) {
	if (g->boss_type != BOSS_TYPE_CHARIOT) return 0;

	int elapsed = BOSS_INTRO_FRAMES - g->boss_intro_timer;
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

	return elapsed >= t4 && elapsed < t5;
}

static int magician_intro_special_anim_active(const game_t *g) {
	int elapsed = BOSS_INTRO_FRAMES - g->boss_intro_timer;
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

	return elapsed >= t4 && elapsed < t5;
}

static void render_intro_boss_attack_preview(const game_t *g, lfb_t *lfb, int boss_x, int boss_y, const sprite1r_t *BS, int allow_tower_walls) {
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

	if (g->boss_type == BOSS_TYPE_TOWER) {
		if (elapsed >= t0 && elapsed < t1) {
			int local = elapsed - t0;
			int y = start_y + local;
			render_tower_asteroid(lfb, center_x, y, local * 2);
		} else if (elapsed >= t2 && elapsed < t3) {
			int local = elapsed - t2;
			int y = start_y + local;
			render_tower_asteroid(lfb, center_x, y, local * 2 + 10);
		} else if (allow_tower_walls && elapsed >= t4 && elapsed < t5) {
			int wall_left = TOWER_WALL_WIDTH;
			int wall_right = LW - TOWER_WALL_WIDTH;
			for (int y = 0; y < LH; y++) {
				for (int x = 0; x < wall_left; x++) l_putpix(lfb, x, y, 0xFF4A2E12);
				for (int x = wall_right; x < LW; x++) l_putpix(lfb, x, y, 0xFF4A2E12);
			}
		}
		return;
	}

	if (g->boss_type == BOSS_TYPE_HERMIT) {
		if (elapsed >= t0 && elapsed < t1) {
			int local = elapsed - t0;
			int travel_frames = 16;
			int local_clamped = (local < travel_frames) ? local : travel_frames;
			int tip_x = center_x + (((local_clamped & 1) == 0) ? -2 : 2);
			int tip_y = start_y + local_clamped * 2;
			uint32_t bolt_color = (local >= travel_frames) ? 0xFFFFFF00 : 0xFF8000FF;
			draw_zigzag_segment(lfb, center_x, start_y, tip_x, tip_y, 2, 6, bolt_color);
		} else if (elapsed >= t2 && elapsed < t3) {
			int local = elapsed - t2;
			int travel_frames = 16;
			int local_clamped = (local < travel_frames) ? local : travel_frames;
			int tip_x = center_x + (((local_clamped & 1) == 0) ? 2 : -2);
			int tip_y = start_y + local_clamped * 2;
			uint32_t bolt_color = (local >= travel_frames) ? 0xFFFFFF00 : 0xFF8000FF;
			draw_zigzag_segment(lfb, center_x, start_y, tip_x, tip_y, 2, 6, bolt_color);
		} else if (elapsed >= t4 && elapsed < t5) {
			const sprite1r_t *AS = g->alien_frame ? &g->ALIEN_B : &g->ALIEN_A;
			int spawn_count = (g->level >= 3) ? 2 : 1;
			int local = elapsed - t4;
			int spawn_y = boss_y + BS->h + 10;
			int rise = (local < 10) ? (10 - local) : 0;

			if (spawn_count == 1) {
				draw_sprite1r(lfb, AS, center_x - AS->w / 2, spawn_y - rise, 0xFFB266FF);
			} else {
				int spacing = AS->w + 10;
				int left_x = center_x - spacing / 2 - AS->w / 2;
				int right_x = center_x + spacing / 2 - AS->w / 2;
				draw_sprite1r(lfb, AS, left_x, spawn_y - rise, 0xFFB266FF);
				draw_sprite1r(lfb, AS, right_x, spawn_y - rise, 0xFFB266FF);
			}
		}
		return;
	}

	if (g->boss_type == BOSS_TYPE_CHARIOT) {
		if (elapsed >= t0 && elapsed < t1) {
			int local = elapsed - t0;
			int y = start_y + local * 2;
			render_intro_regular_shot(lfb, center_x - 8 - local / 2, y, 0xFFFF8C00);
			render_intro_regular_shot(lfb, center_x - 4 - local / 3, y, 0xFFFF8C00);
			render_intro_regular_shot(lfb, center_x, y, 0xFFFF8C00);
			render_intro_regular_shot(lfb, center_x + 4 + local / 3, y, 0xFFFF8C00);
			render_intro_regular_shot(lfb, center_x + 8 + local / 2, y, 0xFFFF8C00);
		} else if (elapsed >= t2 && elapsed < t3) {
			int local = elapsed - t2;
			int y = start_y + local * 2;
			render_intro_regular_shot(lfb, center_x - 8 - local / 2, y, 0xFFFF8C00);
			render_intro_regular_shot(lfb, center_x - 4 - local / 3, y, 0xFFFF8C00);
			render_intro_regular_shot(lfb, center_x, y, 0xFFFF8C00);
			render_intro_regular_shot(lfb, center_x + 4 + local / 3, y, 0xFFFF8C00);
			render_intro_regular_shot(lfb, center_x + 8 + local / 2, y, 0xFFFF8C00);
		} else if (elapsed >= t4 && elapsed < t5) {
			int local = elapsed - t4;
			int rush_y = boss_y + local * 3;
			if (rush_y < LH - 30) {
				draw_sprite1r(lfb, BS, boss_x, rush_y, 0xFFFF8C00);
			} else {
				int ex = center_x;
				int ey = LH - 30;
				int r = 8 + ((local - 18) > 0 ? (local - 18) : 0) / 2;
				if (r > 22) r = 22;
				draw_filled_circle(lfb, ex, ey, r, 0xFFFF4500);
				draw_filled_circle(lfb, ex, ey, r - 3, 0xFFFFA500);
			}
		}
		return;
	}

	if (g->boss_type == BOSS_TYPE_MAGICIAN) {
		if (elapsed >= t0 && elapsed < t1) {
			int local = elapsed - t0;
			int oy = start_y + local * 2;
			draw_filled_circle(lfb, center_x, oy, 7, 0xFF8E8E8E);
			draw_filled_circle(lfb, center_x, oy, 4, 0xFFC0C0C0);
			draw_filled_circle(lfb, center_x - 2, oy - 2, 1, 0xFFE0E0E0);
		} else if (elapsed >= t2 && elapsed < t3) {
			int local = elapsed - t2;
			int oy = start_y + local * 2;
			draw_filled_circle(lfb, center_x, oy, 7, 0xFF8E8E8E);
			draw_filled_circle(lfb, center_x, oy, 4, 0xFFC0C0C0);
			draw_filled_circle(lfb, center_x - 2, oy - 2, 1, 0xFFE0E0E0);
		} else if (elapsed >= t4 && elapsed < t5) {
			int local = elapsed - t4;
			draw_sprite1r(lfb, BS, boss_x, boss_y, magician_color_for_special_state(1));
			int mirror_y = start_y + 8 + (local / 8);
			int half_w = MAGICIAN_MIRROR_WIDTH_BOSS / 2;
			for (int x = center_x - half_w; x <= center_x + half_w; x++) {
				if (iabs_int(x - center_x) <= MAGICIAN_MIRROR_NOTCH_HALF_BOSS) continue;
				l_putpix(lfb, x, mirror_y, 0xFFB0B0B0);
				if ((x & 3) == 0) l_putpix(lfb, x, mirror_y + 1, 0xFFE0E0E0);
			}

			// Show reflection behavior: player shot goes up (white) then comes back down (red).
			{
				int shot_x = center_x - 10;
				int up_frames = 12;
				if (local < up_frames) {
					int tip_y = mirror_y + 2 + (up_frames - local) * 2;
					for (int i = 0; i < 5; i++) {
						l_putpix(lfb, shot_x, tip_y - i, 0xFFFFFFFF);
					}
				} else {
					int down_local = local - up_frames;
					int tip_y = mirror_y + 2 + down_local * 2;
					for (int i = 0; i < 5; i++) {
						l_putpix(lfb, shot_x, tip_y + i, 0xFF00E5FF);
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

static void render_practice_boss_preview(const game_t *g, lfb_t *lfb, int boss_type) {
	if (boss_type < 0 || boss_type >= BOSS_TYPE_COUNT) return;

	game_t preview = *g;
	preview.boss_type = boss_type;
	preview.level = g->practice_level_selection;
	preview.boss_intro_timer = g->practice_preview_timer;
	if (preview.boss_intro_timer < 0) preview.boss_intro_timer = 0;
	preview.boss_frame = ((BOSS_INTRO_FRAMES - preview.boss_intro_timer) / 12) & 1;

	uint32_t preview_type_color = 0xFFFFFFFF;
	if (boss_type == BOSS_TYPE_BLUE) {
		preview_type_color = 0xFF3399FF;
	} else if (boss_type == BOSS_TYPE_YELLOW) {
		preview_type_color = 0xFFFFFF00;
	} else if (boss_type == BOSS_TYPE_TOWER) {
		preview_type_color = 0xFF8B5A2B;
	} else if (boss_type == BOSS_TYPE_HERMIT) {
		preview_type_color = 0xFFB266FF;
	} else if (boss_type == BOSS_TYPE_CHARIOT) {
		preview_type_color = 0xFFFF8C00;
	} else if (boss_type == BOSS_TYPE_MAGICIAN) {
		preview_type_color = 0xFF00E5FF;
	} else {
		if (preview.boss_intro_timer > 0) {
			uint32_t flicker_palette[4] = {0xFF00FF00, 0xFFFFFF00, 0xFFFF0000, 0xFF8000FF};
			int flicker_idx = ((BOSS_INTRO_FRAMES - preview.boss_intro_timer) / 8) & 3;
			preview_type_color = flicker_palette[flicker_idx];
		} else {
			preview_type_color = 0xFF00FF00;
		}
	}

	int right_x = LW / 2 + 6;
	l_draw_text(lfb, right_x, 16, "TYPE:", 1, 0xFFFFFFFF);
	l_draw_text(lfb, right_x + 34, 16, boss_intro_type_text(&preview), 1, preview_type_color);

	l_draw_text(lfb, right_x, 28, "MAIN:", 1, 0xFFFFFFFF);
	l_draw_text(lfb, right_x + 34, 28, boss_intro_main_attack_text(&preview), 1,
			(boss_type == BOSS_TYPE_BLUE) ? 0xFF3399FF :
			(boss_type == BOSS_TYPE_YELLOW) ? 0xFFFFFF00 :
			(boss_type == BOSS_TYPE_TOWER) ? 0xFF8B5A2B :
			(boss_type == BOSS_TYPE_HERMIT) ? 0xFF8000FF :
			(boss_type == BOSS_TYPE_CHARIOT) ? 0xFFFF8C00 :
			(boss_type == BOSS_TYPE_MAGICIAN) ? 0xFFA8A8A8 : 0xFFFF0000);

	l_draw_text(lfb, right_x, 40, "SPECIAL:", 1, 0xFFFFFFFF);
	if (boss_type == BOSS_TYPE_CLASSIC && preview.level >= 3) {
		const char *spec1 = "PLASMA";
		const char *spec2 = "HEAL";
		int sx = right_x + 46;
		int spec1_w = text_width_5x5(spec1, 1);
		l_draw_text(lfb, sx, 40, spec1, 1, 0xFF8000FF);
		l_draw_text(lfb, sx + spec1_w + 4, 40, spec2, 1, 0xFF00FF00);
	} else {
		l_draw_text(lfb, right_x + 46, 40, boss_intro_special_attack_text(&preview), 1,
				(boss_type == BOSS_TYPE_BLUE) ? 0xFF66CCFF :
				(boss_type == BOSS_TYPE_YELLOW) ? 0xFFFFFF00 :
				(boss_type == BOSS_TYPE_TOWER) ? 0xFF4A2E12 :
				(boss_type == BOSS_TYPE_HERMIT) ? 0xFF8000FF :
				(boss_type == BOSS_TYPE_CHARIOT) ? 0xFFFF8C00 :
				(boss_type == BOSS_TYPE_MAGICIAN) ? 0xFF00E5FF : 0xFF8000FF);
	}

	char level_text[20];
	snprintf(level_text, sizeof(level_text), "LEVEL: %d", preview.level);
	l_draw_text(lfb, right_x, 52, level_text, 1, 0xFFFFFFFF);

	const sprite1r_t *BS = boss_sprite_for_frame(&preview, preview.boss_frame);
	int pane_center_x = LW * 3 / 4;
	int boss_x = pane_center_x - BS->w / 2;
	int boss_y = 84;
	if (boss_type != BOSS_TYPE_YELLOW) {
		uint32_t boss_color = (boss_type == BOSS_TYPE_BLUE) ? 0xFF3399FF :
			(boss_type == BOSS_TYPE_TOWER) ? 0xFF8B5A2B :
			(boss_type == BOSS_TYPE_HERMIT) ? 0xFFB266FF :
			(boss_type == BOSS_TYPE_CHARIOT) ? 0xFFFF8C00 :
			(boss_type == BOSS_TYPE_MAGICIAN) ? magician_color_for_special_state(magician_intro_special_anim_active(&preview)) : 0xFF00FF00;
		if (boss_type == BOSS_TYPE_CLASSIC) boss_color = preview_type_color;
		if (!chariot_intro_charge_anim_active(&preview)) {
			draw_sprite1r(lfb, BS, boss_x, boss_y, boss_color);
		}
		render_intro_boss_attack_preview(&preview, lfb, boss_x, boss_y, BS, 0);
	} else {
		const sprite1r_t *AS = preview.alien_frame ? &preview.ALIEN_B : &preview.ALIEN_A;
		int spacing = 8;
		int total_w = YELLOW_BOSS_ALIENS * AS->w + (YELLOW_BOSS_ALIENS - 1) * spacing;
		int swarm_x0 = pane_center_x - total_w / 2;
		int swarm_y = boss_y;
		int elapsed = BOSS_INTRO_FRAMES - preview.boss_intro_timer;

		for (int i = 0; i < YELLOW_BOSS_ALIENS; i++) {
			int x = swarm_x0 + i * (AS->w + spacing);
			if (elapsed >= 42 && elapsed < 66) {
				int wave = ((elapsed / 4) + i * 2) % 4;
				x += wave - 2;
			}
			draw_sprite1r(lfb, AS, x, swarm_y, 0xFFFFFF00);
		}

		int shot_start_y = swarm_y + AS->h + 2;
		if ((elapsed >= 10 && elapsed < 34) || (elapsed >= 42 && elapsed < 66)) {
			int local = (elapsed >= 42) ? (elapsed - 42) : (elapsed - 10);
			int beam_y = shot_start_y + local * 2;
			for (int i = 0; i < YELLOW_BOSS_ALIENS; i++) {
				int beam_x = swarm_x0 + i * (AS->w + spacing) + AS->w / 2;
				render_intro_regular_shot(lfb, beam_x, beam_y, 0xFFFFFF00);
			}
		}
	}
}

static int overworld_node_for_level(int level) {
	switch (level) {
		case 1: return 0;
		case 2: return 1;
		case 3: return 3;
		case 4: return 4;
		case 5: return 6;
		case 6: return 7;
		case 7: return 9;
		default: return OVERWORLD_LEVEL_COUNT - 1;
	}
}

static int overworld_shop_node_after_level(int level) {
	if (level == 2) return 2;
	if (level == 4) return 5;
	if (level == 6) return 8;
	return -1;
}

static int overworld_node_is_shop(int idx) {
	return idx == 2 || idx == 5 || idx == 8;
}

static int overworld_shop_order_from_node(int idx) {
	if (idx == 2) return 0;
	if (idx == 5) return 1;
	if (idx == 8) return 2;
	return -1;
}

static int overworld_node_level_label(int idx) {
	switch (idx) {
		case 0: return 1;
		case 1: return 2;
		case 3: return 3;
		case 4: return 4;
		case 6: return 5;
		case 7: return 6;
		case 9: return 7;
		default: return 0;
	}
}

static void overworld_node_position(int idx, int *x, int *y) {
	static const int nx[OVERWORLD_LEVEL_COUNT] = {36, 61, 87, 111, 136, 161, 188, 216, 245, 274};
	static const int ny[OVERWORLD_LEVEL_COUNT] = {140, 126, 116, 104, 92, 82, 70, 58, 46, 36};

	if (idx < 0) idx = 0;
	if (idx >= OVERWORLD_LEVEL_COUNT) idx = OVERWORLD_LEVEL_COUNT - 1;
	*x = nx[idx];
	*y = ny[idx];
}

static void render_overworld_background(lfb_t *lfb, int elapsed) {
	l_clear(lfb, 0xFF02070E);

	for (int y = 0; y < LH; y += 18) {
		for (int x = 0; x < LW; x += 18) {
			int seed = (x * 1103 + y * 917 + elapsed * 7) & 63;
			if (seed < 5) {
				uint32_t c = (seed & 1) ? 0xFF35567A : 0xFF5D85B4;
				l_putpix(lfb, x, y, c);
				if (x + 1 < LW) l_putpix(lfb, x + 1, y, c);
			}
		}
	}
}

static void render_ship_with_vertical_thruster(const game_t *g,
		lfb_t *lfb,
		int ship_x,
		int ship_y,
		int moving_vertically,
		int elapsed) {
	int draw_x = ship_x - g->PLAYER.w / 2;
	int draw_y = ship_y - g->PLAYER.h / 2;
	draw_sprite1r(lfb, &g->PLAYER, draw_x, draw_y, 0xFF00FF00);

	if (!moving_vertically) return;

	int flame_x = ship_x;
	int flame_y = draw_y + g->PLAYER.h;
	int flicker = elapsed & 3;
	uint32_t core_color = 0xFFFFFF66;
	uint32_t flame_color = (flicker == 0 || flicker == 2) ? 0xFFFFA500 : 0xFFFF6A00;
	int flame_len = 2 + ((elapsed >> 1) & 1);

	l_putpix(lfb, flame_x, flame_y, core_color);
	for (int i = 1; i <= flame_len; i++) {
		l_putpix(lfb, flame_x, flame_y + i, flame_color);
	}
	if ((flicker & 1) == 0) {
		l_putpix(lfb, flame_x - 1, flame_y + 1, flame_color);
		l_putpix(lfb, flame_x + 1, flame_y + 1, flame_color);
	}
}

static void render_overworld_cutscene(const game_t *g, lfb_t *lfb) {
	int elapsed = OVERWORLD_TOTAL_FRAMES - g->overworld_cutscene_timer;
	if (elapsed < 0) elapsed = 0;
	if (elapsed > OVERWORLD_TOTAL_FRAMES) elapsed = OVERWORLD_TOTAL_FRAMES;

	render_overworld_background(lfb, elapsed);

	const char *title = "DEFEAT THE ALIENS";
	int title_w = text_width_5x5(title, 1);
	l_draw_text(lfb, (LW - title_w) / 2, 8, title, 1, 0xFFFFFFFF);

	for (int i = 0; i < OVERWORLD_LEVEL_COUNT - 1; i++) {
		int x0, y0, x1, y1;
		overworld_node_position(i, &x0, &y0);
		overworld_node_position(i + 1, &x1, &y1);
		int dx = x1 - x0;
		int dy = y1 - y0;
		int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
		if (steps < 1) steps = 1;
		for (int s = 0; s <= steps; s++) {
			int px = x0 + (dx * s) / steps;
			int py = y0 + (dy * s) / steps;
			l_putpix(lfb, px, py, 0xFF1A3556);
		}
	}

	int current_level_node = (g->level > 0) ? overworld_node_for_level(g->level) : -1;
	if (g->overworld_current_node >= 0) {
		current_level_node = g->overworld_current_node;
	}
	if (current_level_node >= OVERWORLD_LEVEL_COUNT) current_level_node = OVERWORLD_LEVEL_COUNT - 1;

	int shops_cleared = g->shop_count;
	if (g->in_shop && shops_cleared > 0) {
		shops_cleared--;
	}
	if (shops_cleared < 0) shops_cleared = 0;
	if (shops_cleared > 3) shops_cleared = 3;

	int pulse = (elapsed / 3) & 7;
	int pulse_r = (pulse < 4) ? pulse : (7 - pulse);

	for (int i = 0; i < OVERWORLD_LEVEL_COUNT; i++) {
		int x, y;
		overworld_node_position(i, &x, &y);
		int is_shop = overworld_node_is_shop(i);

		if (is_shop) {
			int shop_order = overworld_shop_order_from_node(i);
			int completed = (shop_order >= 0) ? (shop_order < shops_cleared) : 0;
			if (completed) {
				draw_filled_circle(lfb, x, y, 4, 0xFF0A5A1A);
				draw_filled_circle(lfb, x, y, 2, 0xFF3CFF68);
			} else {
				int r = 3 + pulse_r;
				draw_filled_circle(lfb, x, y, r, 0xFF2A0E3C);
				draw_filled_circle(lfb, x, y, 3, 0xFFC56BFF);
			}
			l_draw_text(lfb, x - 2, y - 10, "S", 1, 0xFFFFFFFF);
		} else {
			int level_label = overworld_node_level_label(i);
			int completed = (level_label > 0 && current_level_node > i);

			if (completed) {
				draw_filled_circle(lfb, x, y, 4, 0xFF0A5A1A);
				draw_filled_circle(lfb, x, y, 2, 0xFF3CFF68);
			} else {
				int r = 3 + pulse_r;
				draw_filled_circle(lfb, x, y, r, 0xFF4C1010);
				draw_filled_circle(lfb, x, y, 3, 0xFFFF3A3A);
			}

			char label[4];
			snprintf(label, sizeof(label), "%d", level_label);
			l_draw_text(lfb, x - 2, y - 10, label, 1, 0xFFFFFFFF);
		}
	}

	int to_x, to_y;
	int from_x = 12;
	int from_y = LH + 10;
	overworld_node_position(g->overworld_cutscene_to_node, &to_x, &to_y);
	if (g->overworld_cutscene_from_node >= 0) {
		overworld_node_position(g->overworld_cutscene_from_node, &from_x, &from_y);
	}

	int ship_x = to_x;
	int ship_y = to_y;
	int prev_ship_y = ship_y;
	if (elapsed < OVERWORLD_FLY_FRAMES) {
		int prev_elapsed = elapsed > 0 ? (elapsed - 1) : 0;
		ship_x = from_x + ((to_x - from_x) * elapsed) / OVERWORLD_FLY_FRAMES;
		ship_y = from_y + ((to_y - from_y) * elapsed) / OVERWORLD_FLY_FRAMES;
		prev_ship_y = from_y + ((to_y - from_y) * prev_elapsed) / OVERWORLD_FLY_FRAMES;
	}
	render_ship_with_vertical_thruster(g, lfb, ship_x, ship_y, ship_y != prev_ship_y, elapsed);

	if (elapsed >= OVERWORLD_FLY_FRAMES) {
		int ring_r = 6 + pulse_r;
		render_intro_ring_points(lfb, to_x, to_y, ring_r, 0xFFFF8A8A);
	}

	int transition_start = OVERWORLD_FLY_FRAMES + OVERWORLD_HOLD_FRAMES;
	if (elapsed >= transition_start) {
		int t = elapsed - transition_start;
		if (t > OVERWORLD_PIXELATE_FRAMES) t = OVERWORLD_PIXELATE_FRAMES;
		int density = (t * 100) / OVERWORLD_PIXELATE_FRAMES;

		int cell = 6;
		int threshold = (density * 255) / 100;
		for (int y = 0; y < LH; y += cell) {
			for (int x = 0; x < LW; x += cell) {
				int seed = (x * 37 + y * 73 + 19) & 255;
				if (seed > threshold) continue;
				for (int yy = y; yy < y + cell && yy < LH; yy++) {
					for (int xx = x; xx < x + cell && xx < LW; xx++) {
						l_putpix(lfb, xx, yy, 0xFF000000);
					}
				}
			}
		}
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

	draw_filled_circle_clipped_y(lfb, cx, cy, base_r + 2, 0xFFFF4500, GAMEPLAY_CLIP_Y_MIN, GAMEPLAY_CLIP_Y_MAX);
	draw_filled_circle_clipped_y(lfb, cx, cy, base_r, 0xFFFF8C00, GAMEPLAY_CLIP_Y_MIN, GAMEPLAY_CLIP_Y_MAX);
	if ((age & 1) == 0) {
		draw_filled_circle_clipped_y(lfb, cx, cy, (base_r > 2) ? (base_r - 2) : 1, 0xFFFFFF00,
				GAMEPLAY_CLIP_Y_MIN, GAMEPLAY_CLIP_Y_MAX);
	}

	int spark_r = base_r + 3;
	for (int i = 0; i < 8; i++) {
		int dx = ((i * 7 + age * 3) % (spark_r * 2 + 1)) - spark_r;
		int dy = ((i * 11 + age * 5) % (spark_r * 2 + 1)) - spark_r;
		uint32_t c = (i & 1) ? 0xFFFFA500 : 0xFFFFFF00;
		putpix_clipped_y(lfb, cx + dx, cy + dy, c, GAMEPLAY_CLIP_Y_MIN, GAMEPLAY_CLIP_Y_MAX);
	}
}

static void render_player_explosion(const game_t *g, lfb_t *lfb) {
	int cx = g->player_x + g->PLAYER.w / 2;
	int cy = g->player_y + g->PLAYER.h / 2;
	int age = PLAYER_DEATH_DELAY_FRAMES - g->player_death_timer;
	render_player_explosion_at(lfb, cx, cy, age);
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

static void render_alien_explosion(lfb_t *lfb, int cx, int cy, int timer, int points, int clip_to_gameplay) {
	int age = ALIEN_EXPLOSION_FRAMES - timer;
	int base_r = 2 + (age / 3);

	if (clip_to_gameplay) {
		draw_filled_circle_clipped_y(lfb, cx, cy, base_r + 2, 0xFFFF4500, GAMEPLAY_CLIP_Y_MIN, GAMEPLAY_CLIP_Y_MAX);
		draw_filled_circle_clipped_y(lfb, cx, cy, base_r, 0xFFFF8C00, GAMEPLAY_CLIP_Y_MIN, GAMEPLAY_CLIP_Y_MAX);
	} else {
		draw_filled_circle(lfb, cx, cy, base_r + 2, 0xFFFF4500);
		draw_filled_circle(lfb, cx, cy, base_r, 0xFFFF8C00);
	}
	if ((age & 1) == 0) {
		if (clip_to_gameplay) {
			draw_filled_circle_clipped_y(lfb, cx, cy, (base_r > 1) ? (base_r - 1) : 1, 0xFFFFFF00,
					GAMEPLAY_CLIP_Y_MIN, GAMEPLAY_CLIP_Y_MAX);
		} else {
			draw_filled_circle(lfb, cx, cy, (base_r > 1) ? (base_r - 1) : 1, 0xFFFFFF00);
		}
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

static void tutorial_reset_layout(game_t *g) {
	memset(g->alien_alive, 0, sizeof(g->alien_alive));
	memset(g->alien_hermit_regen, 0, sizeof(g->alien_hermit_regen));
	memset(g->alien_health, 0, sizeof(g->alien_health));
	memset(g->alien_explode_timer, 0, sizeof(g->alien_explode_timer));
	memset(g->alien_explode_points, 0, sizeof(g->alien_explode_points));
	memset(g->alien_explode_hit_boss, 0, sizeof(g->alien_explode_hit_boss));

	for (int r = TUTORIAL_CENTER_ROW_START; r <= TUTORIAL_CENTER_ROW_END; r++) {
		for (int c = TUTORIAL_CENTER_COL_START; c <= TUTORIAL_CENTER_COL_END; c++) {
			g->alien_alive[r][c] = 1;
			g->alien_health[r][c] = 1;
		}
	}

	g->alien_alive[TUTORIAL_LEAVE_ROW][TUTORIAL_LEAVE_COL] = 1;
	g->alien_health[TUTORIAL_LEAVE_ROW][TUTORIAL_LEAVE_COL] = 1;

	g->alien_alive[TUTORIAL_SHOOTER_ROW][TUTORIAL_SHOOTER_COL] = 1;
	g->alien_health[TUTORIAL_SHOOTER_ROW][TUTORIAL_SHOOTER_COL] = 1;
	g->tutorial_grid_respawn_timer = TUTORIAL_RESPAWN_PERIOD_FRAMES;
	g->tutorial_shooter_respawn_timer = TUTORIAL_RESPAWN_PERIOD_FRAMES;
}

static void tutorial_respawn_alien(game_t *g, int r, int c) {
	g->alien_alive[r][c] = 1;
	g->alien_health[r][c] = 1;
	g->alien_hermit_regen[r][c] = 0;
	g->yellow_boss_marked[r][c] = 0;
	g->alien_explode_timer[r][c] = 0;
	g->alien_explode_points[r][c] = 0;
	g->alien_explode_hit_boss[r][c] = 0;
}

static int tutorial_center_pack_any_missing(const game_t *g) {
	for (int r = TUTORIAL_CENTER_ROW_START; r <= TUTORIAL_CENTER_ROW_END; r++) {
		for (int c = TUTORIAL_CENTER_COL_START; c <= TUTORIAL_CENTER_COL_END; c++) {
			if (!g->alien_alive[r][c]) return 1;
		}
	}
	return 0;
}

static void tutorial_respawn_center_pack(game_t *g) {
	for (int r = TUTORIAL_CENTER_ROW_START; r <= TUTORIAL_CENTER_ROW_END; r++) {
		for (int c = TUTORIAL_CENTER_COL_START; c <= TUTORIAL_CENTER_COL_END; c++) {
			tutorial_respawn_alien(g, r, c);
		}
	}
}

static int player_invulnerable(const game_t *g) {
	if (g->player_dying) return 0;
	return g->player_exit_fly_active || (g->player_iframe_timer > 0) || shield_power_active(g);
}

static int player_visible_with_iframes(const game_t *g) {
	if (!player_invulnerable(g)) return 1;
	return ((g->player_iframe_timer / 4) & 1) == 0;
}

static int player_shield_radius(const game_t *g) {
	int active_timer = 0;
	for (int i = 0; i < 5; i++) {
		if (g->powerup_type_slot[i] == POWERUP_SHIELD && g->powerup_slot_timer[i] > active_timer) {
			active_timer = g->powerup_slot_timer[i];
		}
	}

	int pulse = (active_timer / 5) & 3;
	int extra = (pulse == 1 || pulse == 2) ? 1 : 0;
	return PLAYER_SHIELD_RADIUS + extra;
}

static int projectile_hits_player_shield(const game_t *g, int px, int py) {
	if (!shield_power_active(g)) return 0;

	int cx = g->player_x + g->PLAYER.w / 2;
	int cy = g->player_y + g->PLAYER.h / 2;
	int dx = px - cx;
	int dy = py - cy;
	int r = player_shield_radius(g);
	return (dx * dx + dy * dy) <= (r * r);
}

static void render_player_shield_ring(const game_t *g, lfb_t *lfb) {
	if (!shield_power_active(g) || g->player_dying) return;

	int cx = g->player_x + g->PLAYER.w / 2;
	int cy = g->player_y + g->PLAYER.h / 2;
	int r = player_shield_radius(g);
	uint32_t ring_color = ((g->powerup_spawn_timer / 4) & 1) ? 0xFF66CCFF : 0xFF3399FF;

	for (int y = -r; y <= r; y++) {
		for (int x = -r; x <= r; x++) {
			int d = x * x + y * y;
			if (d <= (r * r) && d >= ((r - 1) * (r - 1))) {
				l_putpix(lfb, cx + x, cy + y, ring_color);
			}
		}
	}
}

static void trigger_player_death(game_t *g);

static void apply_player_damage(game_t *g, int damage) {
	if (damage <= 0) return;
	if (player_invulnerable(g)) return;

	if (g->boss_alive && !g->boss_dying) {
		g->boss_no_hit_taken = 0;
	}

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

static void apply_chariot_explosion_player_damage(game_t *g) {
	int lives_before = g->lives;
	apply_player_damage(g, 1);
	if (g->lives < lives_before && g->lives <= 0) {
		music_play_boom();
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

static void hermit_spawn_regen_alien(game_t *g, int r, int c) {
	g->alien_alive[r][c] = 1;
	g->alien_hermit_regen[r][c] = 1;
	g->alien_health[r][c] = 1;
	g->yellow_boss_marked[r][c] = 0;
	g->alien_explode_timer[r][c] = 0;
	g->alien_explode_points[r][c] = 0;
	g->alien_explode_hit_boss[r][c] = 0;
}

static void hermit_regenerate_aliens(game_t *g) {
	int total_aliens = AROWS * ACOLS;
	int alive = count_aliens_remaining(g);
	int missing = total_aliens - alive;
	if (missing <= 0) return;

	int regen_count = 1;
	if (g->level >= 3) regen_count = 2;
	if (regen_count > missing) regen_count = missing;
	if (regen_count <= 0) return;

	int spawned = 0;
	while (spawned < regen_count) {
		int target_row = -1;
		for (int r = 0; r < AROWS; r++) {
			for (int c = 0; c < ACOLS; c++) {
				if (!g->alien_alive[r][c]) {
					target_row = r;
					break;
				}
			}
			if (target_row >= 0) break;
		}
		if (target_row < 0) break;

		int cols[ACOLS];
		int col_count = 0;
		for (int c = 0; c < ACOLS; c++) {
			if (!g->alien_alive[target_row][c]) {
				cols[col_count++] = c;
			}
		}
		if (col_count <= 0) break;

		for (int i = col_count - 1; i > 0; i--) {
			int j = rand() % (i + 1);
			int tmp = cols[i];
			cols[i] = cols[j];
			cols[j] = tmp;
		}

		int first_col = cols[0];
		hermit_spawn_regen_alien(g, target_row, first_col);
		spawned++;
		if (spawned >= regen_count) break;

		int second_idx = -1;
		for (int i = 1; i < col_count; i++) {
			int c = cols[i];
			if (c < first_col - 1 || c > first_col + 1) {
				second_idx = i;
				break;
			}
		}
		if (second_idx < 0 && col_count > 1) {
			second_idx = 1;
		}

		if (second_idx >= 0) {
			hermit_spawn_regen_alien(g, target_row, cols[second_idx]);
			spawned++;
		}
	}
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
	g->alien_hermit_regen[r][c] = 0;
	g->yellow_boss_marked[r][c] = 0;
	g->alien_health[r][c] = 0;
	g->alien_explode_timer[r][c] = ALIEN_EXPLOSION_FRAMES;
	g->alien_explode_hit_boss[r][c] = 0;
	g->alien_explode_points[r][c] = award_alien_kill_points(g, killed_yellow_boss_alien);
}

static void kill_alien_cleanup_with_explosion(game_t *g, int r, int c) {
	if (!g->alien_alive[r][c]) return;
	g->alien_alive[r][c] = 0;
	g->alien_hermit_regen[r][c] = 0;
	g->yellow_boss_marked[r][c] = 0;
	g->alien_health[r][c] = 0;
	g->alien_explode_timer[r][c] = ALIEN_EXPLOSION_FRAMES;
	g->alien_explode_hit_boss[r][c] = 0;
	g->alien_explode_points[r][c] = 10;
	g->score += 10;
}

static void trigger_random_alien_cleanup_explosion(game_t *g) {
	int remaining = count_aliens_remaining(g);
	if (remaining <= 0) return;

	int pick = rand() % remaining;
	for (int r = 0; r < AROWS; r++) {
		for (int c = 0; c < ACOLS; c++) {
			if (!g->alien_alive[r][c]) continue;
			if (pick == 0) {
				kill_alien_cleanup_with_explosion(g, r, c);
				return;
			}
			pick--;
		}
	}
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

static int player_shot_damage_for_hit(game_t *g, bullet_t *shot) {
	int dmg;
	if (shot->pierce_active) {
		dmg = shot->damage_remaining;
		if (dmg < 0) dmg = 0;
	} else {
		dmg = g->player_damage;
		if (shot->critical) {
			dmg *= 2;
		}
	}
	return dmg;
}

static void trigger_critical_hit_banner(game_t *g, const bullet_t *shot) {
	(void)g;
	(void)shot;
}

static void consume_player_shot_damage(bullet_t *shot, int consumed) {
	if (!shot->pierce_active) return;
	if (consumed < 0) consumed = 0;
	shot->damage_remaining -= consumed;
	if (shot->damage_remaining <= 0) {
		shot->damage_remaining = 0;
		shot->alive = 0;
	}
}

static void handle_bunker_bullet_collisions(game_t *g, bullet_t *shot, int x_drift, int y_dir, int damage_radius);

static void handle_player_shot_collisions(game_t *g, bullet_t *shots, int spread_dir, int can_collect_powerup) {
	for (int si = 0; si < MAX_PSHOTS; si++) {
		if (!shots[si].alive) continue;

		int shot_spread_dir = spread_dir;
		if (shots[si].reflected == 2) shot_spread_dir = 1;
		else if (shots[si].reflected == 3) shot_spread_dir = -1;

		if (shots[si].reflected == 1) {
			handle_enemy_shot_collisions(g, &shots[si], 0, 1);
			continue;
		}

		if (can_collect_powerup && g->level == 0 && !any_powerup_active(g)) {
			for (int pi = 0; pi < TUTORIAL_POWERUP_COUNT; pi++) {
				int px, py;
				tutorial_powerup_icon_position(pi, &px, &py);
				int dx = shots[si].x - px;
				int dy = shots[si].y - py;
				if ((dx * dx + dy * dy) > 36) continue;

				for (int slot = 0; slot < 5; slot++) {
					g->powerup_slot_timer[slot] = 0;
				}
				g->powerup_slot_timer[0] = 600;
				g->powerup_type_slot[0] = tutorial_powerup_type_for_index(pi);

				// Picking up any powerup immediately suppresses the magician curse.
				g->magician_curse_timer = 0;
				g->magician_curse_announce_timer = 0;
				g->powerup_active = 0;
				g->powerup_timer = 0;
				shots[si].alive = 0;
				music_play_powerup();
				break;
			}
		}
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
				// Picking up any powerup immediately suppresses the magician curse.
				g->magician_curse_timer = 0;
				g->magician_curse_announce_timer = 0;
				g->powerup_active = 0;
				shots[si].alive = 0;
			}
		}
		if (!shots[si].alive) continue;

		if (g->boss_alive && !g->boss_dying && g->boss_type == BOSS_TYPE_MAGICIAN && g->magician_wave.active) {
			int orb_hit = 0;
			for (int bi = 0; bi < 5 && !orb_hit; bi++) {
				int bx = bullet_x_with_spread(&shots[si], bi, shot_spread_dir);
				int by = shots[si].y - bi;
				if (magician_wave_hits_point(g, bx, by)) {
					orb_hit = 1;
				}
			}
			if (orb_hit) {
				g->magician_wave.hp--;
				if (explosive_shot_active(g) || g->magician_wave.hp <= 0) {
					g->magician_wave.vx = 0;
					g->magician_wave.vy = -iabs_int(MAGICIAN_ORB_SPEED_Y);
					g->magician_wave.hp = 0;
				}
				trigger_critical_hit_banner(g, &shots[si]);
				shots[si].alive = 0;
			}
		}
		if (!shots[si].alive) continue;

		if (g->boss_alive && !g->boss_dying && g->boss_type == BOSS_TYPE_MAGICIAN) {
			int reflected = 0;
			for (int bi = 0; bi < 5 && !reflected; bi++) {
				int bx = bullet_x_with_spread(&shots[si], bi, shot_spread_dir);
				int by = shots[si].y - bi;
				if (magician_mirror_hits_point(&g->magician_mirror, bx, by) ||
						magician_mirror_hits_point(&g->magician_mirror_alt, bx, by)) {
					reflected = 1;
				}
			}
			if (reflected) {
				reflect_player_shot_downward(&shots[si]);
				shots[si].y += 2;
				continue;
			}
		}

		if (g->boss_attack_type == 2 && g->boss_bomb.alive && !g->boss_bomb.exploding) {
			int bomb_hit = 0;
			for (int bi = 0; bi < 5 && !bomb_hit; bi++) {
				int bx = bullet_x_with_spread(&shots[si], bi, shot_spread_dir);
				int by = shots[si].y - bi;
				int dx = bx - g->boss_bomb.x;
				int dy = by - g->boss_bomb.y;
				if ((dx * dx + dy * dy) <= 25) {
					bomb_hit = 1;
				}
			}
			if (bomb_hit) {
				trigger_critical_hit_banner(g, &shots[si]);
				shots[si].alive = 0;
				g->boss_bomb.hits_taken++;
				if (!g->boss_bomb.reversed && g->boss_bomb.hits_taken >= BOSS_BOMB_HITS_TO_REVERSE) {
					g->boss_bomb.reversed = 1;
					g->boss_bomb.dy = -BOSS_BOMB_SPEED;
				}
			}
		}
		if (!shots[si].alive) continue;

		for (int ai = 0; ai < MAX_TOWER_ASTEROIDS && shots[si].alive; ai++) {
			if (!g->tower_asteroid[ai].alive) continue;
			int asteroid_hit = 0;
			for (int bi = 0; bi < 5 && !asteroid_hit; bi++) {
				int bx = bullet_x_with_spread(&shots[si], bi, shot_spread_dir);
				int by = shots[si].y - bi;
				int dx = bx - g->tower_asteroid[ai].x;
				int dy = by - g->tower_asteroid[ai].y;
				if ((dx * dx + dy * dy) <= (TOWER_ASTEROID_RADIUS * TOWER_ASTEROID_RADIUS)) {
					asteroid_hit = 1;
				}
			}
			if (asteroid_hit) {
				trigger_critical_hit_banner(g, &shots[si]);
				int dmg = player_shot_damage_for_hit(g, &shots[si]);
				if (dmg <= 0) {
					shots[si].alive = 0;
					continue;
				}
				if (explosive_shot_active(g)) {
					start_tower_asteroid_explosion(g, ai);
				} else {
					g->tower_asteroid_hp[ai] -= dmg;
				}
				if (g->tower_asteroid_hp[ai] <= 0) {
					start_tower_asteroid_explosion(g, ai);
				}
				shots[si].alive = 0;
			}
		}
		if (!shots[si].alive) continue;

		if (g->boss_shield_active && g->boss_alive && !g->boss_dying) {
			int shield_hit = 0;
			int sx = boss_shield_x(g);
			int sy = boss_shield_y(g);
			for (int bi = 0; bi < 5 && !shield_hit; bi++) {
				int bx = bullet_x_with_spread(&shots[si], bi, shot_spread_dir);
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
				trigger_critical_hit_banner(g, &shots[si]);
				shots[si].alive = 0;
			}
		}
		if (!shots[si].alive) continue;

		if (g->boss_alive && !g->boss_dying && g->boss_type != BOSS_TYPE_YELLOW) {
			const sprite1r_t *BS = active_boss_sprite(g);
			int boss_hit = 0;
			int chariot_descending = (g->boss_type == BOSS_TYPE_CHARIOT && g->boss_power_active &&
					g->boss_attack_type == CHARIOT_CHARGE_ATTACK && g->boss_special_timer <= 0);
			for (int bi = 0; bi < 5 && !boss_hit; bi++) {
				if (bullet_hits_sprite(BS,
							g->boss_x, g->boss_y,
							bullet_x_with_spread(&shots[si], bi, shot_spread_dir), shots[si].y - bi)) {
					boss_hit = 1;
				}
			}
			if (boss_hit) {
				trigger_critical_hit_banner(g, &shots[si]);
				int dmg = player_shot_damage_for_hit(g, &shots[si]);
				if (g->boss_type == BOSS_TYPE_MAGICIAN && magician_mirror_attack_active(g)) {
					dmg += 1;
				}
				g->boss_health -= dmg;
				if (g->boss_health <= 0) {
					g->boss_health = 0;
					boss_trigger_death(g, double_shot_active(g) ? 1000 : 500);
				} else if (chariot_descending) {
					g->chariot_charge_damage += 1;
					int required_hits = explosive_shot_active(g) ? 1 : 3;
					if (g->chariot_charge_damage >= required_hits) {
						// Interrupt charge: explode where hit, then reset to top.
						g->boss_special_x = g->boss_x;
						g->boss_special_y = g->boss_y;
						g->boss_special_timer = CHARIOT_CHARGE_EXPLOSION_FRAMES;
						music_play_boom_long();
						g->boss_special_hit_applied = 1;
						apply_chariot_charge_explosion_to_aliens(
								g,
								g->boss_x + BS->w / 2,
								g->boss_y + BS->h / 2,
								CHARIOT_CHARGE_EXPLOSION_RADIUS
								);

						int self_damage = g->boss_max_health / 10;
						if (self_damage < 1) self_damage = 1;
						g->boss_health -= self_damage;
						if (g->boss_health <= 0) {
							g->boss_health = 0;
							boss_trigger_death(g, double_shot_active(g) ? 1000 : 500);
						}
					}
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
								bullet_x_with_spread(&shots[si], bi, shot_spread_dir), shots[si].y - bi)) {
						bullet_hit = 1;
					}
				}
				if (bullet_hit) {
					trigger_critical_hit_banner(g, &shots[si]);
					if (shots[si].pierce_active && shots[si].last_hit_r == r && shots[si].last_hit_c == c) {
						continue;
					}

					int hp_before = g->alien_health[r][c];
					int dmg = player_shot_damage_for_hit(g, &shots[si]);
					if (dmg <= 0) {
						shots[si].alive = 0;
						hit = 1;
						continue;
					}

					g->alien_health[r][c] -= dmg;
					if (g->alien_health[r][c] <= 0) {
						kill_alien_with_explosion(g, r, c);
						if (explosive_shot_active(g)) {
							trigger_explosive_chain(g, r, c);
						}
					}

					if (shots[si].pierce_active) {
						shots[si].last_hit_r = r;
						shots[si].last_hit_c = c;
						consume_player_shot_damage(&shots[si], hp_before);
						hit = shots[si].alive ? 0 : 1;
					} else {
						shots[si].alive = 0;
						hit = 1;
					}
				}
			}
		}
		if (!hit && shots[si].alive) {
			handle_bunker_bullet_collisions(g, &shots[si], shot_spread_dir, -1, 3);
		}
	}
}

static void handle_enemy_shot_collisions(game_t *g, bullet_t *shot, int x_drift, int shield_blockable) {
	if (!shot->alive) return;
	if (g->player_dying) return;

	if (shield_blockable && projectile_hits_player_shield(g, shot->x, shot->y)) {
		shot->alive = 0;
		return;
	}

	if (bullet_hits_sprite(&g->PLAYER, g->player_x, g->player_y, shot->x, shot->y)) {
		if (player_invulnerable(g)) {
			shot->alive = 0;
			return;
		}
		apply_player_damage(g, 1);
		shot->alive = 0;
	} else {
		handle_bunker_bullet_collisions(g, shot, x_drift, 1, 3);
	}
}

static void handle_bunker_bullet_collisions(game_t *g, bullet_t *shot, int x_drift, int y_dir, int damage_radius) {
	if (!shot->alive) return;
	// No bunker collisions on level 0 (tutorial)
	if (g->level == 0) return;
	for (int i = 0; i < 4 && shot->alive; i++) {
		sprite1r_t *b = g->bunkers[i];
		int bx = g->bunker_x[i], by = g->bunker_y;
		for (int bi = 0; bi < 5 && shot->alive; bi++) {
			int px = shot->x + (x_drift * (bi / 2));
			int py = shot->y + (y_dir * bi);
			if (bullet_hits_sprite(b, bx, by, px, py)) {
				bunker_damage(b, px - bx, py - by, damage_radius);
				shot->alive = 0;
			}
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
	g->lives = g->hard_mode ? 1 : PLAYER_LIVES;
	g->player_speed = PLAYER_BASE_SPEED;
	g->fire_speed_bonus = 0;
	g->player_damage = PLAYER_BASE_DAMAGE;
	g->shots_fired_counter = 0;
	g->pierce_unlocked = 0;
	g->points_unlocked = 0;
	g->crit_unlocked = 0;
	g->unlock_order_count = 0;
	for (int i = 0; i < 3; i++) g->unlock_order[i] = -1;
	reset_shop_state(g);
	reset_powerup_slots(g, POWERUP_DOUBLE_SHOT);
}

static int is_one_time_shop_item(shop_item_type_t type) {
	return type == SHOP_ITEM_PIERCE || type == SHOP_ITEM_POINTS || type == SHOP_ITEM_CRIT;
}

static void add_unlock_in_order(game_t *g, shop_item_type_t type) {
	for (int i = 0; i < g->unlock_order_count; i++) {
		if (g->unlock_order[i] == (int)type) return;
	}
	if (g->unlock_order_count >= 3) return;
	g->unlock_order[g->unlock_order_count++] = (int)type;
}

static void reset_boss_selection_history(game_t *g) {
	for (int i = 0; i < BOSS_TYPE_COUNT; i++) {
		g->boss_pick_count[i] = 0;
	}
	g->last_random_boss_type = -1;
}

static int pick_random_boss_type(game_t *g) {
	int pool[BOSS_TYPE_COUNT];
	int pool_count = 0;

	// First pass: choose among unseen bosses.
	for (int i = 0; i < BOSS_TYPE_COUNT; i++) {
		if (g->boss_pick_count[i] != 0) continue;
		pool[pool_count++] = i;
	}

	// Fallback: all bosses seen, so pick any except the most recent one.
	if (pool_count == 0) {
		for (int i = 0; i < BOSS_TYPE_COUNT; i++) {
			if (i == g->last_random_boss_type) continue;
			pool[pool_count++] = i;
		}
	}

	// Final fallback for safety (e.g., single-type edge case).
	if (pool_count == 0) {
		for (int i = 0; i < BOSS_TYPE_COUNT; i++) {
			pool[pool_count++] = i;
		}
	}

	int picked = pool[rand() % pool_count];
	if (picked >= 0 && picked < BOSS_TYPE_COUNT) {
		g->boss_pick_count[picked]++;
		g->last_random_boss_type = picked;
	}
	return picked;
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
		case SHOP_ITEM_PIERCE:     return "PIERCE";
		case SHOP_ITEM_POINTS:     return "POINTS";
		case SHOP_ITEM_CRIT:       return "CRIT";
		default:                   return "ITEM";
	}
}

static void enter_shop(game_t *g) {
	g->in_shop = 1;
	g->shop_next_level = g->level + 1;
	g->shop_count++;
	reset_powerup_slots(g, POWERUP_DOUBLE_SHOT);
	g->powerup_active = 0;
	g->powerup_timer = 0;
	g->powerup_spawn_timer = 0;
	if (!g->hard_mode && g->lives < PLAYER_LIVES) g->lives = PLAYER_LIVES;

	int to_node = overworld_shop_node_after_level(g->level);
	if (to_node >= 0) {
		int from_node = g->overworld_current_node;
		if (from_node >= OVERWORLD_LEVEL_COUNT) from_node = OVERWORLD_LEVEL_COUNT - 1;
		if (from_node < 0) from_node = -1;

		g->overworld_cutscene_active = 1;
		g->overworld_cutscene_timer = OVERWORLD_TOTAL_FRAMES;
		g->overworld_cutscene_from_node = from_node;
		g->overworld_cutscene_to_node = to_node;
		g->overworld_current_node = to_node;
	}

	shop_item_type_t shop_pool[6];
	int shop_pool_count = 0;
	shop_pool[shop_pool_count++] = SHOP_ITEM_FIRE_SPEED;
	if (!g->hard_mode) {
		shop_pool[shop_pool_count++] = SHOP_ITEM_LIFE;
	}
	shop_pool[shop_pool_count++] = SHOP_ITEM_DAMAGE;
	if (!g->pierce_unlocked) {
		shop_pool[shop_pool_count++] = SHOP_ITEM_PIERCE;
	}
	if (!g->points_unlocked) {
		shop_pool[shop_pool_count++] = SHOP_ITEM_POINTS;
	}
	if (!g->crit_unlocked) {
		shop_pool[shop_pool_count++] = SHOP_ITEM_CRIT;
	}

	int price_multiplier = g->shop_count; // Base * shops seen
	int spacing = 60;
	int start_x = LW / 2 - spacing;
	int y = LH / 2;

	int used_type[SHOP_ITEM_COUNT] = {0};
	int chosen_type[MAX_SHOP_ITEMS];
	int chosen_count = 0;

	// Guarantee at least one good item when at least one is still available.
	int good_candidates[3];
	int good_count = 0;
	if (!g->pierce_unlocked) good_candidates[good_count++] = SHOP_ITEM_PIERCE;
	if (!g->points_unlocked) good_candidates[good_count++] = SHOP_ITEM_POINTS;
	if (!g->crit_unlocked) good_candidates[good_count++] = SHOP_ITEM_CRIT;
	int one_time_picks = 0;
	if (good_count > 0) {
		int good_pick = good_candidates[rand() % good_count];
		chosen_type[chosen_count++] = good_pick;
		used_type[good_pick] = 1;
		one_time_picks = 1;
	}

	// Fill remaining slots with unique picks from the pool.
	while (chosen_count < MAX_SHOP_ITEMS) {
		shop_item_type_t candidates[6];
		int candidate_count = 0;
		for (int p = 0; p < shop_pool_count; p++) {
			shop_item_type_t t = shop_pool[p];
			if (used_type[t]) continue;
			if (is_one_time_shop_item(t) && one_time_picks >= 2) continue;
			candidates[candidate_count++] = t;
		}

		if (candidate_count == 0) {
			break;
		}

		shop_item_type_t picked = candidates[rand() % candidate_count];
		chosen_type[chosen_count++] = picked;
		used_type[picked] = 1;
		if (is_one_time_shop_item(picked)) one_time_picks++;
	}

	// Randomize display order while preserving uniqueness/guarantees.
	for (int i = chosen_count - 1; i > 0; i--) {
		int j = rand() % (i + 1);
		int tmp = chosen_type[i];
		chosen_type[i] = chosen_type[j];
		chosen_type[j] = tmp;
	}

	for (int i = 0; i < MAX_SHOP_ITEMS; i++) {
		shop_item_type_t picked = (i < chosen_count) ? (shop_item_type_t)chosen_type[i] : SHOP_ITEM_FIRE_SPEED;

		g->shop_items[i].active = 1;
		g->shop_items[i].type = picked;
		g->shop_items[i].price = 500 * price_multiplier;
		g->shop_items[i].x = start_x + i * spacing;
		g->shop_items[i].y = y;
	}

	clear_player_shots(g);
	g->ashot.alive = 0;
	clear_boss_projectiles(g);
	g->fire_cooldown = 0;

	// Position player on left side at the normal gameplay baseline.
	g->player_x = 10;
	g->player_y = LH - 30;

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
						else if (g->shop_items[it].type == SHOP_ITEM_PIERCE) {
							g->pierce_unlocked = 1;
							add_unlock_in_order(g, SHOP_ITEM_PIERCE);
						} else if (g->shop_items[it].type == SHOP_ITEM_POINTS) {
							g->points_unlocked = 1;
							add_unlock_in_order(g, SHOP_ITEM_POINTS);
						} else if (g->shop_items[it].type == SHOP_ITEM_CRIT) {
							g->crit_unlocked = 1;
							add_unlock_in_order(g, SHOP_ITEM_CRIT);
						}
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
		} else if (g->shop_items[i].type == SHOP_ITEM_PIERCE) {
			icon = &g->SHOP_PIERCE;
			icon_color = 0xFF66CCFF;
		}
		if (icon) {
			int icon_x = center_x - icon->w / 2;
			int icon_y = center_y - icon->h / 2;
			draw_sprite1r(lfb, icon, icon_x, icon_y, icon_color);
		} else if (g->shop_items[i].type == SHOP_ITEM_POINTS) {
			draw_points_upgrade_icon(lfb, center_x, center_y);
		} else if (g->shop_items[i].type == SHOP_ITEM_CRIT) {
			draw_crit_upgrade_icon(lfb, center_x, center_y);
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

	// Exit sign in shop.
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

		const char *heal_label = "FULLY HEALED";
		int heal_w = text_width_5x5(heal_label, 1);
		l_draw_text(lfb, (LW - heal_w) / 2, 5, heal_label, 1, 0xFF3CFF68);
	}

	// PLAYER 1 label and health bar
	render_player_health_bar(g, lfb);
	render_fps_counter(lfb);
}

static int aliens_remaining(const game_t *g) {
	for (int r = 0; r < AROWS; r++) for (int c = 0; c < ACOLS; c++) if (g->alien_alive[r][c]) return 1;
	return 0;
}

static const char *credits_lines[] = {
	"LEAD DEVELOPER: COLIN",
	"PLAYTESTERS: COLIN, JOSH 2, MICHAEL",
	"HARDWARE SUPPORT: MICHAEL",
	"LINUX SUPPORT: JOSH 2",
	"COMPATABILITY: JOSH 2",
	"",
	"SPECIAL THANKS:",
	"ECE 554",
	"GEORGE",
};

static int credits_text_height(void) {
	return (int)(sizeof(credits_lines) / sizeof(credits_lines[0])) * CREDITS_LINE_SPACING;
}

static int credits_total_height(const game_t *g) {
	const sprite1r_t *SK = &g->ALIEN_A;
	int alien_h = SK->h * 6;
	return credits_text_height() + CREDITS_ALIEN_GAP + alien_h;
}

static void render_credits_screen(const game_t *g, lfb_t *lfb) {
	int y = g->credits_scroll_y;
	int yy = g->credits_scroll_y + CREDITS_SCROLL_SPEED;
	for (int i = 0; i < (int)(sizeof(credits_lines) / sizeof(credits_lines[0])); i++) {
		int line_w = text_width_5x5(credits_lines[i], 1);
		l_draw_text(lfb, (LW - line_w) / 2, yy + i * CREDITS_LINE_SPACING, credits_lines[i], 1, 0xFF000000);
		l_draw_text(lfb, (LW - line_w) / 2, y + i * CREDITS_LINE_SPACING, credits_lines[i], 1, 0xFFBFBFBF);
	}

	const sprite1r_t *SK = ((g->credits_scroll_y / 8) & 1) ? &g->ALIEN_B : &g->ALIEN_A;
	int scale = 6;
	int sw = SK->w * scale;
	int sh = SK->h * scale;
	int alien_x = (LW - sw) / 2;
	int alien_y = y + credits_text_height() + CREDITS_ALIEN_GAP;
	draw_sprite1r_scaled(lfb, SK, alien_x, alien_y, 0xFF8000FF, scale);
}

static void bunkers_rebuild(game_t *g);

static void setup_practice_run(game_t *g, int boss_type) {
	if (boss_type < 0 || boss_type >= BOSS_TYPE_COUNT) return;
	if (g->practice_level_selection < PRACTICE_LEVEL_MIN) g->practice_level_selection = PRACTICE_LEVEL_MIN;
	if (g->practice_level_selection > PRACTICE_LEVEL_MAX) g->practice_level_selection = PRACTICE_LEVEL_MAX;
	g->hard_mode = 0;
	g->tutorial_mode = 0;
	g->lives = PLAYER_LIVES;
	g->start_screen = 0;
	g->practice_menu_active = 0;
	g->practice_run_active = 1;
	g->overworld_current_node = -1;
	g->practice_return_delay_timer = 0;
	g->forced_boss_type = boss_type;
	reset_win_state(g);
	setup_level(g, g->practice_level_selection, 1);
	g->boss_intro_active = 1;
	g->boss_intro_timer = BOSS_INTRO_FRAMES;
}

static void setup_level(game_t *g, int level, int reset_score) {
	do_clear = 1;
	if (reset_score) g->score = 0;
	g->level = level;
	g->level_complete = 0;
	g->level_complete_timer = 0;
	g->level_just_completed = 0;
	g->boss_intro_active = 0;
	g->boss_intro_timer = 0;
	g->overworld_cutscene_active = 0;
	g->overworld_cutscene_timer = 0;
	g->overworld_cutscene_from_node = -1;
	g->overworld_cutscene_to_node = 0;
	if (level <= 0 || g->practice_run_active) {
		g->overworld_current_node = -1;
	}

	g->powerup_active = 0;
	g->powerup_timer = 0;
	g->powerup_spawn_timer = 0;
	reset_powerup_slots(g, POWERUP_DOUBLE_SHOT);
	g->magician_curse_timer = 0;
	g->magician_curse_pending = 0;
	g->magician_shot_alternate_side = 0;
	g->magician_curse_announce_timer = 0;
	g->magician_curse_announce_shown = 0;

	g->player_x = 5;  // Spawn on left side
	g->player_prev_x = 5;  // Spawn on left side
	g->player_y = LH - 30;
	g->player_prev_y = LH - 30;
	g->player_exit_fly_active = 0;
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
		memset(g->alien_hermit_regen, 0, sizeof(g->alien_hermit_regen));
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

	// Level 0: tutorial level with static instructional alien layout.
	if (level == 0) {
		tutorial_reset_layout(g);
	} else {
		memset(g->alien_alive, 1, sizeof(g->alien_alive));
		memset(g->alien_hermit_regen, 0, sizeof(g->alien_hermit_regen));
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

	g->alien_origin_x = (level == 0) ? 58 : 50;
	g->alien_origin_y = 30;
	g->alien_dx = 1;
	g->alien_step_px = 2;
	g->alien_drop_px = 6;
	g->alien_frame = 0;
	g->alien_timer = 0;
	g->alien_period = 20 - (level - 1) * 2; // Faster aliens at higher levels
	g->alien_cleanup_timer = 0;
	if (g->alien_period < 5) g->alien_period = 5; // Minimum period

	// Boss alien setup (separate from regular aliens)
	// No boss on level 0 (tutorial)
	g->boss_alive = (level > 0) ? 1 : 0;
	if (level > 0) {
		if (g->forced_boss_type >= 0 && g->forced_boss_type < BOSS_TYPE_COUNT) {
			g->boss_type = g->forced_boss_type;
		} else {
			g->boss_type = pick_random_boss_type(g);
		}
	} else {
		g->boss_type = BOSS_TYPE_CLASSIC;
	}
	if (level > 0 && g->boss_type == BOSS_TYPE_MAGICIAN) {
		g->magician_curse_pending = 1;
	}
	g->boss_max_health = BOSS_MAX_HEALTH(level);
	g->boss_health = g->boss_max_health;
	clear_yellow_boss_state(g);
	if (g->boss_type == BOSS_TYPE_YELLOW) {
		select_yellow_boss_aliens(g);
	}
	const sprite1r_t *BS = active_boss_sprite(g);
	g->boss_x = (LW - BS->w) / 2;
	g->boss_y = 18;
	g->boss_start_y = g->boss_y;
	g->boss_dx = 1;
	g->boss_dying = 0;
	g->boss_death_timer = 0;
	g->boss_explode_points = 0;
	g->boss_no_hit_taken = (level > 0) ? 1 : 0;
	g->perfect_text_timer = 0;
	g->boss_shield_active = (g->boss_type == BOSS_TYPE_HERMIT && level >= 3) ? 1 : 0;
	g->boss_frame = 0;
	g->boss_timer = 0;
	g->boss_period = BOSS_PERIOD(level); // Faster boss at higher levels

	clear_player_shots(g);
	g->ashot.alive = 0;
	clear_boss_projectiles(g);
	g->fire_cooldown = 0;

	// Initialize boss power system
	g->boss_power_timer = 0;
	g->boss_power_max = 600;  // 10 seconds at 60 FPS
	g->boss_power_active = 0;
	g->boss_power_cooldown = 0;
	g->boss_laser_last_hit_y = -1000;  // Far off screen
	g->boss_attack_type = (g->boss_type == BOSS_TYPE_BLUE) ? 2 :
		(g->boss_type == BOSS_TYPE_YELLOW) ? YELLOW_BOSS_ATTACK_SHUFFLE :
		(g->boss_type == BOSS_TYPE_TOWER) ? TOWER_WALL_ATTACK :
		(g->boss_type == BOSS_TYPE_HERMIT) ? 0 :
		(g->boss_type == BOSS_TYPE_CHARIOT) ? CHARIOT_CHARGE_ATTACK :
		(g->boss_type == BOSS_TYPE_MAGICIAN) ? MAGICIAN_MIRROR_ATTACK : 0;  // Current attack being executed
	g->next_boss_attack_type = (g->boss_type == BOSS_TYPE_BLUE) ? 2 :
		(g->boss_type == BOSS_TYPE_YELLOW) ? YELLOW_BOSS_ATTACK_SHUFFLE :
		(g->boss_type == BOSS_TYPE_TOWER) ? TOWER_WALL_ATTACK :
		(g->boss_type == BOSS_TYPE_HERMIT) ? 0 :
		(g->boss_type == BOSS_TYPE_CHARIOT) ? CHARIOT_CHARGE_ATTACK :
		(g->boss_type == BOSS_TYPE_MAGICIAN) ? MAGICIAN_MIRROR_ATTACK : 0;  // Next attack to be charged
	g->boss_green_laser_last_hit_y = -1000;  // Far off screen
	g->tower_wall_active = 0;
	g->tower_wall_timer = 0;
	g->tower_wall_left = 0;
	g->tower_wall_right = LW;
	refill_sprite_full(&g->BOSS_SHIELD);
	g->boss_shield_active = (g->boss_type == BOSS_TYPE_HERMIT && level >= 3) ? 1 : 0;

	if (level > 0 && !g->practice_run_active) {
		int to_node = overworld_node_for_level(level);
		int from_node = g->overworld_current_node;

		if (to_node >= OVERWORLD_LEVEL_COUNT) to_node = OVERWORLD_LEVEL_COUNT - 1;
		if (from_node >= OVERWORLD_LEVEL_COUNT) from_node = OVERWORLD_LEVEL_COUNT - 1;
		if (from_node < 0) from_node = -1;

		g->overworld_cutscene_active = 1;
		g->overworld_cutscene_timer = OVERWORLD_TOTAL_FRAMES;
		g->overworld_cutscene_from_node = from_node;
		g->overworld_cutscene_to_node = to_node;
		g->overworld_current_node = to_node;
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
	g->scene = 0;
	g->prev_scene = 0;
	g->PLAYER  = make_sprite_from_ascii(player_rows, 8);
	g->ALIEN_A = make_sprite_from_ascii(alienA_rows, 8);
	g->ALIEN_B = make_sprite_from_ascii(alienB_rows, 8);
	g->BOSS_A  = make_sprite_from_ascii(bossA_rows, 10);
	g->BOSS_B  = make_sprite_from_ascii(bossB_rows, 10);
	g->BOSS2_A = make_sprite_from_ascii(boss2A_rows, 10);
	g->BOSS2_B = make_sprite_from_ascii(boss2B_rows, 10);
	g->BOSS3_A = make_sprite_from_ascii(boss3A_rows, 10);
	g->BOSS3_B = make_sprite_from_ascii(boss3B_rows, 10);
	g->BOSS4_A = make_sprite_from_ascii(boss4A_rows, 10);
	g->BOSS4_B = make_sprite_from_ascii(boss4B_rows, 10);
	g->BOSS5_A = make_sprite_from_ascii(boss5A_rows, 10);
	g->BOSS5_B = make_sprite_from_ascii(boss5B_rows, 10);
	g->BOSS6_A = make_sprite_from_ascii(boss6A_rows, 10);
	g->BOSS6_B = make_sprite_from_ascii(boss6B_rows, 10);
	g->BOSS_SHIELD = make_filled_sprite(g->BOSS4_A.w, 6);
	g->SHOP_LIFE = make_sprite_from_ascii(shop_life_rows, 7);
	g->SHOP_FIRE = make_sprite_from_ascii(shop_fire_rows, 7);
	g->SHOP_MOVE = make_sprite_from_ascii(shop_move_rows, 7);
	g->SHOP_DMG  = make_sprite_from_ascii(shop_dmg_rows, 7);
	g->SHOP_PIERCE = make_sprite_from_ascii(shop_pierce_rows, 7);

	bunkers_rebuild(g);

	g->bunker_x[0] = 55;  g->bunker_x[1] = 115; g->bunker_x[2] = 175; g->bunker_x[3] = 235;
	g->bunker_y = LH - 65;

	g->game_over = 0;
	g->game_over_score = 0;
	g->start_screen = 1;
	g->start_screen_delay_timer = 0;
	g->main_menu_selection = 0;
	g->difficulty_menu_active = 0;
	g->difficulty_menu_selection = 0;
	g->tutorial_mode = 0;
	g->credits_screen_active = 0;
	g->credits_scroll_y = LH;
	g->practice_menu_active = 0;
	g->practice_menu_selection = 0;
	g->practice_level_selection = PRACTICE_LEVEL_MIN;
	g->practice_preview_timer = BOSS_INTRO_FRAMES;
	g->practice_run_active = 0;
	g->practice_return_delay_timer = 0;
	g->forced_boss_type = -1;
	reset_boss_selection_history(g);
	reset_player_progression(g);

	setup_level(g, g->tutorial_mode ? 0 : START_LEVEL, 1);
}

void game_reset(game_t *g) {
	g->game_over = 0;
	g->game_over_score = 0;
	g->game_over_delay_timer = 0;
	g->player_dying = 0;
	g->player_death_timer = 0;
	g->player_iframe_timer = 0;
	g->player_exit_fly_active = 0;
	g->start_screen = 0;
	g->start_screen_delay_timer = 0;
	g->main_menu_selection = 0;
	g->difficulty_menu_active = 0;
	g->difficulty_menu_selection = 0;
	g->credits_screen_active = 0;
	g->credits_scroll_y = LH;
	g->practice_menu_active = 0;
	g->practice_menu_selection = 0;
	g->practice_level_selection = PRACTICE_LEVEL_MIN;
	g->practice_preview_timer = BOSS_INTRO_FRAMES;
	g->practice_run_active = 0;
	g->practice_return_delay_timer = 0;
	g->forced_boss_type = -1;
	reset_boss_selection_history(g);
	g->paused = 0;
	reset_win_state(g);
	reset_player_progression(g);
	g->overworld_current_node = -1;

	setup_level(g, g->tutorial_mode ? 0 : START_LEVEL, 1);
}

void game_update(game_t *g, uint32_t buttons, uint32_t vsync_counter) {
	// Handle reset button (anytime during gameplay)
	static uint32_t prev_buttons = 0;
	uint32_t old_buttons = prev_buttons;
	if ((buttons & BTN_RESET) && !(old_buttons & BTN_RESET)) {
		game_reset(g);
		g->start_screen = 1;
		g->start_screen_delay_timer = 30;
	}

	// Handle pause toggle (active gameplay and boss intro)
	if (!g->start_screen && !g->game_over && !g->level_complete && !g->in_shop && !g->win_screen && !g->overworld_cutscene_active) {
		if ((buttons & BTN_PAUSE) && !(old_buttons & BTN_PAUSE)) {
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
	if (g->perfect_text_timer > 0) {
		g->perfect_text_timer--;
	}
	if (g->magician_curse_announce_timer > 0) {
		g->magician_curse_announce_timer--;
	}
	{
		int magician_fight_active = !g->boss_intro_active &&
			g->boss_alive && !g->boss_dying &&
			g->boss_type == BOSS_TYPE_MAGICIAN;
		int player_has_powerup = any_powerup_active(g);
		if (magician_fight_active && g->level >= 3 && !player_has_powerup) {
			if (!g->magician_curse_announce_shown) {
				g->magician_curse_announce_timer = MAGICIAN_CURSE_TEXT_DURATION_FRAMES;
				g->magician_curse_announce_shown = 1;
			}

			// Curse is active only while the mirror special attack is active.
			if (magician_mirror_attack_active(g)) {
				g->magician_curse_timer = 1;
			} else {
				g->magician_curse_timer = 0;
			}
		} else {
			g->magician_curse_timer = 0;
			g->magician_curse_announce_timer = 0;
		}
	}

	if (g->start_screen) {
		if (g->start_screen_delay_timer > 0) g->start_screen_delay_timer--;
		if (g->start_screen_delay_timer == 0) {
			int up_pressed = (buttons & BTN_UP) && !(old_buttons & BTN_UP);
			int down_pressed = (buttons & BTN_DOWN) && !(old_buttons & BTN_DOWN);
			int left_pressed = (buttons & BTN_LEFT) && !(old_buttons & BTN_LEFT);
			int right_pressed = (buttons & BTN_RIGHT) && !(old_buttons & BTN_RIGHT);
			int select_pressed = (buttons & BTN_FIRE) && !(old_buttons & BTN_FIRE);

			if (g->credits_screen_active) {
				if (select_pressed) {
					do_clear = 1;
					g->credits_screen_active = 0;
					g->main_menu_selection = 0;
				} else {
					if ((vsync_counter & 1u) == 0u) {
						g->credits_scroll_y -= CREDITS_SCROLL_SPEED;
					}
					if (g->credits_scroll_y + credits_total_height(g) < 0) {
						g->credits_screen_active = 0;
						g->main_menu_selection = 0;
					}
				}
			} else if (!g->practice_menu_active && !g->difficulty_menu_active) {
				if (up_pressed || down_pressed) {
					if (up_pressed) {
						do_clear = 1;
						g->main_menu_selection--;
					}
					if (down_pressed) {
						do_clear = 1;
						g->main_menu_selection++;
					}
					if (g->main_menu_selection < 0) {
						g->main_menu_selection = MAIN_MENU_COUNT - 1;
					}
					if (g->main_menu_selection >= MAIN_MENU_COUNT) {
						g->main_menu_selection = 0;
					}
				}
				if (select_pressed) {
					do_clear = 1;
					if (g->main_menu_selection == 0) {
						g->difficulty_menu_active = 1;
						g->difficulty_menu_selection = g->hard_mode ? 2 : 1;
					} else if (g->main_menu_selection == 1) {
						g->hard_mode = 0;
						g->tutorial_mode = 0;
						g->lives = PLAYER_LIVES;
						g->practice_menu_active = 1;
						g->practice_menu_selection = 0;
						g->practice_level_selection = PRACTICE_LEVEL_MIN;
						g->practice_preview_timer = BOSS_INTRO_FRAMES;
					} else {
						g->credits_screen_active = 1;
						g->credits_scroll_y = LH;
					}
				}
			} else if (g->difficulty_menu_active) {
				if (up_pressed) {
					do_clear = 1;
					g->difficulty_menu_selection--;
					if (g->difficulty_menu_selection < 0) g->difficulty_menu_selection = DIFFICULTY_MENU_COUNT - 1;
				}
				if (down_pressed) {
					do_clear = 1;
					g->difficulty_menu_selection++;
					if (g->difficulty_menu_selection >= DIFFICULTY_MENU_COUNT) g->difficulty_menu_selection = 0;
				}
				if (select_pressed) {
					do_clear = 1;
					if (g->difficulty_menu_selection == 3) {
						g->difficulty_menu_active = 0;
						g->main_menu_selection = 0;
					} else {
						g->tutorial_mode = (g->difficulty_menu_selection == 0);
						g->hard_mode = (g->difficulty_menu_selection == 2);
						g->difficulty_menu_active = 0;
						game_reset(g);
					}
				}
			} else {
				if (left_pressed) {
					do_clear = 1;
					g->practice_level_selection--;
					if (g->practice_level_selection < PRACTICE_LEVEL_MIN) g->practice_level_selection = PRACTICE_LEVEL_MAX;
				}
				if (right_pressed) {
					do_clear = 1;
					g->practice_level_selection++;
					if (g->practice_level_selection > PRACTICE_LEVEL_MAX) g->practice_level_selection = PRACTICE_LEVEL_MIN;
				}
				if (up_pressed) {
					do_clear = 1;
					g->practice_menu_selection--;
					if (g->practice_menu_selection < 0) g->practice_menu_selection = PRACTICE_MENU_COUNT - 1;
					g->practice_preview_timer = BOSS_INTRO_FRAMES;
				}
				if (down_pressed) {
					do_clear = 1;
					g->practice_menu_selection++;
					if (g->practice_menu_selection >= PRACTICE_MENU_COUNT) g->practice_menu_selection = 0;
					g->practice_preview_timer = BOSS_INTRO_FRAMES;
				}
				if (g->practice_menu_selection < BOSS_TYPE_COUNT) {
					if (g->practice_preview_timer > 0) g->practice_preview_timer--;
				} else {
					g->practice_preview_timer = BOSS_INTRO_FRAMES;
				}

				if (select_pressed) {
					do_clear = 1;
					if (g->practice_menu_selection == PRACTICE_EXIT_INDEX) {
						g->practice_menu_active = 0;
						g->main_menu_selection = 0;
					} else {
						int selected_boss_type = practice_menu_boss_type_for_index(g->practice_menu_selection);
						setup_practice_run(g, selected_boss_type);
					}
				}
			}
		}
		return;
	}
	if (g->level_complete) {
		if (g->level_complete_timer > 0) g->level_complete_timer--;
		if (g->level_complete_timer == 0) {
			g->level_complete = 0;
			g->level_complete_timer = 0;
			if (g->practice_run_active) {
				g->start_screen = 1;
				g->practice_menu_active = 1;
				g->practice_run_active = 0;
				g->forced_boss_type = -1;
				g->practice_preview_timer = BOSS_INTRO_FRAMES;
				reset_player_progression(g);
				setup_level(g, 0, 1);
			} else if (g->level == 0) {
				// Tutorial mode ends after tutorial fly-up and returns to difficulty select.
				if (g->tutorial_mode) {
					g->start_screen = 1;
					g->start_screen_delay_timer = 0;
					g->main_menu_selection = 0;
					g->difficulty_menu_active = 1;
					g->difficulty_menu_selection = 0;
					g->tutorial_mode = 0;
					reset_player_progression(g);
					setup_level(g, 0, 1);
				} else {
					// Legacy level-0 flow when not launched from tutorial mode.
					setup_level(g, START_LEVEL, 0);
				}
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
		if (g->game_over_delay_timer == 0) {
			if (g->practice_run_active) {
				g->game_over = 0;
				g->game_over_score = 0;
				g->start_screen = 1;
				g->start_screen_delay_timer = 30;
				g->practice_menu_active = 1;
				g->practice_run_active = 0;
				g->forced_boss_type = -1;
				g->practice_preview_timer = BOSS_INTRO_FRAMES;
				reset_player_progression(g);
				setup_level(g, 0, 1);
			} else if (buttons & BTN_FIRE) {
				g->game_over = 0;
				g->game_over_score = 0;
				g->start_screen = 1;
				g->start_screen_delay_timer = 30;
				reset_player_progression(g);
				setup_level(g, 0, 1);
			}
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

		if (g->win_prompt_ready) {
			if (g->practice_run_active) {
				g->start_screen = 1;
				g->start_screen_delay_timer = 30;
				g->practice_menu_active = 1;
				g->practice_run_active = 0;
				g->forced_boss_type = -1;
				g->practice_preview_timer = BOSS_INTRO_FRAMES;
				reset_player_progression(g);
				setup_level(g, 0, 1);
			} else {
				g->start_screen = 1;
				g->start_screen_delay_timer = 0;
				g->main_menu_selection = 0;
				g->practice_menu_active = 0;
				g->credits_screen_active = 1;
				g->credits_scroll_y = LH;
				reset_player_progression(g);
				setup_level(g, 0, 1);
			}
		}
		return;
	}

	if (!g->player_dying && !g->player_exit_fly_active && !g->game_over && tower_walls_closed(g)) {
		g->lives = 0;
		trigger_player_death(g);
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

	if (g->overworld_cutscene_active) {
		int skip_pressed = (buttons & BTN_FIRE) && !(old_buttons & BTN_FIRE);
		int pixelate_start_timer = OVERWORLD_TOTAL_FRAMES - (OVERWORLD_FLY_FRAMES + OVERWORLD_HOLD_FRAMES);
		if (skip_pressed && g->overworld_cutscene_timer > pixelate_start_timer) {
			g->overworld_cutscene_timer = pixelate_start_timer;
		}
		if (g->overworld_cutscene_timer > 0) {
			g->overworld_cutscene_timer--;
		}
		if (g->overworld_cutscene_timer <= 0) {
			do_clear = 1;
			g->overworld_cutscene_active = 0;
			if (!g->in_shop) {
				g->boss_intro_active = 1;
				g->boss_intro_timer = BOSS_INTRO_FRAMES;
			}
		}
		return;
	}

	if (g->in_shop) {
		shop_update(g, buttons, vsync_counter);
		return;
	}

	if (g->boss_intro_active) {
		int skip_pressed = (buttons & BTN_FIRE) && !(old_buttons & BTN_FIRE);
		if (skip_pressed) {
			g->boss_intro_timer = 0;
		}
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

	if (!g->boss_alive && !g->game_over && g->level != 0) {
		clear_boss_projectiles(g);
		g->ashot.alive = 0;
		if (!g->boss_dying && aliens_remaining(g)) {
			if (g->alien_cleanup_timer > 0) {
				g->alien_cleanup_timer--;
			}
			if (g->alien_cleanup_timer <= 0) {
				do_clear = 1;
				trigger_random_alien_cleanup_explosion(g);
				g->alien_cleanup_timer = 2 + (rand() % 4);
			}
		}
	}

	if (g->exit_blink_toggles_remaining > 0) {
		g->exit_blink_timer--;
		if (g->exit_blink_timer <= 0) {
			do_clear = 1;
			g->exit_blink_timer = EXIT_BLINK_INTERVAL_FRAMES;
			g->exit_blink_toggles_remaining--;
		}
	}

	// Decrement score every 0.33 seconds (20 ticks at 60 FPS) while aliens remain.
	static int score_decrement_timer = 0;
	score_decrement_timer++;
	if (score_decrement_timer >= 20) {
		if (g->score > 0 && aliens_remaining(g)) g->score--;
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

	if (g->level == 0) {
		if (!g->alien_alive[TUTORIAL_SHOOTER_ROW][TUTORIAL_SHOOTER_COL]) {
			if (g->tutorial_shooter_respawn_timer > 0) g->tutorial_shooter_respawn_timer--;
			if (g->tutorial_shooter_respawn_timer <= 0) {
				do_clear = 1;
				tutorial_respawn_alien(g, TUTORIAL_SHOOTER_ROW, TUTORIAL_SHOOTER_COL);
				g->tutorial_shooter_respawn_timer = TUTORIAL_RESPAWN_PERIOD_FRAMES;
			}
		} else {
			g->tutorial_shooter_respawn_timer = TUTORIAL_RESPAWN_PERIOD_FRAMES;
		}

		if (tutorial_center_pack_any_missing(g)) {
			if (g->tutorial_grid_respawn_timer > 0) g->tutorial_grid_respawn_timer--;
			if (g->tutorial_grid_respawn_timer <= 0) {
				do_clear = 1;
				tutorial_respawn_center_pack(g);
				g->tutorial_grid_respawn_timer = TUTORIAL_RESPAWN_PERIOD_FRAMES;
			}
		} else {
			g->tutorial_grid_respawn_timer = TUTORIAL_RESPAWN_PERIOD_FRAMES;
		}
	}

	// No powerups on level 0 (tutorial). Also remove spawned powerups once the boss is dead.
	if (g->level != 0) {
		if (!g->boss_alive || g->boss_dying) {
			g->powerup_active = 0;
			g->powerup_timer = 0;
			g->powerup_spawn_timer = 0;
		} else if (!g->powerup_active) {
			g->powerup_spawn_timer++;
			if (g->powerup_spawn_timer >= 900) {
				int tile = 8;
				// Constrain spawning to the current movement corridor and upper half.
				int left_bound = movement_left_bound(g);
				int right_bound = movement_right_bound(g);
				int left_spawn_px = left_bound + 8;
				int right_spawn_px = right_bound - 8;
				if (right_spawn_px <= left_spawn_px) {
					left_spawn_px = left_bound;
					right_spawn_px = right_bound;
				}

				int col_start = left_spawn_px / tile;
				int col_end = (right_spawn_px - 1) / tile;
				if (col_end < col_start) col_end = col_start;
				int cols = col_end - col_start + 1;
				int rows = (LH - 100) / tile;  // Leave space at bottom for player
				if (rows < 1) rows = 1;

				int tx = col_start + (rand() % cols);
				int ty = 30 / tile + (rand() % rows);  // Start from top, avoid bunkers
				g->powerup_x = tx * tile + tile / 2;
				g->powerup_y = ty * tile + tile / 2;
				if (g->practice_run_active) {
					static const powerup_type_t practice_pool[] = {
						POWERUP_TRIPLE_SHOT,
						POWERUP_RAPID_FIRE,
						POWERUP_EXPLOSIVE,
						POWERUP_SHIELD
					};
					int idx = rand() % 4;
					g->powerup_type = practice_pool[idx];
				} else {
					static const powerup_type_t spawn_pool[] = {
						POWERUP_TRIPLE_SHOT,
						POWERUP_RAPID_FIRE,
						POWERUP_EXPLOSIVE,
						POWERUP_SHIELD
					};
					int idx = rand() % 4;
					g->powerup_type = spawn_pool[idx];
				}
				g->powerup_active = 1;
				g->powerup_timer = 300;  // 5 seconds at ~60 FPS
				g->powerup_spawn_timer = 0;
			}
		} else {
			if (g->powerup_timer > 0) g->powerup_timer--;
			if (g->powerup_timer == 0) g->powerup_active = 0;
		}
	}

	if (g->exit_available && !g->player_exit_fly_active) {
		g->player_exit_fly_active = 1;
		clear_player_shots(g);
		clear_boss_projectiles(g);
		g->ashot.alive = 0;
	}

	if (g->player_exit_fly_active) {
		g->player_y -= PLAYER_EXIT_ASCEND_SPEED;
		if (g->player_y <= TOP_HUD_SEPARATOR_Y + 1) {
			g->player_y = TOP_HUD_SEPARATOR_Y + 1;
			g->player_exit_fly_active = 0;
			g->level_complete = 1;
			g->level_complete_timer = 1;
			g->level_just_completed = g->level;
			g->exit_available = 0;
		}
		return;
	}

	update_player_movement(g, buttons);
	update_player_firing(g, buttons);

	if (g->boss_alive && !g->boss_dying) {
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

					int left_bound = movement_left_bound(g);
					int right_bound = movement_right_bound(g);
					if (next_left < left_bound + 5 || next_right > right_bound - 5) {
						int stride = AS->w + spacing_x;
						int min_origin = (left_bound + 5) - minc * stride;
						int max_origin = (right_bound - 5) - grid_w - minc * stride;

						if (max_origin < min_origin) {
							max_origin = min_origin;
						}

						if (next_origin_x < min_origin) {
							g->alien_origin_x = min_origin;
						} else if (next_origin_x > max_origin) {
							g->alien_origin_x = max_origin;
						} else {
							g->alien_origin_x = next_origin_x;
						}

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
		}
	} else if (g->level == 0 && !g->boss_dying) {
		// Tutorial alien animates in place but never moves.
		g->alien_timer++;
		if (g->alien_timer >= g->alien_period) {
			g->alien_timer = 0;
			g->alien_frame ^= 1;
		}
	}

	// Tutorial level uses one fixed shooter alien. Regular levels shoot only while boss is alive.
	if (g->level == 0) {
		const sprite1r_t *AS = g->alien_frame ? &g->ALIEN_B : &g->ALIEN_A;
		int spacing_x = 6, spacing_y = 5;
		if (g->alien_alive[TUTORIAL_SHOOTER_ROW][TUTORIAL_SHOOTER_COL] && (vsync_counter % 30u) == 0u) {
			int ax = g->alien_origin_x + TUTORIAL_SHOOTER_COL * (AS->w + spacing_x);
			int ay = g->alien_origin_y + TUTORIAL_SHOOTER_ROW * (AS->h + spacing_y);

			if (!g->ashot.alive) {
				g->ashot.alive = 1;
				g->ashot.x = ax + AS->w / 2;
				g->ashot.y = ay + AS->h + 1;
			}
		}

		if (g->ashot.alive) {
			g->ashot.y += 3;
			if (g->ashot.y > LH) g->ashot.alive = 0;
		}
	} else if (g->boss_alive && !g->boss_dying) {
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

			if (g->boss_type == BOSS_TYPE_HERMIT && g->alien_hermit_regen[r_fire][c]) {
				spawn_hermit_lightning_from(g, (float)(ax + AS->w / 2), (float)(ay + AS->h + 1));
			} else if (!g->ashot.alive) {
				g->ashot.alive = 1;
				g->ashot.x = ax + AS->w / 2;
				g->ashot.y = ay + AS->h + 1;
			}
		}

		if (g->ashot.alive) {
			g->ashot.y += 3;
			if (g->ashot.y > LH) g->ashot.alive = 0;
		}
	} else {
		g->ashot.alive = 0;
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
		if (g->boss_type == BOSS_TYPE_MAGICIAN) {
			g->boss_power_timer += 2;
		} else if (g->boss_type == BOSS_TYPE_TOWER) {
			if (health_pct <= 33) g->boss_power_timer += 3;
			else if (health_pct <= 67) g->boss_power_timer += 2;
			else g->boss_power_timer += 1;
		} else {
			if (health_pct <= 33) g->boss_power_timer += 3 + (g->level / 6);
			else if (health_pct <= 67) g->boss_power_timer += 2 + (g->level / 6);
			else g->boss_power_timer += 1 + (g->level / 6);
		}
		if (g->boss_power_timer > g->boss_power_max) {
			g->boss_power_timer = g->boss_power_max;
		}

		// Determine next attack type when a new charging cycle starts.
		// Level 7 can increment by >1 per frame, so checking ==1 can be skipped.
		if (prev_power_timer == 0 && g->boss_power_timer > 0) {
			if (g->boss_type == BOSS_TYPE_BLUE) {
				g->next_boss_attack_type = 2;
			} else if (g->boss_type == BOSS_TYPE_TOWER) {
				g->next_boss_attack_type = TOWER_WALL_ATTACK;
			} else if (g->boss_type == BOSS_TYPE_HERMIT) {
				g->next_boss_attack_type = HERMIT_DODGE_ATTACK;
			} else if (g->boss_type == BOSS_TYPE_CHARIOT) {
				g->next_boss_attack_type = CHARIOT_CHARGE_ATTACK;
			} else if (g->boss_type == BOSS_TYPE_MAGICIAN) {
				g->next_boss_attack_type = MAGICIAN_MIRROR_ATTACK;
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
		if (g->boss_power_timer >= g->boss_power_max && g->boss_power_active == 0 && g->boss_power_cooldown == 0) {
			int defer_tower_wall = 0;
			g->boss_power_active = 1;
			g->boss_power_cooldown = (g->boss_type == BOSS_TYPE_TOWER) ? 0 :
				(g->boss_type == BOSS_TYPE_HERMIT) ? 20 :
				(g->boss_type == BOSS_TYPE_CHARIOT) ? 0 : 30;

			// Use the next attack type that was determined during charging
			g->boss_attack_type = g->next_boss_attack_type;

			if (g->boss_attack_type == TOWER_WALL_ATTACK) {
				int wall_thickness = g->tower_wall_active ? g->tower_wall_left : TOWER_WALL_WIDTH;
				int max_thickness = (LW - TOWER_WALL_MIN_GAP) / 2;
				if (max_thickness < TOWER_WALL_WIDTH) max_thickness = TOWER_WALL_WIDTH;
				if (g->tower_wall_active) {
					wall_thickness += TOWER_WALL_STEP;
				}
				if (wall_thickness > max_thickness) wall_thickness = max_thickness;

				int proposed_left = wall_thickness;
				int proposed_right = LW - wall_thickness;
				if (!tower_wall_path_clear(g, proposed_left, proposed_right)) {
					defer_tower_wall = 1;
					g->boss_power_active = 0;
					g->boss_power_cooldown = 30; // Wait an extra 0.5s before retrying.
				} else {
					g->boss_laser.alive = 0;
					g->boss_bomb.alive = 0;
					g->boss_bomb.exploding = 0;
					g->boss_bomb.explode_timer = 0;
					g->boss_bomb.hit_player = 0;
					g->tower_wall_active = 1;
					g->tower_wall_timer = TOWER_WALL_DURATION_FRAMES;
					g->tower_wall_left = proposed_left;
					g->tower_wall_right = proposed_right;
					g->boss_power_active = 0;
				}
			} else if (g->boss_attack_type == 2) {
				g->boss_laser.alive = 0;
				g->boss_bomb.alive = 0;
				g->boss_bomb.exploding = 0;
				g->boss_bomb.explode_timer = 0;
				g->boss_bomb.hit_player = 0;
				g->boss_bomb.alive = 1;
				g->boss_bomb.exploding = 0;
				g->boss_bomb.explode_timer = 0;
				g->boss_bomb.hit_player = 0;
				g->boss_bomb.hits_taken = 0;
				g->boss_bomb.reversed = 0;
				g->boss_bomb.reverse_damage_applied = 0;
				g->boss_bomb.dy = BOSS_BOMB_SPEED;
				g->boss_bomb.x = g->boss_x + boss_w / 2;
				g->boss_bomb.y = g->boss_y + boss_h + 2;
			} else if (g->boss_attack_type == HERMIT_DODGE_ATTACK) {
				g->boss_laser.alive = 0;
				g->boss_bomb.alive = 0;
				g->boss_bomb.exploding = 0;
				g->boss_bomb.explode_timer = 0;
				g->boss_bomb.hit_player = 0;
				hermit_regenerate_aliens(g);
				int left_bound = movement_left_bound(g);
				int right_bound = movement_right_bound(g);
				int reflected_x = right_bound - boss_w - (g->boss_x - left_bound);
				if (reflected_x < left_bound) reflected_x = left_bound;
				if (reflected_x > right_bound - boss_w) reflected_x = right_bound - boss_w;
				g->boss_x = reflected_x;
				g->boss_dx = -g->boss_dx;
				g->boss_power_active = 0;
			} else if (g->boss_attack_type == CHARIOT_CHARGE_ATTACK) {
				g->boss_laser.alive = 0;
				g->boss_bomb.alive = 0;
				g->boss_bomb.exploding = 0;
				g->boss_bomb.explode_timer = 0;
				g->boss_bomb.hit_player = 0;
				g->chariot_arc_active = 0;
				g->chariot_charge_start_x = g->boss_x;
				g->chariot_charge_start_y = g->boss_y;
				g->boss_special_x = g->boss_x;
				g->boss_special_y = g->boss_y;
				g->boss_special_timer = 0;
				g->boss_special_hit_applied = 0;
				g->chariot_charge_damage = 0;

				{
					int left_bound = movement_left_bound(g);
					int right_bound = movement_right_bound(g);
					int target_x = g->player_x + g->PLAYER.w / 2 - boss_w / 2;
					if (target_x < left_bound) target_x = left_bound;
					if (target_x > right_bound - boss_w) target_x = right_bound - boss_w;
					g->chariot_charge_target_x = target_x;
				}
			} else if (g->boss_attack_type == MAGICIAN_MIRROR_ATTACK) {
				g->boss_laser.alive = 0;
				g->boss_bomb.alive = 0;
				g->boss_bomb.exploding = 0;
				g->boss_bomb.explode_timer = 0;
				g->boss_bomb.hit_player = 0;
				spawn_magician_mirror(g);
				g->boss_power_active = 0;
			} else {
				g->boss_laser.alive = 0;
				g->boss_bomb.alive = 0;
				g->boss_bomb.exploding = 0;
				g->boss_bomb.explode_timer = 0;
				g->boss_bomb.hit_player = 0;
				g->boss_laser.alive = 1;
				g->boss_laser.x = g->boss_x + boss_w / 2;
				g->boss_laser.y = g->boss_y + boss_h + 1;
			}

			if (!defer_tower_wall) {
				g->boss_laser_last_hit_y = -1000;  // Reset hit tracking
				g->boss_green_laser_last_hit_y = -1000;  // Reset green laser tracking
				g->boss_power_timer = 0;  // Reset for next charge
				if (g->boss_type == BOSS_TYPE_CLASSIC &&
						(g->boss_attack_type == 0 || g->boss_attack_type == 1)) {
					music_play_tower_asteroid_boom();
				} else {
					music_play_laser();
				}
			}

			// Boss recovers 10% health if using green laser
			if (g->boss_attack_type == 1) {
				g->boss_health += g->boss_max_health / 10;
				if (g->boss_health > g->boss_max_health) {
					g->boss_health = g->boss_max_health;
				}
			}
		}

		// Handle stun/frozen state during special attack.
		// Tower keeps moving even while using cooldown as a wall-retry delay.
		int freeze_boss = 0;
		if (g->boss_power_cooldown > 0) {
			freeze_boss = (g->boss_type != BOSS_TYPE_TOWER);
			if (g->boss_type == BOSS_TYPE_MAGICIAN && g->boss_attack_type == MAGICIAN_MIRROR_ATTACK) {
				freeze_boss = 0;
			}
			g->boss_power_cooldown--;
		}

		if (!freeze_boss) {
			if (g->boss_type == BOSS_TYPE_CHARIOT && g->boss_power_active && g->boss_attack_type == CHARIOT_CHARGE_ATTACK) {
				int left_bound = movement_left_bound(g);
				int right_bound = movement_right_bound(g);
				int target_x = g->chariot_charge_target_x;
				if (target_x < left_bound) target_x = left_bound;
				if (target_x > right_bound - boss_w) target_x = right_bound - boss_w;

				if (g->boss_special_timer <= 0) {
					if (g->boss_special_x < target_x) {
						g->boss_special_x += CHARIOT_CHARGE_SPEED_X;
						if (g->boss_special_x > target_x) g->boss_special_x = target_x;
					} else if (g->boss_special_x > target_x) {
						g->boss_special_x -= CHARIOT_CHARGE_SPEED_X;
						if (g->boss_special_x < target_x) g->boss_special_x = target_x;
					}

					g->boss_special_y += CHARIOT_CHARGE_SPEED_Y;
					g->boss_x = g->boss_special_x;
					g->boss_y = g->boss_special_y;

					if (!g->boss_special_hit_applied &&
							rects_overlap(g->boss_x, g->boss_y, boss_w, boss_h,
								g->player_x, g->player_y, g->PLAYER.w, g->PLAYER.h)) {
						apply_player_damage(g, 1);
						g->boss_special_hit_applied = 1;
					}

					if (g->boss_special_y + boss_h >= BOTTOM_HUD_SEPARATOR_Y - 1) {
						g->boss_special_y = BOTTOM_HUD_SEPARATOR_Y - 1 - boss_h;
						g->boss_y = g->boss_special_y;
						g->boss_special_timer = CHARIOT_CHARGE_EXPLOSION_FRAMES;
						music_play_boom_long();
						apply_chariot_charge_explosion_to_aliens(
								g,
								g->boss_x + boss_w / 2,
								g->boss_y + boss_h / 2,
								CHARIOT_CHARGE_EXPLOSION_RADIUS
								);

						if (!g->boss_special_hit_applied) {
							int pcx = g->player_x + g->PLAYER.w / 2;
							int pcy = g->player_y + g->PLAYER.h / 2;
							int ecx = g->boss_x + boss_w / 2;
							int ecy = g->boss_y + boss_h / 2;
							int dx = pcx - ecx;
							int dy = pcy - ecy;
							if ((dx * dx + dy * dy) <= (CHARIOT_CHARGE_EXPLOSION_RADIUS * CHARIOT_CHARGE_EXPLOSION_RADIUS)) {
								apply_chariot_explosion_player_damage(g);
							}
							g->boss_special_hit_applied = 1;
						}
					}
				} else {
					int age = CHARIOT_CHARGE_EXPLOSION_FRAMES - g->boss_special_timer;
					int radius = 8 + age / 2;
					if (radius > CHARIOT_CHARGE_EXPLOSION_RADIUS) radius = CHARIOT_CHARGE_EXPLOSION_RADIUS;
					if (circle_intersects_rect(g->boss_x + boss_w / 2, g->boss_y + boss_h / 2, radius,
								g->player_x, g->player_y, g->PLAYER.w, g->PLAYER.h)) {
						apply_chariot_explosion_player_damage(g);
					}

					g->boss_special_timer--;
					if (g->boss_special_timer <= 0) {
						g->boss_special_x = g->chariot_charge_start_x;
						g->boss_special_y = g->chariot_charge_start_y;
						g->boss_x = g->chariot_charge_start_x;
						g->boss_y = g->chariot_charge_start_y;
						if (g->boss_x < left_bound) g->boss_x = left_bound;
						if (g->boss_x > right_bound - boss_w) g->boss_x = right_bound - boss_w;
						g->boss_power_active = 0;
					}
				}
			} else {
				// Normal movement
				g->boss_timer++;
				if (g->boss_timer >= g->boss_period) {
					g->boss_timer = 0;
					g->boss_frame ^= 1;
				}

				int next_bx = g->boss_x + g->boss_dx;
				int left_bound = movement_left_bound(g);
				int right_bound = movement_right_bound(g);
				if (next_bx < left_bound || next_bx > right_bound - boss_w) {
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
				if (g->boss_type != BOSS_TYPE_BLUE && g->boss_type != BOSS_TYPE_TOWER && g->boss_type != BOSS_TYPE_HERMIT && g->boss_type != BOSS_TYPE_CHARIOT && g->boss_type != BOSS_TYPE_MAGICIAN && alive_count <= total_aliens / 2) {
					if (g->boss_timer == 0) g->boss_y += 1;
				}
			}
		}

		if (g->boss_type == BOSS_TYPE_MAGICIAN) {
			update_magician_mirror(g);
		}

		// Game over if boss reaches player's y position
		int chariot_diving = (g->boss_type == BOSS_TYPE_CHARIOT && g->boss_power_active &&
				g->boss_attack_type == CHARIOT_CHARGE_ATTACK);
		if (!g->player_exit_fly_active && !chariot_diving && g->boss_y + boss_h >= g->player_y) {
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
					g->boss_bomb.y += g->boss_bomb.dy;
					int hit_boss_while_reversing = 0;
					if (g->boss_bomb.reversed) {
						if (g->boss_bomb.x >= g->boss_x && g->boss_bomb.x < g->boss_x + boss_w &&
								g->boss_bomb.y >= g->boss_y && g->boss_bomb.y < g->boss_y + boss_h) {
							hit_boss_while_reversing = 1;
						}
					}
					if ((!g->boss_bomb.reversed && (g->boss_bomb.y >= g->player_y || g->boss_bomb.y >= LH - 6)) ||
							(g->boss_bomb.reversed && g->boss_bomb.y <= g->boss_start_y) ||
							hit_boss_while_reversing) {
						if (g->boss_bomb.reversed && !hit_boss_while_reversing) {
							g->boss_bomb.y = g->boss_start_y;
						}
						g->boss_bomb.exploding = 1;
						g->boss_bomb.explode_timer = BOSS_BOMB_EXPLOSION_FRAMES;
						g->boss_bomb.dy = 0;
						music_play_boom_long();
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

	// Keep player projectiles out of the top HUD strip.
	for (int i = 0; i < MAX_PSHOTS; i++) {
		if (g->pshot[i].alive && g->pshot[i].y <= TOP_HUD_SEPARATOR_Y) {
			g->pshot[i].alive = 0;
		}
		if (g->pshot_left[i].alive && g->pshot_left[i].y <= TOP_HUD_SEPARATOR_Y) {
			g->pshot_left[i].alive = 0;
		}
		if (g->pshot_right[i].alive && g->pshot_right[i].y <= TOP_HUD_SEPARATOR_Y) {
			g->pshot_right[i].alive = 0;
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

			if (g->boss_type != BOSS_TYPE_CHARIOT && g->boss_type != BOSS_TYPE_MAGICIAN) {
				g->chariot_arc_active = 0;
				for (int i = 0; i < CHARIOT_ARC_SHOT_COUNT; i++) {
					g->boss_arc_shot[i].alive = 0;
				}
			}

			if (g->boss_type == BOSS_TYPE_TOWER) {
				int total_aliens = AROWS * ACOLS;
				int remaining_aliens = count_aliens_remaining(g);
				const int tower_period_max = 180;
				const int tower_period_min = 70;
				int tower_period = tower_period_min + (remaining_aliens * (tower_period_max - tower_period_min)) / total_aliens;
				if (tower_period < tower_period_min) tower_period = tower_period_min;
				if (tower_period > tower_period_max) tower_period = tower_period_max;

				int level_speedup = (g->level - 1) * 4;
				if (level_speedup < 0) level_speedup = 0;
				if (level_speedup > 40) level_speedup = 40;
				tower_period -= level_speedup;
				if (tower_period < 30) tower_period = 30;

				if ((vsync_counter % (uint32_t)tower_period) == 0u) {
					int active_count = 0;
					for (int ai = 0; ai < MAX_TOWER_ASTEROIDS; ai++) {
						if (g->tower_asteroid[ai].alive) active_count++;
					}

					int spawn_slot = -1;
					if (active_count < 10) {
						for (int ai = 0; ai < MAX_TOWER_ASTEROIDS; ai++) {
							if (!g->tower_asteroid[ai].alive && !g->tower_asteroid_exploding[ai]) {
								spawn_slot = ai;
								break;
							}
						}
					}

					if (spawn_slot >= 0) {
						const sprite1r_t *BS = active_boss_sprite(g);
						g->tower_asteroid[spawn_slot].alive = 1;
						g->tower_asteroid[spawn_slot].x = g->boss_x + BS->w / 2;
						g->tower_asteroid[spawn_slot].y = g->boss_y + BS->h + 2;
						g->tower_asteroid_hp[spawn_slot] = 3;
						g->tower_asteroid_dx[spawn_slot] = ((rand() & 1) == 0) ? -1 : 1;
						g->tower_asteroid_spin[spawn_slot] = 0;
						g->tower_asteroid_exploding[spawn_slot] = 0;
						g->tower_asteroid_explode_timer[spawn_slot] = 0;
						g->tower_asteroid_boss_damage_applied[spawn_slot] = 0;
					}
				}

				for (int ai = 0; ai < MAX_TOWER_ASTEROIDS; ai++) {
					if (!g->tower_asteroid[ai].alive) continue;
					if ((vsync_counter & 1u) == 0u) {
						g->tower_asteroid[ai].y += 1;
						g->tower_asteroid[ai].x += g->tower_asteroid_dx[ai];
						if (g->tower_asteroid[ai].x < movement_left_bound(g) + TOWER_ASTEROID_RADIUS) {
							g->tower_asteroid[ai].x = movement_left_bound(g) + TOWER_ASTEROID_RADIUS;
							g->tower_asteroid_dx[ai] = -g->tower_asteroid_dx[ai];
						}
						if (g->tower_asteroid[ai].x > movement_right_bound(g) - TOWER_ASTEROID_RADIUS - 1) {
							g->tower_asteroid[ai].x = movement_right_bound(g) - TOWER_ASTEROID_RADIUS - 1;
							g->tower_asteroid_dx[ai] = -g->tower_asteroid_dx[ai];
						}
					}
					g->tower_asteroid_spin[ai]++;
					if (g->tower_asteroid[ai].y - TOWER_ASTEROID_RADIUS > LH) {
						g->tower_asteroid[ai].alive = 0;
						g->tower_asteroid_hp[ai] = 0;
					}
				}
				g->boss_shot.alive = 0;
				for (int i = 0; i < 3; i++) {
					g->boss_triple_shot[i].alive = 0;
				}
			} else if (g->boss_type == BOSS_TYPE_HERMIT) {
				int hermit_period = 150;

				if ((vsync_counter % (uint32_t)hermit_period) == 0u) {
					hermit_spawn_lightning(g);
				}

				for (int i = 0; i < HERMIT_MAX_LIGHTNINGS; i++) {
					hermit_lightning_t *l = &g->hermit_lightning[i];
					if (!l->active) continue;

					if (l->flash_timer > 0) {
						l->flash_timer--;
						if (!l->yellow_hit_applied && !shield_power_active(g) &&
								segment_hits_player(g, l->start_x, l->start_y, l->tip_x, l->tip_y)) {
							apply_player_damage(g, 1);
							l->yellow_hit_applied = 1;
						}

						destroy_bunkers_along_segment(g, l->start_x, l->start_y, l->tip_x, l->tip_y);

						hermit_move_point_toward(&l->start_x, &l->start_y, l->tip_x, l->tip_y, HERMIT_LIGHTNING_FADE_SPEED);

						if (l->flash_timer == 0) {
							l->active = 0;
						}
						continue;
					}

					l->tip_x += l->vx;
					l->tip_y += l->vy;

					if (hermit_lightning_span(l) > HERMIT_LIGHTNING_MAX_TRAIL) {
						hermit_move_point_toward(&l->start_x, &l->start_y, l->tip_x, l->tip_y, HERMIT_LIGHTNING_TAIL_SPEED);
					}

					destroy_bunkers_along_segment(g, l->start_x, l->start_y, l->tip_x, l->tip_y);

					int hit_player_now = (!shield_power_active(g) &&
							segment_hits_player(g, l->start_x, l->start_y, l->tip_x, l->tip_y));
					if (l->tip_y >= (float)LH || l->tip_x < 0.0f || l->tip_x >= (float)LW || hit_player_now) {
						if (l->tip_y > (float)LH) l->tip_y = (float)LH;
						if (l->tip_x < 0.0f) l->tip_x = 0.0f;
						if (l->tip_x > (float)(LW - 1)) l->tip_x = (float)(LW - 1);
						if (hit_player_now && !l->yellow_hit_applied) {
							apply_player_damage(g, 1);
							l->yellow_hit_applied = 1;
						}
						l->flash_timer = HERMIT_LIGHTNING_YELLOW_FRAMES;
					}
				}

				g->boss_shot.alive = 0;
				g->boss_laser.alive = 0;
				for (int i = 0; i < 3; i++) {
					g->boss_triple_shot[i].alive = 0;
				}
			} else if (g->boss_type == BOSS_TYPE_CHARIOT) {
				if (!g->boss_power_active || g->boss_attack_type != CHARIOT_CHARGE_ATTACK) {
					if (!any_chariot_arc_shot_alive(g) && !g->chariot_arc_active &&
							(vsync_counter % (uint32_t)period) == 0u) {
						const sprite1r_t *BS = active_boss_sprite(g);
						int cx = g->boss_x + BS->w / 2;
						int cy = g->boss_y + BS->h + 1;
						int center_x = g->boss_x + BS->w / 2;
						int left_third = LW / 3;
						int right_third = (LW * 2) / 3;

						if (center_x < left_third) {
							g->chariot_arc_active = 1;
							g->chariot_arc_next_index = 1;
							g->chariot_arc_step = 1;
							g->chariot_arc_delay = CHARIOT_ARC_SHOT_DELAY;
							spawn_chariot_arc_shot(g, 0, cx, cy);
						} else if (center_x > right_third) {
							g->chariot_arc_active = 1;
							g->chariot_arc_next_index = CHARIOT_ARC_SHOT_COUNT - 2;
							g->chariot_arc_step = -1;
							g->chariot_arc_delay = CHARIOT_ARC_SHOT_DELAY;
							spawn_chariot_arc_shot(g, CHARIOT_ARC_SHOT_COUNT - 1, cx, cy);
						} else {
							g->chariot_arc_active = 0;
							int middle_start = (CHARIOT_ARC_SHOT_COUNT - CHARIOT_ARC_MIDDLE_SHOT_COUNT) / 2;
							int middle_end = middle_start + CHARIOT_ARC_MIDDLE_SHOT_COUNT;
							for (int i = middle_start; i < middle_end; i++) {
								spawn_chariot_arc_shot(g, i, cx, cy);
							}
						}
					}

					if (g->chariot_arc_active) {
						if (g->chariot_arc_delay > 0) g->chariot_arc_delay--;
						if (g->chariot_arc_delay <= 0) {
							if (g->chariot_arc_next_index >= 0 &&
									g->chariot_arc_next_index < CHARIOT_ARC_SHOT_COUNT) {
								const sprite1r_t *BS = active_boss_sprite(g);
								int cx = g->boss_x + BS->w / 2;
								int cy = g->boss_y + BS->h + 1;
								spawn_chariot_arc_shot(g, g->chariot_arc_next_index, cx, cy);
								g->chariot_arc_next_index += g->chariot_arc_step;
								g->chariot_arc_delay = CHARIOT_ARC_SHOT_DELAY;
							} else {
								g->chariot_arc_active = 0;
							}
						}
					}
				}

				for (int i = 0; i < CHARIOT_ARC_SHOT_COUNT; i++) {
					if (!g->boss_arc_shot[i].alive) continue;
					int dx = g->boss_arc_shot_dx[i];
					g->boss_arc_shot[i].x += dx;
					g->boss_arc_shot[i].y += 2;
					reflect_x_from_magician_side_walls(g, &g->boss_arc_shot[i].x, &dx, 2);
					g->boss_arc_shot_dx[i] = dx;
					if (g->boss_arc_shot[i].y > LH || g->boss_arc_shot[i].x < 0 || g->boss_arc_shot[i].x >= LW) {
						g->boss_arc_shot[i].alive = 0;
					}
				}

				g->boss_shot.alive = 0;
				g->boss_laser.alive = 0;
				for (int i = 0; i < 3; i++) {
					g->boss_triple_shot[i].alive = 0;
				}
			} else if (g->boss_type == BOSS_TYPE_MAGICIAN) {
				int magician_period = period * MAGICIAN_ORB_FIRE_PERIOD_MULT;
				if (magician_period < 1) magician_period = 1;
				if (!any_magician_wave_alive(g) && (vsync_counter % (uint32_t)magician_period) == 0u) {
					spawn_magician_wave(g);
				}
				update_magician_wave(g, vsync_counter);

				g->boss_shot.alive = 0;
				g->boss_laser.alive = 0;
				for (int i = 0; i < 3; i++) {
					g->boss_triple_shot[i].alive = 0;
				}
			} else if (g->boss_type == BOSS_TYPE_BLUE) {
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
						int dx = g->boss_triple_shot_dx[i];
						g->boss_triple_shot[i].x += dx;
						g->boss_triple_shot[i].y += 2;
						reflect_x_from_magician_side_walls(g, &g->boss_triple_shot[i].x, &dx, 2);
						g->boss_triple_shot_dx[i] = dx;
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

	// Keep gameplay projectiles out of the bottom HUD strip.
	// Only active projectiles are culled here; explosion effects are left intact.
	if (g->ashot.alive && g->ashot.y >= BOTTOM_HUD_SEPARATOR_Y) {
		g->ashot.alive = 0;
	}
	if (g->boss_shot.alive && g->boss_shot.y >= BOTTOM_HUD_SEPARATOR_Y) {
		g->boss_shot.alive = 0;
	}
	for (int i = 0; i < 3; i++) {
		if (g->boss_triple_shot[i].alive && g->boss_triple_shot[i].y >= BOTTOM_HUD_SEPARATOR_Y) {
			g->boss_triple_shot[i].alive = 0;
		}
	}
	for (int i = 0; i < CHARIOT_ARC_SHOT_COUNT; i++) {
		if (g->boss_arc_shot[i].alive && g->boss_arc_shot[i].y >= BOTTOM_HUD_SEPARATOR_Y) {
			g->boss_arc_shot[i].alive = 0;
		}
	}
	for (int i = 0; i < YELLOW_BOSS_ALIENS; i++) {
		if (g->yellow_beam_shot[i].alive && g->yellow_beam_shot[i].y >= BOTTOM_HUD_SEPARATOR_Y) {
			g->yellow_beam_shot[i].alive = 0;
		}
	}
	if (g->boss_laser.alive && g->boss_laser.y >= BOTTOM_HUD_SEPARATOR_Y) {
		g->boss_laser.alive = 0;
		g->boss_power_active = 0;
	}
	for (int ai = 0; ai < MAX_TOWER_ASTEROIDS; ai++) {
		if (g->tower_asteroid[ai].alive &&
				(g->tower_asteroid[ai].y + TOWER_ASTEROID_RADIUS) >= BOTTOM_HUD_SEPARATOR_Y) {
			g->tower_asteroid[ai].alive = 0;
			g->tower_asteroid_hp[ai] = 0;
			g->tower_asteroid_dx[ai] = 0;
		}
	}

	// collisions: player shots
	int magician_dual_player_shot_mode =
		g->boss_alive && !g->boss_dying && (g->boss_type == BOSS_TYPE_MAGICIAN);
	handle_player_shot_collisions(g, g->pshot, 0, 1);
	handle_player_shot_collisions(g, g->pshot_left, -1, magician_dual_player_shot_mode);
	handle_player_shot_collisions(g, g->pshot_right, 1, magician_dual_player_shot_mode);

	if (g->boss_type != BOSS_TYPE_YELLOW) {
		boss_apply_alien_explosions_to_boss(g);
	}

	// collisions: alien and boss shots
	handle_enemy_shot_collisions(g, &g->ashot, 0, 1);
	handle_enemy_shot_collisions(g, &g->boss_shot, 0, 1);
	for (int i = 0; i < 3; i++) {
		handle_enemy_shot_collisions(g, &g->boss_triple_shot[i], g->boss_triple_shot_dx[i], 1);
	}
	for (int i = 0; i < CHARIOT_ARC_SHOT_COUNT; i++) {
		handle_enemy_shot_collisions(g, &g->boss_arc_shot[i], g->boss_arc_shot_dx[i], 1);
	}
	for (int i = 0; i < YELLOW_BOSS_ALIENS; i++) {
		handle_enemy_shot_collisions(g, &g->yellow_beam_shot[i], 0, 1);
	}

	for (int ai = 0; ai < MAX_TOWER_ASTEROIDS; ai++) {
		if (g->tower_asteroid[ai].alive) {
			if (shield_power_active(g)) {
				int r = player_shield_radius(g) + TOWER_ASTEROID_RADIUS;
				int cx = g->player_x + g->PLAYER.w / 2;
				int cy = g->player_y + g->PLAYER.h / 2;
				int dx = g->tower_asteroid[ai].x - cx;
				int dy = g->tower_asteroid[ai].y - cy;
				if ((dx * dx + dy * dy) <= (r * r)) {
					g->tower_asteroid[ai].alive = 0;
					g->tower_asteroid_hp[ai] = 0;
					g->tower_asteroid_dx[ai] = 0;
					continue;
				}
			}

			if (circle_hits_sprite(&g->PLAYER, g->player_x, g->player_y,
						g->tower_asteroid[ai].x, g->tower_asteroid[ai].y, TOWER_ASTEROID_RADIUS)) {
				apply_player_damage(g, 1);
				start_tower_asteroid_explosion(g, ai);
			}

			if (g->tower_asteroid[ai].alive && g->level != 0) {
				for (int i = 0; i < 4 && g->tower_asteroid[ai].alive; i++) {
					sprite1r_t *b = g->bunkers[i];
					if (!sprite_has_any_bits(b)) continue;
					int bx = g->bunker_x[i], by = g->bunker_y;
					if (circle_hits_sprite(b, bx, by,
								g->tower_asteroid[ai].x, g->tower_asteroid[ai].y, TOWER_ASTEROID_RADIUS)) {
						bunker_damage(b, g->tower_asteroid[ai].x - bx, g->tower_asteroid[ai].y - by, 6);
						start_tower_asteroid_explosion(g, ai);
					}
				}
			}
		}

		if (g->tower_asteroid_exploding[ai]) {
			int radius = tower_asteroid_explosion_radius(g, ai);
			apply_tower_asteroid_explosion_effects(g, ai, radius);
			if (g->tower_asteroid_explode_timer[ai] > 0) {
				g->tower_asteroid_explode_timer[ai]--;
			}
			if (g->tower_asteroid_explode_timer[ai] <= 0) {
				g->tower_asteroid_exploding[ai] = 0;
				g->tower_asteroid_explode_timer[ai] = 0;
			}
		}
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

	// Blue charged bomb / black-hole explosion: same radius progression as boss death explosion and 1 damage to player.
	if (g->boss_attack_type == 2 && g->boss_bomb.alive && g->boss_bomb.exploding) {
		int radius = boss_bomb_explosion_radius(g);
		apply_reversed_bomb_explosion_effects(g, radius);
		int pcx = g->player_x + g->PLAYER.w / 2;
		int pcy = g->player_y + g->PLAYER.h / 2;
		int dx = pcx - g->boss_bomb.x;
		int dy = pcy - g->boss_bomb.y;

		if (blue_black_hole_enabled(g) && !g->player_dying) {
			int alive_count = 0;
			for (int r = 0; r < AROWS; r++) {
				for (int c = 0; c < ACOLS; c++) {
					alive_count += g->alien_alive[r][c] ? 1 : 0;
				}
			}
			int total_aliens = AROWS * ACOLS;

			int pull_step = g->player_speed / 2;
			if (alive_count < total_aliens / 2) {
				pull_step = (g->player_speed * 5) / 4;
			}
			if (pull_step < 1) pull_step = 1;

			if (g->boss_bomb.reversed && g->boss_alive && !g->boss_dying) {
				const sprite1r_t *BS = active_boss_sprite(g);
				int bcx = g->boss_x + BS->w / 2;
				int delta_x = g->boss_bomb.x - bcx;
				if (delta_x > 0) {
					int step = (delta_x < pull_step) ? delta_x : pull_step;
					g->boss_x += step;
				} else if (delta_x < 0) {
					int step = ((-delta_x) < pull_step) ? (-delta_x) : pull_step;
					g->boss_x -= step;
				}

				int left_bound = movement_left_bound(g);
				int right_bound = movement_right_bound(g);
				if (g->boss_x < left_bound) g->boss_x = left_bound;
				if (g->boss_x > right_bound - BS->w) g->boss_x = right_bound - BS->w;
			} else {
				int delta_x = g->boss_bomb.x - pcx;
				if (delta_x > 0) {
					int step = (delta_x < pull_step) ? delta_x : pull_step;
					g->player_x += step;
				} else if (delta_x < 0) {
					int step = ((-delta_x) < pull_step) ? (-delta_x) : pull_step;
					g->player_x -= step;
				}

				int left_bound = movement_left_bound(g);
				int right_bound = movement_right_bound(g);
				if (g->player_x < left_bound) g->player_x = left_bound;
				if (g->player_x > right_bound - g->PLAYER.w) g->player_x = right_bound - g->PLAYER.w;

				// Recompute center after movement for hit test.
				pcx = g->player_x + g->PLAYER.w / 2;
				dx = pcx - g->boss_bomb.x;
			}
		}

		if (!g->boss_bomb.hit_player && (dx * dx + dy * dy) <= (radius * radius)) {
			g->boss_bomb.hit_player = 1;
			apply_player_damage(g, 1);
		}
	}

	if (g->boss_type == BOSS_TYPE_YELLOW) {
		update_yellow_boss_health(g);
	}

	if (g->practice_run_active && !g->boss_alive && !g->boss_dying) {
		if (g->practice_return_delay_timer > 0) {
			g->practice_return_delay_timer--;
			return;
		}
		g->start_screen = 1;
		g->start_screen_delay_timer = 30;
		g->practice_menu_active = 1;
		g->practice_run_active = 0;
		g->practice_return_delay_timer = 0;
		g->forced_boss_type = -1;
		g->practice_preview_timer = BOSS_INTRO_FRAMES;
		reset_player_progression(g);
		setup_level(g, 0, 1);
		return;
	}

	if (!g->game_over) {
		const sprite1r_t *AS = g->alien_frame ? &g->ALIEN_B : &g->ALIEN_A;
		int spacing_x = 6, spacing_y = 5;
		for (int r = 0; r < AROWS && !g->game_over; r++) {
			for (int c = 0; c < ACOLS && !g->game_over; c++) {
				if (!g->alien_alive[r][c]) continue;
				int ay = g->alien_origin_y + r * (AS->h + spacing_y);
				if (!g->player_exit_fly_active && ay >= g->player_y) {
					g->game_over = 1;
					g->game_over_score = g->score;
					g->game_over_delay_timer = 90;
				}
			}
		}
	}

	if (!g->game_over && !g->boss_alive && !g->exit_available) {
		if (g->level == 0) {
			if (!g->alien_alive[TUTORIAL_LEAVE_ROW][TUTORIAL_LEAVE_COL]) {
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

void check_scene(game_t *g) {
	if (g->scene != g->prev_scene) {
		l_clear_2();
		g->prev_scene = g->scene;
	}
}

void game_render(game_t *g, lfb_t *lfb) {
	check_scene(g);
	if (do_clear) {
		l_clear_2();
		do_clear = 0;
	}
	if (g->start_screen) {
		g->scene = 0;
		l_clear(lfb, 0xFF000000);
		if (g->credits_screen_active) {
			g->scene = 10;
			render_credits_screen(g, lfb);
		} else if (!g->practice_menu_active && !g->difficulty_menu_active) {
			const char *title = "ALIENS";
			const char *options[MAIN_MENU_COUNT] = {"START GAME", "PRACTICE MODE", "CREDITS"};

			int title_scale = 3;
			int title_w = text_width_5x5(title, title_scale);
			int title_x = (LW - title_w) / 2;
			int title_y = 38;
			l_draw_text(lfb, title_x, title_y, title, title_scale, 0xFF00FF00);

			int option_scale = 2;
			int option_base_y = title_y + 42;
			for (int i = 0; i < MAIN_MENU_COUNT; i++) {
				int is_selected = (i == g->main_menu_selection);
				int opt_w = text_width_5x5(options[i], option_scale);
				int opt_x = (LW - opt_w) / 2;
				int opt_y = option_base_y + i * 28;
				if (is_selected) {
					l_draw_text(lfb, opt_x - 18, opt_y, ">", option_scale, 0xFFFFFFFF);
				}
				l_draw_text(lfb, opt_x, opt_y, options[i], option_scale,
						is_selected ? 0xFFFFFFFF : 0xFF9A9A9A);
			}

			const char *hint = "UP/DOWN TO CHOOSE  SPACE TO SELECT";
			int hint_w = text_width_5x5(hint, 1);
			l_draw_text(lfb, (LW - hint_w) / 2, LH - 22, hint, 1, 0xFFBFBFBF);
		} else if (g->difficulty_menu_active) {
			g->scene = 1;
			const char *entries[DIFFICULTY_MENU_COUNT] = {"TUTORIAL", "EASY MODE", "HARD MODE", "BACK"};

			int base_y = 16;
			int row_h = 16;
			for (int i = 0; i < DIFFICULTY_MENU_COUNT; i++) {
				int is_selected = (i == g->difficulty_menu_selection);
				uint32_t color = is_selected ? 0xFFFFFFFF : 0xFF9A9A9A;
				if (is_selected && i == 2) color = 0xFFFF0000;
				uint32_t cursor_color = is_selected ? 0xFF00FF00 : 0xFFFFFFFF;
				int entry_y = base_y + i * row_h;
				if (is_selected) {
					l_draw_text(lfb, 16, entry_y, ">", 1, cursor_color);
				}
				l_draw_text(lfb, 26, entry_y, entries[i], 1, color);
			}

			int right_x = LW / 2 + 6;
			if (g->difficulty_menu_selection == 0) {
				l_draw_text(lfb, right_x, 16, "FOR FIRST TIME PLAYERS.", 1, 0xFFFFFFFF);
			} else if (g->difficulty_menu_selection == 1) {
				l_draw_text(lfb, right_x, 16, "THE CLASSIC EXPERIENCE.", 1, 0xFFFFFFFF);
			} else if (g->difficulty_menu_selection == 2) {
				l_draw_text(lfb, right_x, 16, "NO HEALING.", 1, 0xFFFFFFFF);
				l_draw_text(lfb, right_x, 28, "ONE LIFE.", 1, 0xFFFFFFFF);
			} else {
				l_draw_text(lfb, right_x, 16, "RETURN TO MAIN MENU.", 1, 0xFFFFFFFF);
			}

			const char *hint = "UP/DOWN DIFFICULTY  SPACE SELECT";
			int hint_w = text_width_5x5(hint, 1);
			l_draw_text(lfb, (LW - hint_w) / 2, LH - 18, hint, 1, 0xFFBFBFBF);
		} else {
			g->scene = 2;
			const char *entries[PRACTICE_MENU_COUNT] = {
				boss_menu_label(BOSS_TYPE_MAGICIAN),
				boss_menu_label(BOSS_TYPE_CLASSIC),
				boss_menu_label(BOSS_TYPE_CHARIOT),
				boss_menu_label(BOSS_TYPE_HERMIT),
				boss_menu_label(BOSS_TYPE_TOWER),
				boss_menu_label(BOSS_TYPE_YELLOW),
				boss_menu_label(BOSS_TYPE_BLUE),
				"EXIT TO MAIN MENU"
			};

			int base_y = 16;
			int row_h = 13;
			for (int i = 0; i < PRACTICE_MENU_COUNT; i++) {
				int is_selected = (i == g->practice_menu_selection);
				uint32_t color = 0xFFAAAAAA;
				uint32_t cursor_color = 0xFF00FF00;

				if (i < BOSS_TYPE_COUNT) {
					if (is_selected) {
						int boss_type = practice_menu_boss_type_for_index(i);
						color = practice_menu_boss_color(boss_type);
						cursor_color = color;
					}
				} else {
					color = is_selected ? 0xFF00FF00 : 0xFFFFFFFF;
				}

				int entry_y = base_y + i * row_h;
				if (is_selected) {
					l_draw_text(lfb, 16, entry_y, ">", 1, cursor_color);
				}
				l_draw_text(lfb, 26, entry_y, entries[i], 1, color);
			}

			if (g->practice_menu_selection < BOSS_TYPE_COUNT) {
				int selected_boss_type = practice_menu_boss_type_for_index(g->practice_menu_selection);
				render_practice_boss_preview(g, lfb, selected_boss_type);
			}

			const char *hint = "UP/DOWN BOSS  LEFT/RIGHT LEVEL  SPACE ENTER";
			int hint_w = text_width_5x5(hint, 1);
			l_draw_text(lfb, (LW - hint_w) / 2, LH - 18, hint, 1, 0xFFBFBFBF);
		}
		return;
	}

	if (g->game_over) {
		g->scene = 3;
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
			uint32_t player_color = explosive_shot_active(g) ? 0xFFFF0000 :
				(triple_shot_active(g) ? 0xFF0000FF : 0xFF00FF00);
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
		g->scene = 4;
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

		uint32_t player_color = explosive_shot_active(g) ? 0xFFFF0000 :
			(triple_shot_active(g) ? 0xFF0000FF : 0xFF00FF00);
		draw_sprite1r(lfb, &g->PLAYER, cx - p_w / 2, cy - p_h / 2, player_color);

		int r = 22;
		int hx[WIN_RING_ALIENS] = { cx, cx + r, cx + r, cx, cx - r, cx - r };
		int hy[WIN_RING_ALIENS] = { cy - r, cy - r/2, cy + r/2, cy + r, cy + r/2, cy - r/2 };
		for (int i = 0; i < WIN_RING_ALIENS; i++) {
			if (g->win_alien_explode_timer[i] > 0) {
				render_alien_explosion(lfb, hx[i], hy[i], g->win_alien_explode_timer[i], 0, 0);
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

		return;
	}

	if (g->overworld_cutscene_active) {
		g->scene = 5;
		render_overworld_cutscene(g, lfb);
		return;
	}

	if (g->in_shop) {
		g->scene = 6;
		shop_render(g, lfb);
		return;
	}

	if (g->boss_intro_active) {
		g->scene = 7;
		l_clear(lfb, 0xFF000000);

		const char *type_prefix = "ALIEN TYPE:";
		const char *type_value = boss_intro_type_text(g);
		const char *main_prefix = "MAIN ATTACK:";
		const char *main_value = boss_intro_main_attack_text(g);
		const char *special_prefix = "SPECIAL ATTACK:";
		const char *special_value = boss_intro_special_attack_text(g);
		const char *desc_line1 = boss_intro_desc_line1(g);
		const char *desc_line2 = boss_intro_desc_line2(g);

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
		} else if (g->boss_type == BOSS_TYPE_TOWER) {
			uint32_t tower_flicker_palette[4] = {0xFF6A4421, 0xFF8B5A2B, 0xFFC08A4B, 0xFF8B5A2B};
			int flicker_idx = ((BOSS_INTRO_FRAMES - g->boss_intro_timer) / 8) & 3;
			type_color = tower_flicker_palette[flicker_idx];
		} else if (g->boss_type == BOSS_TYPE_HERMIT) {
			uint32_t hermit_flicker_palette[4] = {0xFF8000FF, 0xFFB266FF, 0xFFD9B3FF, 0xFFB266FF};
			int flicker_idx = ((BOSS_INTRO_FRAMES - g->boss_intro_timer) / 8) & 3;
			type_color = hermit_flicker_palette[flicker_idx];
		} else if (g->boss_type == BOSS_TYPE_CHARIOT) {
			uint32_t chariot_flicker_palette[4] = {0xFFFF6A00, 0xFFFF8C00, 0xFFFFB347, 0xFFFF8C00};
			int flicker_idx = ((BOSS_INTRO_FRAMES - g->boss_intro_timer) / 8) & 3;
			type_color = chariot_flicker_palette[flicker_idx];
		} else if (g->boss_type == BOSS_TYPE_MAGICIAN) {
			type_color = 0xFF00E5FF;
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
			(g->boss_type == BOSS_TYPE_YELLOW) ? 0xFFFFFF00 :
			(g->boss_type == BOSS_TYPE_TOWER) ? 0xFF8B5A2B :
			(g->boss_type == BOSS_TYPE_HERMIT) ? 0xFF8000FF :
			(g->boss_type == BOSS_TYPE_CHARIOT) ? 0xFFFF8C00 :
			(g->boss_type == BOSS_TYPE_MAGICIAN) ? 0xFFA8A8A8 : 0xFFFF0000;
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
		} else if (g->boss_type == BOSS_TYPE_TOWER) {
			int special_value_w = text_width_5x5(special_value, scale);
			special_x = (LW - (special_prefix_w + 6 + special_value_w)) / 2;
			special_text_x = special_x + special_prefix_w + 6;
			l_draw_text(lfb, special_x, y, special_prefix, scale, 0xFFFFFFFF);
			l_draw_text(lfb, special_text_x, y, special_value, scale, 0xFF4A2E12);
		} else if (g->boss_type == BOSS_TYPE_HERMIT) {
			int special_value_w = text_width_5x5(special_value, scale);
			special_x = (LW - (special_prefix_w + 6 + special_value_w)) / 2;
			special_text_x = special_x + special_prefix_w + 6;
			l_draw_text(lfb, special_x, y, special_prefix, scale, 0xFFFFFFFF);
			l_draw_text(lfb, special_text_x, y, special_value, scale, 0xFFB266FF);
		} else if (g->boss_type == BOSS_TYPE_CHARIOT) {
			int special_value_w = text_width_5x5(special_value, scale);
			special_x = (LW - (special_prefix_w + 6 + special_value_w)) / 2;
			special_text_x = special_x + special_prefix_w + 6;
			l_draw_text(lfb, special_x, y, special_prefix, scale, 0xFFFFFFFF);
			l_draw_text(lfb, special_text_x, y, special_value, scale, 0xFFFF8C00);
		} else if (g->boss_type == BOSS_TYPE_MAGICIAN) {
			int special_value_w = text_width_5x5(special_value, scale);
			special_x = (LW - (special_prefix_w + 6 + special_value_w)) / 2;
			special_text_x = special_x + special_prefix_w + 6;
			l_draw_text(lfb, special_x, y, special_prefix, scale, 0xFFFFFFFF);
			l_draw_text(lfb, special_text_x, y, special_value, scale, 0xFF00E5FF);
		} else if (g->level >= 3) {
			const char *spec1 = "PLASMA";
			const char *spec2 = "HEAL";
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
		uint32_t intro_boss_color = 0xFF00FF00;
		if (g->boss_type != BOSS_TYPE_YELLOW) {
			intro_boss_color = (g->boss_type == BOSS_TYPE_BLUE) ? 0xFF3399FF :
				(g->boss_type == BOSS_TYPE_TOWER) ? 0xFF8B5A2B :
				(g->boss_type == BOSS_TYPE_HERMIT) ? 0xFFB266FF :
				(g->boss_type == BOSS_TYPE_CHARIOT) ? 0xFFFF8C00 :
				(g->boss_type == BOSS_TYPE_MAGICIAN) ? magician_color_for_special_state(magician_intro_special_anim_active(g)) : 0xFF00FF00;
			if (g->boss_type == BOSS_TYPE_CLASSIC) {
				intro_boss_color = type_color;
			}
			if (!chariot_intro_charge_anim_active(g)) {
				draw_sprite1r(lfb, BS, boss_x, boss_y, intro_boss_color);
			}
		} else {
			intro_boss_color = 0xFFFFFF00;
		}
		render_intro_boss_attack_preview(g, lfb, boss_x, boss_y, BS, 1);

		uint32_t desc_color = intro_boss_color;
		if (g->boss_type == BOSS_TYPE_CLASSIC) {
			desc_color = 0xFF00FF00;
		} else if (g->boss_type == BOSS_TYPE_MAGICIAN) {
			desc_color = 0xFF00E5FF;
		}
		int desc1_w = text_width_5x5(desc_line1, 1);
		int desc2_w = text_width_5x5(desc_line2, 1);
		int desc1_x = (LW - desc1_w) / 2;
		int desc2_x = (LW - desc2_w) / 2;
		int desc1_y = LH - 30;
		int desc2_y = LH - 18;
		l_draw_text(lfb, desc1_x, desc1_y, desc_line1, 1, desc_color);
		l_draw_text(lfb, desc2_x, desc2_y, desc_line2, 1, desc_color);

		if (g->paused) {
			const char *paused_text = "PAUSED";
			int paused_w = text_width_5x5(paused_text, 1);
			int paused_x = LW - paused_w - 5;
			l_draw_text(lfb, paused_x, 5, paused_text, 1, 0xFFFFFFFF);
		}
		return;
	}

	l_clear(lfb, 0xFF000000);

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

	render_score_display(g, lfb);

	// Boss health bar on the left side of screen
	if (g->boss_alive) {
		uint32_t boss_color = (g->boss_type == BOSS_TYPE_BLUE) ? 0xFF3399FF :
			(g->boss_type == BOSS_TYPE_YELLOW) ? 0xFFFFFF00 :
			(g->boss_type == BOSS_TYPE_TOWER) ? 0xFF8B5A2B :
			(g->boss_type == BOSS_TYPE_HERMIT) ? 0xFFB266FF :
			(g->boss_type == BOSS_TYPE_CHARIOT) ? 0xFFFF8C00 :
			(g->boss_type == BOSS_TYPE_MAGICIAN) ? 0xFF00E5FF : 0xFF00FF00;
		int health_pct = (g->boss_max_health > 0) ? (g->boss_health * 100) / g->boss_max_health : 0;
		if (g->boss_type == BOSS_TYPE_BLUE) {
			if (health_pct <= 33) boss_color = 0xFF114488;
			else if (health_pct <= 67) boss_color = 0xFF1E6AD6;
		} else if (g->boss_type == BOSS_TYPE_YELLOW) {
			if (health_pct <= 33) boss_color = 0xFFFFC000;
			else if (health_pct <= 67) boss_color = 0xFFFFD84D;
		} else if (g->boss_type == BOSS_TYPE_TOWER) {
			if (health_pct <= 33) boss_color = 0xFF4A2E12;
			else if (health_pct <= 67) boss_color = 0xFF6A4421;
		} else if (g->boss_type == BOSS_TYPE_HERMIT) {
			if (health_pct <= 33) boss_color = 0xFF6A2AA9;
			else if (health_pct <= 67) boss_color = 0xFF9A4DDA;
		} else if (g->boss_type == BOSS_TYPE_CHARIOT) {
			if (health_pct <= 33) boss_color = 0xFFCC5C00;
			else if (health_pct <= 67) boss_color = 0xFFE67700;
		} else if (g->boss_type == BOSS_TYPE_MAGICIAN) {
			if (health_pct <= 33) boss_color = 0xFF0090AA;
			else if (health_pct <= 67) boss_color = 0xFF00B8D1;
		} else {
			if (health_pct <= 33) boss_color = 0xFFFF0000;
			else if (health_pct <= 67) boss_color = 0xFFFFFF00;
		}

		if (g->boss_type == BOSS_TYPE_MAGICIAN) {
			boss_color = magician_color_for_special_state(magician_mirror_attack_active(g));
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
		else if (g->next_boss_attack_type == TOWER_WALL_ATTACK) power_color = 0xFF8B5A2B;  // Tower walls
		else if (g->next_boss_attack_type == HERMIT_DODGE_ATTACK) power_color = 0xFFB266FF;  // Hermit dodge
		else if (g->next_boss_attack_type == CHARIOT_CHARGE_ATTACK) power_color = 0xFFFF8C00;  // Chariot charge
		else if (g->next_boss_attack_type == MAGICIAN_MIRROR_ATTACK) power_color = 0xFF00E5FF;  // Magician mirrors
		else power_color = 0xFF808080;  // Gray (unknown)

		int power_fill_w = (g->boss_power_max > 0) ? (g->boss_power_timer * power_bar_w) / g->boss_power_max : 0;
		draw_bar(lfb, power_bar_x, power_bar_y, power_bar_w, power_bar_h, power_fill_w, power_color);
	}

	// Draw a separator line below the top HUD (boss/level info) to frame gameplay area.
	{
		for (int x = 0; x < LW; x++) {
			l_putpix(lfb, x, TOP_HUD_SEPARATOR_Y, 0xFFFFFFFF);
		}
	}

	{
		static struct timespec fps_last_ts = {0, 0};
		static int fps_frame_count = 0;
		static int fps_value = 0;

		struct timespec fps_now;
		timespec_get(&fps_now, TIME_UTC);
		fps_frame_count++;

		if (fps_last_ts.tv_sec == 0 && fps_last_ts.tv_nsec == 0) {
			fps_last_ts = fps_now;
		}

		double fps_elapsed = (double)(fps_now.tv_sec - fps_last_ts.tv_sec) +
			(double)(fps_now.tv_nsec - fps_last_ts.tv_nsec) / 1000000000.0;
		if (fps_elapsed >= 1.0) {
			fps_value = (int)((fps_frame_count / fps_elapsed) + 0.5);
			fps_frame_count = 0;
			fps_last_ts = fps_now;
		}

		const char *label = "LEVEL:";
		int label_scale = 1;
		int label_w = text_width_5x5(label, label_scale);
		int x = LW - (label_w + 6 + 3) - 5;

		char fps_text[16];
		snprintf(fps_text, sizeof(fps_text), "FPS:%d", fps_value);
		int fps_w = text_width_5x5(fps_text, 1);
		int fps_x = x - fps_w - 8;
		l_draw_text(lfb, fps_x, 5, fps_text, 1, 0xFFFFFFFF);

		l_draw_text(lfb, x, 5, label, label_scale, 0xFFFFFFFF);
		l_draw_score(lfb, x + label_w + 6, 5, g->level, 0xFFFFFFFF);
	}

	// Draw a separator line above the bottom HUD (player/powerup info) with a small gap.
	{
		for (int x = 0; x < LW; x++) {
			l_putpix(lfb, x, BOTTOM_HUD_SEPARATOR_Y, 0xFFFFFFFF);
		}
	}

	render_player_health_bar(g, lfb);

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
		uint32_t boss_color = (g->boss_type == BOSS_TYPE_BLUE) ? 0xFF3399FF :
			(g->boss_type == BOSS_TYPE_TOWER) ? 0xFF8B5A2B :
			(g->boss_type == BOSS_TYPE_HERMIT) ? 0xFFB266FF :
			(g->boss_type == BOSS_TYPE_CHARIOT) ? 0xFFFF8C00 :
			(g->boss_type == BOSS_TYPE_MAGICIAN) ? magician_color_for_special_state(0) : 0xFF00FF00;
		if (g->boss_type == BOSS_TYPE_BLUE) {
			if (health_pct <= 33) boss_color = 0xFF114488;
			else if (health_pct <= 67) boss_color = 0xFF1E6AD6;
		} else if (g->boss_type == BOSS_TYPE_TOWER) {
			if (health_pct <= 33) boss_color = 0xFF4A2E12;
			else if (health_pct <= 67) boss_color = 0xFF6A4421;
		} else if (g->boss_type == BOSS_TYPE_HERMIT) {
			if (health_pct <= 33) boss_color = 0xFF6A2AA9;
			else if (health_pct <= 67) boss_color = 0xFF9A4DDA;
		} else if (g->boss_type == BOSS_TYPE_CHARIOT) {
			if (health_pct <= 33) boss_color = 0xFFCC5C00;
			else if (health_pct <= 67) boss_color = 0xFFE67700;
		} else if (g->boss_type == BOSS_TYPE_MAGICIAN) {
			if (health_pct <= 33) boss_color = 0xFF0090AA;
			else if (health_pct <= 67) boss_color = 0xFF00B8D1;
		} else {
			if (health_pct <= 33) boss_color = 0xFFFF0000;
			else if (health_pct <= 67) boss_color = 0xFFFFFF00;
		}

		if (g->boss_type == BOSS_TYPE_MAGICIAN) {
			boss_color = magician_color_for_special_state(magician_mirror_attack_active(g));
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
		int hide_for_chariot_explosion = (g->boss_type == BOSS_TYPE_CHARIOT && g->boss_power_active &&
				g->boss_attack_type == CHARIOT_CHARGE_ATTACK && g->boss_special_timer > 0);
		if (!hide_for_chariot_explosion) {
			draw_sprite1r(lfb, BS, g->boss_x, g->boss_y, boss_color);
		}

		if (g->boss_type == BOSS_TYPE_CHARIOT && g->boss_power_active && g->boss_attack_type == CHARIOT_CHARGE_ATTACK) {
			if (g->boss_special_timer > 0) {
				int cx = g->boss_x + BS->w / 2;
				int cy = g->boss_y + BS->h / 2;
				int age = CHARIOT_CHARGE_EXPLOSION_FRAMES - g->boss_special_timer;
				int r = 8 + age / 2;
				if (r > CHARIOT_CHARGE_EXPLOSION_RADIUS) r = CHARIOT_CHARGE_EXPLOSION_RADIUS;
				draw_filled_circle_clipped_y(lfb, cx, cy, r, 0xFFFF4500, GAMEPLAY_CLIP_Y_MIN, GAMEPLAY_CLIP_Y_MAX);
				draw_filled_circle_clipped_y(lfb, cx, cy, r - 3, 0xFFFFA500, GAMEPLAY_CLIP_Y_MIN, GAMEPLAY_CLIP_Y_MAX);
			}
		}

		if (g->boss_shield_active) {
			uint32_t shield_color = (g->boss_type == BOSS_TYPE_HERMIT) ? 0xFF8000FF : 0xFF00FF00;
			draw_sprite1r(lfb, &g->BOSS_SHIELD, boss_shield_x(g), boss_shield_y(g), shield_color);
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
			int axx = g->alien_old_x + c * (AS->w + spacing_x);
			int ay = g->alien_origin_y + r * (AS->h + spacing_y);
			int ayy = g->alien_old_y + r * (AS->h + spacing_y);

			// Red if 3+, Green if 2, white if 1
			uint32_t alien_color;
			if (g->level == 0 && tutorial_is_leave_alien(r, c)) {
				alien_color = 0xFFB266FF;
			} else if (g->yellow_boss_marked[r][c]) {
				alien_color = 0xFFFFFF00;
			} else if (g->alien_hermit_regen[r][c]) {
				alien_color = 0xFFB266FF;
			} else {
				alien_color = (g->alien_health[r][c] >= 3) ? 0xFFFF0000 : (g->alien_health[r][c] == 2) ? 0xFF00FF00 : 0xFFFFFFFF;
			}

			if (ax != axx || ay != ayy) {
				draw_sprite1r(lfb, AS, axx, ayy, 0xFF000000);
			}
			draw_sprite1r(lfb, AS, ax, ay, alien_color);
		}
	}

	g->alien_old_x = g->alien_origin_x;
	g->alien_old_y = g->alien_origin_y;

	if (g->level == 0) {
		const sprite1r_t *TS = g->alien_frame ? &g->ALIEN_B : &g->ALIEN_A;
		int spacing_x = 6;
		int leave_x = g->alien_origin_x + TUTORIAL_LEAVE_COL * (TS->w + spacing_x);
		int leave_y = g->alien_origin_y + TUTORIAL_LEAVE_ROW * (TS->h + 5);
		int label_w = text_width_5x5("LEAVE", 1);
		int label_x = leave_x + (TS->w / 2) - (label_w / 2);
		int label_y = leave_y - 9;
		l_draw_text(lfb, label_x, label_y, "LEAVE", 1, 0xFFB266FF);
	}

	for (int r = 0; r < AROWS; r++) {
		for (int c = 0; c < ACOLS; c++) {
			if (g->alien_explode_timer[r][c] <= 0) continue;
			int ax = g->alien_origin_x + c * (AS->w + spacing_x) + AS->w / 2;
			int ay = g->alien_origin_y + r * (AS->h + spacing_y) + AS->h / 2;
			render_alien_explosion(lfb, ax, ay, g->alien_explode_timer[r][c], g->alien_explode_points[r][c], 1);
		}
	}

	uint8_t do_render_player = 0;

	if (g->player_dying) {
		render_player_explosion(g, lfb);
	} else if (player_visible_with_iframes(g)) {
		int post_boss_exit_fly = g->player_exit_fly_active && !g->boss_alive;
		if (post_boss_exit_fly) {
			int cx = g->player_x + g->PLAYER.w / 2;
			int cy = g->player_y + g->PLAYER.h / 2;
			render_ship_with_vertical_thruster(g, lfb, cx, cy, 1, g->powerup_spawn_timer);
		} else {
			do_render_player = 1;
		}
	}

	render_player_shield_ring(g, lfb);

	if (g->level == 0) {
		if (!any_powerup_active(g)) {
			for (int i = 0; i < TUTORIAL_POWERUP_COUNT; i++) {
				int px, py;
				tutorial_powerup_icon_position(i, &px, &py);
				draw_powerup_icon(lfb, px, py, tutorial_powerup_type_for_index(i));
			}
		}
	} else if (g->powerup_active) {
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
	for (int s = 0; s < CHARIOT_ARC_SHOT_COUNT; s++) {
		if (!g->boss_arc_shot[s].alive) continue;
		int x_dir = g->boss_arc_shot_dx[s];
		for (int i = 0; i < 5; i++) {
			int x = g->boss_arc_shot[s].x + ((x_dir * i) / 2);
			int y = g->boss_arc_shot[s].y + i;
			l_putpix(lfb, x, y, 0xFFFF8C00);
		}
	}
	render_magician_wave(g, lfb);
	for (int s = 0; s < YELLOW_BOSS_ALIENS; s++) {
		if (!g->yellow_beam_shot[s].alive) continue;
		for (int i = 0; i < 5; i++) {
			l_putpix(lfb, g->yellow_beam_shot[s].x, g->yellow_beam_shot[s].y + i, 0xFFFFFF00);
		}
	}
	for (int ai = 0; ai < MAX_TOWER_ASTEROIDS; ai++) {
		if (g->tower_asteroid[ai].alive) {
			render_tower_asteroid(lfb, g->tower_asteroid[ai].x, g->tower_asteroid[ai].y, g->tower_asteroid_spin[ai]);
		} else if (g->tower_asteroid_exploding[ai]) {
			int r = tower_asteroid_explosion_radius(g, ai);
			draw_filled_circle_clipped_y(lfb, g->tower_asteroid[ai].x, g->tower_asteroid[ai].y, r, 0xFFFF4500,
					GAMEPLAY_CLIP_Y_MIN, GAMEPLAY_CLIP_Y_MAX);
			draw_filled_circle_clipped_y(lfb, g->tower_asteroid[ai].x, g->tower_asteroid[ai].y, r - 2, 0xFFFF8C00,
					GAMEPLAY_CLIP_Y_MIN, GAMEPLAY_CLIP_Y_MAX);
			if ((g->tower_asteroid_explode_timer[ai] & 1) == 0) {
				int core_r = r - 4;
				if (core_r < 1) core_r = 1;
				draw_filled_circle_clipped_y(lfb, g->tower_asteroid[ai].x, g->tower_asteroid[ai].y, core_r, 0xFFFFFF00,
						GAMEPLAY_CLIP_Y_MIN, GAMEPLAY_CLIP_Y_MAX);
			}
		}
	}

	// Emperor plasma beam: stacked rectangular slices with fading intensity,
	// plus a tapered trailing tail at the back end.
	if (g->boss_laser.alive) {
		const sprite1r_t *BS = active_boss_sprite(g);
		int laser_w = BS->w;
		int laser_center_x = g->boss_laser.x;
		uint32_t laser_color = (g->boss_attack_type == 1) ? 0xFF00FF00 : 0xFF8000FF;

		const int seg_count = 5;
		const int seg_offset_y[5] = {0, 3, 7, 12, 18};
		const int seg_height[5] = {3, 3, 3, 3, 2};
		const int seg_shrink[5] = {0, 0, 1, 2, 3};
		const int seg_intensity[5] = {100, 80, 62, 44, 28};

		for (int s = 0; s < seg_count; s++) {
			int seg_w = laser_w - (seg_shrink[s] * 2);
			if (seg_w < 1) continue;
			int seg_left = laser_center_x - seg_w / 2;
			int seg_top = g->boss_laser.y - seg_offset_y[s];
			uint32_t seg_color = color_with_intensity(laser_color, seg_intensity[s]);

			for (int py = 0; py < seg_height[s]; py++) {
				int y = seg_top + py;
				if (y < GAMEPLAY_CLIP_Y_MIN || y > GAMEPLAY_CLIP_Y_MAX) continue;
				for (int px = 0; px < seg_w; px++) {
					int x = seg_left + px;
					if (x >= 0 && x < LW) {
						l_putpix(lfb, x, y, seg_color);
					}
				}
			}
		}

		// Draw a tapered tail behind the faded rectangles.
		int tail_start_y = g->boss_laser.y - 22;
		for (int i = 0; i < 8; i++) {
			int y = tail_start_y - i;
			if (y < GAMEPLAY_CLIP_Y_MIN || y > GAMEPLAY_CLIP_Y_MAX) continue;
			int half_w = 3 - (i / 2);
			if (half_w < 0) half_w = 0;
			int intensity = 22 - (i * 2);
			if (intensity < 6) intensity = 6;
			uint32_t tail_color = color_with_intensity(laser_color, intensity);
			for (int x = laser_center_x - half_w; x <= laser_center_x + half_w; x++) {
				if (x >= 0 && x < LW) {
					l_putpix(lfb, x, y, tail_color);
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
				render_black_hole_explosion_clipped(lfb, g->boss_bomb.x, g->boss_bomb.y, r);
			} else {
				draw_filled_circle_clipped_y(lfb, g->boss_bomb.x, g->boss_bomb.y, r, 0xFF0B3A8F,
						GAMEPLAY_CLIP_Y_MIN, GAMEPLAY_CLIP_Y_MAX);
				draw_filled_circle_clipped_y(lfb, g->boss_bomb.x, g->boss_bomb.y, r - 3, 0xFF1E6AD6,
						GAMEPLAY_CLIP_Y_MIN, GAMEPLAY_CLIP_Y_MAX);
				if ((g->boss_bomb.explode_timer & 1) == 0) {
					int core_r = r - 7;
					if (core_r < 1) core_r = 1;
					draw_filled_circle_clipped_y(lfb, g->boss_bomb.x, g->boss_bomb.y, core_r, 0xFF66CCFF,
							GAMEPLAY_CLIP_Y_MIN, GAMEPLAY_CLIP_Y_MAX);
				}
			}
		}
	}

	for (int i = 0; i < HERMIT_MAX_LIGHTNINGS; i++) {
		const hermit_lightning_t *l = &g->hermit_lightning[i];
		if (!l->active) continue;
		uint32_t bolt_color = (l->flash_timer > 0) ? 0xFFFFFF00 : 0xFF8000FF;
		draw_zigzag_segment(lfb, (int)l->start_x, (int)l->start_y, (int)l->tip_x, (int)l->tip_y, 3, 7, bolt_color);
	}

	if (magician_mirror_is_reflective(&g->magician_mirror)) {
		int half_w = g->magician_mirror.width / 2;
		for (int x = g->magician_mirror.x - half_w; x <= g->magician_mirror.x + half_w; x++) {
			if (!magician_mirror_has_segment_at_x(&g->magician_mirror, x)) continue;
			putpix_clipped_y(lfb, x, g->magician_mirror.y, 0xFFB0B0B0, GAMEPLAY_CLIP_Y_MIN, GAMEPLAY_CLIP_Y_MAX);
			if ((x & 3) == 0) {
				putpix_clipped_y(lfb, x, g->magician_mirror.y + 1, 0xFFE0E0E0, GAMEPLAY_CLIP_Y_MIN, GAMEPLAY_CLIP_Y_MAX);
			}
		}
	}

	if (magician_mirror_is_reflective(&g->magician_mirror_alt)) {
		int half_w = g->magician_mirror_alt.width / 2;
		for (int x = g->magician_mirror_alt.x - half_w; x <= g->magician_mirror_alt.x + half_w; x++) {
			if (!magician_mirror_has_segment_at_x(&g->magician_mirror_alt, x)) continue;
			putpix_clipped_y(lfb, x, g->magician_mirror_alt.y, 0xFFB0B0B0, GAMEPLAY_CLIP_Y_MIN, GAMEPLAY_CLIP_Y_MAX);
			if ((x & 3) == 0) {
				putpix_clipped_y(lfb, x, g->magician_mirror_alt.y + 1, 0xFFE0E0E0, GAMEPLAY_CLIP_Y_MIN, GAMEPLAY_CLIP_Y_MAX);
			}
		}
	}

	render_magician_side_walls(lfb, g);

	render_tower_walls(lfb, g);

	render_fps_counter(lfb);

	if (g->perfect_text_timer > 0) {
		const char *perfect_text = "PERFECT";
		int perfect_scale = 4;
		int perfect_w = text_width_5x5(perfect_text, perfect_scale);
		int perfect_x = (LW - perfect_w) / 2;
		int perfect_y = (LH - (perfect_scale * 5)) / 2;
		l_draw_text(lfb, perfect_x, perfect_y, perfect_text, perfect_scale, 0xFF00FF00);
	}

	if (g->paused) {
		const char *paused_text = "PAUSED";
		int panel_w = (LW * 3) / 4;
		int panel_h = LH / 3;
		int panel_x = (LW - panel_w) / 2;
		int panel_y = (LH - panel_h) / 2;

		for (int y = panel_y; y < panel_y + panel_h; y++) {
			for (int x = panel_x; x < panel_x + panel_w; x++) {
				l_putpix(lfb, x, y, 0xFF000000);
			}
		}

		for (int x = panel_x; x < panel_x + panel_w; x++) {
			l_putpix(lfb, x, panel_y, 0xFFFFFFFF);
			l_putpix(lfb, x, panel_y + panel_h - 1, 0xFFFFFFFF);
		}
		for (int y = panel_y; y < panel_y + panel_h; y++) {
			l_putpix(lfb, panel_x, y, 0xFFFFFFFF);
			l_putpix(lfb, panel_x + panel_w - 1, y, 0xFFFFFFFF);
		}

		int scale = 3;
		int text_w = text_width_5x5(paused_text, scale);
		int text_x = panel_x + (panel_w - text_w) / 2;
		int text_y = panel_y + (panel_h - (scale * 5)) / 2;
		l_draw_text(lfb, text_x, text_y, paused_text, scale, 0xFFFFFFFF);
	}

	if (do_render_player) render_player(g, lfb);
}
