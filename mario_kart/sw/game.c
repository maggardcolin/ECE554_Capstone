// game.c: Mario Kart style pseudo-3D racing game
// Implements raycasting-style road projection, racing physics, AI, and items.
#include "game.h"
#include "gfx.h"
#include "sprite.h"
#include "font.h"
#include "../hw_contract.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ─── Rendering constants ──────────────────────────────────────────────────────
#define HORIZON_Y   80                  // y-coordinate of the horizon line
#define ROAD_ROWS   (LH - HORIZON_Y)    // number of scanlines for the road (100)
#define ROAD_HW     110                 // road half-width at the very bottom (pixels)
#define CURVE_K     55.0f               // curve accumulation strength
// For depth: depth = CAM_DEPTH / (y - HORIZON_Y). Controls perspective squeeze.
#define CAM_DEPTH   1800.0f

// ─── Racing constants ─────────────────────────────────────────────────────────
#define MAX_SPEED       1.6f    // max speed in track-segments per frame
#define ACCEL           0.032f  // acceleration per frame
#define BRAKE           0.070f  // braking deceleration per frame
#define DRAG            0.012f  // passive friction per frame
#define TURN_RATE       0.034f  // lateral movement speed (road-width units per frame)
#define GRASS_MULT      0.60f   // speed multiplier when off-road (|x| > 1)
#define SPIN_SPEED_MULT 0.25f   // speed multiplier during spin-out
#define BOOST_MULT      1.55f   // speed multiplier during mushroom boost

#define OPP_SPEED       1.30f   // base AI speed (slightly below player max)
#define OPP_STEER       0.025f  // AI lateral correction per frame

#define SHELL_SPEED     5.0f    // shell travel speed (segments/frame)
#define MAX_SHELL_DIST  2000.0f // shell disappears after this distance

#define ITEM_PICKUP_DIST  25.0f // track distance within which item is collected
#define ITEM_PICKUP_X     0.55f // lateral clearance for item collection
#define BOX_RESPAWN_TICKS 300   // ticks before item box respawns (~5s at 60fps)

#define COUNTDOWN_TICKS   60    // ticks per countdown number (3, 2, 1, GO) — ~2s at 30fps
#define FINISH_WAIT_TICKS 300   // stay on finish screen before quitting
#define AUTO_START_TICKS  300   // auto-start race if no button pressed (title screen demo mode)
#define NO_LAP_TIME  0x7FFFFFFF // sentinel: no lap completed yet

// ─── Colors ───────────────────────────────────────────────────────────────────
#define COL_SKY_TOP  0xFF081828u
#define COL_SKY_BOT  0xFF1A6FBFu
#define COL_MOUNT    0xFF28485Au
#define COL_GRASS1   0xFF206010u
#define COL_GRASS2   0xFF309018u
#define COL_ROAD1    0xFF545454u
#define COL_ROAD2    0xFF444444u
#define COL_RUMBLE1  0xFFBB2020u
#define COL_RUMBLE2  0xFFEEEEEEu
#define COL_LINE     0xFFFFFFFFu
#define COL_WHITE    0xFFFFFFFFu
#define COL_BLACK    0xFF000000u
#define COL_YELLOW   0xFFFFCC00u
#define COL_ORANGE   0xFFFF8800u
#define COL_RED      0xFFFF3030u
#define COL_GREEN    0xFF30CC30u
#define COL_BLUE     0xFF3060FFu
#define COL_DARKGRAY 0xFF222222u
#define COL_BOX      0xFFFFDD00u
#define COL_BANANA   0xFFFFCC00u
#define COL_SHELL    0xFFFF4422u

static const uint32_t KART_COLOR[4] = {
    0xFFFF3030u,   // player  - red
    0xFF30CC30u,   // opp 0   - green
    0xFF3060FFu,   // opp 1   - blue
    0xFFFFAA00u,   // opp 2   - yellow
};

