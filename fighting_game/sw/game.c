/*
 * game.c – Street-Fighter-style 2D fighting game
 *
 * Screen: 320 × 180 logical pixels (×4 upscale → 1280×720 physical)
 *
 * Layout
 * ──────
 *   y 0-12   : HUD  (health bars, timer)
 *   y 12-145 : arena / sky
 *   y 145-180: ground
 *   GROUND_Y = 145  (character feet touch this row)
 *
 * Characters are 16 px wide, 28 px tall (logical).  Drawn with coloured
 * rectangles that give a multi-tone martial-artist silhouette.
 *
 * Controls (player 1, left side)
 * ───────────────────────────────
 *   LEFT / RIGHT : walk
 *   FIRE         : punch
 *   DOWN / S      : block (negates all punch damage while held)
 */

#include "game.h"
#include "font.h"
#include "gfx.h"
#include "../hw_contract.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Resolution scaling helpers ──────────────────────────────────────────── */
#define BASE_W 320
#define BASE_H 180

#define SX(v)  (((v) * LW + (BASE_W / 2)) / BASE_W)
#define SY(v)  (((v) * LH + (BASE_H / 2)) / BASE_H)
#define SX1(v) ((SX(v) > 0) ? SX(v) : 1)
#define SY1(v) ((SY(v) > 0) ? SY(v) : 1)

/* ── Tuning constants ────────────────────────────────────────────────────── */
#define GROUND_Y      SY(145)
#define P1_START_X     SX(75)
#define P2_START_X    SX(245)
#define MAX_HP          100
#define PUNCH_DAMAGE      5
#define PUNCH_RANGE    SX(28)  /* centre-to-centre px for a punch to land */
#define PUNCH_DURATION   18   /* total frames of punch animation */
#define PUNCH_HIT_FRAME   9   /* frame at which the fist connects */
#define PUNCH_COOLDOWN   22   /* minimum frames between punches (unused: timer handles it) */
#define HIT_Y_RANGE    SY(12) /* vertical overlap needed for punch to connect */
#define HIT_STUN         10   /* frames opponent is staggered */
#define JUMP_V0        SY1(5)
#define JUMP_GRAVITY   SY1(1)
#define JUMP_GRAVITY_FRAMES 3
#define TIMER_SECS      120
#define TIMER_FRAMES    (TIMER_SECS * 30)
#define RESTART_DELAY    20   /* frames before restart input is accepted */

/* Animation frame rates */
#define IDLE_RATE   24   /* frames per idle step */
#define WALK_RATE    7
#define PUNCH_RATE  (PUNCH_DURATION / 2)

/* CPU AI */
#define CPU_THINK_RATE   8   /* frames between AI updates */
#define CPU_SPEED      SX1(2)
#define IDEAL_DIST     SX(22)
#define RETREAT_DIST   SX(16)
#define CPU_BLOCK_RANGE SX(36)
#define CPU_BLOCK_CHANCE 35   /* % chance to block when player attacks nearby */
#define CPU_JUMP_CHANCE 22

/* Arena */
#define ARENA_L     SX(15)      /* left  wall (player can't go further left)  */
#define ARENA_R    (LW - SX(15))/* right wall */

/* HUD bar layout */
#define HUD_H      SY(13)
#define BAR_Y      SY(3)
#define BAR_H      SY1(7)
#define P1_BAR_X   SX(5)
#define BAR_W      SX(100)
#define P2_BAR_X  (LW - SX(5) - BAR_W - SX1(2))

/* ── Colour palettes ─────────────────────────────────────────────────────── */
typedef struct { uint32_t skin, hair, gi, belt, shoe; } pal_t;

