#pragma once
#include <stdint.h>
#include "gfx.h"
#include "sprite.h"

/// Powerup types - extend this enum to add new powerups
typedef enum {
    POWERUP_DOUBLE_SHOT = 0,  // Double damage (20 points per kill instead of 10)
    POWERUP_TRIPLE_SHOT = 1,  // Triple bullet spread (main + ±15 degrees)
    POWERUP_RAPID_FIRE = 2,   // Rapid fire (changes fire cooldown)
    POWERUP_EXPLOSIVE = 3,    // Explosive kills can chain to nearby aliens
    POWERUP_COUNT = 4         // Total number of powerup types (update when adding new powerups)
} powerup_type_t;

/// Shop item types - extend to add more items later
typedef enum {
    SHOP_ITEM_FIRE_SPEED = 0,
    SHOP_ITEM_MOVE_SPEED = 1,
    SHOP_ITEM_LIFE = 2,
    SHOP_ITEM_DAMAGE = 3,
    SHOP_ITEM_PIERCE = 4,
    SHOP_ITEM_COUNT = 5
} shop_item_type_t;

typedef enum {
    BOSS_TYPE_CLASSIC = 0,
    BOSS_TYPE_BLUE = 1,
    BOSS_TYPE_YELLOW = 2,
    BOSS_TYPE_COUNT = 3
} boss_type_t;

typedef struct {
    int x, y;
    int alive;
    int damage_remaining;
    int pierce_active;
    int last_hit_r;
    int last_hit_c;
} bullet_t;

typedef struct {
    int x;
    int y;
    int dy;
    int alive;
    int exploding;
    int explode_timer;
    int hit_player;
    int hits_taken;
    int reversed;
    int reverse_damage_applied;
} boss_bomb_t;

#define MAX_PSHOTS 8
#define PLAYER_LIVES 5
#define PLAYER_BASE_SPEED 2
#define PLAYER_BASE_DAMAGE 1
#define MAX_SHOP_ITEMS 3
#define ACOLS 11
#define AROWS 5
#define START_LEVEL 1

#define BOSS_MAX_HEALTH(level) (\
    level == 0 ? 0 : \
    (20 + (level - 1) * 5) \
)
#define BOSS_PERIOD(level) (\
    ((15 - (level - 1) * 2) > 5 ? (15 - (level - 1) * 2) \
    : 5) \
)

