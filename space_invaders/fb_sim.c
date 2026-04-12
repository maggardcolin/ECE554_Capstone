// hw_sim.c: Hardware simulator - SDL2-based framebuffer display, input handler, and vsync generator
// Simulates PYNQ hardware: creates shared memory MMIO for game software,
// handles SDL2 window/rendering, keyboard input, and double-buffered framebuffer swap
// USAGE: Run hw_sim first, then run sw_game in another terminal
#include "hw_contract.h"
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <linux/input-event-codes.h>
#include <libevdev-1.0/libevdev/libevdev.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <time.h>
#include <SDL/SDL.h>

#define SHM_NAME "/pynq_fbmmio"

/// shm_total_size: Calculate total shared memory size needed
/// Returns: Bytes needed = page-aligned registers + 2 framebuffers (double-buffered)
static size_t shm_total_size(void) {
    size_t regs = sizeof(mmio_regs_t);
    size_t fbs  = (size_t)FB_COUNT * (size_t)FB_SIZE;
    size_t page = 4096;
    size_t regs_pages = (regs + page - 1) / page;
    return regs_pages * page + fbs;
}

static int tty_fd = -1;

void restore_tty(void) {
    if (tty_fd >= 0) {
    	ioctl(tty_fd, KDSKBMODE, K_XLATE);
    	ioctl(tty_fd, KDSETMODE, KD_TEXT);
	close(tty_fd);
	tty_fd = -1;
    }
}

void setup_handlers();

void setup_tty(void) {
    tty_fd = open("/dev/tty", O_RDWR | O_NOCTTY);
    if (tty_fd < 0) return;

    setup_handlers();

    ioctl(tty_fd, KDSETMODE, KD_GRAPHICS);
    ioctl(tty_fd, KDSKBMODE, K_OFF);
}

void handle_signal(int sig) {
    restore_tty();
    _exit(1);
}

void setup_handlers(void) {
    atexit(restore_tty);
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGSEGV, handle_signal);
}

int find_keyboard_event(char *out_path, size_t out_size) {
	DIR *dir = opendir("/dev/input");
	if (!dir) return -1;

	struct dirent *ent;
	while ((ent = readdir(dir)) != NULL) {
		if (strncmp(ent->d_name, "event", 5) != 0) continue;

		char path[256];
		snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);

		int fd = open(path, O_RDONLY | O_NONBLOCK);
		if (fd < 0) continue;

		struct libevdev *dev = NULL;
		if (libevdev_new_from_fd(fd, &dev) < 0) {
			close(fd);
			continue;
		}

		if (libevdev_has_event_type(dev, EV_KEY) &&
		    libevdev_has_event_code(dev, EV_KEY, KEY_A)) {
			strncpy(out_path, path, out_size);
			libevdev_free(dev);
			closedir(dir);
			return fd;
		}

		libevdev_free(dev);
		close(fd);
	}

	closedir(dir);
	return -1;
}

