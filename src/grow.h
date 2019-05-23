#ifndef GROW_H_
#define GROW_H_

#include <stddef.h>

void *growe(void **bufp, size_t *lenp, size_t *capp, size_t esize);

#define GROWE(bufp, lenp, capp, esize) growe((void **)(bufp), lenp, capp, esize)

char *growc(char **bufp, size_t *lenp, size_t *capp, size_t num);

#endif /* GROW_H_ */