static const pal_t PAL_P1 = {
    .skin = 0xFFFFCC99u,
    .hair = 0xFF222222u,
    .gi   = 0xFFEEEEEEu,   /* white gi */
    .belt = 0xFFCC0000u,   /* red belt */
    .shoe = 0xFF111111u,
};
static const pal_t PAL_P2 = {
    .skin = 0xFFFFCC99u,
    .hair = 0xFF111111u,
    .gi   = 0xFF1133AAu,   /* blue gi */
    .belt = 0xFF000000u,   /* black belt */
    .shoe = 0xFF222233u,
};

/* ── Draw helpers ────────────────────────────────────────────────────────── */
static void l_rect(lfb_t *lfb, int x, int y, int w, int h, uint32_t c) {
    for (int dy = 0; dy < h; dy++)
        for (int dx = 0; dx < w; dx++)
            l_putpix(lfb, x + dx, y + dy, c);
}

static int fighter_airborne(const fighter_t *f) {
    return (f->jump_y > 0) || (f->jump_vy != 0);
}

static void start_jump(fighter_t *f) {
    if (!fighter_airborne(f)) {
        f->jump_vy = JUMP_V0;
        f->jump_grav_tick = 0;
    }
}

static void update_jump_physics(fighter_t *f) {
    if (!fighter_airborne(f)) return;

    f->jump_y += f->jump_vy;
    f->jump_grav_tick++;
    if (f->jump_grav_tick >= JUMP_GRAVITY_FRAMES) {
        f->jump_grav_tick = 0;
        f->jump_vy -= JUMP_GRAVITY;
    }

    if (f->jump_y <= 0) {
        f->jump_y = 0;
        f->jump_vy = 0;
    }
}

/* ── Fighter drawing ─────────────────────────────────────────────────────── */
/*
 * Character anatomy (all y-offsets from character top, = GROUND_Y - 28):
 *
 *   Hair      0-1   (2 px)   7 wide
 *   Head      2-7   (6 px)   7 wide
 *   Neck      8     (1 px)   3 wide
 *   Body/Gi   9-17  (9 px)   9 wide   ← shoulders  at 9, waist at 17
 *   Belt      18    (1 px)   9 wide
 *   Hips      19-20 (2 px)   9 wide
 *   Legs      21-26 (6 px)   4 wide × 2 (gap in centre)
 *   Feet      27    (1 px)   5 wide × 2
 *   Total     28 px
 *
 * Arms hang alongside body (y 9-20). During punch, the opposite arm
 * throws across the body from shoulder height.
 * cx = center x at ground level, so top = GROUND_Y - 28.
 */