// ─── Inline helpers ───────────────────────────────────────────────────────────
static inline float fwrap(float v, float len) {
    v = fmodf(v, len);
    if (v < 0) v += len;
    return v;
}
static inline float fclampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
// Blend two ARGB colors: t=0 → a, t=1 → b
static inline uint32_t lerp_col(uint32_t a, uint32_t b, float t) {
    uint32_t ar=(a>>16)&0xFF, ag=(a>>8)&0xFF, ab_=(a)&0xFF;
    uint32_t br=(b>>16)&0xFF, bg=(b>>8)&0xFF, bb_=(b)&0xFF;
    uint32_t r=(uint32_t)(ar+(br-ar)*t);
    uint32_t g=(uint32_t)(ag+(bg-ag)*t);
    uint32_t bv=(uint32_t)(ab_+(bb_-ab_)*t);
    return 0xFF000000u|(r<<16)|(g<<8)|bv;
}
// Draw a filled horizontal span
static inline void hline(lfb_t *lfb, int y, int x0, int x1, uint32_t col) {
    if ((unsigned)y >= (unsigned)LH) return;
    if (x0 > x1) { int tmp=x0; x0=x1; x1=tmp; }
    if (x0 < 0) x0 = 0;
    if (x1 >= LW) x1 = LW - 1;
    uint32_t *row = lfb->px + y * LW;
    for (int x = x0; x <= x1; x++) row[x] = col;
}
// Draw a filled rect
static inline void fillrect(lfb_t *lfb, int x, int y, int w, int h, uint32_t col) {
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) l_putpix(lfb, x+dx, y+dy, col);
    }
}

// ─── Track building ───────────────────────────────────────────────────────────
/// init_track: Build an oval racing circuit from 200 track segments.
/// The track forms a clockwise oval with two sweeping right-hand turns
/// and two parallel straights.
static void init_track(seg_t *t) {
    //  0- 24: start straight (finish line at seg 0)
    // 25- 39: entry into turn 1 (curve ramps up)
    // 40- 89: turn 1 (tight right)
    // 90-104: exit turn 1 (curve ramps down)
    //105-129: back straight
    //130-144: entry into turn 2
    //145-179: turn 2 (tight right)
    //180-199: exit turn 2 + finish straight
    for (int i = 0; i < TRACK_SEGS; i++) {
        float c = 0.0f;
        if (i < 25) {
            c = 0.0f;
        } else if (i < 40) {
            c = 0.22f * (float)(i - 25) / 15.0f;
        } else if (i < 90) {
            c = 0.22f;
        } else if (i < 105) {
            c = 0.22f * (float)(105 - i) / 15.0f;
        } else if (i < 130) {
            c = 0.0f;
        } else if (i < 145) {
            c = 0.22f * (float)(i - 130) / 15.0f;
        } else if (i < 180) {
            c = 0.22f;
        } else {
            c = 0.22f * (float)(200 - i) / 20.0f;
        }
        t[i].curve = c;
        t[i].hill  = 0.0f;
    }
}

// ─── Initialisation ───────────────────────────────────────────────────────────
void game_init(game_t *g) {
    memset(g, 0, sizeof(*g));

    init_track(g->track);

    // Player starts at the start/finish line, center of road
    g->player.pos   = 0.0f;
    g->player.x     = 0.0f;
    g->player.speed = 0.0f;
    g->player.laps  = 0;
    g->player.item  = ITEM_NONE;

    // AI opponents spaced just behind the start line
    for (int i = 0; i < NUM_OPPS; i++) {
        g->opps[i].pos      = -(float)(i + 1) * SEG_LEN * 0.5f; // behind start
        g->opps[i].x        = (i % 2 == 0) ? 0.3f : -0.3f;
        g->opps[i].speed    = 0.0f;
        g->opps[i].color_idx= i + 1;
        g->opps[i].target_x = 0.0f;
        g->opps[i].laps     = 0;
    }

    // Item boxes: evenly spaced, alternating sides
    float box_spacing = TRACK_LEN / NUM_BOXES;
    float box_x_patt[5] = { 0.0f, 0.4f, -0.4f, 0.4f, -0.4f };
    for (int i = 0; i < NUM_BOXES; i++) {
        g->boxes[i].track_pos = box_spacing * (i + 0.5f);
        g->boxes[i].x         = box_x_patt[i];
        g->boxes[i].active    = true;
        g->boxes[i].respawn   = 0;
    }

    // Sprites
    g->spr_kart    = make_sprite_from_ascii(kart_rows,    kart_h);
    g->spr_itembox = make_sprite_from_ascii(itembox_rows, itembox_h);
    g->spr_banana  = make_sprite_from_ascii(banana_rows,  banana_h);
    g->spr_shell   = make_sprite_from_ascii(shell_rows,   shell_h);

    g->state     = GS_TITLE;
    g->timer     = 0;
    g->countdown = COUNTDOWN_TICKS * 4; // 3-2-1-GO
    g->best_lap  = NO_LAP_TIME;
    g->lap_start = 0;
}

// ─── Helper: compute race progress (laps*TRACK_LEN + pos_in_lap) ──────────────
// Handles opponents starting at negative positions (behind the start line):
// when laps == 0 and pos < 0, return pos directly so the racer correctly
// appears behind any racer with progress >= 0.
static float race_progress(float pos, int laps) {
    if (laps == 0 && pos < 0.0f) return pos;
    return (float)laps * TRACK_LEN + fwrap(pos, TRACK_LEN);
}

