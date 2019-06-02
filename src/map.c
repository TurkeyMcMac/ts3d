#include "map.h"
#include "dir-iter.h"
#include "grow.h"
#include "json.h"
#include "json-util.h"
#include "string.h"
#include "util.h"
#include "xalloc.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#define FACE(dir, name) \
	if ((got = json_map_get(nd, name, JN_STRING))) { \
		const d3d_texture **txtrp = (void *)table_get(txtrs, got->str);\
		if (txtrp) block->faces[dir] = *txtrp; \
	} \
	if (!all_solid && (got = json_map_get(nd, name"_solid", JN_BOOLEAN))) \
		if (got->boolean) *wall |= 1 << dir;
	FACE(D3D_DNORTH, "north");
	FACE(D3D_DSOUTH, "south");
	FACE(D3D_DEAST, "east");
	FACE(D3D_DWEST, "west");
	FACE(D3D_DUP, "top");
	FACE(D3D_DDOWN, "bottom");
#undef FACE
}

int map_get_wall(const struct map *map, size_t x, size_t y)
{
	if (!d3d_board_get(map->board, x, y)) return -1;
	uint8_t here = map->walls[y * d3d_board_width(map->board) + x];
#define IN_DIRECTION(dir) do { \
	uint8_t bit = 1 << dir; \
	if (here & bit) break; \
	size_t there_x = x, there_y = y; \
	move_direction(dir, &there_x, &there_y); \
	if (d3d_board_get(map->board, there_x, there_y)) { \
		uint8_t there = \
			map->walls[there_y * d3d_board_width(map->board) \
			+ there_x]; \
		if (there & (1 << flip_direction(dir))) here |= bit; \
	} else { \
		here |= bit; \
	} \
} while (0)
	IN_DIRECTION(D3D_DNORTH);
	IN_DIRECTION(D3D_DSOUTH);
	IN_DIRECTION(D3D_DWEST);
	IN_DIRECTION(D3D_DEAST);
#undef IN_DIRECTION
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
				map->walls[y * width + x] = walls[idx];
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
