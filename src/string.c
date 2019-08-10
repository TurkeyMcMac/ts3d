#include "string.h"
#include "grow.h"
#include "xalloc.h"
#include <string.h>

void string_init(struct string *str, size_t cap)
{
	str->len = 0;
	str->text = xmalloc(cap);
}

char *string_grow(struct string *str, size_t *cap, size_t num)
{
	return growc(&str->text, &str->len, cap, num);
}

void string_shrink_to_fit(struct string *str)
{
	str->text = xrealloc(str->text, str->len);
}

void string_pushn(struct string *str, size_t *cap, const char *push,
	size_t npush)
{
	char *place = string_grow(str, cap, npush);
	memcpy(place, push, npush);
}

void string_pushz(struct string *str, size_t *cap, const char *push)
{
	string_pushn(str, cap, push, strlen(push));
}

void string_pushc(struct string *str, size_t *cap, int push)
{
	*string_grow(str, cap, 1) = push;
}

#if CTF_TESTS_ENABLED

#	include "libctf.h"
#	include <assert.h>

#endif /* CTF_TESTS_ENABLED */