static void draw_fighter(lfb_t *lfb, const fighter_t *f, const pal_t *pal) {
    int cx     = f->x;
    int facing = f->facing;   /* +1 right, -1 left */
    anim_t anim = f->anim;
    int frame  = f->anim_frame;
    int foot_y = GROUND_Y - f->jump_y;

    /* ── Defeated: lying flat ── */
    if (anim == ANIM_DEFEATED) {
        /* body horizontal, head at facing end */
        int bx = cx - SX(9);
        l_rect(lfb, bx,      GROUND_Y - SY(6),  SX1(18), SY1(4), pal->gi);
        l_rect(lfb, bx,      GROUND_Y - SY(6),  SX1(18), SY1(1), pal->belt); /* belt line */
        int hx = (facing == 1) ? bx + SX(14) : bx - SX(5);
        int hy = GROUND_Y - SY(9);
        l_rect(lfb, hx,      GROUND_Y - SY(9),  SX1(6),  SY1(6), pal->skin);
        l_rect(lfb, hx,      GROUND_Y - SY(11), SX1(6),  SY1(2), pal->hair);
        /* defeated face */
        l_rect(lfb, hx + SX(1), hy + SY(2), SX1(1), SY1(1), pal->hair);
        l_rect(lfb, hx + SX(4), hy + SY(2), SX1(1), SY1(1), pal->hair);
        l_rect(lfb, hx + SX(2), hy + SY(4), SX1(2), SY1(1), 0xFF663322u);
        l_rect(lfb, bx,      GROUND_Y - SY(2),  SX1(10), SY1(2), pal->gi);   /* legs stretched */
        return;
    }

    /* ── Victory: arms raised ── */
    if (anim == ANIM_VICTORY) {
        int ty = GROUND_Y - SY(28);
        l_rect(lfb, cx - SX(3), ty,           SX1(7), SY1(2),  pal->hair);
        l_rect(lfb, cx - SX(3), ty + SY(2),   SX1(7), SY1(6),  pal->skin);
        /* victory face */
        l_rect(lfb, cx - SX(2), ty + SY(4), SX1(1), SY1(1), pal->hair);
        l_rect(lfb, cx + SX(1), ty + SY(4), SX1(1), SY1(1), pal->hair);
        l_rect(lfb, cx - SX(1), ty + SY(6), SX1(3), SY1(1), 0xFFAA6644u);
        l_rect(lfb, cx - SX(1), ty + SY(8),   SX1(3), SY1(1),  pal->skin);
        l_rect(lfb, cx - SX(4), ty + SY(9),   SX1(9), SY1(9),  pal->gi);
        l_rect(lfb, cx - SX(4), ty + SY(18),  SX1(9), SY1(1),  pal->belt);
        l_rect(lfb, cx - SX(4), ty + SY(19),  SX1(9), SY1(2),  pal->gi);
        l_rect(lfb, cx - SX(4), ty + SY(21),  SX1(4), SY1(6),  pal->gi);
        l_rect(lfb, cx + SX(1), ty + SY(21),  SX1(4), SY1(6),  pal->gi);
        l_rect(lfb, cx - SX(5), ty + SY(27),  SX1(5), SY1(1),  pal->shoe);
        l_rect(lfb, cx + SX(1), ty + SY(27),  SX1(5), SY1(1),  pal->shoe);
        /* arms raised above head */
        l_rect(lfb, cx - SX(8), ty - SY(6),  SX1(3), SY1(16), pal->skin);   /* left arm */
        l_rect(lfb, cx + SX(6), ty - SY(6),  SX1(3), SY1(16), pal->skin);   /* right arm */
        return;
    }

    /* ── Normal poses: IDLE / WALK / PUNCH / HIT ── */

    /* body bob on idle frame 1 */
    int bob = (anim == ANIM_IDLE && frame == 1) ? SY1(1) : 0;

    /* walk: legs alternate */
    int ll_dy = 0, rl_dy = 0;
    if (anim == ANIM_WALK) {
        ll_dy = (frame == 0) ? -SY(2) : SY(2);
        rl_dy = -ll_dy;
    } else if (anim == ANIM_JUMP) {
        ll_dy = -SY(3);
        rl_dy = -SY(3);
    }

    /* hit stagger: body shifts back */
    int hit_dx = (anim == ANIM_HIT) ? (-facing * SX(3)) : 0;

    /* punch frame flag */
    int punch_frame = (anim == ANIM_PUNCH && frame == 1);

    int bx = cx + hit_dx;
    int ty = foot_y - SY(28) + bob;

    /* hair */
    l_rect(lfb, bx - SX(3), ty,          SX1(7), SY1(2), pal->hair);
    /* head */
    l_rect(lfb, bx - SX(3), ty + SY(2),  SX1(7), SY1(6), pal->skin);
    /* facial details */
    l_rect(lfb, bx - SX(2), ty + SY(4), SX1(1), SY1(1), pal->hair);      /* left eye */
    l_rect(lfb, bx + SX(1), ty + SY(4), SX1(1), SY1(1), pal->hair);      /* right eye */
    l_rect(lfb, bx - SX(1), ty + SY(6), SX1(3), SY1(1), 0xFFAA6644u);    /* mouth */
    /* neck */
    l_rect(lfb, bx - SX(1), ty + SY(8),  SX1(3), SY1(1), pal->skin);
    /* gi body */
    l_rect(lfb, bx - SX(4), ty + SY(9),  SX1(9), SY1(9), pal->gi);
    /* gi lapel */
    l_rect(lfb, bx - SX(1), ty + SY(10), SX1(1), SY1(8), pal->belt);
    l_rect(lfb, bx + SX(1), ty + SY(10), SX1(1), SY1(8), pal->belt);
    /* belt */
    l_rect(lfb, bx - SX(4), ty + SY(18), SX1(9), SY1(1), pal->belt);
    /* hips */
    l_rect(lfb, bx - SX(4), ty + SY(19), SX1(9), SY1(2), pal->gi);

    /* back arm (opposite to facing = used for cross-body punch) */
    int back_x = (facing == 1) ? bx - SX(6) : bx + SX(4);
    /* front arm */
    int front_x = (facing == 1) ? bx + SX(4) : bx - SX(6);

    if (anim == ANIM_BLOCK) {
        /* Both forearms are raised in front of face for a guard pose. */
        int guard_y = ty + SY(4);
        l_rect(lfb, bx - SX(4), guard_y + SY(2), SX1(3), SY1(8), pal->gi);
        l_rect(lfb, bx + SX(2), guard_y + SY(2), SX1(3), SY1(8), pal->gi);
        l_rect(lfb, bx - SX(4), guard_y, SX1(3), SY1(2), pal->skin);
        l_rect(lfb, bx + SX(2), guard_y, SX1(3), SY1(2), pal->skin);
    } else if (punch_frame) {
        /* Punching opposite arm: thrown arm leaves its resting side silhouette. */
        int forearm_w = SX1(10);
        int forearm_h = SY1(3);
        int forearm_y = ty + SY(11);
        int forearm_x = (facing == 1) ? (back_x + SX1(3)) : (back_x - forearm_w);
        int fist_x    = (facing == 1) ? (forearm_x + forearm_w) : (forearm_x - SX1(3));

        l_rect(lfb, forearm_x, forearm_y, forearm_w, forearm_h, pal->gi);
        l_rect(lfb, fist_x, forearm_y, SX1(3), SY1(3), pal->skin);

        /* Guarding arm tucks in so we don't visually create an extra third arm. */
        l_rect(lfb, front_x, ty + SY(10), SX1(3), SY1(6), pal->gi);
        l_rect(lfb, front_x, ty + SY(16), SX1(3), SY1(2), pal->skin);
    } else {
        l_rect(lfb, back_x,  ty + SY(9),  SX1(3), SY1(9), pal->gi);
        l_rect(lfb, back_x,  ty + SY(18), SX1(3), SY1(3), pal->skin);
        l_rect(lfb, front_x, ty + SY(9),  SX1(3), SY1(9), pal->gi);
        l_rect(lfb, front_x, ty + SY(18), SX1(3), SY1(3), pal->skin);
    }

    /* legs */
    if (anim == ANIM_JUMP) {
        l_rect(lfb, bx - SX(4), ty + SY(20), SX1(4), SY1(4), pal->gi);
        l_rect(lfb, bx + SX(1), ty + SY(20), SX1(4), SY1(4), pal->gi);
        l_rect(lfb, bx - SX(5), ty + SY(24), SX1(5), SY1(1), pal->shoe);
        l_rect(lfb, bx + SX(1), ty + SY(24), SX1(5), SY1(1), pal->shoe);
    } else {
        l_rect(lfb, bx - SX(4), ty + SY(21) + ll_dy, SX1(4), SY1(6), pal->gi);
        l_rect(lfb, bx + SX(1), ty + SY(21) + rl_dy, SX1(4), SY1(6), pal->gi);
        /* feet */
        l_rect(lfb, bx - SX(5), ty + SY(27) + ll_dy, SX1(5), SY1(1), pal->shoe);
        l_rect(lfb, bx + SX(1), ty + SY(27) + rl_dy, SX1(5), SY1(1), pal->shoe);
    }
}

