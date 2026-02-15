#pragma once
#include <stdint.h>
#include "gfx.h"

#define MAX_MENU_ITEMS 4

typedef struct {
  const char* items[MAX_MENU_ITEMS];
  int itemCount;
  int selectedItem;
  char *game_exec;
} menu_t;

void menu_init(menu_t *m);
void menu_reset(menu_t *m);
int menu_update(menu_t *m, uint32_t buttons, uint32_t vsync_counter);
void menu_render(menu_t *m, lfb_t *lfb);

#define MOD(a,b) ({__typeof__(a) _a = (a); __typeof__(b) _b = (b); __typeof__(a) _r = _a % _b; _r < 0 ? _r + _b : _r;})
