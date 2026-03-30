#include "../space_invaders/hw_contract.h"
#include "../space_invaders/sw/shm_map.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SHM_NAME "/pynq_fbmmio"

#define TEX_SIZE 1024
#define HORIZON (H / 3)
#define FP_SHIFT 6
#define FP_ONE (1 << FP_SHIFT)

#define RGBA_BYTES_PER_PIXEL 4
#define TEX_PIXELS (TEX_SIZE * TEX_SIZE)
#define TEX_BYTES (TEX_PIXELS * RGBA_BYTES_PER_PIXEL)

typedef struct {
    int cam_x;
    int cam_y;
    int cam_z;
    int angle;
    int fov;
} mode7_state_t;

static int32_t sin_table[256];
static int32_t cos_table[256];

static inline int mul_fp(int a, int b) {
    return (int)(((int64_t)a * (int64_t)b) >> FP_SHIFT);
}

static inline int mul_fp_div(int a, int b, int c) {
    if (c == 0) {
        return 0;
    }
    return (int)(((int64_t)a * (int64_t)b) / c);
}

static void init_trig_tables(void) {
    for (int i = 0; i < 256; i++) {
        double rad = ((double)i / 256.0) * 2.0 * M_PI;
        sin_table[i] = (int32_t)llround(sin(rad) * FP_ONE);
        cos_table[i] = (int32_t)llround(cos(rad) * FP_ONE);
    }
}

static int load_rgba_texture(const char *path, uint32_t *dst_argb) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return -1;
    }

    uint8_t *rgba = (uint8_t *)malloc(TEX_BYTES);
    if (!rgba) {
        fclose(f);
        return -1;
    }

    size_t got = fread(rgba, 1, TEX_BYTES, f);
    fclose(f);
    if (got != TEX_BYTES) {
        free(rgba);
        return -1;
    }

    for (int i = 0; i < TEX_PIXELS; i++) {
        uint8_t r = rgba[4 * i + 0];
        uint8_t g = rgba[4 * i + 1];
        uint8_t b = rgba[4 * i + 2];
        uint8_t a = rgba[4 * i + 3];
        dst_argb[i] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }

    free(rgba);
    return 0;
}

static void generate_fallback_floor(uint32_t *tex) {
    for (int y = 0; y < TEX_SIZE; y++) {
        for (int x = 0; x < TEX_SIZE; x++) {
            int checker = ((x >> 5) ^ (y >> 5)) & 1;
            uint8_t c = checker ? 0xAA : 0x66;
            tex[y * TEX_SIZE + x] = 0xFF000000u | ((uint32_t)c << 16) | ((uint32_t)c << 8) | c;
        }
    }
}

static void generate_fallback_sky(uint32_t *tex) {
    for (int y = 0; y < TEX_SIZE; y++) {
        for (int x = 0; x < TEX_SIZE; x++) {
            int grad = 90 + (y * 120) / TEX_SIZE;
            int cloud = (((x * 31) ^ (y * 17) ^ (x * y)) & 0x3F);
            int blue = grad + (cloud >> 3);
            int green = (grad * 3) / 5 + (cloud >> 4);
            int red = green / 2;
            if (red > 255) red = 255;
            if (green > 255) green = 255;
            if (blue > 255) blue = 255;
            tex[y * TEX_SIZE + x] = 0xFF000000u | ((uint32_t)red << 16) | ((uint32_t)green << 8) | (uint32_t)blue;
        }
    }
}

static void update_state(mode7_state_t *s, uint32_t buttons) {
    if (buttons & BTN_LEFT) {
        s->angle = (s->angle + 2) & 255;
    }
    if (buttons & BTN_RIGHT) {
        s->angle = (s->angle - 2) & 255;
    }

    int speed = 2 << FP_SHIFT;
    int dir = (s->angle + 64) & 255;
    int dx = mul_fp(cos_table[dir], speed);
    int dy = mul_fp(sin_table[dir], speed);

    if (buttons & BTN_UP) {
        s->cam_x += dx;
        s->cam_y += dy;
    }
    if (buttons & BTN_DOWN) {
        s->cam_x -= dx;
        s->cam_y -= dy;
    }
}

