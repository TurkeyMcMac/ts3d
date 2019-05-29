#include "json-util.h"
#include "table.h"
#include "xalloc.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
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
	struct json_reader_ctx *ctx = xmalloc(sizeof(*ctx));
	ctx->file = fopen(path, "r");
	if (!ctx->file) goto error_fopen;
	if (json_alloc(rdr, NULL, 8, xmalloc, free, xrealloc))
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
void create_json_key_tab(void)
{
	if (json_key_tab_created) return;
	table_init(&json_key_tab, JKEY_COUNT);
#define JKEY(name) \
	table_add(&json_key_tab, #name, (void *)(intptr_t)JKEY_##name);
#include "json-keys.h"
#undef JKEY
	table_freeze(&json_key_tab);
	json_key_tab_created = true;
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

struct json_node {
	enum json_node_kind {
		JN_EMPTY,
		JN_NULL,
		JN_MAP,
		JN_LIST,
		JN_STRING,
		JN_NUMBER,
		JN_BOOLEAN,
		JN_ERROR,
		JN_END_
	} kind;
	union json_node_data {
		table map;
		struct json_node_data_list {
			size_t n_vals;
			struct json_node *vals;
		} list;
		char *str;
		double num;
		bool boolean;
	} d;
};

static void parse_node(json_reader *rdr, struct json_node *nd, char **keyp)
{
	size_t cap;
	struct json_item item;
	if (json_read_item(rdr, &item) < 0) {
		print_json_error(rdr, &item);
		nd->kind = JN_ERROR;
		return;
	}
	*keyp = item.key.text;
	switch (item.type) {
	case JSON_EMPTY:
		nd->kind = JN_EMPTY;
		break;
	case JSON_NULL:
		nd->kind = JN_NULL;
		break;
	case JSON_MAP:
		nd->kind = JN_MAP;
		table_init(&nd->d.map, 8);
		for (;;) {
			char *key;
			struct json_node *entry = xmalloc(sizeof(*entry));
			parse_node(rdr, entry, &key);
			if (entry->kind == JN_END_) {
				break;
			} else if (entry->kind == JN_ERROR) {
				// TODO: free children
				table_free(&nd->d.map);
				nd->kind = JN_ERROR;
				return;
			}
			// TODO: check duplicates
			table_add(&nd->d.map, key, entry);
		}
		table_freeze(&nd->d.map);
		break;
	case JSON_LIST:
		nd->kind = JN_LIST;
		cap = 8;
		nd->d.list.n_vals = 0;
		nd->d.list.vals = xmalloc(cap * sizeof(*nd->d.list.vals));
		for (;;) {
			char *key;
			struct json_node *entry = GROWE(&nd->d.list.vals,
				&nd->d.list.n_vals, &cap,
				sizeof(*nd->d.list.vals));
			parse_node(rdr, entry, &key);
			if (entry->kind == JN_END_) {
				break;
			} else if (entry->kind == JN_ERROR) {
				// TODO: free children
				free(nd->d.list.vals);
				nd->kind = JN_ERROR;
				return;
			}
		}
		break;
	case JSON_STRING:
		nd->kind = JN_STRING;
		nd->d.str = item.val.str.text;
		break;
	case JSON_NUMBER:
		nd->kind = JN_NUMBER;
		nd->d.num = item.val.num;
		break;
	case JSON_BOOLEAN:
		nd->kind = JN_BOOLEAN;
		nd->d.boolean = item.val.boolean;
		break;
	case JSON_END_MAP:
	case JSON_END_LIST:
		nd->kind = JN_ENDE_;
		break;
	default:
		break;
	}
}

void parse_json_tree(const char *path, struct json_node *root)
{
	json_reader rdr;
	struct json_reader_ctx *ctx = xmalloc(sizeof(*ctx));
	ctx->file = fopen(path, "r");
	if (!ctx->file) {
		free(ctx);
		return -1;
	}
	json_alloc(&rdr, NULL, 8, xmalloc, free, xrealloc);
	json_source(&rdr, ctx->buf, sizeof(ctx->buf), ctx, refill);
	ctx->path = path;
	ctx->line = 1;
	char *key;
	parse_node(&rdr, root, &key);
}


