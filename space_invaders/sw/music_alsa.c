#include "music_alsa.h"

#ifndef NO_ALSA

#include <alsa/asoundlib.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define AUDIO_RATE 44100
#define AUDIO_CH 1
#define AUDIO_FRAMES 512

#define DUTY_MELODY 0.50f
#define DUTY_BASS 0.25f
#define DUTY_HARM 0.125f

#define DING_FREQ 1046.50f
#define DING_DURATION_SAMPLES (AUDIO_RATE / 5)
#define DING_GAIN 0.18f

#define BOOM_FREQ_START 180.0f
#define BOOM_FREQ_END 70.0f
#define BOOM_DURATION_SAMPLES (AUDIO_RATE / 4)
#define BOOM_GAIN 0.24f

#define BOOM_LONG_FREQ_START 160.0f
#define BOOM_LONG_FREQ_END 45.0f
#define BOOM_LONG_DURATION_SAMPLES (AUDIO_RATE * 2 / 3)
#define BOOM_LONG_GAIN 0.28f

#define LASER_FREQ_START 320.0f
#define LASER_FREQ_END 780.0f
#define LASER_DURATION_SAMPLES (AUDIO_RATE / 5)
#define LASER_GAIN 0.16f

#define POWERUP_FREQ_START 520.0f
#define POWERUP_FREQ_END 1240.0f
#define POWERUP_DURATION_SAMPLES (AUDIO_RATE / 6)
#define POWERUP_GAIN 0.17f

#define R 0.0f

typedef struct {
    float melody[64];
    float bass[64];
    float harm[64];
    int len;
    int bpm;
    float gain;
} pattern_t;

typedef struct {
    snd_pcm_t *pcm;
    pthread_t thread;
    pthread_mutex_t lock;
    int running;
    music_mode_t mode;
    int ding_pending;
    int boom_pending;
    int boom_long_pending;
    int laser_pending;
    int powerup_pending;
} music_ctx_t;

static music_ctx_t g_music;

static const pattern_t PATTERN_MENU = {
    .melody = {},
    .bass = {},
    .harm = {},
    .len = 16,
    .bpm = 118,
    .gain = 0.18f,
};

static const pattern_t PATTERN_SILENT = {
    .melody = {R, R, R, R, R, R, R, R,
               R, R, R, R, R, R, R, R},
    .bass = {R, R, R, R, R, R, R, R,
             R, R, R, R, R, R, R, R},
    .harm = {R, R, R, R, R, R, R, R,
             R, R, R, R, R, R, R, R},
    .len = 16,
    .bpm = 120,
    .gain = 0.0f,
};

static const pattern_t PATTERN_GAMEPLAY = {
    .melody = {},
    .bass = {130.81f, R, R, R, 174.61f, R, R, R,
             146.83f, R, R, R, 130.81f, R, R, R},
    .harm = {},
    .len = 16,
    .bpm = 142,
    .gain = 0.20f,
};

static const pattern_t PATTERN_BOSS = {
    .melody = {
        349.23f, R, 349.23f, 415.30f, 466.16f, R, 466.16f, 523.25f,
        349.23f, R, 349.23f, 415.30f, R, R, R, R,

        349.23f, R, 349.23f, 415.30f, 466.16f, R, 466.16f, 523.25f,
        349.23f, R, 349.23f, 415.30f, R, R, R, R,

        R, R, R, R, R, R, R, R,
        R, R, R, R, R, R, R, R,

        R, R, R, R, R, R, R, R,
        R, R, R, R, R, R, R, R
    },
    .bass = {
        87.31f, 87.31f, R, 87.31f, 92.50f, 92.50f, R, 92.50f,
        77.78f, 77.78f, R, 77.78f, 87.31f, 87.31f, 92.50f, R,

        87.31f, 87.31f, R, 87.31f, 92.50f, 92.50f, R, 92.50f,
        77.78f, 77.78f, R, 77.78f, 87.31f, 87.31f, 92.50f, R,

        87.31f, 87.31f, R, 87.31f, 92.50f, 92.50f, R, 92.50f,
        77.78f, 77.78f, R, 77.78f, 87.31f, 87.31f, 92.50f, R,

        87.31f, 87.31f, R, 87.31f, 92.50f, 92.50f, R, 92.50f,
        77.78f, 77.78f, R, 77.78f, 87.31f, 87.31f, 92.50f, R
    },
    .harm = {
        174.61f, R, 233.08f, R, 261.63f, R, 311.13f, R,
        174.61f, R, 233.08f, R, 261.63f, R, 311.13f, R,

        174.61f, R, 233.08f, R, 261.63f, R, 311.13f, R,
        174.61f, R, 233.08f, R, 261.63f, R, 311.13f, R,

        174.61f, R, 233.08f, R, 261.63f, R, 311.13f, R,
        174.61f, R, 233.08f, R, 261.63f, R, 311.13f, R,

        174.61f, R, 233.08f, R, 261.63f, R, 311.13f, R,
        174.61f, R, 233.08f, R, 261.63f, R, 311.13f, R
    },
    .len = 64,
    .bpm = 128,
    .gain = 0.22f,
};

