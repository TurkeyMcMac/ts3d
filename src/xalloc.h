#ifndef XALLOC_H_
#define XALLOC_H_

// Memory allocation alternatives that abort on failure, never returning NULL.

#include <stdlib.h>

void *xmalloc(size_t);

void *xcalloc(size_t, size_t);

void *xrealloc(void *, size_t);

#endif /* XALLOC_H_ */
