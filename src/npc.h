#ifndef NPC_H_
#define NPC_H_

#include "d3d.h"
#include "table.h"

struct npc_type {
	char *name;
	double width;
	double height;
	int transparent;
	size_t n_frames;
	const d3d_texture **frames;
	int flags;
};

#define NPC_INVALID (1 << 0)
#define NPC_INVALID_TEXTURE (1 << 1)

int load_npc_type(const char *path, struct npc_type *npc, table *txtrs);

char *npc_type_to_string(const struct npc_type *npc);

#endif /* NPC_H_ */
