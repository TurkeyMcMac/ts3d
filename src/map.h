#ifndef MAP_H_
#define MAP_H_

#include "d3d.h"
#include "loader.h"
#include "npc.h"
#include <stdbool.h>
#include <stdint.h>

// Not yet used.
struct map_npc_start {
	d3d_vec_s pos;
	const struct npc_type *type;
	size_t frame;
};

struct map {
	// The allocated map name.
	char *name;
	// The board of visual blocks from the blocks array.
	d3d_board *board;
	// The grid of the same width and height as the board, documenting the
	// walls. If the bit (1 << direction) is set in a tile, there is a wall
	// in that direction.
	uint8_t *walls;
	// The blocks used in the board. These refer to textures in the table
	// passed to load_map(s).
	d3d_block_s *blocks;
	// The starting position of the player on the board (in blocks).
	d3d_vec_s player_pos;
	// The starting direction of the player on the board (in radians).
	double player_facing;
	// The starting number of NPCs.
	size_t n_npcs;
	// The list of NPC types and corresponding starting positions. This
	// refers to data in the npcs table passed to load_map(s).
	struct map_npc_start *npcs;
	// See MAP_INVALID, etc. below.
	int flags;
};

// Set in a map returned by load_map if the JSON was valid but could not
// adequately describe a map.
#define MAP_INVALID 0x01

// Move an object's position so as not to conflict with the map's walls. The
// object's malleable position is stored in pos. The object is a square with
// side length (2 * radius).
void map_check_walls(struct map *map, d3d_vec_s *pos, double radius);

// Load a map with the name or use one previously loaded. Allocate the map.
struct map *load_map(struct loader *ldr, const char *name);

// Convert a map to an allocated string for debugging.
char *map_to_string(const struct map *map);

// Free a loaded map. Does nothing when given NULL.
void map_free(struct map *map);

#endif /* MAP_H_ */
