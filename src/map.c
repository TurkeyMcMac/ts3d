#include "map.h"
#include "dir-iter.h"
#include "grow.h"
#include "json.h"
#include "json-util.h"
#include "string.h"
#include "util.h"
#include "xalloc.h"
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *get_wall(struct map *map, size_t x, size_t y)
{
	return &map->walls[y * d3d_board_width(map->board) + x];
}

static uint8_t get_wall_ck(const struct map *map, long lx, long ly)
{
	size_t x = (size_t)lx, y = (size_t)ly;
	if (x < d3d_board_width(map->board) && y < d3d_board_height(map->board))
	{
		return *get_wall((struct map *)map, x, y);
	} else {
		return 0;
	}
}

static void parse_block(d3d_block_s *block, uint8_t *wall, struct json_node *nd,
	table *txtrs)
{
	union json_node_data *got;
	*wall = 0;
	memset(block, 0, sizeof(*block));
	if (nd->kind != JN_MAP) return;
	bool all_solid = false;
	if ((got = json_map_get(nd, "all_solid", JN_BOOLEAN))) {
		if (got->boolean) {
			*wall = 0xFF;
			all_solid = true;
		}
	}
	struct { const char *txtr, *wall; d3d_direction dir; } faces[] = {
		{"north" , "north_solid", D3D_DNORTH},
		{"south" , "south_solid", D3D_DSOUTH},
		{"east"  , "east_solid" , D3D_DEAST },
		{"west"  , "west_solid" , D3D_DWEST },
		{"top"   , "" /* N/A */ , D3D_DUP   },
		{"bottom", "" /* N/A */ , D3D_DDOWN }
	};
	for (size_t i = 0; i < ARRSIZE(faces); ++i) {
		if ((got = json_map_get(nd, faces[i].txtr, JN_STRING))) {
			const d3d_texture **txtrp =
				(void *)table_get(txtrs, got->str);
			if (txtrp) block->faces[faces[i].dir] = *txtrp;
		}
		if (!all_solid
		 && (got = json_map_get(nd, faces[i].wall, JN_BOOLEAN)))
			if (got->boolean) *wall |= 1 << faces[i].dir;
	}
}

void map_check_walls(struct map *map, d3d_vec_s *pos, double radius)
{
	long x = floor(pos->x), y = floor(pos->y);
	long west, north, east, south;
	west = floor(pos->x - radius);
	north = floor(pos->y - radius);
	east = floor(pos->x + radius);
	south = floor(pos->y + radius);
	uint8_t ns_mask = 1 << D3D_DNORTH | 1 << D3D_DSOUTH;
	uint8_t ew_mask = 1 << D3D_DEAST | 1 << D3D_DWEST;
	uint8_t here = get_wall_ck(map, x, y);
	uint8_t blocked = here;
	if (north < y) {
		blocked |= get_wall_ck(map, x, north) & ew_mask;
	} else if (south > y) {
		blocked |= get_wall_ck(map, x, south) & ew_mask;
	}
	if (west < x) {
		blocked |= get_wall_ck(map, west, y) & ns_mask;
	} else if (east > x) {
		blocked |= get_wall_ck(map, east, y) & ns_mask;
	}
	bool correct_n = false, correct_s = false,
	     correct_w = false, correct_e = false;
	if (north < y) {
		correct_n = bitat(blocked, D3D_DNORTH);
	} else if (south > y) {
		correct_s = bitat(blocked, D3D_DSOUTH);
	}
	if (west < x) {
		correct_w = bitat(blocked, D3D_DWEST);
	} else if (east > x) {
		correct_e = bitat(blocked, D3D_DEAST);
	}
	if (correct_n) {
		if (correct_e) {
			(void)(	(correct_n = bitat(here, D3D_DNORTH))
			&&	(correct_e = bitat(here, D3D_DEAST)));
		} else if (correct_w) {
			(void)(	(correct_n = bitat(here, D3D_DNORTH))
			&&	(correct_w = bitat(here, D3D_DWEST)));
		}
	} else if (correct_s) {
		if (correct_e) {
			(void)(	(correct_s = bitat(here, D3D_DSOUTH))
			&&	(correct_e = bitat(here, D3D_DEAST)));
		} else if (correct_w) {
			(void)(	(correct_s = bitat(here, D3D_DSOUTH))
			&&	(correct_w = bitat(here, D3D_DWEST)));
		}
	}
	if (correct_n) {
		pos->y = y + radius;
	} else if (correct_s) {
		pos->y = y + 1 - radius;
	}
	if (correct_w) {
		pos->x = x + radius;
	} else if (correct_e) {
		pos->x = x + 1 - radius;
	}
}

static uint8_t normalize_wall(const struct map *map, size_t x, size_t y)
{
	uint8_t here = map->walls[y * d3d_board_width(map->board) + x];
	d3d_direction dirs[] = {D3D_DNORTH, D3D_DSOUTH, D3D_DWEST, D3D_DEAST};
	for (size_t i = 0; i < ARRSIZE(dirs); ++i) {
		d3d_direction dir = dirs[i];
		uint8_t bit = 1 << dir;
		if (here & bit) break;
		size_t there_x = x, there_y = y;
		move_direction(dir, &there_x, &there_y);
		if (d3d_board_get(map->board, there_x, there_y)) {
			uint8_t there =
				map->walls[there_y * d3d_board_width(map->board)
				+ there_x];
			if (bitat(there, flip_direction(dir))) here |= bit;
		} else {
			here |= bit;
		}
	}
	return here;
}

