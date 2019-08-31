#ifndef NPC_H_
#define NPC_H_

#include "d3d.h"
#include "loader.h"
#include "table.h"

struct npc_frame {
	// The texture displayed.
	const d3d_texture *txtr;
	// Time where frame is visible, in game ticks.
	long duration;
};

struct npc_type {
	// The allocated type name.
	char *name;
	// The width of the NPC in blocks.
	double width;
	// The height of the NPC in blocks.
	double height;
	// The transparent pixel in the frames, or -1 for none.
	int transparent;
	// The number of frames.
	size_t n_frames;
	// The allocated list of n_frames frames.
	struct npc_frame *frames;
	// NPC to spawn on death.
	struct npc_type *death_spawn;
	// Ticks to stay alive, or -1 for forever.
	long lifetime;
};

// Load an NPC with the name or use one previously loaded. Allocate the NPC.
struct npc_type *load_npc_type(struct loader *ldr, const char *name);

// Allocate a string representation for debugging.
char *npc_type_to_string(const struct npc_type *npc);

// Free a loaded NPC type. Does nothing when given NULL.
void npc_type_free(struct npc_type *npc);

#endif /* NPC_H_ */