static const pattern_t PATTERN_SHOP = {
    .melody = {},
    .bass = {164.81f, R, 196.00f, R, 220.00f, R, 196.00f, R,
             164.81f, R, 196.00f, R, 220.00f, R, 196.00f, R},
    .harm = {},
    .len = 16,
    .bpm = 124,
    .gain = 0.17f,
};

static const pattern_t PATTERN_GAME_OVER = {
    .melody = {659.25f, 622.25f, 587.33f, 523.25f, 493.88f, 440.00f, 392.00f, 349.23f,
               329.63f, R, R, R, R, R, R, R},
    .bass = {164.81f, R, 146.83f, R, 130.81f, R, 123.47f, R,
             110.00f, R, R, R, R, R, R, R},
    .harm = {329.63f, R, 293.66f, R, 261.63f, R, 246.94f, R,
             220.00f, R, R, R, R, R, R, R},
    .len = 16,
    .bpm = 88,
    .gain = 0.20f,
};

static const pattern_t PATTERN_PAUSED = {
    .melody = {523.25f, R, R, R, 523.25f, R, R, R,
               659.25f, R, R, R, 659.25f, R, R, R},
    .bass = {130.81f, R, R, R, 130.81f, R, R, R,
             146.83f, R, R, R, 146.83f, R, R, R},
    .harm = {R, R, R, R, R, R, R, R,
             R, R, R, R, R, R, R, R},
    .len = 16,
    .bpm = 60,
    .gain = 0.12f,
};

static const pattern_t *pattern_for_mode(music_mode_t mode) {
    switch (mode) {
    case MUSIC_MODE_MENU:
        return &PATTERN_MENU;
    case MUSIC_MODE_SILENT:
        return &PATTERN_SILENT;
    case MUSIC_MODE_GAMEPLAY:
        return &PATTERN_GAMEPLAY;
    case MUSIC_MODE_BOSS:
        return &PATTERN_BOSS;
    case MUSIC_MODE_SHOP:
        return &PATTERN_SHOP;
    case MUSIC_MODE_GAME_OVER:
        return &PATTERN_GAME_OVER;
    case MUSIC_MODE_PAUSED:
        return &PATTERN_PAUSED;
    default:
        return &PATTERN_GAMEPLAY;
    }
}

static float square(float *phase, float freq, float duty) {
    if (freq <= 0.0f) {
        return 0.0f;
    }
    *phase += freq / (float)AUDIO_RATE;
    if (*phase >= 1.0f) {
        *phase -= 1.0f;
    }
    return (*phase < duty) ? 1.0f : -1.0f;
}

