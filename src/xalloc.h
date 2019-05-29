#ifndef XALLOC_H_
#define XALLOC_H_

#include <stdlib.h>

void *xmalloc(size_t);

void *xcalloc(size_t, size_t);

void *xrealloc(void *, size_t);

#endif /* XALLOC_H_ */
