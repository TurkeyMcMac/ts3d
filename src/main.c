#include "load-texture.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
	const char *path = argv[1];
	d3d_texture *txtr = load_texture(path);
	if (!txtr) {
		fprintf(stderr, "Error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	printf("Texture from %s:\n", path);
	for (size_t y = 0; y < d3d_texture_height(txtr); ++y) {
		for (size_t x = 0; x < d3d_texture_width(txtr); ++x) {
			putchar(*d3d_texture_get(txtr, x, y));
		}
		putchar('\n');
	}
}