static void *audio_thread(void *arg) {
    (void)arg;

    int16_t buffer[AUDIO_FRAMES];
    float phase1 = 0.0f;
    float phase2 = 0.0f;
    float phase3 = 0.0f;
    int step = 0;
    int sample_in_step = 0;
    int game_over_finished = 0;
    music_mode_t active_mode = MUSIC_MODE_MENU;
    int ding_samples_left = 0;
    float ding_phase = 0.0f;
    int boom_samples_left = 0;
    float boom_phase = 0.0f;
    int boom_long_samples_left = 0;
    float boom_long_phase = 0.0f;
    int laser_samples_left = 0;
    float laser_phase = 0.0f;
    int powerup_samples_left = 0;
    float powerup_phase = 0.0f;

    while (1) {
        pthread_mutex_lock(&g_music.lock);
        int running = g_music.running;
        music_mode_t mode = g_music.mode;
        if (g_music.ding_pending) {
            ding_samples_left = DING_DURATION_SAMPLES;
            ding_phase = 0.0f;
            g_music.ding_pending = 0;
        }
        if (g_music.boom_pending) {
            boom_samples_left = BOOM_DURATION_SAMPLES;
            boom_phase = 0.0f;
            g_music.boom_pending = 0;
        }
        if (g_music.boom_long_pending) {
            boom_long_samples_left = BOOM_LONG_DURATION_SAMPLES;
            boom_long_phase = 0.0f;
            g_music.boom_long_pending = 0;
            boom_samples_left = 0;
        }
        if (g_music.laser_pending) {
            laser_samples_left = LASER_DURATION_SAMPLES;
            laser_phase = 0.0f;
            g_music.laser_pending = 0;
        }
        if (g_music.powerup_pending) {
            powerup_samples_left = POWERUP_DURATION_SAMPLES;
            powerup_phase = 0.0f;
            g_music.powerup_pending = 0;
        }
        pthread_mutex_unlock(&g_music.lock);

        if (!running) {
            break;
        }

        const pattern_t *pat = pattern_for_mode(mode);
        if (mode != active_mode) {
            active_mode = mode;
            step = 0;
            sample_in_step = 0;
            game_over_finished = 0;
            phase1 = 0.0f;
            phase2 = 0.0f;
            phase3 = 0.0f;
        }

        int samples_per_step = (AUDIO_RATE * 60) / (pat->bpm * 4);
        if (samples_per_step <= 0) {
            samples_per_step = 1;
        }

        for (int i = 0; i < AUDIO_FRAMES; i++) {
            float f1 = 0.0f;
            float f2 = 0.0f;
            float f3 = 0.0f;
            if (!(mode == MUSIC_MODE_GAME_OVER && game_over_finished)) {
                f1 = pat->melody[step];
                f2 = pat->bass[step];
                f3 = pat->harm[step];
            }

            float s1 = square(&phase1, f1, DUTY_MELODY);
            float s2 = square(&phase2, f2, DUTY_BASS);
            float s3 = square(&phase3, f3, DUTY_HARM);
            float music_mix = (0.10f * s1) + (0.20f * s2) + (0.10f * s3);
            float ding_mix = 0.0f;
            float boom_mix = 0.0f;
            float boom_long_mix = 0.0f;
            float laser_mix = 0.0f;
            float powerup_mix = 0.0f;

            if (ding_samples_left > 0) {
                float env = (float)ding_samples_left / (float)DING_DURATION_SAMPLES;
                float ding = square(&ding_phase, DING_FREQ, 0.50f);
                ding_mix = ding * DING_GAIN * env;
                ding_samples_left--;
            }

            if (boom_samples_left > 0) {
                float env = (float)boom_samples_left / (float)BOOM_DURATION_SAMPLES;
                float freq = BOOM_FREQ_END + (BOOM_FREQ_START - BOOM_FREQ_END) * env;
                float boom = square(&boom_phase, freq, 0.50f);
                boom_mix = boom * BOOM_GAIN * env;
                boom_samples_left--;
            }

            if (boom_long_samples_left > 0) {
                float env = (float)boom_long_samples_left / (float)BOOM_LONG_DURATION_SAMPLES;
                float freq = BOOM_LONG_FREQ_END + (BOOM_LONG_FREQ_START - BOOM_LONG_FREQ_END) * env;
                float boom_long = square(&boom_long_phase, freq, 0.50f);
                boom_long_mix = boom_long * BOOM_LONG_GAIN * env;
                boom_long_samples_left--;
            }

            if (laser_samples_left > 0) {
                float progress = 1.0f - ((float)laser_samples_left / (float)LASER_DURATION_SAMPLES);
                float freq = LASER_FREQ_START + (LASER_FREQ_END - LASER_FREQ_START) * progress;
                float env = 1.0f - progress;
                float laser = square(&laser_phase, freq, 0.40f);
                laser_mix = laser * LASER_GAIN * env;
                laser_samples_left--;
            }

            if (powerup_samples_left > 0) {
                float progress = 1.0f - ((float)powerup_samples_left / (float)POWERUP_DURATION_SAMPLES);
                float freq = POWERUP_FREQ_START + (POWERUP_FREQ_END - POWERUP_FREQ_START) * progress;
                float env = 1.0f - (progress * 0.6f);
                float p = square(&powerup_phase, freq, 0.45f);
                powerup_mix = p * POWERUP_GAIN * env;
                powerup_samples_left--;
            }

            int sample = (int)(((music_mix * pat->gain) + ding_mix + boom_mix + boom_long_mix + laser_mix + powerup_mix) * 32767.0f);
            if (sample > 32767) sample = 32767;
            if (sample < -32768) sample = -32768;
            buffer[i] = (int16_t)sample;

            sample_in_step++;
            if (sample_in_step >= samples_per_step) {
                sample_in_step = 0;
                if (!(mode == MUSIC_MODE_GAME_OVER && game_over_finished)) {
                    step++;
                    if (step >= pat->len) {
                        if (mode == MUSIC_MODE_GAME_OVER) {
                            step = pat->len - 1;
                            game_over_finished = 1;
                        } else {
                            step = 0;
                        }
                    }
                }
            }
        }

        snd_pcm_sframes_t wrote = snd_pcm_writei(g_music.pcm, buffer, AUDIO_FRAMES);
        if (wrote == -EPIPE) {
            snd_pcm_prepare(g_music.pcm);
            continue;
        }
        if (wrote < 0) {
            snd_pcm_recover(g_music.pcm, (int)wrote, 1);
        }
    }

    snd_pcm_drain(g_music.pcm);
    return NULL;
}

