#pragma once
#include <stdint.h>
#include "gfx.h"
#include "sprite.h"

typedef struct { int x,y; int alive; } bullet_t;

typedef struct {
    int score;

    sprite1r_t PLAYER, ALIEN_A, ALIEN_B;
    sprite1r_t BUNKER0, BUNKER1, BUNKER2, BUNKER3;
    sprite1r_t *bunkers[4];

    int player_x, player_y;

    enum { ACOLS = 11, AROWS = 5 };
    uint8_t alien_alive[AROWS][ACOLS];

    int alien_origin_x, alien_origin_y;
    int alien_dx;
    int alien_step_px, alien_drop_px;
    int alien_frame;
    int alien_timer;
    int alien_period;

    bullet_t pshot;
    bullet_t ashot;
    int fire_cooldown;

    int bunker_x[4];
    int bunker_y;

} game_t;

void game_init(game_t *g);
void game_reset(game_t *g);
void game_update(game_t *g, uint32_t buttons, uint32_t vsync_counter);
void game_render(game_t *g, lfb_t *lfb);
