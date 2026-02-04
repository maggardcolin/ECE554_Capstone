#pragma once
#include <stdint.h>
#include "gfx.h"
#include "sprite.h"

/// Powerup types - extend this enum to add new powerups
typedef enum {
    POWERUP_DOUBLE_SHOT = 0,  // Double damage (20 points per kill instead of 10)
    POWERUP_TRIPLE_SHOT = 1,  // Triple bullet spread (main + ±15 degrees)
    POWERUP_RAPID_FIRE = 2,   // Rapid fire (changes fire cooldown)
    POWERUP_COUNT = 3         // Total number of powerup types (update when adding new powerups)
} powerup_type_t;

/// Shop item types - extend to add more items later
typedef enum {
    SHOP_ITEM_FIRE_SPEED = 0,
    SHOP_ITEM_MOVE_SPEED = 1,
    SHOP_ITEM_LIFE = 2,
    SHOP_ITEM_COUNT = 3
} shop_item_type_t;

typedef struct { int x,y; int alive; } bullet_t;

#define MAX_PSHOTS 8
#define PLAYER_LIVES 3
#define MAX_SHOP_ITEMS 3
#define ACOLS 11
#define AROWS 5

typedef struct {
    int score;
    int level;
    int start_screen;
    int start_screen_delay_timer;
    int lives;
    int level_complete;
    int level_complete_timer;
    int level_just_completed;

    // Powerup system
    int powerup_active;
    int powerup_x;
    int powerup_y;
    int powerup_timer;
    int powerup_spawn_timer;
    powerup_type_t powerup_type;     // Currently active powerup type
    int powerup_slot_timer[5];       // Duration of each powerup slot (600 ticks = 10 seconds)
    int powerup_type_slot[5];        // Type of powerup in each slot

    sprite1r_t PLAYER, ALIEN_A, ALIEN_B, BOSS_A, BOSS_B;
    sprite1r_t BUNKER0, BUNKER1, BUNKER2, BUNKER3;
    sprite1r_t *bunkers[4];

    int player_x, player_y;

    uint8_t alien_alive[AROWS][ACOLS];
    int alien_health[AROWS][ACOLS];  // Hit points per alien (1 = one hit, 2 = two hits, etc.)

    int alien_origin_x, alien_origin_y;
    int alien_dx;
    int alien_step_px, alien_drop_px;
    int alien_frame;
    int alien_timer;
    int alien_period;

    // Boss alien state
    int boss_alive;
    int boss_health; // 0-20 HP
    int boss_max_health; // Max HP for current level
    int boss_x, boss_y;
    int boss_dx;
    int boss_frame;
    int boss_timer;
    int boss_period;

    bullet_t pshot[MAX_PSHOTS];       // Center shots
    bullet_t pshot_left[MAX_PSHOTS];  // Triple-shot: -15 degree angle
    bullet_t pshot_right[MAX_PSHOTS]; // Triple-shot: +15 degree angle
    bullet_t ashot;
    bullet_t boss_shot;
    int fire_cooldown;

    int bunker_x[4];
    int bunker_y;

    int game_over;
    int game_over_score;
    int game_over_delay_timer;

    // Player upgrades (shop)
    int player_speed;          // Base 2, increased by shop item
    int fire_speed_bonus;      // Each point reduces cooldown

    // Shop state
    int in_shop;
    int shop_count;            // Number of shops seen
    int shop_next_level;       // Level to start after exiting shop
    int shop_anim_timer;
    int shopkeeper_frame;
    int exit_available;        // 1 when boss is killed and exit is open
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
