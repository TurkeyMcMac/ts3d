#include "util.h"
#include "xalloc.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

char *str_dup(const char *str)
{
	size_t size = strlen(str) + 1;
	return memcpy(xmalloc(size), str, size);
}

int sbprintf(char * restrict str, size_t size, const char * restrict fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	int writ = vsnprintf(str, size, fmt, va);
	if ((size_t)writ >= size && writ > 0) writ = size - 1;
	va_end(va);
	return writ;
}
