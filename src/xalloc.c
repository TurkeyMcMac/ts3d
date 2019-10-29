#include "xalloc.h"
#include "util.h"
#include <assert.h>
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

struct rc {
	long rc;
	long padding_; // TODO: is this necessary?
	char mem[];
};

#define rc_size(mem_size) (sizeof(struct rc) + (mem_size))

void *rc_xmalloc(size_t size)
{
	struct rc *rc = xmalloc(rc_size(size));
	rc->rc = 1;
	return rc->mem;
}

void *rc_xrealloc(void *mem, size_t size)
{
	if (mem) {
		struct rc *rc = container_of(mem, struct rc, mem);
		assert(rc->rc == 1);
		rc = xrealloc(rc, rc_size(size));
		return rc->mem;
	} else {
		return rc_xmalloc(size);
	}
}

long rc_inc(void *mem)
{
	struct rc *rc = container_of(mem, struct rc, mem);
	assert(rc->rc > 0);
	return ++rc->rc;
}

void rc_dec_free(void *mem)
{
	if (mem) {
		rc_dec(mem);
		rc_free(mem);
	}
}

long rc_dec(void *mem)
{
	struct rc *rc = container_of(mem, struct rc, mem);
	assert(rc->rc > 0);
	return --rc->rc;
}

void rc_free(void *mem)
{
	if (mem) {
		struct rc *rc = container_of(mem, struct rc, mem);
		assert(rc->rc == 0);
		free(rc);
	}
}

#if CTF_TESTS_ENABLED

#	include "libctf.h"
#	include <string.h>

CTF_TEST(xmalloc_0,
	xmalloc(0);
)

CTF_TEST(xcalloc_0,
	xcalloc(0, 0);
)

CTF_TEST(xrealloc_0,
	xrealloc(NULL, 0);
)

#define SIZE 8
CTF_TEST(refcount,
	char copy[SIZE];
	char *mem = rc_xrealloc(NULL, SIZE);
	memset(copy, 'A', SIZE);
	memcpy(mem, copy, SIZE);
	assert(rc_inc(mem) == 2);
	assert(rc_inc(mem) == 3);
	assert(rc_dec(mem) == 2);
	assert(rc_dec(mem) == 1);
	mem = rc_xrealloc(mem, 80);
	mem[70] = 'B';
	assert(!memcmp(mem, copy, SIZE));
	rc_dec_free(mem);
)

#endif /* CTF_TESTS_ENABLED */