/* ── Background ──────────────────────────────────────────────────────────── */
static void draw_background(lfb_t *lfb) {
    /* sky gradient (three bands) */
    l_rect(lfb, 0, SY(12),  LW, SY1(44), 0xFF7788BBu);
    l_rect(lfb, 0, SY(56),  LW, SY1(44), 0xFF8899CCu);
    l_rect(lfb, 0, SY(100), LW, SY1(45), 0xFF99AADDu);

    /* ground */
    l_rect(lfb, 0, SY(145), LW, SY1(35), 0xFF886644u);
    /* ground edge highlight */
    l_rect(lfb, 0, SY(145), LW, SY1(2), 0xFFAA9966u);

    /* simple dojo floor marks */
    for (int x = SX(30); x < LW; x += SX1(60)) {
        l_rect(lfb, x, SY(147), SX1(2), SY1(20), 0xFF997755u);
    }
}

/* ── HUD ─────────────────────────────────────────────────────────────────── */
static void draw_hud(lfb_t *lfb, const game_t *g) {
    /* clear HUD area */
    l_rect(lfb, 0, 0, LW, HUD_H, 0xFF111111u);

    /* --- P1 health bar (left side, fills left → right) --- */
    int p1_hp = g->fighters[0].hp;
    int p1_fill = (p1_hp * BAR_W + (MAX_HP / 2)) / MAX_HP;
    l_rect(lfb, P1_BAR_X,     BAR_Y, BAR_W + 2, BAR_H, 0xFF333333u);  /* bg */
    if (p1_fill > 0)
        l_rect(lfb, P1_BAR_X + 1, BAR_Y + 1, p1_fill, BAR_H - 2, 0xFF22DD22u);
    l_rect(lfb, P1_BAR_X,     BAR_Y, BAR_W + 2, 1, 0xFF888888u);  /* border */
    l_rect(lfb, P1_BAR_X,     BAR_Y + BAR_H - 1, BAR_W + 2, 1, 0xFF888888u);
    l_draw_text(lfb, P1_BAR_X, BAR_Y - 1, "P1", 1, 0xFFFFFFFFu);

    /* --- P2 health bar (right side, fills right → left) --- */
    int p2_hp = g->fighters[1].hp;
    int p2_fill = (p2_hp * BAR_W + (MAX_HP / 2)) / MAX_HP;
    int p2_x  = P2_BAR_X;
    l_rect(lfb, p2_x,     BAR_Y, BAR_W + 2, BAR_H, 0xFF333333u);
    if (p2_fill > 0)
        l_rect(lfb, p2_x + 1 + (BAR_W - p2_fill), BAR_Y + 1, p2_fill, BAR_H - 2, 0xFF2222DDu);
    l_rect(lfb, p2_x, BAR_Y, BAR_W + 2, 1, 0xFF888888u);
    l_rect(lfb, p2_x, BAR_Y + BAR_H - 1, BAR_W + 2, 1, 0xFF888888u);
    l_draw_text(lfb, p2_x + BAR_W - SX(6), BAR_Y - 1, "P2", 1, 0xFFFFFFFFu);

    /* --- Timer --- */
    int secs = g->timer_frames / 30;
    char tbuf[16];
    snprintf(tbuf, sizeof(tbuf), "%d", secs);
    int tw = (int)strlen(tbuf) * 4;  /* 3px char + 1 spacing */
    l_draw_score(lfb, LW / 2 - tw / 2, BAR_Y, secs, 0xFFFFFF44u);
}