// ─── Helper: drop a banana ────────────────────────────────────────────────────
static void drop_banana(game_t *g, float track_pos, float x) {
    for (int i = 0; i < NUM_BANANAS; i++) {
        if (!g->bananas[i].active) {
            g->bananas[i].track_pos = fwrap(track_pos, TRACK_LEN);
            g->bananas[i].x        = x;
            g->bananas[i].active   = true;
            return;
        }
    }
    // No free slot: overwrite oldest (index 0)
    g->bananas[0].track_pos = fwrap(track_pos, TRACK_LEN);
    g->bananas[0].x         = x;
    g->bananas[0].active    = true;
}

// ─── Helper: fire a shell ─────────────────────────────────────────────────────
static void fire_shell(game_t *g, float track_pos, float x, bool from_player) {
    for (int i = 0; i < NUM_SHELLS; i++) {
        if (!g->shells[i].active) {
            g->shells[i].track_pos   = fwrap(track_pos, TRACK_LEN);
            g->shells[i].x           = x;
            g->shells[i].speed       = SHELL_SPEED;
            g->shells[i].active      = true;
            g->shells[i].from_player = from_player;
            return;
        }
    }
}

// ─── Update: AI opponents ─────────────────────────────────────────────────────
static void update_ai(game_t *g) {
    for (int i = 0; i < NUM_OPPS; i++) {
        opp_t *o = &g->opps[i];

        if (o->spin_timer > 0) {
            o->spin_timer--;
            o->speed *= 0.95f;
        } else {
            // Accelerate toward race speed
            o->speed += ACCEL * 0.7f;
            if (o->speed > OPP_SPEED) o->speed = OPP_SPEED;
        }

        // Gentle oscillation to look more natural + steer toward target_x
        o->target_x += 0.008f * sinf((float)(g->timer + i * 40) * 0.05f);
        o->target_x = fclampf(o->target_x, -0.7f, 0.7f);
        float steer_err = o->target_x - o->x;
        o->x += fclampf(steer_err, -OPP_STEER, OPP_STEER);
        o->x = fclampf(o->x, -1.3f, 1.3f);

        o->pos += o->speed * SEG_LEN;

        // Lap counting: same integer-division method as the player.
        // Guard o->pos > 0 prevents false count when crossing from
        // negative (behind start) to positive for the first time.
        if (o->pos > 0.0f) {
            int new_laps = (int)(o->pos / TRACK_LEN);
            if (new_laps > o->laps) o->laps = new_laps;
        }
    }
}

// ─── Update: shells ───────────────────────────────────────────────────────────
static void update_shells(game_t *g) {
    for (int i = 0; i < NUM_SHELLS; i++) {
        shell_t *sh = &g->shells[i];
        if (!sh->active) continue;

        float moved = sh->speed * SEG_LEN;
        sh->track_pos = fwrap(sh->track_pos + moved, TRACK_LEN);

        // Check if shell hits player (if fired by AI) or opponent (if fired by player)
        if (sh->from_player) {
            for (int j = 0; j < NUM_OPPS; j++) {
                opp_t *o = &g->opps[j];
                float tp = fwrap(o->pos, TRACK_LEN);
                float d = sh->track_pos - tp;
                if (d > TRACK_LEN / 2)  d -= TRACK_LEN;
                if (d < -TRACK_LEN / 2) d += TRACK_LEN;
                if (fabsf(d) < 60.0f && fabsf(sh->x - o->x) < 0.6f) {
                    o->spin_timer = 120;
                    sh->active = false;
                    break;
                }
            }
        } else {
            float ptp = fwrap(g->player.pos, TRACK_LEN);
            float d = sh->track_pos - ptp;
            if (d > TRACK_LEN / 2)  d -= TRACK_LEN;
            if (d < -TRACK_LEN / 2) d += TRACK_LEN;
            if (fabsf(d) < 60.0f && fabsf(sh->x - g->player.x) < 0.6f
                && g->player.invuln == 0) {
                g->player.spin_timer = 120;
                g->player.invuln     = 180;
                sh->active = false;
            }
        }

        // Shell lifetime (travels MAX_SHELL_DIST then vanishes)
        sh->speed -= 0.002f;
        if (sh->speed <= 0.0f) sh->active = false;
    }
}

