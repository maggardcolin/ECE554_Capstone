#pragma once

typedef enum {
    MUSIC_MODE_MENU = 0,
    MUSIC_MODE_SILENT,
    MUSIC_MODE_GAMEPLAY,
    MUSIC_MODE_BOSS,
    MUSIC_MODE_SHOP,
    MUSIC_MODE_GAME_OVER,
    MUSIC_MODE_PAUSED
} music_mode_t;

int music_init(void);
void music_set_mode(music_mode_t mode);
void music_shutdown(void);
