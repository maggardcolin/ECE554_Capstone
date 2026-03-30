#include "shm_map.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static size_t shm_total_size(void) {
    size_t regs = sizeof(mmio_regs_t);
    size_t fbs = (size_t)FB_COUNT * (size_t)FB_SIZE;
    size_t page = 4096;
    size_t regs_pages = (regs + page - 1) / page;
    return regs_pages * page + fbs;
}

int shm_open_map(shm_ctx_t *ctx, const char *name) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->total_size = shm_total_size();

    ctx->fd = shm_open(name, O_RDWR, 0666);
    if (ctx->fd < 0) {
        perror("shm_open (start hw_sim first)");
        return -1;
    }

    ctx->base = mmap(NULL, ctx->total_size, PROT_READ | PROT_WRITE, MAP_SHARED, ctx->fd, 0);
    if (ctx->base == MAP_FAILED) {
        perror("mmap");
        close(ctx->fd);
        return -1;
    }

    size_t page = 4096;
    ctx->regs = (mmio_regs_t *)(ctx->base + 0);
    ctx->fb_base = ctx->base + page;
    return 0;
}

void shm_close_unmap(shm_ctx_t *ctx) {
    if (ctx->base && ctx->base != MAP_FAILED) {
        munmap(ctx->base, ctx->total_size);
    }
    if (ctx->fd > 0) {
        close(ctx->fd);
    }
    memset(ctx, 0, sizeof(*ctx));
}

uint32_t *shm_back_buffer_ptr(shm_ctx_t *ctx) {
    uint32_t bi = ctx->regs->back_idx % FB_COUNT;
    return (uint32_t *)(ctx->fb_base + (size_t)bi * (size_t)FB_SIZE);
}