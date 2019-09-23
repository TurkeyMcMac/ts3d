#include "load-texture.h"
#include "d3d.h"
#include "json-util.h"
#include "map.h"
#include "ent.h"
#include "pixel.h"
#include "sim-worker.h"
#include "ticker.h"
#include "util.h"
#include "xalloc.h"
#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#	define M_PI 3.14159265358979323846
#endif

#define CAM_RADIUS 0.1

#define PIXEL_ASPECT 0.625
#define FOV_X 2.0

#define FORWARD_COEFF 0.025
#define BACKWARD_COEFF 0.015
#define TURN_COEFF 0.055
#define SIDEWAYS_COEFF 0.02

void display_frame(d3d_camera *cam)
{
	for (size_t x = 0; x < d3d_camera_width(cam); ++x) {
		for (size_t y = 0; y < d3d_camera_height(cam); ++y) {
			d3d_pixel pix = *d3d_camera_get(cam, x, y);
			int cell = COLOR_PAIR(pix + 1) | '#';
			if (pixel_is_bold(pix)) cell |= A_BOLD;
			mvaddch(y, x, cell);
		}
	}
	refresh();
}

void end_win(void) { endwin(); }

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s map\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	d3d_malloc = xmalloc;
	d3d_realloc = xrealloc;
	struct loader ldr;
	loader_init(&ldr, "data");
	struct map *map = load_map(&ldr, argv[1]);
	if (!map) {
		logger_printf(loader_logger(&ldr), LOGGER_ERROR,
			"Failed to load map \"%s\"\n", argv[1]);
		loader_free(&ldr);
		exit(EXIT_FAILURE);
	}
	loader_print_summary(&ldr);
	srand(time(NULL)); // For random_start_frame
	d3d_board *board = map->board;
	initscr();
	atexit(end_win);
	d3d_camera *cam = d3d_new_camera(FOV_X,
		LINES * FOV_X / COLS / PIXEL_ASPECT, COLS, LINES);
	start_color();
	for (int fg = 0; fg < 8; ++fg) {
		for (int bg = 0; bg < 8; ++bg) {
			init_pair((fg << 3 | bg) + 1, fg, bg);
		}
	}
	struct sim_worker sim;
	sim_worker_init(&sim, cam, map);
	struct ticker timer;
	ticker_init(&timer, 15);
	int key;
	timeout(0);
	while ((key = tolower(getch())) != 'x') {
		sim_worker_start_tick(&sim, key);
		sim_worker_finish_tick(&sim);
		display_frame(cam);
		tick(&timer);
	}
	d3d_free_camera(cam);
	loader_free(&ldr);
}
