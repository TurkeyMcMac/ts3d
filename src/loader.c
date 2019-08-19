#include "loader.h"
#include "string.h"
#include "util.h"
#include "xalloc.h"
#include <stdlib.h>
#include <string.h>

void loader_init(struct loader *ldr, const char *root)
{
	ldr->txtrs_dir = mid_cat(root, '/', "textures");
	table_init(&ldr->txtrs, 16);
	ldr->npcs_dir = mid_cat(root, '/', "npcs");
	table_init(&ldr->npcs, 16);
	ldr->maps_dir = mid_cat(root, '/', "maps");
	table_init(&ldr->maps, 16);
}

static void **load(table *tab, const char *root, const char *name, FILE **file)
{
	void **item = table_get(tab, name);
	*file = NULL;
	if (!item) {
		char *path = mid_cat(root, '/', name);
		*file = fopen(path, "r");
		if (*file) {
			table_add(tab, name, NULL);
			item = table_get(tab, name); // TODO: more efficient?
		}
		free(path);
	}
	return item;
}

// Load with .json suffix
static void **loadj(table *tab, const char *root, const char *name, FILE **file)
{
	char *fname = mid_cat(name, '.', "json");
	return load(tab, root, fname, file);
}

struct npc_type **loader_npc(struct loader *ldr, const char *name, FILE **file)
{
	return (struct npc_type **)loadj(&ldr->npcs, ldr->npcs_dir, name, file);
}

struct map **loader_map(struct loader *ldr, const char *name, FILE **file)
{
	return (struct map **)loadj(&ldr->maps, ldr->maps_dir, name, file);
}

d3d_texture **loader_texture(struct loader *ldr, const char *name, FILE **file)
{
	return (d3d_texture **)load(&ldr->txtrs, ldr->txtrs_dir, name, file);
}

void loader_free(struct loader *ldr)
{
	table_free(&ldr->txtrs);
	free(ldr->txtrs_dir);
	table_free(&ldr->npcs);
	free(ldr->npcs_dir);
	table_free(&ldr->maps);
	free(ldr->maps_dir);
}
