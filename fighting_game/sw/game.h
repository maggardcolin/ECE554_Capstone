#pragma once
#include <stdint.h>
#include "gfx.h"

/* Animation states */
typedef enum {
    ANIM_IDLE = 0,
    ANIM_WALK,
    ANIM_PUNCH,
    ANIM_HIT,
    ANIM_BLOCK,
    ANIM_JUMP,
    ANIM_VICTORY,
    ANIM_DEFEATED,
} anim_t;

/* Game phase */
typedef enum {
    STATE_FIGHT = 0,
    STATE_RESULT,
} game_state_t;

typedef enum {
    WINNER_PLAYER = 0,
    WINNER_CPU,
    WINNER_DRAW,
} winner_t;

typedef struct {
    int      x;            /* center-x position at ground level */
    int      hp;           /* 0 – MAX_HP */
    int      facing;       /* +1 = right, -1 = left */
    anim_t   anim;
    int      anim_frame;   /* 0 or 1 */
    int      anim_timer;   /* frames since last frame toggle */
    int      punch_timer;  /* -1 = not punching, else frames elapsed */
    int      hit_stun;     /* frames of stagger remaining */
    int      punch_hit_done; /* flag: hit already checked this punch */
    int      jump_y;       /* vertical displacement above ground (px) */
    int      jump_vy;      /* vertical velocity (+up, -down) */
    int      jump_grav_tick; /* slows gravity application for floatier jumps */
} fighter_t;

typedef struct {
    game_state_t state;
    fighter_t    fighters[2];   /* [0] = player, [1] = CPU */
    int          timer_frames;  /* countdown frames (3600 = 120 s at 30 fps) */
    winner_t     winner;
    int          cpu_think_timer;
    int          result_delay;  /* frames since result was reached */
} game_t;

void game_init(game_t *g);
void game_update(game_t *g, uint32_t buttons, uint32_t vsync_counter);
void game_render(game_t *g, lfb_t *lfb);
