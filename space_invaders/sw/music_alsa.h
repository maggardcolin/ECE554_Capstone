#pragma once

typedef enum {
    MUSIC_MODE_MENU = 0,
    MUSIC_MODE_SILENT,
    MUSIC_MODE_GAMEPLAY,
    MUSIC_MODE_BOSS_INTRO,
    MUSIC_MODE_BOSS,
    MUSIC_MODE_BOSS_BLUE,
    MUSIC_MODE_BOSS_YELLOW,
    MUSIC_MODE_BOSS_TOWER,
    MUSIC_MODE_BOSS_HERMIT,
    MUSIC_MODE_BOSS_CHARIOT,
    MUSIC_MODE_BOSS_MAGICIAN,
    MUSIC_MODE_TUTORIAL,
    MUSIC_MODE_SHOP,
    MUSIC_MODE_GAME_OVER,
    MUSIC_MODE_WIN,
    MUSIC_MODE_PAUSED,
    MUSIC_MODE_SQUARE,
    MUSIC_MODE_DING,
    MUSIC_MODE_BOOM,
    MUSIC_MODE_BOOM_LONG,
    MUSIC_MODE_TOWER_ASTEROID_BOOM,
    MUSIC_MODE_LASER,
    MUSIC_MODE_POWERUP
} music_mode_t;

int music_init(void);
void music_set_mode(music_mode_t mode);
void music_set_track_active(int track_index, int active);
void music_play_ding(void);
void music_play_boom(void);
void music_play_boom_long(void);
void music_play_tower_asteroid_boom(void);
void music_play_laser(void);
void music_play_powerup(void);
void music_shutdown(void);
