#include "load-texture.h"
#include "d3d.h"
#include "json-util.h"
#include "map.h"
#include "npc.h"
#include "pixel.h"
#include "xalloc.h"
#include <curses.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PIXEL_ASPECT 0.625
#define FOV_X 2.0

int main(int argc, char *argv[])
{
	initscr();
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
	d3d_board *board = ((struct map *)*table_get(&maps, "columns"))->board;
	d3d_camera *cam = d3d_new_camera(FOV_X,
		LINES * FOV_X / COLS / PIXEL_ASPECT, COLS, LINES);
	*d3d_camera_empty_pixel(cam) = EMPTY_PIXEL;
	d3d_camera_position(cam)->x = 2.4;
	d3d_camera_position(cam)->y = 3.4;
	d3d_draw_walls(cam, board);
	start_color();
	for (int fg = 0; fg < 8; ++fg) {
		for (int bg = 0; bg < 8; ++bg) {
			init_pair((fg << 3 | bg) + 1, fg, bg);
		}
	}
	for (size_t x = 0; x < d3d_camera_width(cam); ++x) {
		for (size_t y = 0; y < d3d_camera_height(cam); ++y) {
			d3d_pixel pix = *d3d_camera_get(cam, x, y);
			int cell = COLOR_PAIR(pix + 1) | '#';
			if (pixel_is_bold(pix)) cell |= A_BOLD;
			mvaddch(y, x, cell);
		}
	}
	refresh();
	getch();
	endwin();
}
