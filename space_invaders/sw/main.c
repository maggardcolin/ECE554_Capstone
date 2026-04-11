#include "../hw_contract.h"
#include "shm_map.h"
#include "gfx.h"
#include "game.h"
#include <stdio.h>

#define SHM_NAME "/pynq_fbmmio"

/// main: Software game loop - connects to hardware simulator and runs game
/// Parameters: none (no command-line arguments)
/// Returns: 0 on normal exit, 1 if initialization fails
/// Notes: Creates logical framebuffer, initializes game, runs update/render loop,
///        waits for hardware buffer swaps via shared memory MMIO,
///        exits on BTN_QUIT input.
int sw_sim_main(void) {
    shm_ctx_t shm;
    if (shm_open_map(&shm, SHM_NAME) != 0) return 1;

    lfb_t lfb;
    if (lfb_init(&lfb) != 0) {
        fprintf(stderr, "lfb_init failed\n");
        shm_close_unmap(&shm);
        return 1;
    }

    game_t game;
    game_init(&game);

    uint32_t last_ack = shm.regs->swap_ack;

    while (1) {
        uint32_t buttons = shm.regs->buttons;
        if (buttons & BTN_QUIT) break;

        game_update(&game, buttons, shm.regs->vsync_counter);
        game_render(&game, &lfb);

        uint32_t *back = shm_back_buffer_ptr(&shm);
        upscale_to_physical(back, &lfb);

        shm.regs->swap_request = 1;
        while (shm.regs->swap_ack == last_ack) { /* spin */ }
        last_ack = shm.regs->swap_ack;
    }

    lfb_free(&lfb);
    shm_close_unmap(&shm);
    return 0;
}
