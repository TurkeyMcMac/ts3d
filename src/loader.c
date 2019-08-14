#include "loader.h"
#include "string.h"
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

void loader_free(struct loader *ldr)
{
	table_free(&ldr->txtrs);
	free(ldr->txtrs_dir);
	table_free(&ldr->npcs);
	free(ldr->npcs_dir);
	table_free(&ldr->maps);
	free(ldr->maps_dir);
}