// ─── Update: item boxes ───────────────────────────────────────────────────────
static void update_boxes(game_t *g) {
    float ptp = fwrap(g->player.pos, TRACK_LEN);
    for (int i = 0; i < NUM_BOXES; i++) {
        itembox_t *b = &g->boxes[i];
        if (!b->active) {
            if (b->respawn > 0) b->respawn--;
            else b->active = true;
            continue;
        }
        float d = b->track_pos - ptp;
        if (d > TRACK_LEN / 2)  d -= TRACK_LEN;
        if (d < -TRACK_LEN / 2) d += TRACK_LEN;
        if (fabsf(d) < ITEM_PICKUP_DIST && fabsf(g->player.x - b->x) < ITEM_PICKUP_X) {
            b->active   = false;
            b->respawn  = BOX_RESPAWN_TICKS;
            if (g->player.item == ITEM_NONE) {
                g->player.item = (rand() % 2 == 0) ? ITEM_BANANA : ITEM_SHELL;
            }
        }
    }
}

// ─── Update: bananas ─────────────────────────────────────────────────────────
static void update_bananas(game_t *g) {
    float ptp = fwrap(g->player.pos, TRACK_LEN);
    for (int i = 0; i < NUM_BANANAS; i++) {
        banana_t *bn = &g->bananas[i];
        if (!bn->active) continue;
        // Check player collision
        if (g->player.invuln == 0) {
            float d = bn->track_pos - ptp;
            if (d > TRACK_LEN / 2)  d -= TRACK_LEN;
            if (d < -TRACK_LEN / 2) d += TRACK_LEN;
            if (fabsf(d) < 20.0f && fabsf(g->player.x - bn->x) < 0.4f) {
                g->player.spin_timer = 120;
                g->player.invuln     = 180;
                bn->active = false;
            }
        }
        // AI opponent collision
        for (int j = 0; j < NUM_OPPS; j++) {
            opp_t *o = &g->opps[j];
            float otp = fwrap(o->pos, TRACK_LEN);
            float d = bn->track_pos - otp;
            if (d > TRACK_LEN / 2)  d -= TRACK_LEN;
            if (d < -TRACK_LEN / 2) d += TRACK_LEN;
            if (fabsf(d) < 20.0f && fabsf(o->x - bn->x) < 0.4f) {
                o->spin_timer = 90;
                bn->active = false;
                break;
            }
        }
    }
}

// ─── Main update ─────────────────────────────────────────────────────────────
void game_update(game_t *g, uint32_t buttons, uint32_t tick) {
    (void)tick;

    // ── Title screen ──
    if (g->state == GS_TITLE) {
        g->timer++;
        // Auto-start after 5 seconds if no button pressed (demo/screenshot mode)
        bool auto_start = (g->timer > AUTO_START_TICKS);
        if ((buttons & (BTN_FIRE | BTN_SEL)) || auto_start) {
            g->state     = GS_COUNTDOWN;
            g->countdown = COUNTDOWN_TICKS * 4;
            g->timer     = 0;
            g->lap_start = 0;
        }
        return;
    }

    // ── Countdown ──
    if (g->state == GS_COUNTDOWN) {
        g->countdown--;
        if (g->countdown <= 0) {
            g->state     = GS_RACE;
            g->timer     = 0;
            g->lap_start = 0;
        }
        return;
    }

    // ── Finish screen ──
    if (g->state == GS_FINISH) {
        g->finish_timer++;
        if (g->finish_timer > FINISH_WAIT_TICKS || (buttons & BTN_QUIT)) {
            g->prop_quit = true;
        }
        return;
    }

    // ── Race update ──
    g->timer++;

    player_t *p = &g->player;

    // Countdown timers
    if (p->invuln     > 0) p->invuln--;
    if (p->spin_timer > 0) p->spin_timer--;
    if (p->boost_timer> 0) p->boost_timer--;

    if (p->spin_timer == 0) {
        // Normal driving
        if (buttons & BTN_UP) {
            p->speed += ACCEL;
        } else if (buttons & BTN_DOWN) {
            p->speed -= BRAKE;
        }
        p->speed -= DRAG;

        // Steering
        if (buttons & BTN_LEFT)  p->x -= TURN_RATE;
        if (buttons & BTN_RIGHT) p->x += TURN_RATE;

        // On grass: slow down
        if (fabsf(p->x) > 1.05f) p->speed *= GRASS_MULT;

        float max_spd = (p->boost_timer > 0) ? MAX_SPEED * BOOST_MULT : MAX_SPEED;
        p->speed = fclampf(p->speed, 0.0f, max_spd);
    } else {
        // Spinning out
        p->speed *= 0.95f;
        p->x += 0.04f * sinf((float)p->spin_timer * 0.5f); // wobble
    }
    p->x = fclampf(p->x, -2.5f, 2.5f);

    // Use item (FIRE)
    static uint32_t prev_fire = 0;
    if ((buttons & BTN_FIRE) && !(prev_fire & BTN_FIRE)) {
        if (p->item == ITEM_BANANA) {
            drop_banana(g, fwrap(p->pos, TRACK_LEN) - SEG_LEN * 0.5f, p->x);
            p->item = ITEM_NONE;
        } else if (p->item == ITEM_SHELL) {
            fire_shell(g, fwrap(p->pos, TRACK_LEN), p->x, true);
            p->item = ITEM_NONE;
        }
    }
    prev_fire = buttons;

    // Advance player
    p->pos += p->speed * SEG_LEN;

    // Lap detection: when pos crosses a TRACK_LEN boundary
    int new_laps = (int)(p->pos / TRACK_LEN);
    if (new_laps > p->laps) {
        int lap_time = g->timer - g->lap_start;
        if (lap_time < g->best_lap) g->best_lap = lap_time;
        g->lap_start = g->timer;
        p->laps = new_laps;
        if (p->laps >= TRACK_LAPS) {
            g->state       = GS_FINISH;
            g->finish_timer= 0;
        }
    }

    update_ai(g);
    update_shells(g);
    update_boxes(g);
    update_bananas(g);

    // Race position: count how many opponents are ahead
    float my_prog = race_progress(p->pos, p->laps);
    p->place = 1;
    for (int i = 0; i < NUM_OPPS; i++) {
        if (race_progress(g->opps[i].pos, g->opps[i].laps) > my_prog) p->place++;
    }
}

