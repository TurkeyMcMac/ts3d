#ifndef LOADER_H_
#define LOADER_H_

#include "table.h"

struct loader {
	table txtrs;
	char *txtrs_dir;
	table npcs;
	char *npcs_dir;
	table maps;
	char *maps_dir;
};

void loader_init(struct loader *ldr, const char *root);

void loader_free(struct loader *ldr);

#endif /* LOADER_H_ */
