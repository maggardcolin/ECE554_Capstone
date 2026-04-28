#include "../hw_contract.h"
#include "shm_map.h"
#include "gfx.h"
#include "game.h"
#include "music_alsa.h"
#include <stdio.h>

#define SHM_NAME "/pynq_fbmmio"

// update the specific bit in the hardware register to trigger the music engine
void write_music_register(uint1_t value, uint8_t shift) {
    // read the current register value
    uint32_t reg_value = *(volatile uint32_t *)0xA0000018

    // write the bit at the specified shift position    
    if (value) {
        reg_value |= (1 << shift);  // Set the bit
    } else {
        reg_value &= ~(1 << shift); // Clear the bit
    }
    *(volatile uint32_t *)0xA0000018 = reg_value;
}

// clear all bits in the music register (e.g., on game loop exit)
void clear_music_register(void) {
    *(volatile uint32_t *)0xA0000018 = 0;
}

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

    int music_ok = (music_init() == 0);
    clear_music_register();
    if (!music_ok) {
        fprintf(stderr, "music: disabled (ALSA init failed)\n");
    }

    uint32_t last_ack = shm.regs->swap_ack;

    while (1) {
        uint32_t buttons = shm.regs->buttons;
        if (buttons & BTN_QUIT) break;

        game_update(&game, buttons, shm.regs->vsync_counter);

        if (music_ok) {
            music_mode_t mode = MUSIC_MODE_GAMEPLAY;
            if (game.start_screen) mode = MUSIC_MODE_MENU;
            else if (game.game_over) mode = MUSIC_MODE_GAME_OVER;
            else if (game.win_screen) mode = MUSIC_MODE_WIN;
            else if (game.level_complete) mode = MUSIC_MODE_SILENT;
            else if (game.level == 0 && !game.level_complete) mode = MUSIC_MODE_SILENT;
            else if (game.overworld_cutscene_active) mode = MUSIC_MODE_SILENT;
            else if (game.paused) mode = MUSIC_MODE_PAUSED;
            else if (game.in_shop) mode = MUSIC_MODE_SHOP;
            else if (game.boss_intro_active) mode = MUSIC_MODE_BOSS_INTRO;
            else if (game.boss_alive) {
                if (game.boss_type == BOSS_TYPE_BLUE) mode = MUSIC_MODE_BOSS_BLUE;
                else if (game.boss_type == BOSS_TYPE_YELLOW) mode = MUSIC_MODE_BOSS_YELLOW;
                else if (game.boss_type == BOSS_TYPE_TOWER) mode = MUSIC_MODE_BOSS_TOWER;
                else if (game.boss_type == BOSS_TYPE_HERMIT) mode = MUSIC_MODE_BOSS_HERMIT;
                else if (game.boss_type == BOSS_TYPE_CHARIOT) mode = MUSIC_MODE_BOSS_CHARIOT;
                else if (game.boss_type == BOSS_TYPE_MAGICIAN) mode = MUSIC_MODE_BOSS_MAGICIAN;
                else mode = MUSIC_MODE_BOSS;
            }
            music_set_mode(mode);
        }

        game_render(&game, &lfb);

        uint32_t *back = shm_back_buffer_ptr(&shm);
        upscale_to_physical(back, &lfb);

        shm.regs->swap_request = 1;
        while (shm.regs->swap_ack == last_ack) { /* spin */ }
        last_ack = shm.regs->swap_ack;
    }

    if (music_ok) {
        music_shutdown();
    }
    lfb_free(&lfb);
    shm_close_unmap(&shm);
    return 0;
}

#ifdef __HW_SIM__
int main(void) {
  sw_sim_main();
}
#endif


