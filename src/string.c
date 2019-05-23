#include "string.h"
#include "grow.h"
#include <stdlib.h>

char *string_grow(struct string *str, size_t *cap, size_t num)
{
	return growc(&str->text, &str->len, cap, num);
}

void string_shrink_to_fit(struct string *str)
{
	str->text = realloc(str->text, str->len);
}
