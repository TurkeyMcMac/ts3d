#include "grow.h"
#include <stdlib.h>

void *growe(void **bufp, size_t *lenp, size_t *capp, size_t esize)
{
	//printf("buf = %p, len = %lu, cap = %lu, esize = %lu\n",
	//	*bufp, *lenp, *capp, esize);
	if (*lenp >= *capp) {
		size_t new_cap = *lenp * 3 / 2 + 1;
		void *new_buf = realloc(*bufp, new_cap * esize);
		if (!new_buf) return NULL;
		*capp = new_cap;
		*bufp = new_buf;
	}
	return (char *)*bufp + (*lenp)++ * esize;
}

char *growc(char **bufp, size_t *lenp, size_t *capp, size_t num)
{
	size_t old_len = *lenp;
	//printf("'%*s' + %ld\n", *lenp, *bufp, num);
	*lenp += num;
	if (*lenp > *capp) {
		size_t new_cap = *lenp * 3 / 2 + 1;
		void *new_buf = realloc(*bufp, new_cap);
		if (!new_buf) return NULL;
		*capp = new_cap;
		*bufp = new_buf;
	}
	return *bufp + old_len;
}