/* ── Animation update ────────────────────────────────────────────────────── */
static void update_fighter_anim(fighter_t *f) {
    if (f->anim == ANIM_BLOCK) {
        f->anim_frame = 0;
        f->anim_timer = 0;
        return;
    }

    /* Punch is timer-driven, not rate-driven */
    if (f->punch_timer >= 0) {
        f->punch_timer++;
        f->anim_frame = (f->punch_timer >= PUNCH_HIT_FRAME) ? 1 : 0;
        if (f->punch_timer >= PUNCH_DURATION) {
            f->punch_timer = -1;
            f->anim        = ANIM_IDLE;
            f->anim_frame  = 0;
            f->anim_timer  = 0;
        }
        return;
    }

    /* Hit stun */
    if (f->hit_stun > 0) {
        f->hit_stun--;
        f->anim = ANIM_HIT;
        f->anim_frame = (f->hit_stun / 2) & 1;
        if (f->hit_stun == 0) { f->anim = ANIM_IDLE; f->anim_frame = 0; }
        return;
    }

    /* Normal idle / walk */
    int rate = (f->anim == ANIM_WALK) ? WALK_RATE : IDLE_RATE;
    if (f->anim == ANIM_JUMP) rate = WALK_RATE;
    f->anim_timer++;
    if (f->anim_timer >= rate) {
        f->anim_timer = 0;
        f->anim_frame ^= 1;
    }
}

