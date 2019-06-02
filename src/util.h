#ifndef UTIL_H_
#define UTIL_H_

#include "d3d.h"
#include <stddef.h>

#ifdef __GNUC__
#	define ATTRIBUTE(...) __attribute__((__VA_ARGS__))
#else
#	define ATTRIBUTE(...)
#endif

char *str_dup(const char *str);

// Version of snprintf which returns size-1 if it exceeds the buffer length:
int sbprintf(char * restrict str, size_t size, const char * restrict fmt, ...)
	ATTRIBUTE(format(printf, 3, 4));

d3d_direction flip_direction(d3d_direction dir);

void move_direction(d3d_direction dir, size_t *x, size_t *y);

#define bitat(bits, idx) ((bits) >> (idx) & 1)

#endif /* UTIL_H_ */
