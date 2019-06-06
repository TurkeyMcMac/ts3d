#ifndef DIR_ITER_H_
#define DIR_ITER_H_

#include <dirent.h>

// A function for iterating over a directory entry, called by dir_iter. ent is
// the entry. ctx is whatever ctx was passed to dir_iter. If this returns non-
// zero, the absolute value of what was returned is immediately returned by
// dir_iter.
typedef int (*dir_iter_f)(struct dirent *ent, void *ctx);

// Iterate through files in a directory at the given path with a given function
// and ctx (see above.) If one iteration fails, a positive number is returned.
// If a system error occurs, errno is set and a negative is returned. Otherwise,
// zero is returned.
int dir_iter(const char *path, dir_iter_f iter_fn, void *ctx);

#endif /* DIR_ITER_H_ */