// ─── Rendering helpers ───────────────────────────────────────────────────────

/// draw_mountain_silhouette: Simple distant mountain range above the horizon
static void draw_mountains(lfb_t *lfb) {
    for (int x = 0; x < LW; x++) {
        // Two overlapping sine waves create a mountain profile
        float h1 = 6.0f  * sinf((float)x * 0.035f);
        float h2 = 4.0f  * sinf((float)x * 0.068f + 1.2f);
        float h3 = 3.0f  * sinf((float)x * 0.021f + 0.7f);
        int peak = HORIZON_Y - 8 - (int)(h1 + h2 + h3);
        if (peak < 0) peak = 0;
        for (int y = peak; y < HORIZON_Y; y++) l_putpix(lfb, x, y, COL_MOUNT);
    }
}

/// draw_sky: Vertical gradient from dark blue (top) to lighter blue (horizon)
static void draw_sky(lfb_t *lfb) {
    for (int y = 0; y < HORIZON_Y; y++) {
        float t = (float)y / (HORIZON_Y - 1);
        uint32_t col = lerp_col(COL_SKY_TOP, COL_SKY_BOT, t);
        hline(lfb, y, 0, LW - 1, col);
    }
}

/// draw_road: Pseudo-3D scanline road renderer.
/// For each scanline below the horizon, we compute:
///   - perspective-correct depth (CAM_DEPTH / dy)
///   - road half-width (proportional to dy)
///   - road center (shifted by accumulated curve + player lateral offset)
///   - alternating stripe colour for depth cueing
/// Results are also stored in g->road_cx[] and g->road_hw[] for sprite placement.
static void draw_road(lfb_t *lfb, game_t *g) {
    float ptp  = fwrap(g->player.pos, TRACK_LEN);  // player track position
    float x_off= 0.0f;  // accumulated curve offset in pixels (grows toward horizon)

    for (int y = LH - 1; y > HORIZON_Y; y--) {
        int dy = y - HORIZON_Y;  // rows below horizon (1..ROAD_ROWS)

        // Perspective-correct depth for this scanline
        float depth = CAM_DEPTH / (float)dy;

        // Which track segment are we looking at?
        float wz = fwrap(ptp + depth, TRACK_LEN);
        int   si = (int)(wz / SEG_LEN) % TRACK_SEGS;
        if (si < 0) si += TRACK_SEGS;
        bool alt = (si % 2 == 0);

        // Road geometry
        float t  = (float)dy / (float)ROAD_ROWS;         // 0 at horizon, 1 at bottom
        float hw = ROAD_HW * t;                           // road half-width (px)
        float cx = (float)(LW / 2) - g->player.x * hw + x_off;  // road center (px)

        g->road_cx[y] = cx;
        g->road_hw[y] = hw;

        // Colour sets (two alternating schemes for depth banding)
        uint32_t gc = alt ? COL_GRASS1 : COL_GRASS2;
        uint32_t rc = alt ? COL_ROAD1  : COL_ROAD2;
        uint32_t rb = alt ? COL_RUMBLE1: COL_RUMBLE2;

        int ihw = (int)hw;
        int icx = (int)cx;
        int rw  = (ihw / 6) + 1;   // rumble strip width

        // Fill the whole scanline with grass first
        hline(lfb, y, 0, LW - 1, gc);

        // Road surface
        hline(lfb, y, icx - ihw, icx + ihw, rc);

        // Rumble strips at road edges
        hline(lfb, y, icx - ihw - rw, icx - ihw - 1, rb);
        hline(lfb, y, icx + ihw + 1,  icx + ihw + rw, rb);

        // Centre dashed white line (alternates per segment, 3 px wide)
        if (alt && ihw > 8) {
            hline(lfb, y, icx - 1, icx + 1, COL_LINE);
        }

        // Accumulate curve offset for next (higher) scanline.
        // Using 1/dy weights the curve heavier near the horizon — correct perspective.
        x_off += g->track[si].curve * CURVE_K / (float)(dy + 1);
    }

    // Fill the horizon line itself
    g->road_cx[HORIZON_Y] = (float)(LW / 2);
    g->road_hw[HORIZON_Y] = 0.0f;
}

