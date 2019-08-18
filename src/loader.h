#ifndef LOADER_H_
#define LOADER_H_

#include "table.h"
struct loader;
#include "npc.h"
#include "map.h"
#include "d3d.h"
#include <stdio.h>

struct loader {
	table txtrs;
	char *txtrs_dir;
	table npcs;
	char *npcs_dir;
	table maps;
	char *maps_dir;
};

void loader_init(struct loader *ldr, const char *root);

struct npc_type **loader_npc(struct loader *ldr, const char *name, FILE **file);

struct map **loader_map(struct loader *ldr, const char *name, FILE **file);

d3d_texture **loader_texture(struct loader *ldr, const char *name, FILE **file);

void loader_free(struct loader *ldr);

#endif /* LOADER_H_ */
