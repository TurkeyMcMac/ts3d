#include "load-texture.h"
#include "d3d.h"
#include "json-util.h"
#include "map.h"
#include "ent.h"
#include "pixel.h"
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

#define FORWARD_COEFF 0.05
#define BACKWARD_COEFF 0.03
#define TURN_COEFF 0.055
#define SIDEWAYS_COEFF 0.04

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
	size_t n_ents = map->n_ents;
	d3d_sprite_s *sprites = xmalloc(n_ents * sizeof(*sprites));
	struct ent *ents = xmalloc(n_ents * sizeof(*ents));
	for (size_t i = 0; i < n_ents; ++i) {
		struct ent_type *type = map->ents[i].type;
		ent_init(&ents[i], type, &sprites[i], &map->ents[i].pos);
	}
	d3d_board *board = map->board;
	initscr();
	atexit(end_win);
	d3d_camera *cam = d3d_new_camera(FOV_X,
		LINES * FOV_X / COLS / PIXEL_ASPECT, COLS, LINES);
	d3d_vec_s *pos = d3d_camera_position(cam);
	pos->x = 3.5;
	pos->y = 3.5;
	start_color();
	for (int fg = 0; fg < 8; ++fg) {
		for (int bg = 0; bg < 8; ++bg) {
			init_pair((fg << 3 | bg) + 1, fg, bg);
		}
	}
	struct ticker timer;
	ticker_init(&timer, 15);
	double *facing = d3d_camera_facing(cam);
	int key;
	*facing = M_PI / 2;
	timeout(0);
	while ((key = getch()) != 'x') {
		switch (tolower(key)) {
			double sideway;
		case 'w': // Forward
			pos->x += FORWARD_COEFF * cos(*facing);
			pos->y += FORWARD_COEFF * sin(*facing);
			break;
		case 'q': // Turn CCW
			*facing += TURN_COEFF;
			break;
		case 's': // Backward
			pos->x -= BACKWARD_COEFF * cos(*facing);
			pos->y -= BACKWARD_COEFF * sin(*facing);
			break;
		case 'e': // Turn CW
			*facing -= TURN_COEFF;
			break;
		case 'a': // Left
			sideway = *facing + M_PI / 2;
			pos->x += SIDEWAYS_COEFF * cos(sideway);
			pos->y += SIDEWAYS_COEFF * sin(sideway);
			break;
		case 'd': // Right
			sideway = *facing - M_PI / 2;
			pos->x += SIDEWAYS_COEFF * cos(sideway);
			pos->y += SIDEWAYS_COEFF * sin(sideway);
			break;
		}
		map_check_walls(map, pos, CAM_RADIUS);
		d3d_draw_walls(cam, board);
		d3d_draw_sprites(cam, n_ents, sprites);
		for (size_t i = 0; i < n_ents; ++i) {
			ent_tick(&ents[i]);
			d3d_vec_s *epos = ent_pos(&ents[i]);
			d3d_vec_s *evel = ent_vel(&ents[i]);
			epos->x += evel->x;
			epos->y += evel->y;
			d3d_vec_s disp;
			double dist;
			if (ents[i].type->turn_chance > rand()) {
				disp.x = epos->x - pos->x;
				disp.y = epos->y - pos->y;
				dist = hypot(disp.x, disp.y) /
					-ents[i].type->speed;
				disp.x /= dist;
				disp.y /= dist;
			} else {
				disp.x = disp.y = 0;
			}
			d3d_vec_s move = *epos;
			map_check_walls(map, &move, CAM_RADIUS);
			disp.x += move.x - epos->x;
			disp.y += move.y - epos->y;
			*epos = move;
			if (disp.x != 0.0) evel->x = disp.x;
			if (disp.y != 0.0) evel->y = disp.y;
		}
		display_frame(cam);
		for (size_t i = 0; i < n_ents; ++i) {
			if (ent_is_dead(&ents[i])) {
				--n_ents;
				ent_destroy(&ents[i]);
				ent_relocate(&ents[n_ents], &ents[i], &sprites[i]);
			}
		}
		tick(&timer);
	}
	d3d_free_camera(cam);
	free(ents);
	free(sprites);
	loader_free(&ldr);
}
