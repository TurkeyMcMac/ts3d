#include "util.h"
#include "xalloc.h"
#include <string.h>

char *str_dup(const char *str)
{
	size_t size = strlen(str) + 1;
	return memcpy(xmalloc(size), str, size);
}
