#include "load-textures.h"
#include "load-texture.h"
#include "d3d.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

int load_textures(const char *dirpath, table *txtrs)
{
	int errnum;
	DIR *dir = opendir(dirpath);
	if (!dir) goto error_opendir;
	struct dirent *ent;
	size_t path_len = 0;
	size_t path_cap = 32;
	char *path = malloc(path_cap);
	if (!path) goto error_malloc;
	table_init(txtrs, 16);
	errno = 0;
	while ((ent = readdir(dir))) {
		if (*ent->d_name == '.') continue;
		while ((path_len = snprintf(path, path_cap, "%s/%s", dirpath,
				ent->d_name) + 1) > path_cap) {
			path_cap = path_len;
			path = realloc(path, path_cap);
			if (!path) goto error_realloc;
		}
		d3d_texture *txtr = load_texture(path);
		if (!txtr) goto error_load_texture;
		if (table_add(txtrs, ent->d_name, txtr)) goto error_table_add;
	}
	if (errno) goto error_readdir;
	free(path);
	table_freeze(txtrs);
	return 0;

error_table_add:
error_load_texture:
error_realloc:
error_readdir:
	table_free(txtrs);
	free(path);
error_malloc:
	errnum = errno;
	closedir(dir);
	errno = errnum;
error_opendir:
	return -1;
}
