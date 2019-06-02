#ifndef MAP_H_
#define MAP_H_

#include "d3d.h"
#include "npc.h"
#include <stdbool.h>
#include <stdint.h>

struct map_npc_start {
	d3d_vec_s pos;
	const struct npc_type *type;
};

struct map {
	char *name;
	d3d_board *board;
	uint8_t *walls;
	d3d_block_s *blocks;
	d3d_vec_s player_pos;
	double player_facing;
	struct map_npc_start *npcs;
	int flags;
};

#define MAP_INVALID 0x01

int map_get_wall(const struct map *map, size_t x, size_t y);

bool map_has_wall(const struct map *map, size_t x, size_t y, d3d_direction dir);

int load_map(const char *path, struct map *map, table *npcs, table *txtrs);

int load_maps(const char *dirpath, table *maps, table *npcs, table *txtrs);

char *map_to_string(const struct map *map);

void map_free(struct map *map);

#endif /* MAP_H_ */