/* ── Punch hit detection ─────────────────────────────────────────────────── */
static void check_hit(game_t *g, int attacker) {
    fighter_t *a = &g->fighters[attacker];
    fighter_t *t = &g->fighters[1 - attacker];

    if (a->punch_hit_done) return;

    int dist     = t->x - a->x;
    int abs_dist = (dist < 0) ? -dist : dist;
    int y_delta  = t->jump_y - a->jump_y;
    int abs_y    = (y_delta < 0) ? -y_delta : y_delta;
    int toward   = (a->facing == 1 && dist > 0) || (a->facing == -1 && dist < 0);

    if (toward && abs_dist <= PUNCH_RANGE && abs_y <= HIT_Y_RANGE) {
        if (t->anim == ANIM_BLOCK) {
            a->punch_hit_done = 1;
            return;
        }

        t->hp -= PUNCH_DAMAGE;
        if (t->hp < 0) t->hp = 0;
        t->hit_stun      = HIT_STUN;
        a->punch_hit_done = 1;
    }
}

/* ── Player update ───────────────────────────────────────────────────────── */
static void update_player(game_t *g, uint32_t buttons) {
    fighter_t *p = &g->fighters[0];
    int block_held = ((buttons & BTN_DOWN) != 0) && !fighter_airborne(p);
    int jump_pressed = (buttons & BTN_UP) != 0;

    if (jump_pressed && p->hit_stun == 0 && p->punch_timer < 0 && !fighter_airborne(p)) {
        start_jump(p);
    }

    /* Hold-to-block: guard pose and negate incoming punch damage. */
    if (block_held && p->punch_timer < 0 && p->hit_stun == 0) {
        p->anim = ANIM_BLOCK;
        p->anim_frame = 0;
    }

    /* Only accept input when not punching or stunned */
    if (p->punch_timer < 0 && p->hit_stun == 0 && !block_held) {
        if ((buttons & BTN_FIRE) && p->punch_timer < 0) {
            p->anim           = ANIM_PUNCH;
            p->punch_timer    = 0;
            p->anim_frame     = 0;
            p->punch_hit_done = 0;
        } else if (buttons & BTN_LEFT) {
            p->x    -= SX1(2);
            p->anim  = fighter_airborne(p) ? ANIM_JUMP : ANIM_WALK;
            p->facing = -1;
        } else if (buttons & BTN_RIGHT) {
            p->x    += SX1(2);
            p->anim  = fighter_airborne(p) ? ANIM_JUMP : ANIM_WALK;
            p->facing = 1;
        } else if (fighter_airborne(p)) {
            p->anim = ANIM_JUMP;
        } else {
            p->anim = ANIM_IDLE;
        }
    }

    /* Bounds */
    if (p->x < ARENA_L) p->x = ARENA_L;
    if (p->x > ARENA_R) p->x = ARENA_R;

    /* Check hit at the punch contact frame */
    if (p->punch_timer == PUNCH_HIT_FRAME) check_hit(g, 0);

    /* Always face opponent when idle */
    if (p->anim == ANIM_IDLE || p->anim == ANIM_HIT || p->anim == ANIM_BLOCK || p->anim == ANIM_JUMP) {
        p->facing = (g->fighters[1].x > p->x) ? 1 : -1;
    }
}

