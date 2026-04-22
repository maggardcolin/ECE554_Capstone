#include <stdio.h>
#include <unistd.h>
#include <pigpio.h>
#include "music_alsa.h"

#define NUM_TRACKS 16

// Map Raspberry Pi GPIO pins to specific music modes.
typedef struct 
{
    int gpio_pin;
    music_mode_t mode;
} track_mapping_t;

track_mapping_t tracks[NUM_TRACKS] = 
{
    {2,  MUSIC_MODE_MENU},
    {3,  MUSIC_MODE_SILENT},
    {4,  MUSIC_MODE_GAMEPLAY},
    {17, MUSIC_MODE_BOSS_INTRO},
    {27, MUSIC_MODE_BOSS},
    {22, MUSIC_MODE_BOSS_BLUE},
    {10, MUSIC_MODE_BOSS_YELLOW},
    {9,  MUSIC_MODE_BOSS_TOWER},
    {11, MUSIC_MODE_BOSS_HERMIT},
    {5,  MUSIC_MODE_BOSS_CHARIOT},
    {6,  MUSIC_MODE_BOSS_MAGICIAN},
    {13, MUSIC_MODE_TUTORIAL},
    {19, MUSIC_MODE_SHOP},
    {26, MUSIC_MODE_GAME_OVER},
    {14, MUSIC_MODE_WIN},
    {15, MUSIC_MODE_PAUSED}
};

// Track which pins are currently high (for debouncing)
int pin_states[NUM_TRACKS] = {0};

int main(void) 
{
    // Initialize ALSA Music Engine
    if (music_init() != 0) 
    {
        fprintf(stderr, "Failed to initialize ALSA music engine.\n");
        return 1;
    }

    // Initialize GPIO Library
    if (gpioInitialise() < 0) 
    {
        fprintf(stderr, "Failed to initialize pigpio. Did you run with sudo?\n");
        music_shutdown();
        return 1;
    }

    // Configure GPIO pins as Inputs
    for (int i = 0; i < NUM_TRACKS; i++) 
    {
        int pin = tracks[i].gpio_pin;
        gpioSetMode(pin, PI_INPUT);
        
        // Use internal pull-down resistors. 
        // This keeps the pin LOW (0) until you connect it to 3.3V (HIGH/1).
        gpioSetPullUpDown(pin, PI_PUD_DOWN);
    }

    printf("Music GPIO listener started. Send 3.3V to a configured pin to activate multiple tracks simultaneously...\n");

    while (1) 
    {
        for (int i = 0; i < NUM_TRACKS; i++) 
        {
            int pin = tracks[i].gpio_pin;
            int pin_read = gpioRead(pin);

            // Detect state change (transition from low to high or high to low)
            if (pin_read == 1 && pin_states[i] == 0) 
            {
                // Pin went HIGH
                printf("GPIO %d (track %d) activated\n", pin, (int)tracks[i].mode);
                music_set_track_active(i, 1);
                pin_states[i] = 1;
            } else if (pin_read == 0 && pin_states[i] == 1)
            {
                // Pin went LOW
                printf("GPIO %d (track %d) deactivated\n", pin, (int)tracks[i].mode);
                music_set_track_active(i, 0);
                pin_states[i] = 0;
            }
        }
        
        time_sleep(0.05);
    }

    // Cleanup (unreachable in this infinite loop, but good practice)
    gpioTerminate();
    music_shutdown();
    return 0;
}
