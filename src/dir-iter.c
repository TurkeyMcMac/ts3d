#include "dir-iter.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

int dir_iter(const char *path, dir_iter_f iter_fn, void *ctx)
{
	int errnum;
	int ret = -1;
	DIR *dir = opendir(path);
	if (!dir) goto error_opendir;
	struct dirent *ent;
	errno = 0;
	while ((ent = readdir(dir))) {
		// Ignore dot files:
		if (*ent->d_name == '.') continue;
		ret = abs(iter_fn(ent, ctx));
		if (ret != 0) goto end;
		errno = 0;
	}
	ret = -errno;
end:
	errnum = errno;
	closedir(dir);
	errno = errnum;
error_opendir:
	return ret;
}