/* ── CPU AI ──────────────────────────────────────────────────────────────── */
static void update_cpu(game_t *g) {
    fighter_t *cpu = &g->fighters[1];
    fighter_t *plr = &g->fighters[0];

    cpu->facing = (plr->x < cpu->x) ? -1 : 1;  /* always face player */

    /* Check hit if cpu is punching */
    if (cpu->punch_timer == PUNCH_HIT_FRAME) check_hit(g, 1);

    /* Stunned / punching: don't think */
    if (cpu->punch_timer >= 0 || cpu->hit_stun > 0) return;

    g->cpu_think_timer++;
    if (g->cpu_think_timer < CPU_THINK_RATE) return;
    g->cpu_think_timer = 0;

    int dist     = plr->x - cpu->x;
    int abs_dist = (dist < 0) ? -dist : dist;
    int dir      = (dist > 0) ? 1 : -1;

    int r = rand() % 100;
    int attack_chance  = (cpu->hp < 35) ? 58 : 74;
    int retreat_chance = (cpu->hp < 35) ? 24 : 8;
    int block_chance   = (cpu->hp < 35) ? (CPU_BLOCK_CHANCE + 10) : CPU_BLOCK_CHANCE;
    int jump_chance    = (cpu->hp < 35) ? (CPU_JUMP_CHANCE + 8) : CPU_JUMP_CHANCE;

    if (!fighter_airborne(cpu) && abs_dist <= CPU_BLOCK_RANGE && plr->punch_timer >= 0 && r < block_chance) {
        /* Reactive defense: block some incoming attacks. */
        cpu->anim = ANIM_BLOCK;
    } else if (!fighter_airborne(cpu) && abs_dist <= IDEAL_DIST && plr->anim == ANIM_BLOCK && r < jump_chance) {
        /* Jump over defensive players to create better punish angles. */
        start_jump(cpu);
        cpu->x += dir * CPU_SPEED;
        cpu->anim = ANIM_JUMP;
    } else if (!fighter_airborne(cpu) && abs_dist <= CPU_BLOCK_RANGE && r < 8) {
        /* Occasional proactive guard to vary behavior. */
        cpu->anim = ANIM_BLOCK;
    } else if (abs_dist <= PUNCH_RANGE && r < attack_chance) {
        /* In range: often punch */
        cpu->anim           = ANIM_PUNCH;
        cpu->punch_timer    = 0;
        cpu->anim_frame     = 0;
        cpu->punch_hit_done = 0;
    } else if (abs_dist < RETREAT_DIST || r < retreat_chance) {
        /* Dynamic retreat: sometimes back off to reset spacing. */
        cpu->x   -= dir * CPU_SPEED;
        cpu->anim = fighter_airborne(cpu) ? ANIM_JUMP : ANIM_WALK;
    } else if (abs_dist > IDEAL_DIST) {
        /* Walk toward player */
        cpu->x   += dir * CPU_SPEED;
        cpu->anim = fighter_airborne(cpu) ? ANIM_JUMP : ANIM_WALK;
    } else if (!fighter_airborne(cpu) && r < 10) {
        /* Occasional neutral jump to break rhythm. */
        start_jump(cpu);
        cpu->anim = ANIM_JUMP;
    } else {
        cpu->anim = fighter_airborne(cpu) ? ANIM_JUMP : ANIM_IDLE;
    }

    if (cpu->x < ARENA_L) cpu->x = ARENA_L;
    if (cpu->x > ARENA_R) cpu->x = ARENA_R;
}

