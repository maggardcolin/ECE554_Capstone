#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "gfx.h"
#include "sprite.h"

// ─── Track ────────────────────────────────────────────────────────────────────
#define SEG_LEN      100.0f   // world units per track segment
#define TRACK_SEGS   200      // total segments per lap
#define TRACK_LEN    (SEG_LEN * TRACK_SEGS)
#define TRACK_LAPS   3        // laps to finish

/// One segment of the track (100 world-unit strip of road)
typedef struct {
    float curve;  // lateral curvature per world unit: + = right, - = left
    float hill;   // vertical change (for future hills; currently unused)
} seg_t;

// ─── Items ────────────────────────────────────────────────────────────────────
#define ITEM_NONE    0
#define ITEM_BANANA  1
#define ITEM_SHELL   2

#define NUM_OPPS     3   // AI opponents
#define NUM_BOXES    5   // item boxes on track
#define NUM_BANANAS  8   // max simultaneous dropped bananas
#define NUM_SHELLS   4   // max simultaneous flying shells

// ─── Player ───────────────────────────────────────────────────────────────────
typedef struct {
    float pos;          // total distance traveled (increases monotonically)
    float x;            // lateral position: -1=left edge, 0=center, 1=right edge
    float speed;        // current speed (segments per frame, 0..MAX_SPEED)
    int   laps;         // completed laps
    int   place;        // current race place (1-4)
    int   item;         // held item: ITEM_NONE / ITEM_BANANA / ITEM_SHELL
    int   spin_timer;   // spinning-out countdown (frames)
    int   boost_timer;  // speed-boost countdown (frames)
    int   invuln;       // invulnerability frames after spin
} player_t;

// ─── AI Opponent ──────────────────────────────────────────────────────────────
typedef struct {
    float pos;          // total distance
    float x;            // lateral position
    float speed;
    int   laps;
    int   spin_timer;
    int   color_idx;    // 1-3 (index into kart colors table)
    float target_x;     // steering target
} opp_t;

// ─── Item Box ─────────────────────────────────────────────────────────────────
typedef struct {
    float track_pos;    // position on track (0..TRACK_LEN)
    float x;            // lateral position
    bool  active;
    int   respawn;      // countdown to re-activate
} itembox_t;

// ─── Banana ───────────────────────────────────────────────────────────────────
typedef struct {
    float track_pos;
    float x;
    bool  active;
} banana_t;

// ─── Shell ────────────────────────────────────────────────────────────────────
typedef struct {
    float track_pos;
    float x;
    float speed;
    bool  active;
    bool  from_player;  // true if fired by player (hits opponents), false if from AI
} shell_t;

// ─── Game state machine ───────────────────────────────────────────────────────
typedef enum {
    GS_TITLE,
    GS_COUNTDOWN,
    GS_RACE,
    GS_FINISH,
} game_state_t;

// ─── Full game state ──────────────────────────────────────────────────────────
typedef struct {
    seg_t       track[TRACK_SEGS];
    player_t    player;
    opp_t       opps[NUM_OPPS];
    itembox_t   boxes[NUM_BOXES];
    banana_t    bananas[NUM_BANANAS];
    shell_t     shells[NUM_SHELLS];
    game_state_t state;
    int         timer;          // race timer (ticks, 60 fps)
    int         countdown;      // countdown frames remaining
    int         finish_timer;   // timer shown at finish screen
    int         best_lap;       // best lap time (ticks)
    int         lap_start;      // tick when current lap started
    bool        prop_quit;      // signal to shut down (ESC on finish)

    // Sprite resources
    sprite1r_t  spr_kart;
    sprite1r_t  spr_itembox;
    sprite1r_t  spr_banana;
    sprite1r_t  spr_shell;

    // Rendering scratch: road center x and half-width per scanline
    // (populated during game_render, used for sprite placement)
    float road_cx[LH];
    float road_hw[LH];
} game_t;

// ─── Public API ───────────────────────────────────────────────────────────────
void game_init(game_t *g);
void game_update(game_t *g, uint32_t buttons, uint32_t tick);
void game_render(game_t *g, lfb_t *lfb);
