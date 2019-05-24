#include "load-textures.h"
#include "d3d.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *texture_to_string(void *data)
{
	d3d_texture *txtr = data;
	char *str = malloc(64);
	if (!str) return NULL;
	snprintf(str, 64, "texture { width = %lu, height = %lu }",
		d3d_texture_width(txtr), d3d_texture_height(txtr));
	return str;
}

int main(int argc, char *argv[])
{
	table txtrs;
	const char *path = argv[1];
	if (load_textures(path, &txtrs)) {
		fprintf(stderr, "Error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	printf("Textures from %s:\n%s\n", path,
		table_to_string(&txtrs, texture_to_string));
}
