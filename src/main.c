#include "load-texture.h"
#include "d3d.h"
#include "json-util.h"
#include "map.h"
#include "npc.h"
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
	table txtrs, npcs, maps;
	d3d_malloc = xmalloc;
	d3d_realloc = xrealloc;
	load_textures("data/textures", &txtrs);
	load_npc_types("data/npcs", &npcs, &txtrs);
	load_maps("data/maps", &maps, &npcs, &txtrs);
	struct map **mapp = (void *)table_get(&maps, argv[1]);
	if (!mapp) {
		fprintf(stderr, "%s: map '%s' not found\n", argv[0], argv[1]);
		exit(EXIT_FAILURE);
	}
	struct map *map = *mapp;
	d3d_sprite_s *sprites = xmalloc(map->n_npcs * sizeof(*sprites));
	long *durations = xcalloc(map->n_npcs, sizeof(*durations));
	for (size_t i = 0; i < map->n_npcs; ++i) {
		d3d_sprite_s *sp = &sprites[i];
		const struct npc_type *npc = map->npcs[i].type;
		sp->pos = map->npcs[i].pos;
		sp->scale.x = npc->width;
		sp->scale.y = npc->height;
		sp->txtr = npc->frames[0].txtr;
		sp->transparent = npc->transparent;
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
		d3d_draw_sprites(cam, map->n_npcs, sprites);
		for (size_t i = 0; i < map->n_npcs; ++i) {
			if (durations[i] == 0) {
				size_t *frame = &map->npcs[i].frame;
				durations[i] = map->npcs[i].type
					->frames[*frame].duration;
				sprites[i].txtr = map->npcs[i].type
					->frames[(*frame)++].txtr;
				*frame %= map->npcs[i].type->n_frames;
			} else {
				--durations[i];
			}
			d3d_vec_s *spos = &sprites[i].pos;
			d3d_vec_s disp = {spos->x - pos->x, spos->y - pos->y};
			double dist = hypot(disp.x, disp.y);
			disp.x /= dist * -400;
			disp.y /= dist * -400;
			d3d_vec_s move = *spos;
			map_check_walls(map, &move, CAM_RADIUS);
			disp.x += move.x - spos->x;
			disp.y += move.y - spos->y;
			dist = hypot(disp.x, disp.y);
			disp.x /= dist * 250;
			disp.y /= dist * 250;
			spos->x += disp.x;
			spos->y += disp.y;
		}
		display_frame(cam);
		tick(&timer);
	}
}
