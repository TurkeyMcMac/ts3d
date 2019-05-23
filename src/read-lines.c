#include "read-lines.h"
#include "grow.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct string *read_lines(const char *path, size_t *nlines)
{
	int errnum;
	size_t lines_cap;
	struct string *lines;
	FILE *file = fopen(path, "r");
	if (!file) goto error_fopen;
	*nlines = 0;
	lines_cap = 16;
	lines = malloc(lines_cap * sizeof(*lines));
	if (!lines) goto error_malloc_lines;
	char buf[BUFSIZ];
	size_t nread = sizeof(buf);
	char *head = buf + nread;
	do {
		size_t line_cap = 16;
		struct string *line =
			GROWE(&lines, nlines, &lines_cap, sizeof(*lines));
		if (!line) goto error_growe;
		line->len = 0;
		line->text = malloc(line_cap);
		if (!line->text) goto error_malloc_line;
		for (bool last_one = false; !last_one;) {
			if (head >= buf + sizeof(buf)) {
				nread = fread(buf, 1, sizeof(buf), file);
				last_one = nread < sizeof(buf);
				head = buf;
			}
			char *nl = memchr(head, '\n', nread + buf - head);
			last_one = last_one || nl;
			if (!nl) nl = buf + nread;
			char *added = string_grow(line, &line_cap, nl - head);
			if (!added) goto error_string_grow;
			memcpy(added, head, nl - head);
			head = nl + 1;
		}
		if (line->len == 0) line->len = 1;
		string_shrink_to_fit(line);
	} while (!errno && !(feof(file) && head >= buf + nread));
	if (errno) goto error_fread;
	fclose(file);
	return lines;

error_fread:
error_string_grow:
error_malloc_line:
error_growe:
	for (size_t i = 0; i < *nlines; ++i) {
		free(lines[i].text);
	}
	free(lines);
error_malloc_lines:
	errnum = errno;
	fclose(file);
	errno = errnum;
error_fopen:
	return NULL;
}
