#ifndef STRING_H_
#define STRING_H_

#include <stddef.h>

struct string {
	size_t len;
	char *text;
};

char *string_grow(struct string *str, size_t *cap, size_t num);

void string_shrink_to_fit(struct string *str);

#endif /* STRING_H_ */
