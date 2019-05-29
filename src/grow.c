#include "grow.h"
#include "xalloc.h"

void *growe(void **bufp, size_t *lenp, size_t *capp, size_t esize)
{
	if (*lenp >= *capp) {
		*capp = *lenp * 3 / 2 + 1;
		*bufp = xrealloc(*bufp, *capp * esize);
	}
	return (char *)*bufp + (*lenp)++ * esize;
}

char *growc(char **bufp, size_t *lenp, size_t *capp, size_t num)
{
	size_t old_len = *lenp;
	*lenp += num;
	if (*lenp > *capp) {
		*capp = *lenp * 3 / 2 + 1;
		*bufp = xrealloc(*bufp, *capp);
	}
	return *bufp + old_len;
}
