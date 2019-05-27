#include "load-textures.h"
#include "load-texture.h"
#include "d3d.h"
#include "dir-iter.h"
#include "string.h"
#include <stdlib.h>

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
	if (string_pushz(&path, &cap, arg->dirpath)
	 || string_pushc(&path, &cap, '/')
	 || string_pushz(&path, &cap, ent->d_name)
	 || string_pushc(&path, &cap, '\0'))
		goto error;
	d3d_texture *txtr = load_texture(path.text);
	if (!txtr) goto error;
	if (table_add(txtrs, ent->d_name, txtr)) {
		free(txtr);
		goto error;
	}
	return 0;

error:
	free(path.text);
	return -1;
}

int load_textures(const char *dirpath, table *txtrs)
{
	struct texture_iter_arg arg = {
		.txtrs = txtrs,
		.dirpath = dirpath
	};
	if (table_init(txtrs, 32)) return -1;
	if (dir_iter(dirpath, texture_iter, &arg)) {
		table_free(txtrs);
		return -1;
	}
	table_freeze(txtrs);
	return 0;
}