bool map_has_wall(const struct map *map, size_t x, size_t y, d3d_direction dir);

int load_map(const char *path, struct map *map, table *npcs, table *txtrs)
{
	struct json_node jtree;
	map->flags = MAP_INVALID;
	map->name = NULL;
	map->board = NULL;
	map->walls = NULL;
	map->blocks = NULL;
	map->npcs = NULL;
	if (parse_json_tree(path, &jtree)) return -1;
	if (jtree.kind != JN_MAP) goto end;
	union json_node_data *got;
	map->flags = 0;
	if ((got = json_map_get(&jtree, "name", JN_STRING))) {
		map->name = got->str;
		got->str = NULL;
	} else {
		map->name = str_dup("");
	}
	uint8_t *walls = NULL;
	size_t n_blocks = 0;
	if ((got = json_map_get(&jtree, "blocks", JN_LIST))) {
		n_blocks = got->list.n_vals;
		walls = xmalloc(n_blocks);
		map->blocks = xmalloc(n_blocks * sizeof(*map->blocks));
		for (size_t i = 0; i < n_blocks; ++i) {
			parse_block(&map->blocks[i], &walls[i],
				&got->list.vals[i], txtrs);
		}
	}
	struct json_node_data_list *layout = NULL;
	if ((got = json_map_get(&jtree, "layout", JN_LIST)))
		layout = &got->list;
	size_t width = 0, height = 0;
	if (layout) {
		height = layout->n_vals;
		for (size_t y = 0; y < height; ++y) {
			if (layout->vals[y].kind != JN_LIST) continue;
			struct json_node_data_list *row =
				&layout->vals[y].d.list;
			if (row->n_vals > width) width = row->n_vals;
		}
	}
	if (layout && width > 0 && height > 0) {
		map->board = d3d_new_board(width, height);
		map->walls = xcalloc(height, width);
		for (size_t y = 0; y < height; ++y) {
			if (layout->vals[y].kind != JN_LIST) continue;
			struct json_node_data_list *row =
				&layout->vals[y].d.list;
			for (size_t x = 0; x < row->n_vals; ++x) {
				if (row->vals[x].kind != JN_NUMBER) continue;
				size_t idx = row->vals[x].d.num;
				if (idx >= n_blocks) continue;
				*d3d_board_get(map->board, x, y) =
					&map->blocks[idx];
				*get_wall(map, x, y) = walls[idx];
			}
		}
		for (size_t y = 0; y < height; ++y) {
			for (size_t x = 0; x < width; ++x) {
				*get_wall(map, x, y) =
					normalize_wall(map, x, y);
			}
		}
	} else {
		map->board = d3d_new_board(1, 1);
		map->walls = xcalloc(1, 1);
	}
	free(walls);
end:
	free_json_tree(&jtree);
	return 0;
}

struct map_iter_arg {
	table *maps;
	table *npcs;
	table *txtrs;
	const char *dirpath;
};

static int map_iter(struct dirent *ent, void *ctx)
{
	int retval = -1;
	struct map_iter_arg *arg = ctx;
	table *maps = arg->maps;
	table *npcs = arg->npcs;
	table *txtrs = arg->txtrs;
	size_t cap = 0;
	struct string path = {0};
	char *suffix = strstr(ent->d_name, ".json");
	if (!suffix) return 0;
	string_pushz(&path, &cap, arg->dirpath);
	string_pushc(&path, &cap, '/');
	string_pushz(&path, &cap, ent->d_name);
	string_pushc(&path, &cap, '\0');
	*suffix = '\0';
	struct map *map = xmalloc(sizeof(*map));
	if (load_map(path.text, map, npcs, txtrs)) goto error_load_map;
	if (map->flags & MAP_INVALID) {
		retval = 0;
		goto invalid_map;
	}
	table_add(maps, str_dup(ent->d_name), map);
	return 0;

invalid_map:
	map_free(map);
error_load_map:
	free(map);
	free(path.text);
	return retval;
}

static int free_map_entry(const char *key, void **val)
{
	free((char *)key);
	map_free(*val);
	free(*val);
	return 0;
}

int load_maps(const char *dirpath, table *maps, table *npcs, table *txtrs)
{
	struct map_iter_arg arg = {
		.maps = maps,
		.npcs = npcs,
		.txtrs = txtrs,
		.dirpath = dirpath
	};
	table_init(maps, 32);
	if (dir_iter(dirpath, map_iter, &arg)) {
		table_each(npcs, free_map_entry);
		table_free(maps);
		return -1;
	}
	table_freeze(maps);
	return 0;
}

char *map_to_string(const struct map *map)
{
	char *str = xmalloc(128);
	snprintf(str, 128, "map { width = %zu, height = %zu }",
		d3d_board_width(map->board), d3d_board_height(map->board));
	return str;
}

void map_free(struct map *map)
{
	free(map->name);
	d3d_free_board(map->board);
	free(map->walls);
	free(map->blocks);
	free(map->npcs);
}
