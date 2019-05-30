#include "xalloc.h"
#include "util.h"
#include <unistd.h>

void *xmalloc(size_t size)
{
	void *ptr = malloc(size);
	if (!ptr && size != 0) {
		char buf[128];
		write(STDERR_FILENO, buf, sbprintf(buf, sizeof(buf),
			"malloc(%lu) failed. Aborting.\n", size));
		abort();
	}
	return ptr;
}

void *xcalloc(size_t count, size_t size)
{
	void *ptr = calloc(count, size);
	if (!ptr && count * size != 0) {
		char buf[128];
		write(STDERR_FILENO, buf, sbprintf(buf, sizeof(buf),
			"calloc(%lu, %lu) failed. Aborting.\n", count, size));
		abort();
	}
	return ptr;
}

void *xrealloc(void *ptr, size_t size)
{
	ptr = realloc(ptr, size);
	if (!ptr && size != 0) {
		char buf[128];
		write(STDERR_FILENO, buf, sbprintf(buf, sizeof(buf),
			"realloc(%p, %lu) failed. Aborting.\n", ptr, size));
		abort();
	}
	return ptr;
}
