#include "load-texture.h"
#include "d3d.h"
#include "json-util.h"
#include "map.h"
#include "npc.h"
#include "pixel.h"
#include "util.h"
#include "xalloc.h"
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
	if (argc < 4) {
		fprintf(stderr, "Usage: %s maps npcs textures\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	const char *txtrs_path = argv[3];
	const char *npcs_path = argv[2];
	const char *maps_path = argv[1];
	table txtrs, npcs, maps;
	d3d_malloc = xmalloc;
	d3d_realloc = xrealloc;
	load_textures(txtrs_path, &txtrs);
	load_npc_types(npcs_path, &npcs, &txtrs);
	load_maps(maps_path, &maps, &npcs, &txtrs);
	struct map *map = *table_get(&maps, "columns");
	d3d_board *board = map->board;
	initscr();
	atexit(end_win);
	d3d_camera *cam = d3d_new_camera(FOV_X,
		LINES * FOV_X / COLS / PIXEL_ASPECT, COLS, LINES);
	*d3d_camera_empty_pixel(cam) = EMPTY_PIXEL;
	d3d_camera_position(cam)->x = 4.4;
	d3d_camera_position(cam)->y = 3.4;
	*d3d_camera_facing(cam) = 3.4;
	start_color();
	for (int fg = 0; fg < 8; ++fg) {
		for (int bg = 0; bg < 8; ++bg) {
			init_pair((fg << 3 | bg) + 1, fg, bg);
		}
	}
	for (;;) {
		d3d_draw_walls(cam, board);
		display_frame(cam);
		// The straight-forward player movement angle:
		double move_angle = *d3d_camera_facing(cam);
		switch (getch()) {
		case 'w': // Forward
			break;
		case 'a': // Left
			move_angle += M_PI / 2;
			break;
		case 's': // Backward
			move_angle += M_PI;
			break;
		case 'd': // Right
			move_angle -= M_PI / 2;
			break;
		case 'q': // Turn left
			*d3d_camera_facing(cam) += 0.04;
			continue;
		case 'e': // Turn right
			*d3d_camera_facing(cam) -= 0.04;
			continue;
		case 'x': // Quit the game
			exit(0);
		default: // Other keys are ignored
			continue;
		}
		d3d_vec_s *cam_pos = d3d_camera_position(cam);
		d3d_vec_s old_pos = *cam_pos;
		cam_pos->x += cos(move_angle) * 0.04;
		cam_pos->y += sin(move_angle) * 0.04;
		size_t x = floor(cam_pos->x), y = floor(cam_pos->y);
		int wall = map_get_wall(map, x, y);
		if (wall >= 0) {
			double tilex = fmod(cam_pos->x, 1);
			double tiley = fmod(cam_pos->y, 1);
			if (bitat(wall, D3D_DNORTH) && tiley < CAM_RADIUS)
				cam_pos->y = y + CAM_RADIUS;
			if (bitat(wall, D3D_DSOUTH) && tiley > 1 - CAM_RADIUS)
				cam_pos->y = y + 1 - CAM_RADIUS;
			if (bitat(wall, D3D_DWEST) && tilex < CAM_RADIUS)
				cam_pos->x = x + CAM_RADIUS;
			if (bitat(wall, D3D_DEAST) && tilex > 1 - CAM_RADIUS)
				cam_pos->x = x + 1 - CAM_RADIUS;
		} else {
			*cam_pos = old_pos;
		}
	}
}
