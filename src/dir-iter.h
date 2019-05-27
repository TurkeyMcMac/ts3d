#ifndef DIR_ITER_H_
#define DIR_ITER_H_

#include <dirent.h>

typedef int (*dir_iter_f)(struct dirent *ent, void *ctx);

int dir_iter(const char *path, dir_iter_f iter_fn, void *ctx);

#endif /* DIR_ITER_H_ */