int music_init(void) {
    memset(&g_music, 0, sizeof(g_music));
    g_music.mode = MUSIC_MODE_MENU;

    if (pthread_mutex_init(&g_music.lock, NULL) != 0) {
        fprintf(stderr, "music: pthread_mutex_init failed\n");
        return -1;
    }

    int rc = snd_pcm_open(&g_music.pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) {
        fprintf(stderr, "music: snd_pcm_open failed: %s\n", snd_strerror(rc));
        pthread_mutex_destroy(&g_music.lock);
        return -1;
    }

    rc = snd_pcm_set_params(g_music.pcm,
                            SND_PCM_FORMAT_S16_LE,
                            SND_PCM_ACCESS_RW_INTERLEAVED,
                            AUDIO_CH,
                            AUDIO_RATE,
                            1,
                            20000);
    if (rc < 0) {
        fprintf(stderr, "music: snd_pcm_set_params failed: %s\n", snd_strerror(rc));
        snd_pcm_close(g_music.pcm);
        g_music.pcm = NULL;
        pthread_mutex_destroy(&g_music.lock);
        return -1;
    }

    g_music.running = 1;
    if (pthread_create(&g_music.thread, NULL, audio_thread, NULL) != 0) {
        fprintf(stderr, "music: pthread_create failed\n");
        g_music.running = 0;
        snd_pcm_close(g_music.pcm);
        g_music.pcm = NULL;
        pthread_mutex_destroy(&g_music.lock);
        return -1;
    }

    return 0;
}

void music_set_mode(music_mode_t mode) {
    pthread_mutex_lock(&g_music.lock);
    if (g_music.running) {
        g_music.mode = mode;
    }
    pthread_mutex_unlock(&g_music.lock);
}

void music_play_ding(void) {
    pthread_mutex_lock(&g_music.lock);
    if (g_music.running) {
        g_music.ding_pending = 1;
    }
    pthread_mutex_unlock(&g_music.lock);
}

void music_play_boom(void) {
    pthread_mutex_lock(&g_music.lock);
    if (g_music.running) {
        g_music.boom_pending = 1;
    }
    pthread_mutex_unlock(&g_music.lock);
}

void music_play_boom_long(void) {
    pthread_mutex_lock(&g_music.lock);
    if (g_music.running) {
        g_music.boom_long_pending = 1;
    }
    pthread_mutex_unlock(&g_music.lock);
}

void music_play_laser(void) {
    pthread_mutex_lock(&g_music.lock);
    if (g_music.running) {
        g_music.laser_pending = 1;
    }
    pthread_mutex_unlock(&g_music.lock);
}

void music_play_powerup(void) {
    pthread_mutex_lock(&g_music.lock);
    if (g_music.running) {
        g_music.powerup_pending = 1;
    }
    pthread_mutex_unlock(&g_music.lock);
}

void music_shutdown(void) {
    pthread_mutex_lock(&g_music.lock);
    int was_running = g_music.running;
    g_music.running = 0;
    pthread_mutex_unlock(&g_music.lock);

    if (was_running) {
        pthread_join(g_music.thread, NULL);
    }
    if (g_music.pcm) {
        snd_pcm_close(g_music.pcm);
        g_music.pcm = NULL;
    }
    pthread_mutex_destroy(&g_music.lock);
}

#else

int music_init(void) {
    return -1;
}

void music_set_mode(music_mode_t mode) {
    (void)mode;
}

void music_play_ding(void) {
}

void music_play_boom(void) {
}

void music_play_boom_long(void) {
}

void music_play_laser(void) {
}

void music_play_powerup(void) {
}

void music_shutdown(void) {
}

#endif
