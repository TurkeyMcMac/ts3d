#ifndef UTIL_H_
#define UTIL_H_

#include <stddef.h>

#ifdef __GNUC__
#	define ATTRIBUTE(...) __attribute__((__VA_ARGS__))
#else
#	define ATTRIBUTE(...)
#endif

char *str_dup(const char *str);

#endif /* UTIL_H_ */
