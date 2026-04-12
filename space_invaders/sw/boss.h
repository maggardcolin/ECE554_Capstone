#pragma once

#include <stdint.h>
#include "game.h"

void boss_trigger_death(game_t *g, int points_awarded);
void boss_apply_explosion_to_aliens(game_t *g);
void boss_apply_alien_explosions_to_boss(game_t *g);
void boss_render_explosion(const game_t *g, lfb_t *lfb);
