#ifndef XALLOC_H_
#define XALLOC_H_

#include <stdlib.h>

// Memory allocation alternatives that abort on failure, never returning NULL.

void *xmalloc(size_t);

void *xcalloc(size_t, size_t);

void *xrealloc(void *, size_t);

void *rc_xmalloc(size_t size);

void *rc_xrealloc(void *mem, size_t size);

long rc_inc(void *mem);

void rc_dec_free(void *mem);

long rc_dec(void *mem);

void rc_free(void *mem);

#endif /* XALLOC_H_ */