/* ── Result check ────────────────────────────────────────────────────────── */
static void check_result(game_t *g) {
    if (g->state != STATE_FIGHT) return;

    int p1_dead = (g->fighters[0].hp <= 0);
    int p2_dead = (g->fighters[1].hp <= 0);
    int timeout = (g->timer_frames <= 0);

    if (!p1_dead && !p2_dead && !timeout) return;

    g->state = STATE_RESULT;

    if (p2_dead && !p1_dead)
        g->winner = WINNER_PLAYER;
    else if (p1_dead && !p2_dead)
        g->winner = WINNER_CPU;
    else if (timeout) {
        int ph = g->fighters[0].hp, ch = g->fighters[1].hp;
        g->winner = (ph > ch) ? WINNER_PLAYER : (ch > ph) ? WINNER_CPU : WINNER_DRAW;
    } else
        g->winner = WINNER_DRAW;

    g->fighters[0].anim = (g->winner == WINNER_PLAYER) ? ANIM_VICTORY : ANIM_DEFEATED;
    g->fighters[1].anim = (g->winner == WINNER_CPU)    ? ANIM_VICTORY : ANIM_DEFEATED;
    g->fighters[0].punch_timer = -1;
    g->fighters[1].punch_timer = -1;
    g->fighters[0].hit_stun = 0;
    g->fighters[1].hit_stun = 0;
    g->fighters[0].anim_frame = 0;
    g->fighters[1].anim_frame = 0;
}

/* ── Public API ──────────────────────────────────────────────────────────── */
void game_init(game_t *g) {
    memset(g, 0, sizeof(*g));

    g->fighters[0].x           = P1_START_X;
    g->fighters[0].hp          = MAX_HP;
    g->fighters[0].facing      = 1;    /* P1 faces right */
    g->fighters[0].anim        = ANIM_IDLE;
    g->fighters[0].punch_timer = -1;

    g->fighters[1].x           = P2_START_X;
    g->fighters[1].hp          = MAX_HP;
    g->fighters[1].facing      = -1;   /* CPU faces left */
    g->fighters[1].anim        = ANIM_IDLE;
    g->fighters[1].punch_timer = -1;

    g->state        = STATE_FIGHT;
    g->timer_frames = TIMER_FRAMES;
}

void game_update(game_t *g, uint32_t buttons, uint32_t vsync_counter) {
    (void)vsync_counter;

    if (g->state == STATE_FIGHT) {
        g->timer_frames--;
        if (g->timer_frames < 0) g->timer_frames = 0;

        update_player(g, buttons);
        update_cpu(g);

        update_jump_physics(&g->fighters[0]);
        update_jump_physics(&g->fighters[1]);

        update_fighter_anim(&g->fighters[0]);
        update_fighter_anim(&g->fighters[1]);

        check_result(g);
    } else {
        if (g->result_delay > RESTART_DELAY && (buttons & (BTN_FIRE | BTN_SEL))) {
            game_init(g);
            return;
        }

        /* Result screen: animate victory/defeat poses */
        g->result_delay++;
        update_fighter_anim(&g->fighters[0]);
        update_fighter_anim(&g->fighters[1]);
    }
}

void game_render(game_t *g, lfb_t *lfb) {
    draw_background(lfb);
    draw_hud(lfb, g);

    /* Draw both fighters */
    draw_fighter(lfb, &g->fighters[0], &PAL_P1);
    draw_fighter(lfb, &g->fighters[1], &PAL_P2);

    /* Result overlay */
    if (g->state == STATE_RESULT) {
        if (g->winner == WINNER_PLAYER) {
            l_draw_text(lfb, LW / 2 - SX(28), SY(65), "VICTORY", 2, 0xFFFFFF00u);
        } else if (g->winner == WINNER_CPU) {
            l_draw_text(lfb, LW / 2 - SX(28), SY(65), "FAILURE", 2, 0xFFFF4444u);
        } else {
            l_draw_text(lfb, LW / 2 - SX(16), SY(65), "DRAW", 2, 0xFFAAAAAAu);
        }

        l_draw_text(lfb, LW / 2 - SX(52), SY(88), "PRESS SPACE TO RESTART", 1, 0xFFFFFFFFu);
    }
}
