#include "xalloc.h"
#include "util.h"
#include <unistd.h>

void *xmalloc(size_t size)
{
	void *ptr = malloc(size);
	if (!ptr && size != 0) {
		char buf[128];
		ssize_t w = write(STDERR_FILENO, buf, sbprintf(buf, sizeof(buf),
			"malloc(%zu) failed. Aborting.\n", size));
		size = (size_t)w; // Silence unused warning
		abort();
	}
	return ptr;
}

void *xcalloc(size_t count, size_t size)
{
	void *ptr = calloc(count, size);
	if (!ptr && count * size != 0) {
		char buf[128];
		ssize_t w = write(STDERR_FILENO, buf, sbprintf(buf, sizeof(buf),
			"calloc(%zu, %zu) failed. Aborting.\n", count, size));
		size = (size_t)w; // Silence unused warning
		abort();
	}
	return ptr;
}

void *xrealloc(void *ptr, size_t size)
{
	ptr = realloc(ptr, size);
	if (!ptr && size != 0) {
		char buf[128];
		ssize_t w = write(STDERR_FILENO, buf, sbprintf(buf, sizeof(buf),
			"realloc(%p, %zu) failed. Aborting.\n", ptr, size));
		size = (size_t)w; // Silence unused warning
		abort();
	}
	return ptr;
}

#if CTF_TESTS_ENABLED

#	include "libctf.h"
#	include <assert.h>

CTF_TEST(ts3d_xmalloc_0,
	xmalloc(0);
)

CTF_TEST(ts3d_xcalloc_0,
	xcalloc(0, 0);
)

CTF_TEST(ts3d_xrealloc_0,
	xrealloc(NULL, 0);
)

#endif /* CTF_TESTS_ENABLED */
