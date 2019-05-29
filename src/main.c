#include "load-texture.h"
#include "d3d.h"
#include "json-util.h"
#include "npc.h"
#include "xalloc.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *texture_to_string(void *data)
{
	d3d_texture *txtr = data;
	char *str = xmalloc(64);
	snprintf(str, 64, "texture { width = %lu, height = %lu }",
		d3d_texture_width(txtr), d3d_texture_height(txtr));
	return str;
}

int main(int argc, char *argv[])
{
	if (argc < 3) {
		fprintf(stderr, "Usage: %s textures npcs\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	const char *txtrs_path = argv[1];
	const char *npcs_path = argv[2];
	table txtrs, npcs;
	d3d_malloc = xmalloc;
	d3d_realloc = xrealloc;
	create_json_key_tab();
	if (load_textures(txtrs_path, &txtrs)) {
		fprintf(stderr, "Error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	printf("Textures from %s:\n%s\n", txtrs_path,
		table_to_string(&txtrs, texture_to_string));
	if (load_npc_types(npcs_path, &npcs, &txtrs)) {
		fprintf(stderr, "Error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	printf("NPC types from %s:\n%s\n", npcs_path,
		table_to_string(&npcs, (char *(*)(void *))npc_type_to_string));

	free_json_key_tab();
}
