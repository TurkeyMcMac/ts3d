#include "load-texture.h"
#include "read-lines.h"

d3d_texture *load_texture(const char *path)
{
	d3d_texture *txtr;
	size_t width;
	size_t height;
	struct string *lines = read_lines(path, &height);
	if (!lines) return NULL;
	width = 0;
	for (size_t y = 0; y < height; ++y) {
		if (lines[y].len > width) width = lines[y].len;
	}
	if (width > 0 && height > 0) {
		txtr = d3d_new_texture(width, height);
		if (!txtr) return NULL;
		for (size_t y = 0; y < height; ++y) {
			struct string *line = &lines[y];
			size_t x;
			for (x = 0; x < line->len; ++x) {
				*d3d_texture_get(txtr, x, y) = line->text[x];
			}
			for (; x < width; ++x) {
				*d3d_texture_get(txtr, x, y) = ' ';
			}
		}
	} else {
		txtr = d3d_new_texture(1, 1);
		if (!txtr) return NULL;
		*d3d_texture_get(txtr, 0, 0) = ' ';
	}
	return txtr;
}
