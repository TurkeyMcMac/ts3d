#ifndef LOADER_H_
#define LOADER_H_

#include "table.h"
struct loader;
#include "npc.h"
#include "map.h"
#include "d3d.h"
#include "logger.h"
#include <stdio.h>

// An object for loading game resources recursively. The fields are private.
struct loader {
	table txtrs;
	char *txtrs_dir;
	table npcs;
	char *npcs_dir;
	table maps;
	char *maps_dir;
	struct logger log;
};

// Initialize a loader with the given data root directory. If the directory is
// invalid, you are currently told later when you try to load an item.
void loader_init(struct loader *ldr, const char *root);

// The following three functions load a named item. If the name is invalid or
// there was an error (in errno), NULL is returned. If the item was already
// loaded, a double pointer to it is returned. If the item can be loaded, file
// is set to the file from which it can be loaded and a pointer to be later set
// by the user to the parsed item is returned.

struct npc_type **loader_npc(struct loader *ldr, const char *name, FILE **file);

struct map **loader_map(struct loader *ldr, const char *name, FILE **file);

d3d_texture **loader_texture(struct loader *ldr, const char *name, FILE **file);

// Get a pointer to a loader's logger. Initially, this will have the default
// settings.
struct logger *loader_logger(struct loader *ldr);

// Free a loader, its logger, and the items it has loaded.
void loader_free(struct loader *ldr);

#endif /* LOADER_H_ */