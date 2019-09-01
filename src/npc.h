#ifndef NPC_H_
#define NPC_H_

#include "d3d.h"
#include "loader.h"
#include "table.h"
#include <stdbool.h>

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

struct npc {
	struct npc_type *type;
	d3d_sprite_s *sprite;
	long lifetime;
	size_t frame;
	long frame_duration;
};

// Initialize an NPC. The sprite will be owned and manipulated by it.
void npc_init(struct npc *npc, struct npc_type *type, d3d_sprite_s *sprite,
	const d3d_vec_s *pos);

// Go forward a tick of the normal lifecycle. Does not move the NPC.
void npc_tick(struct npc *npc);

// Move an NPC in memory. Its sprite will move to the given location. The
// destinations will be overwritten and the source will become effectively
// uninitialized.
void npc_relocate(struct npc *npc, struct npc *to_npc, d3d_sprite_s *to_sprite);

// Returns whether or not the NPC is dead and should be removed.
bool npc_is_dead(const struct npc *npc);

// Clean up NPC resources. Does not free the pointer.
void npc_destroy(struct npc *npc);

#endif /* NPC_H_ */
