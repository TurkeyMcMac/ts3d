#ifndef READ_LINES_H_
#define READ_LINES_H_

#include "string.h"
#include <stddef.h>

struct string *read_lines(const char *path, size_t *nlines);

#endif /* READ_LINES_H_ */