/// project_track_object: Convert a world track position/x to screen coordinates.
/// Returns false if the object is not visible (behind player or beyond draw dist).
///   depth     - out: world depth of this object from player
///   screen_y  - out: y coordinate on screen (HORIZON_Y to LH-1)
///   screen_cx - out: centre x of object (using road_cx + lateral offset)
///   scale     - out: integer scale for sprite (1..5, proportional to 1/depth)
static bool project_obj(const game_t *g,
                        float obj_track_pos, float obj_x,
                        float *depth_out, int *sy, float *sxc, int *scale) {
    float ptp   = fwrap(g->player.pos, TRACK_LEN);
    float d = obj_track_pos - ptp;

    // Wrap to shortest signed distance
    if (d > TRACK_LEN / 2)  d -= TRACK_LEN;
    if (d < -TRACK_LEN / 2) d += TRACK_LEN;

    if (d < 5.0f || d > CAM_DEPTH) return false;  // behind or too far

    *depth_out = d;
    int dy = (int)(CAM_DEPTH / d);
    if (dy < 1) dy = 1;
    int screen_y_row = HORIZON_Y + dy;
    if (screen_y_row >= LH) return false;

    *sy    = screen_y_row;
    float hw  = g->road_hw[screen_y_row];
    float cx  = g->road_cx[screen_y_row];
    *sxc   = cx + (obj_x - g->player.x) * hw;
    // Scale: proportional to inverse depth, clamped 1-5
    int sc = (int)(CAM_DEPTH / d / 20.0f);
    if (sc < 1) sc = 1;
    if (sc > 5) sc = 5;
    *scale = sc;
    return true;
}

/// draw_track_sprite: Draw a sprite at a world-track position with depth scaling
static void draw_track_sprite(lfb_t *lfb, game_t *g,
                               const sprite1r_t *spr, uint32_t col,
                               float track_pos, float obj_x) {
    float depth; int sy; float sxc; int scale;
    if (!project_obj(g, track_pos, obj_x, &depth, &sy, &sxc, &scale)) return;
    int x0 = (int)sxc - spr->w * scale / 2;
    int y0 = sy - spr->h * scale;
    draw_sprite1r_shadow(lfb, spr, x0, y0, col, scale);
}

/// draw_kart: Draw an opponent kart at depth
static void draw_kart(lfb_t *lfb, game_t *g, float kart_track_pos, float kart_x,
                      uint32_t col, int spin) {
    float depth; int sy; float sxc; int scale;
    if (!project_obj(g, kart_track_pos, kart_x, &depth, &sy, &sxc, &scale)) return;

    // Apply a flash when spinning out
    if (spin > 0 && (spin % 8) < 4) col = 0xFFFFFFFF;

    int x0 = (int)sxc - g->spr_kart.w * scale / 2;
    int y0 = sy - g->spr_kart.h * scale;
    draw_sprite1r_shadow(lfb, &g->spr_kart, x0, y0, col, scale);
}

// ─── HUD ─────────────────────────────────────────────────────────────────────

