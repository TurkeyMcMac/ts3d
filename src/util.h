#ifndef UTIL_H_
#define UTIL_H_

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

#endif /* UTIL_H_ */
