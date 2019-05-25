#include "string.h"
#include "grow.h"
#include <stdlib.h>
#include <string.h>

int string_init(struct string *str, size_t cap)
{
	str->len = 0;
	str->text = malloc(cap);
	if (!str->text) return -1;
	return 0;
}

char *string_grow(struct string *str, size_t *cap, size_t num)
{
	return growc(&str->text, &str->len, cap, num);
}

void string_shrink_to_fit(struct string *str)
{
	str->text = realloc(str->text, str->len);
}

int string_pushn(struct string *str, size_t *cap, const char *push,
	size_t npush)
{
	char *place = string_grow(str, cap, npush);
	if (!place) return -1;
	memcpy(place, push, npush);
	return 0;
}

int string_pushz(struct string *str, size_t *cap, const char *push)
{
	return string_pushn(str, cap, push, strlen(push));
}

int string_pushc(struct string *str, size_t *cap, int push)
{
	char *place = string_grow(str, cap, 1);
	if (!place) return -1;
	*place = push;
	return 0;
}
