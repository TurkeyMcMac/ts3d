#include "loader.h"
#include "string.h"
#include "xalloc.h"
#include <stdlib.h>
#include <string.h>

void loader_init(struct loader *ldr, const char *root)
{
	const char *txtrs_rel = "textures";
	const char *npcs_rel = "npcs";
	const char *maps_rel = "maps";
	size_t root_len = strlen(root);
	size_t buf_cap = root_len;
	struct string buf;
	string_init(&buf, buf_cap);
	string_pushn(&buf, &buf_cap, root, root_len);
	string_pushn(&buf, &buf_cap, txtrs_rel, strlen(txtrs_rel) + 1);
	ldr->txtrs_dir = buf.text;
	table_init(&ldr->txtrs, 16);
	string_init(&buf, buf_cap);
	string_pushn(&buf, &buf_cap, root, root_len);
	string_pushn(&buf, &buf_cap, npcs_rel, strlen(npcs_rel) + 1);
	ldr->npcs_dir = buf.text;
	table_init(&ldr->npcs, 16);
	string_init(&buf, buf_cap);
	string_pushn(&buf, &buf_cap, root, root_len);
	string_pushn(&buf, &buf_cap, maps_rel, strlen(maps_rel) + 1);
	ldr->maps_dir = buf.text;
	table_init(&ldr->maps, 16);
}

static void **load(table *tab, const char *root, const char *name, FILE **file)
{
	void **item = table_get(tab, name);
	*file = NULL;
	if (!item) {
		size_t root_len = strlen(root), name_len = strlen(name);
		size_t path_len = root_len + name_len + 1;
		char *path = xmalloc(path_len + 1);
		memcpy(path, root, root_len);
		path[root_len] = '/';
		memcpy(path + root_len + 1, name, name_len + 1);
		*file = fopen(path, "r");
		if (*file) {
			table_add(tab, name, NULL);
			item = table_get(tab, name); // TODO: more efficient?
		}
		free(path);
	}
	return item;
}

struct npc_type **loader_npc(struct loader *ldr, const char *name, FILE **file)
{
	return (struct npc_type **)load(&ldr->npcs, ldr->npcs_dir, name, file);
}

struct map **loader_map(struct loader *ldr, const char *name, FILE **file)
{
	return (struct map **)load(&ldr->maps, ldr->maps_dir, name, file);
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