/// draw_hud: Render all heads-up-display elements
static void draw_hud(lfb_t *lfb, game_t *g) {
    // Speed bar (bottom left)
    int bar_x = 4, bar_y = LH - 10, bar_w = 50, bar_h = 5;
    fillrect(lfb, bar_x, bar_y, bar_w, bar_h, COL_DARKGRAY);
    float spd_frac = g->player.speed / MAX_SPEED;
    if (spd_frac > 1.0f) spd_frac = 1.0f;
    int fill = (int)(bar_w * spd_frac);
    uint32_t spd_col = (spd_frac > 0.8f) ? COL_RED : (spd_frac > 0.5f) ? COL_YELLOW : COL_GREEN;
    fillrect(lfb, bar_x, bar_y, fill, bar_h, spd_col);
    l_draw_text(lfb, bar_x, bar_y - 8, "SPD", 1, COL_WHITE);

    // Lap counter (top left)
    {
        char buf[24];
        snprintf(buf, sizeof(buf), "LAP %d/%d", g->player.laps < TRACK_LAPS ? g->player.laps + 1 : TRACK_LAPS, TRACK_LAPS);
        l_draw_text(lfb, 4, 4, buf, 1, COL_WHITE);
    }

    // Race position (top left, below lap)
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "P%d/4", g->player.place);
        l_draw_text(lfb, 4, 14, buf, 1, COL_YELLOW);
    }

    // Race timer (top centre)
    {
        int secs = g->timer / 60;
        int frac = (g->timer % 60) * 10 / 60;
        char buf[24];
        snprintf(buf, sizeof(buf), "%d:%02d.%d", secs / 60, secs % 60, frac);
        l_draw_text(lfb, LW / 2 - 20, 4, buf, 1, COL_WHITE);
    }

    // Best lap (top right)
    if (g->best_lap < NO_LAP_TIME) {
        int secs = g->best_lap / 60;
        int frac = (g->best_lap % 60) * 10 / 60;
        char buf[24];
        snprintf(buf, sizeof(buf), "BEST %d:%02d.%d", secs / 60, secs % 60, frac);
        l_draw_text(lfb, LW - 68, 4, buf, 1, COL_YELLOW);
    }

    // Held item indicator (bottom right)
    if (g->player.item != ITEM_NONE) {
        const char *iname = (g->player.item == ITEM_BANANA) ? "BANANA" : "SHELL";
        l_draw_text(lfb, LW - 52, LH - 10, iname, 1, COL_ORANGE);
        // Draw item sprite
        if (g->player.item == ITEM_BANANA) {
            draw_sprite1r(lfb, &g->spr_banana, LW - 8 - g->spr_banana.w, LH - 16, COL_BANANA);
        } else {
            draw_sprite1r(lfb, &g->spr_shell, LW - 8 - g->spr_shell.w, LH - 14, COL_SHELL);
        }
    }

    // Spin-out warning
    if (g->player.spin_timer > 0) {
        l_draw_text(lfb, LW / 2 - 15, LH / 2 - 4, "SPIN!", 1, COL_RED);
    }

    // Boost indicator
    if (g->player.boost_timer > 0) {
        l_draw_text(lfb, LW / 2 - 15, LH / 2 - 4, "BOOST!", 1, COL_YELLOW);
    }
}

/// draw_player_kart: Draw the player's kart at the bottom-centre of screen
static void draw_player_kart(lfb_t *lfb, game_t *g) {
    const sprite1r_t *s = &g->spr_kart;
    int scale = 2;
    // Determine colour: flash white during invulnerability
    uint32_t col = KART_COLOR[0];
    if (g->player.invuln > 0 && (g->player.invuln % 8) < 4) col = COL_WHITE;
    // Wobble when spinning
    int wobble_x = 0;
    if (g->player.spin_timer > 0)
        wobble_x = (int)(5.0f * sinf((float)g->player.spin_timer * 0.6f));

    int x0 = LW / 2 - s->w * scale / 2 + wobble_x;
    int y0 = LH - s->h * scale - 4;
    draw_sprite1r_shadow(lfb, s, x0, y0, col, scale);
}

