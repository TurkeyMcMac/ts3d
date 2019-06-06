#ifndef READ_LINES_H_
#define READ_LINES_H_

#include "string.h"
#include <stddef.h>

// Read the lines from a file path. *nlines is set to the number of lines read.
// Newlines are not included in the lines. Lines are not NUL-terminated. NULL is
// returned on failure.
struct string *read_lines(const char *path, size_t *nlines);

#endif /* READ_LINES_H_ */
