#include "hw_contract.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __APPLE__
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

#define SHM_NAME "/pynq_fbmmio"

static size_t shm_total_size(void) {
    size_t regs = sizeof(mmio_regs_t);
    size_t fbs = (size_t)FB_COUNT * (size_t)FB_SIZE;
    size_t page = 4096;
    size_t regs_pages = (regs + page - 1) / page;
    return regs_pages * page + fbs;
}

int main(void) {
    shm_unlink(SHM_NAME);
    size_t total = shm_total_size();

    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR | O_EXCL, 0666);
    if (fd < 0) {
        perror("shm_open");
        return errno;
    }
    if (ftruncate(fd, (off_t)total) != 0) {
        perror("ftruncate");
        return errno;
    }

    uint8_t *base = mmap(NULL, total, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (base == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    size_t page = 4096;
    mmio_regs_t *regs = (mmio_regs_t *)(base + 0);
    uint8_t *fb_base = base + page;

    memset((void *)regs, 0, sizeof(*regs));
    regs->front_idx = 0;
    regs->back_idx = 1;
    regs->prop_quit = 0;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init failed\n");
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow("hw_sim 720p", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, 0);
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow failed\n");
        return 1;
    }

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren) {
        fprintf(stderr, "SDL_CreateRenderer failed\n");
        return 1;
    }

    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, W, H);
    if (!tex) {
        fprintf(stderr, "SDL_CreateTexture failed\n");
        return 1;
    }

    bool running = true;
    Uint64 now = SDL_GetPerformanceCounter();
    Uint64 last = 0;
    const double target_frame_time = 1.0 / 30.0;

    while (running && !regs->prop_quit) {
        last = now;
        regs->buttons = 0;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                regs->buttons |= BTN_QUIT;
            }
        }

        const Uint8 *k = SDL_GetKeyboardState(NULL);
        if (k[SDL_SCANCODE_LEFT] || k[SDL_SCANCODE_A]) regs->buttons |= BTN_LEFT;
        if (k[SDL_SCANCODE_RIGHT] || k[SDL_SCANCODE_D]) regs->buttons |= BTN_RIGHT;
        if (k[SDL_SCANCODE_UP] || k[SDL_SCANCODE_W]) regs->buttons |= BTN_UP;
        if (k[SDL_SCANCODE_DOWN] || k[SDL_SCANCODE_S]) regs->buttons |= BTN_DOWN;
        if (k[SDL_SCANCODE_SPACE]) regs->buttons |= BTN_FIRE;
        if (k[SDL_SCANCODE_ESCAPE]) regs->buttons |= BTN_QUIT;
        if (k[SDL_SCANCODE_P]) regs->buttons |= BTN_PAUSE;
        if (k[SDL_SCANCODE_R]) regs->buttons |= BTN_RESET;
        if (k[SDL_SCANCODE_RETURN] || k[SDL_SCANCODE_SPACE]) regs->buttons |= BTN_SEL;

        if (regs->buttons & BTN_QUIT) {
            running = false;
        }

        regs->vsync_counter++;

        if (regs->swap_request) {
            uint32_t new_front = regs->back_idx % FB_COUNT;
            uint32_t new_back = regs->front_idx % FB_COUNT;
            regs->front_idx = new_front;
            regs->back_idx = new_back;
            regs->swap_request = 0;
            regs->swap_ack = regs->vsync_counter;
        }

        uint32_t fi = regs->front_idx % FB_COUNT;
        uint8_t *front = fb_base + (size_t)fi * (size_t)FB_SIZE;

        SDL_UpdateTexture(tex, NULL, front, W * 4);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);

        now = SDL_GetPerformanceCounter();
        double delta_time = (double)((now - last) * 1000) / (double)SDL_GetPerformanceFrequency();
        delta_time /= 1000.0;

        if (delta_time < target_frame_time) {
            SDL_Delay((Uint32)((target_frame_time - delta_time) * 1000.0));
        }
    }

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();

    munmap(base, total);
    close(fd);
    shm_unlink(SHM_NAME);
    return 0;
}