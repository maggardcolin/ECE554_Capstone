#pragma once

#include <stdint.h>
#include "game.h"

void clear_player_shots(game_t *g);
void update_player_shots(game_t *g);
void update_player_movement(game_t *g, uint32_t buttons);
void update_player_firing(game_t *g, uint32_t buttons);

void render_player(const game_t *g, lfb_t *lfb);
void render_score_display(const game_t *g, lfb_t *lfb);
void render_player_health_bar(const game_t *g, lfb_t *lfb);
void render_player_shots(const game_t *g, lfb_t *lfb);
