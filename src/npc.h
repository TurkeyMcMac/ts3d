#ifndef NPC_H_
#define NPC_H_

#include "d3d.h"
#include "table.h"

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
	const d3d_texture **frames;
	// The flags. See NPC_INVALID, etc. below.
	int flags;
};

// The valid JSON was not a valid NPC
#define NPC_INVALID (1 << 0)
// A texture referenced was not found in the table. Always accompanied by
// NPC_INVALID.
#define NPC_INVALID_TEXTURE (1 << 1)

// Load an npc type from a file path, returning negative on system error. The
// texture pointers in the txtrs table may be referenced by the new type.
int load_npc_type(const char *path, struct npc_type *npc, table *txtrs);

// Like load_npc_type, but loads from all the files in the given directory. The
// NPC type structures are allocated and put in the table npcs. Negative is
// returned on fatal system errors.
int load_npc_types(const char *dirpath, table *npcs, table *txtrs);

// Allocate a string representation for debugging.
char *npc_type_to_string(const struct npc_type *npc);

// Free a loaded NPC type.
void npc_type_free(struct npc_type *npc);

#endif /* NPC_H_ */