typedef struct {
    int score;
    int level;
    int start_screen;
    int start_screen_delay_timer;
    int main_menu_selection;   // 0 = start game, 1 = practice mode
    int practice_menu_active;
    int practice_menu_selection; // 0..2 boss entries, 3 exit
    int practice_level_selection;
    int practice_preview_timer;
    int practice_run_active;
    int practice_return_delay_timer;
    int forced_boss_type;      // -1 random, otherwise one of boss_type_t
    int lives;
    int level_complete;
    int level_complete_timer;
    int level_just_completed;
    int boss_intro_active;
    int boss_intro_timer;
    int overworld_cutscene_active;
    int overworld_cutscene_timer;
    int overworld_cutscene_from_node;
    int overworld_cutscene_to_node;

    // Powerup system
    int powerup_active;
    int powerup_x;
    int powerup_y;
    int powerup_timer;
    int powerup_spawn_timer;
    powerup_type_t powerup_type;     // Currently active powerup type
    int powerup_slot_timer[5];       // Duration of each powerup slot (600 ticks = 10 seconds)
    int powerup_type_slot[5];        // Type of powerup in each slot

    sprite1r_t PLAYER, ALIEN_A, ALIEN_B, BOSS_A, BOSS_B, BOSS2_A, BOSS2_B;
    sprite1r_t BOSS_SHIELD;
    sprite1r_t SHOP_LIFE, SHOP_FIRE, SHOP_MOVE, SHOP_DMG, SHOP_PIERCE;
    sprite1r_t BUNKER0, BUNKER1, BUNKER2, BUNKER3;
    sprite1r_t *bunkers[4];

    int player_x, player_y;

    uint8_t alien_alive[AROWS][ACOLS];
    int alien_health[AROWS][ACOLS];  // Hit points per alien (1 = one hit, 2 = two hits, etc.)
    int alien_explode_timer[AROWS][ACOLS];
    int alien_explode_points[AROWS][ACOLS];
    int alien_explode_hit_boss[AROWS][ACOLS];

    int alien_origin_x, alien_origin_y;
    int alien_dx;
    int alien_step_px, alien_drop_px;
    int alien_frame;
    int alien_timer;
    int alien_period;

    // Boss alien state
    int boss_alive;
    int boss_type; // 0 = classic, 1 = blue, 2 = yellow swarm
    int boss_health; // 0-20 HP
    int boss_max_health; // Max HP for current level
    int boss_x, boss_y;
    int boss_start_y;
    int boss_dx;
    int boss_dying;
    int boss_death_timer;
    int boss_explode_points;
    int boss_shield_active;
    int boss_frame;
    int boss_timer;
    int boss_period;
    int boss_power_timer;      // Charges from 0 to 600 (10 seconds)
    int boss_power_max;        // 600 ticks
    int boss_power_active;     // 1 when special attack is happening
    int boss_power_cooldown;   // Duration of purple/frozen state (30 ticks = 0.5 seconds)
    int boss_laser_last_hit_y; // Last y position where laser hit player (to prevent multiple hits)
    int boss_attack_type;      // 0 = purple laser, 1 = green heal laser, 2 = blue bomb, 3 = yellow shuffle
    int next_boss_attack_type; // The attack type that will be used for the next charge
    int boss_green_laser_last_hit_y; // Last y position where green laser hit aliens

    uint8_t yellow_boss_marked[AROWS][ACOLS]; // 1 for aliens that belong to the yellow boss swarm
    bullet_t yellow_beam_shot[5];             // Yellow boss synchronized beam shots

    bullet_t pshot[MAX_PSHOTS];       // Center shots
    bullet_t pshot_left[MAX_PSHOTS];  // Triple-shot: -15 degree angle
    bullet_t pshot_right[MAX_PSHOTS]; // Triple-shot: +15 degree angle
    bullet_t ashot;
    bullet_t boss_shot;
    bullet_t boss_laser;              // Purple laser straight down
    bullet_t boss_triple_shot[3];     // Blue boss basic attack (3-way spread)
    int boss_triple_shot_dx[3];       // Horizontal motion per triple-shot projectile
    boss_bomb_t boss_bomb;            // Blue boss charged bomb attack
    int fire_cooldown;

    int bunker_x[4];
    int bunker_y;

    int game_over;
    int game_over_score;
    int game_over_delay_timer;
    int win_screen;
    int win_explosion_index;
    int win_explosion_delay_timer;
    int win_alien_explode_timer[6];
    int win_prompt_ready;
    int player_dying;
    int player_death_timer;
    int player_iframe_timer;

    int paused;

    // Player upgrades (shop)
    int player_speed;          // Base 2, increased by shop item
    int fire_speed_bonus;      // Each point reduces cooldown
    int player_damage;         // Base 1, increased by shop item
    int pierce_unlocked;       // Permanent shop upgrade

    // Shop state
    int in_shop;
    int shop_count;            // Number of shops seen
    int shop_next_level;       // Level to start after exiting shop
    int shop_anim_timer;
    int shopkeeper_frame;
    int exit_available;        // 1 when boss is killed and exit is open
    int exit_was_available;
    int exit_blink_timer;
    int exit_blink_toggles_remaining;
    struct {
        int active;
        int x, y;
        shop_item_type_t type;
        int price;
    } shop_items[MAX_SHOP_ITEMS];

} game_t;

void game_init(game_t *g);
void game_reset(game_t *g);
void game_update(game_t *g, uint32_t buttons, uint32_t vsync_counter);
void game_render(game_t *g, lfb_t *lfb);
