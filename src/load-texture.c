#include "load-texture.h"
#include "read-lines.h"
#include "dir-iter.h"
#include "string.h"
#include <stdlib.h>

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

struct texture_iter_arg {
	table *txtrs;
	const char *dirpath;
};

static int texture_iter(struct dirent *ent, void *ctx)
{
	struct texture_iter_arg *arg = ctx;
	table *txtrs = arg->txtrs;
	size_t cap = 0;
	struct string path = {0};
	string_pushz(&path, &cap, arg->dirpath);
	string_pushc(&path, &cap, '/');
	string_pushz(&path, &cap, ent->d_name);
	string_pushc(&path, &cap, '\0');
	d3d_texture *txtr = load_texture(path.text);
	if (!txtr) {
		free(path.text);
		return -1;
	}
	table_add(txtrs, ent->d_name, txtr);
	return 0;
}

int load_textures(const char *dirpath, table *txtrs)
{
	struct texture_iter_arg arg = {
		.txtrs = txtrs,
		.dirpath = dirpath
	};
	table_init(txtrs, 32);
	if (dir_iter(dirpath, texture_iter, &arg)) {
		table_free(txtrs);
		return -1;
	}
	table_freeze(txtrs);
	return 0;
}