static void render_frame(uint32_t *dst, const mode7_state_t *s, const uint32_t *floor_tex, const uint32_t *sky_tex) {
    for (int y = 0; y <= HORIZON; y++) {
        int v = (y * TEX_SIZE / (HORIZON + 1)) & (TEX_SIZE - 1);
        int u_offset = (-(s->angle << 2) * 3) & (TEX_SIZE - 1);

        int row = y * W;
        for (int x = 0; x < W; x++) {
            int u = (x + u_offset) & (TEX_SIZE - 1);
            dst[row + x] = sky_tex[v * TEX_SIZE + u];
        }
    }

    int sin_a = sin_table[s->angle & 255];
    int cos_a = cos_table[s->angle & 255];

    for (int y = HORIZON + 1; y < H; y++) {
        int p = (y - HORIZON) << FP_SHIFT;
        int z = mul_fp_div(s->fov, s->cam_z, p);

        int row = y * W;
        for (int x = 0; x < W; x++) {
            int offset_x = (x - (W >> 1)) << FP_SHIFT;

            int floor_x = mul_fp_div(z, offset_x, s->fov);
            int floor_y = z;

            int world_x = mul_fp(floor_x, cos_a) - mul_fp(floor_y, sin_a);
            int world_y = mul_fp(floor_x, sin_a) + mul_fp(floor_y, cos_a);

            int tex_x = world_x + s->cam_x;
            int tex_y = world_y + s->cam_y;

            int u = (tex_x >> FP_SHIFT) & (TEX_SIZE - 1);
            int v = (tex_y >> FP_SHIFT) & (TEX_SIZE - 1);

            uint32_t pcol = floor_tex[v * TEX_SIZE + u];
            if ((pcol >> 24) != 0) {
                dst[row + x] = pcol;
            } else {
                dst[row + x] = 0xDEADBEEFu;
            }
        }
    }
}

int main(void) {
    shm_ctx_t shm;
    if (shm_open_map(&shm, SHM_NAME) != 0) {
        return 1;
    }

    init_trig_tables();

    uint32_t *floor_tex = (uint32_t *)malloc(sizeof(uint32_t) * TEX_PIXELS);
    uint32_t *floor_tex_2 = (uint32_t *)malloc(sizeof(uint32_t) * TEX_PIXELS);
    uint32_t *sky_tex = (uint32_t *)malloc(sizeof(uint32_t) * TEX_PIXELS);
    if (!floor_tex || !floor_tex_2 || !sky_tex) {
        fprintf(stderr, "texture allocation failed\n");
        free(floor_tex);
        free(floor_tex_2);
        free(sky_tex);
        shm_close_unmap(&shm);
        return 1;
    }

    if (load_rgba_texture("map3.rgba", floor_tex) != 0) {
        generate_fallback_floor(floor_tex);
    }
    if (load_rgba_texture("map2.rgba", floor_tex_2) != 0) {
        generate_fallback_floor(floor_tex_2);
    }
    if (load_rgba_texture("sky.rgba", sky_tex) != 0) {
        generate_fallback_sky(sky_tex);
    }

    mode7_state_t state;
    state.cam_x = 50 << FP_SHIFT;
    state.cam_y = 50 << FP_SHIFT;
    state.cam_z = 10 << FP_SHIFT;
    state.angle = 0;
    state.fov = 320 << FP_SHIFT;

    uint32_t last_ack = shm.regs->swap_ack;

    while (1) {
        uint32_t buttons = shm.regs->buttons;
        if (buttons & BTN_QUIT) {
            break;
        }

        update_state(&state, buttons);

        uint32_t *back = shm_back_buffer_ptr(&shm);
        render_frame(back, &state, floor_tex, sky_tex);

        shm.regs->swap_request = 1;
        while (shm.regs->swap_ack == last_ack) {
        }
        last_ack = shm.regs->swap_ack;
    }

    shm.regs->prop_quit = 1;
    free(floor_tex);
    free(floor_tex_2);
    free(sky_tex);
    shm_close_unmap(&shm);
    return 0;
}