// ─── Main render ─────────────────────────────────────────────────────────────
void game_render(game_t *g, lfb_t *lfb) {

    // ── Title screen ──
    if (g->state == GS_TITLE) {
        l_clear(lfb, 0xFF0A1020u);
        l_draw_text(lfb, LW/2 - 42, LH/2 - 20, "MARIO KART",  2, COL_YELLOW);
        l_draw_text(lfb, LW/2 - 51, LH/2 +  4, "PRESS FIRE TO RACE", 1, COL_WHITE);
        l_draw_text(lfb, LW/2 - 48, LH/2 + 16, "ARROWS=STEER/ACCEL", 1, COL_DARKGRAY);
        l_draw_text(lfb, LW/2 - 42, LH/2 + 26, "SPACE=USE ITEM",     1, COL_DARKGRAY);
        // Draw a static kart for decoration
        int kx = LW/2 - g->spr_kart.w;
        int ky = LH/2 - g->spr_kart.h * 3 - 10;
        draw_sprite1r_scaled(lfb, &g->spr_kart, kx,      ky, KART_COLOR[0], 3);
        draw_sprite1r_scaled(lfb, &g->spr_kart, kx + 38, ky, KART_COLOR[1], 3);
        return;
    }

    // ── Countdown screen ──
    if (g->state == GS_COUNTDOWN) {
        draw_sky(lfb);
        draw_mountains(lfb);
        draw_road(lfb, g);
        draw_player_kart(lfb, g);
        int phase = g->countdown / COUNTDOWN_TICKS;  // 3,2,1,0
        if (phase >= 1) {
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", phase % 10);
            l_draw_text(lfb, LW/2 - 7, LH/2 - 12, buf, 4, COL_YELLOW);
        } else {
            l_draw_text(lfb, LW/2 - 9, LH/2 - 6, "GO!", 3, COL_GREEN);
        }
        return;
    }

    // ── Finish screen ──
    if (g->state == GS_FINISH) {
        l_clear(lfb, 0xFF0A1020u);
        l_draw_text(lfb, LW/2 - 28, LH/2 - 16, "FINISH!", 2, COL_YELLOW);
        int secs = g->timer / 60;
        int frac = (g->timer % 60) * 10 / 60;
        char buf[24];
        snprintf(buf, sizeof(buf), "TIME %d:%02d.%d", secs/60, secs%60, frac);
        l_draw_text(lfb, LW/2 - 36, LH/2 + 2,  buf, 1, COL_WHITE);
        if (g->best_lap < NO_LAP_TIME) {
            int bs = g->best_lap / 60;
            int bf = (g->best_lap % 60) * 10 / 60;
            snprintf(buf, sizeof(buf), "BEST LAP %d:%02d.%d", bs/60, bs%60, bf);
            l_draw_text(lfb, LW/2 - 42, LH/2 + 12, buf, 1, COL_GREEN);
        }
        snprintf(buf, sizeof(buf), "PLACE %d/4", g->player.place);
        l_draw_text(lfb, LW/2 - 24, LH/2 + 24, buf, 1, COL_YELLOW);
        l_draw_text(lfb, LW/2 - 42, LH/2 + 36, "PRESS ESC TO QUIT", 1, COL_DARKGRAY);
        return;
    }

    // ── Normal race rendering ──

    // 1. Sky
    draw_sky(lfb);

    // 2. Mountains at horizon
    draw_mountains(lfb);

    // 3. Road (builds road_cx[] and road_hw[] tables used by sprite placement)
    draw_road(lfb, g);

    // 4. Track objects (sorted back-to-front by depth for painter's algorithm)
    // Item boxes
    for (int i = 0; i < NUM_BOXES; i++) {
        if (g->boxes[i].active) {
            draw_track_sprite(lfb, g, &g->spr_itembox, COL_BOX,
                              g->boxes[i].track_pos, g->boxes[i].x);
        }
    }

    // Bananas
    for (int i = 0; i < NUM_BANANAS; i++) {
        if (g->bananas[i].active) {
            draw_track_sprite(lfb, g, &g->spr_banana, COL_BANANA,
                              g->bananas[i].track_pos, g->bananas[i].x);
        }
    }

    // Shells
    for (int i = 0; i < NUM_SHELLS; i++) {
        if (g->shells[i].active) {
            draw_track_sprite(lfb, g, &g->spr_shell, COL_SHELL,
                              g->shells[i].track_pos, g->shells[i].x);
        }
    }

    // 5. Opponent karts (farthest first — painter's algorithm approximation)
    // Sort opponents by distance (simple insertion sort on 3 items)
    int order[NUM_OPPS] = {0, 1, 2};
    float ptp = fwrap(g->player.pos, TRACK_LEN);
    for (int i = 0; i < NUM_OPPS - 1; i++) {
        for (int j = i + 1; j < NUM_OPPS; j++) {
            float da = fwrap(g->opps[order[i]].pos, TRACK_LEN) - ptp;
            float db = fwrap(g->opps[order[j]].pos, TRACK_LEN) - ptp;
            if (da > TRACK_LEN/2) da -= TRACK_LEN;
            if (db > TRACK_LEN/2) db -= TRACK_LEN;
            // Draw farthest first (largest distance first)
            if (da < db) { int tmp = order[i]; order[i] = order[j]; order[j] = tmp; }
        }
    }
    for (int k = 0; k < NUM_OPPS; k++) {
        int i = order[k];
        draw_kart(lfb, g,
                  fwrap(g->opps[i].pos, TRACK_LEN),
                  g->opps[i].x,
                  KART_COLOR[g->opps[i].color_idx],
                  g->opps[i].spin_timer);
    }

    // 6. Player kart (always drawn at bottom-centre, largest scale)
    draw_player_kart(lfb, g);

    // 7. HUD overlay
    draw_hud(lfb, g);
}
