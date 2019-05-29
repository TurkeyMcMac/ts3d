#include "json-util.h"
#include "table.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct json_reader_ctx {
	char buf[BUFSIZ];
	const char *path;
	FILE *file;
	int line;
};

static int refill(char **buf, size_t *size, void *ctx_v)
{
	struct json_reader_ctx *ctx = ctx_v;
	size_t got = fread(*buf, 1, *size, ctx->file);
	char *left = *buf;
	while ((left = memchr(left, '\n', got - (left - *buf)))) {
		++ctx->line;
		++left;
	}
	if (got < *size) {
		*size = got;
		return feof(ctx->file) ? 0 : -JSON_ERROR_ERRNO;
	}
	return 1;

}

int init_json_reader(const char *path, json_reader *rdr)
{
	int errnum;
	struct json_reader_ctx *ctx = malloc(sizeof(*ctx));
	if (!ctx) goto error_malloc;
	ctx->file = fopen(path, "r");
	if (!ctx->file) goto error_fopen;
	if (json_alloc(rdr, NULL, 8, malloc, free, realloc))
		goto error_json_alloc;
	json_source(rdr, ctx->buf, sizeof(ctx->buf), ctx, refill);
	ctx->path = path;
	ctx->line = 1;
	return 0;

error_json_alloc:
	errnum = errno;
	fclose(ctx->file);
	errno = errnum;
error_fopen:
	free(ctx);
error_malloc:
	return -1;
}

void free_json_reader(json_reader *rdr)
{
	struct json_reader_ctx *ctx = *json_get_ctx(rdr);
	int errnum = errno;
	fclose(ctx->file);
	errno = errnum;
	free(ctx);
	json_free(rdr);
}

void print_json_error(const json_reader *rdr, const struct json_item *item)
{
	const char *msg;
	struct json_reader_ctx *ctx = *json_get_ctx((json_reader *)rdr);
	int line = ctx->line;
	size_t bufsiz;
	char *buf;
	json_get_buf(rdr, &buf, &bufsiz);
	if (item->val.erridx < bufsiz) {
		char *left = buf + item->val.erridx;
		while ((left = memchr(left, '\n', bufsiz - (left - buf)))) {
			--line;
			++left;
		}
	}
	switch (item->type) {
	case JSON_ERROR_MEMORY:
		msg = "Out of memory";
		break;
	case JSON_ERROR_NUMBER_FORMAT:
		msg = "Invalid number format";
		break;
	case JSON_ERROR_TOKEN:
		msg = "Invalid token";
		break;
	case JSON_ERROR_EXPECTED_STRING:
		msg = "Expected string literal";
		break;
	case JSON_ERROR_EXPECTED_COLON:
		msg = "Expected colon";
		break;
	case JSON_ERROR_BRACKETS:
		msg = "Mismatched brackets";
		break;
	case JSON_ERROR_UNCLOSED_QUOTE:
		msg = "Unclosed quote";
		break;
	case JSON_ERROR_ESCAPE:
		msg = "Invalid escape sequence";
		break;
	case JSON_ERROR_CONTROL_CHAR:
		msg = "Raw control character present in string";
		break;
	case JSON_ERROR_EXPECTED_VALUE:
		msg = "Expected value";
		break;
	case JSON_ERROR_IO:
		msg = "Input reading failed";
		break;
	case JSON_ERROR_ERRNO:
		msg = strerror(errno);
		break;
	default:
		msg = "Unknown error";
		break;
	}
	fprintf(stderr, "JSON parse error in '%s', line %d: %s\n",
		ctx->path, line, msg);
}

static bool json_key_tab_created = false;
static table json_key_tab;
int create_json_key_tab(void)
{
	if (json_key_tab_created) return 0;
	if (table_init(&json_key_tab, JKEY_COUNT)) return -1;
#define JKEY(name) \
	if (table_add(&json_key_tab, #name, (void *)(intptr_t)JKEY_##name)) \
		goto error_table_add;
#include "json-keys.h"
#undef JKEY
	table_freeze(&json_key_tab);
	json_key_tab_created = true;
	return 0;

error_table_add:
	table_free(&json_key_tab);
	return -1;
}

enum json_key_code translate_json_key(const char *key)
{
	if (!key) return JKEY_NULL;
	intptr_t *codep = (void *)table_get(&json_key_tab, key);
	if (!codep) return JKEY_NOT_FOUND;
	return *codep;
}

void free_json_key_tab(void)
{
	if (!json_key_tab_created) return;
	table_free(&json_key_tab);
	json_key_tab_created = false;
}
