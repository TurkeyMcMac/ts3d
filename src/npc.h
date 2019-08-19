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
	// The flags. See NPC_INVALID, etc. below.
	int flags;
};

// The valid JSON was not a valid NPC
#define NPC_INVALID (1 << 0)
// A texture referenced was not found in the table. Always accompanied by
// NPC_INVALID.
#define NPC_INVALID_TEXTURE (1 << 1)

// TODO: doc
struct npc_type *load_npc_type(struct loader *ldr, const char *name);

// Allocate a string representation for debugging.
char *npc_type_to_string(const struct npc_type *npc);

// Free a loaded NPC type.
void npc_type_free(struct npc_type *npc);

#endif /* NPC_H_ */
