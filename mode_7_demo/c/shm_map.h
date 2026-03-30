#pragma once

#include <stddef.h>
#include <stdint.h>

#include "hw_contract.h"

typedef struct {
    int fd;
    size_t total_size;
    uint8_t *base;
    mmio_regs_t *regs;
    uint8_t *fb_base;
} shm_ctx_t;

int shm_open_map(shm_ctx_t *ctx, const char *name);
void shm_close_unmap(shm_ctx_t *ctx);
uint32_t *shm_back_buffer_ptr(shm_ctx_t *ctx);