#ifndef STRING_H_
#define STRING_H_

#include <stddef.h>

struct string {
	size_t len;
	char *text;
};

void string_init(struct string *str, size_t cap);

char *string_grow(struct string *str, size_t *cap, size_t num);

void string_shrink_to_fit(struct string *str);

void string_pushn(struct string *str, size_t *cap, const char *push,
	size_t npush);

void string_pushz(struct string *str, size_t *cap, const char *push);

void string_pushc(struct string *str, size_t *cap, int push);

#endif /* STRING_H_ */
