#include "menu.h"
#include "../hw_contract.h"
#include "font.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

// OSX and linux define their label points differently
#ifdef __APPLE__
  #define LABELPATH "/Volumes"
	#define MOUNTVOLS "/Volumes"
#else
  #define LABELPATH "/dev/disk/by-label"
	#define MOUNTVOLS "/game-mnt"
#endif

#define GAMELABEL "GAMESIM"

/**
 * See ../../space_invaders/sw/game.c for definition
 */
static int text_width_5x5(const char *text, int scale) {
  if (scale < 1) scale = 1;
  int count = 0;
  for (const char *p = text; *p; p++) count++;
  if (count == 0) return 0;
  return (count * 6 - 1) * scale;
}

/**
 * Checks if game "cartridge" (disk) exists on system
 */
int check_for_game() {
  char path[256];
  
  snprintf(path, sizeof(path), "%s/%s", LABELPATH, GAMELABEL);

  struct stat st;
  return stat(path, &st) == 0;
}

/**
 * Starts the game in a new process. The main menu process waits for game to quit, then
 * continues running as normal.
 */
void exec_game(char *game) {
  if (game == NULL) return;
  if (*game == '\0') return;

  char *argv[] = {"sw_game", NULL};

  int pid = fork();

  if (pid < 0) { perror("fork"); exit(1); }

  if (pid == 0) {
    execv(game, argv);
    printf("failed to exec game");
    exit(1);
  } else {
    wait(NULL);
  }
}

/**
 * Fills a basic menu_t item with preset values
 */
void menu_init(menu_t *m) {
  memset(m, 0, sizeof(*m));

  m->items[0] = "Empty Cartridge";
  m->items[1] = "Settings";
  m->items[2] = "Exit";

  m->itemCount = 3;
  m->selectedItem = 0;

  m->game_exec = "\0";
}

/**
 * Currently does nothing and is not called. Here for the future
 */
void menu_reset(menu_t *m) {
  m->selectedItem = 0;
}

/**
 * Updates the menu every new frame based on input from the hardware
 */
int menu_update(menu_t *m, uint32_t buttons, uint32_t vsync_counter) {
  static uint32_t prev_buttons = 0;
  if ((buttons & BTN_UP) && !(prev_buttons & BTN_UP)) {
    m->selectedItem--; m->selectedItem = MOD(m->selectedItem, m->itemCount);
  }
  if ((buttons & BTN_DOWN) && !(prev_buttons & BTN_DOWN)) {
    m->selectedItem++; m->selectedItem = MOD(m->selectedItem, m->itemCount);
  }

  prev_buttons = buttons;

  if (buttons & BTN_SEL) {
    switch (m->selectedItem) {
			// Executes the game cartridge
      case 0:
        exec_game(m->game_exec);
        break;

      case 1:
        // Currently unused, for future settings menu (mostly just sound)
        break;

			// Exits the simulator (to be determined what it does on the physical console)
      case 2:
        return 1;
        break;

      default:
        // How did you get here?
        break;
    }
  }

  // If a game cartridge was inserted, read the necessary data
  if (check_for_game()) {
    char title_path[256];
    char game_path[256];
    snprintf(title_path, sizeof(title_path), "%s/%s/title", MOUNTVOLS, GAMELABEL);
    snprintf(game_path, sizeof(game_path), "%s/%s/sw_game", MOUNTVOLS, GAMELABEL);

    char buf[17];
    FILE *f = fopen(title_path, "r");
    // Its possible the cartridge was removed right before reading
    // so this makes sure we don't read null pointer references
    if (!f) { m->items[0] = "EMPTY CARTRIDGE"; m->game_exec = "\0"; return 0; }

    fgets(buf, sizeof(buf), f);
    fclose(f);

    m->items[0] = strdup(buf);
    m->game_exec = strdup(game_path);
  } else {
    // Else just default the values
    m->items[0] = "EMPTY CARTRIDGE";
    m->game_exec = "\0";
  }

  return 0;
}

/**
 * How the menu should be displayed on the hardware.
 * Currently a WIP for better aesthetics
 */
void menu_render(menu_t *m, lfb_t *lfb) {
  l_clear(lfb, 0xFF000000);
  for (int i = 0; i < m->itemCount; i++) {
    int entry_w = text_width_5x5(m->items[i], 2);
    int entry_x = (LW - entry_w) / 2;
    int entry_y = (LH / 2 - 40) + (40 * i);
    l_draw_text(lfb, entry_x, entry_y, m->items[i], 2, (i == m->selectedItem) ? 0xFF00FF00 : 0xFFFFFFFF);
  }
}