/// main: Hardware simulator main loop
/// Creates shared memory and MMIO registers, initializes SDL2 window/renderer,
/// polls keyboard input, increments vsync counter, handles buffer swaps,
/// and displays framebuffer at ~60 Hz (~16.7ms per frame)
/// Returns: 0 on normal exit (window closed), 1 on initialization error
int fb_sim_main(void) {
    shm_unlink(SHM_NAME);
    size_t total = shm_total_size();

    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR | O_EXCL, 0666);
    if (fd < 0) { perror("shm_open"); return errno; }
    if (ftruncate(fd, (off_t)total) != 0) { perror("ftruncate"); return errno; }

    uint8_t *base = mmap(NULL, total, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (base == MAP_FAILED) { perror("mmap"); return 1; }

    size_t page = 4096;
    mmio_regs_t *regs = (mmio_regs_t *)(base + 0);
    uint8_t *fb_base  = base + page;

    memset((void*)regs, 0, sizeof(*regs));
    regs->front_idx = 0;
    regs->back_idx  = 1;
    regs->prop_quit = 0;

    int fb = open("/dev/fb0", O_RDWR);
    if (fb < 0) { perror("open"); return 1; }

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;

    ioctl(fb, FBIOGET_FSCREENINFO, &finfo);
    ioctl(fb, FBIOGET_VSCREENINFO, &vinfo);

    long screensize = finfo.line_length * vinfo.yres;

    uint8_t *fbp = mmap(NULL, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0);
    if (fbp == MAP_FAILED) { perror("mmap"); return 1; }

    // SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    // SDL_setenv("SDL_INPUT_LINUX_EVDEV", "1", 1);
    // SDL_setenv("SDL_EVDEV_KBD", "/dev/input/event1", 1);

    struct libevdev *dev = NULL;
    // char kb_path[256];
    //int input_fd = find_keyboard_event(kb_path, sizeof(kb_path));
    int input_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);

    if (input_fd < 0) {
    	printf("No keyboard found\n");
	return 1;
    }

    libevdev_new_from_fd(input_fd, &dev);

    setup_tty();

    // SDL_Window *win = SDL_CreateWindow("hw_sim 720p",
    //     SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, 0);
    // if (!win) { fprintf(stderr, "SDL_CreateWindow failed\n"); return 1; }

    // SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    // if (!ren) { fprintf(stderr, "SDL_CreateRenderer failed\n"); return 1; }

    // SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
    //     SDL_TEXTUREACCESS_STREAMING, W, H);
    // if (!tex) { fprintf(stderr, "SDL_CreateTexture failed\n"); return 1; }
    
	bool running = true;
	const double target_frame_time_sec = 1.0 / 60.0;
	struct timespec prev_frame_time;
	if (clock_gettime(CLOCK_MONOTONIC, &prev_frame_time) != 0) {
		perror("clock_gettime");
		return 1;
	}

	while (running) {
		struct timespec frame_start_time;
		if (clock_gettime(CLOCK_MONOTONIC, &frame_start_time) != 0) {
			perror("clock_gettime");
			break;
		}

		double delta_time_sec =
			(double)(frame_start_time.tv_sec - prev_frame_time.tv_sec) +
			(double)(frame_start_time.tv_nsec - prev_frame_time.tv_nsec) / 1e9;
		if (delta_time_sec < 0.0) {
			delta_time_sec = 0.0;
		}
		prev_frame_time = frame_start_time;

        // const Uint8 *k = SDL_GetKeyboardState(NULL);
        // if (k[SDL_SCANCODE_LEFT]  || k[SDL_SCANCODE_A]) regs->buttons |= BTN_LEFT;
        // if (k[SDL_SCANCODE_RIGHT] || k[SDL_SCANCODE_D]) regs->buttons |= BTN_RIGHT;
        // if (k[SDL_SCANCODE_UP]    || k[SDL_SCANCODE_W]) regs->buttons |= BTN_UP;
        // if (k[SDL_SCANCODE_DOWN]  || k[SDL_SCANCODE_S]) regs->buttons |= BTN_DOWN;
        // if (k[SDL_SCANCODE_SPACE]) regs->buttons |= BTN_FIRE;
        // if (k[SDL_SCANCODE_ESCAPE]) regs->buttons |= BTN_QUIT;
        // if (k[SDL_SCANCODE_P]) regs->buttons |= BTN_PAUSE;
        // if (k[SDL_SCANCODE_R]) regs->buttons |= BTN_RESET;
        // if (k[SDL_SCANCODE_RETURN] || k[SDL_SCANCODE_SPACE]) regs->buttons |= BTN_SEL;
	
	struct input_event ev;
	int rc;

	while ((rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev)) == 0) {
		uint32_t ev_btn = 0;

		printf("Key pressed: %d\t Mode: %d\n", ev.code, ev.value);

		switch (ev.code) {
			case KEY_LEFT:
			case KEY_A:
				printf("Left!\n");
				ev_btn |= 0x1; 
				break;
			case KEY_RIGHT:
			case KEY_D:
				printf("Right!\n");
				ev_btn |= 0x2; 
				break;
			case KEY_W:
				printf("Up!\n");
				ev_btn |= BTN_UP; 
				break;
			case KEY_S:
				printf("Down!\n");
				ev_btn |= BTN_DOWN; 
				break;
			case KEY_SPACE:
				printf("Fire!\n");
				ev_btn |= (BTN_FIRE | BTN_SEL); 
				break;
			case KEY_ESC:
				printf("Quit!\n");
				ev_btn |= BTN_QUIT;
				break;
	        	case KEY_P:
				printf("Pause!\n");
				ev_btn |= BTN_PAUSE;
				break;
			case KEY_R:
				ev_btn |= BTN_RESET;
				break;
			case KEY_ENTER:
				ev_btn |= BTN_SEL;
				break;
			default:
				break;
		}

		if (ev.value == 1 || ev.value == 2) regs->buttons |= ev_btn;
		else if (ev.value == 0) regs->buttons &= ~ev_btn;

		printf("0b%032b\n", regs->buttons);
	}

        if (regs->buttons & BTN_QUIT) running = false;

        regs->vsync_counter++;

        if (regs->swap_request) {
            uint32_t new_front = regs->back_idx % FB_COUNT;
            uint32_t new_back  = regs->front_idx % FB_COUNT;
            regs->front_idx = new_front;
            regs->back_idx  = new_back;
            regs->swap_request = 0;
            regs->swap_ack = regs->vsync_counter;
        }

        uint32_t fi = regs->front_idx % FB_COUNT;
        uint8_t *front = fb_base + (size_t)fi * (size_t)FB_SIZE;

        // SDL_UpdateTexture(tex, NULL, front, W * 4);
        // SDL_RenderClear(ren);
        // SDL_RenderCopy(ren, tex, NULL, NULL);
        // SDL_RenderPresent(ren);
	
	int cw = (W < (int)vinfo.xres) ? W : (int)vinfo.xres;
	int ch = (H < (int)vinfo.yres) ? H : (int)vinfo.yres;

	for (int y = 0; y < ch; y++) {
		uint8_t *src = front + y * W * 4;
		
		for (int i = 0; i < 3; i++) {
			uint16_t *dst = fbp + (3*y+i) * finfo.line_length;
			for (int x = 0; x < cw; x++) {
				for (int j = 0; j < 3; j++) {
  					uint16_t b = src[4*x + 0];
  					uint16_t g = src[4*x + 1];
  					uint16_t r = src[4*x + 2];
  					uint16_t a = src[4*x + 3];
  
  					dst[3*x + j] = ((b >> 3) & 31) | (((g >> 2) & 63) << 5) | (((r >> 3) & 31) << 11);
				}
			}
		}
	}

	if (delta_time_sec < target_frame_time_sec) {
		double sleep_sec = target_frame_time_sec - delta_time_sec;
		struct timespec sleep_time;
		sleep_time.tv_sec = (time_t)sleep_sec;
		sleep_time.tv_nsec = (long)((sleep_sec - (double)sleep_time.tv_sec) * 1e9);
		nanosleep(&sleep_time, NULL);
	}
    }

    restore_tty();

    // SDL_DestroyTexture(tex);
    // SDL_DestroyRenderer(ren);
    // SDL_DestroyWindow(win);
    SDL_Quit();

    munmap(base, total); munmap(fbp, screensize);
    close(fd); close(fb);
    shm_unlink(SHM_NAME);
    return 0;
}

