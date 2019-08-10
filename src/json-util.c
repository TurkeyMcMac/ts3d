#include "json-util.h"
#include "grow.h"
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

static void print_json_error(const json_reader *rdr,
	const struct json_item *item)
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

union json_node_data *json_map_get(struct json_node *map, const char *key,
	enum json_node_kind kind)
{
	if (map->kind != JN_MAP) return NULL;
	void **got = table_get(&map->d.map, key);
	if (!got) return NULL;
	struct json_node *nd = *got;
	if (nd->kind != kind) return NULL;
	return &nd->d;
}

static int free_json_pair(const char *key, void **val)
{
	free((char *)key);
	free_json_tree(*val);
	free(*val);
	return 0;
}

static void parse_node(json_reader *rdr, struct json_node *nd, char **keyp)
{
	size_t cap;
	struct json_item item;
	if (json_read_item(rdr, &item) < 0) {
		print_json_error(rdr, &item);
		nd->kind = JN_ERROR;
		return;
	}
	*keyp = item.key.bytes;
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
				table_each(&nd->d.map, free_json_pair);
				table_free(&nd->d.map);
				nd->kind = JN_ERROR;
				return;
			}
			if (table_add(&nd->d.map, key, entry)) {
				// With duplicate keys, the first is kept
				free_json_tree(entry);
				free(entry);
			}
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
			struct json_node *entry = GROWE(nd->d.list.vals,
				nd->d.list.n_vals, cap);
			parse_node(rdr, entry, &key);
			if (entry->kind == JN_END_) {
				--nd->d.list.n_vals;
				break;
			} else if (entry->kind == JN_ERROR) {
				for (size_t i = 0; i < nd->d.list.n_vals; ++i) {
					free_json_tree(&nd->d.list.vals[i]);
				}
				free(nd->d.list.vals);
				nd->kind = JN_ERROR;
				return;
			}
		}
		break;
	case JSON_STRING:
		nd->kind = JN_STRING;
		nd->d.str = item.val.str.bytes;
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
		nd->kind = JN_END_;
		break;
	default:
		break;
	}
}

static int parse_json_tree_file(const char *path, FILE *file,
	struct json_node *root)
{
	json_reader rdr;
	struct json_reader_ctx *ctx = xmalloc(sizeof(*ctx));
	ctx->file = file;
	json_alloc(&rdr, NULL, 8, xmalloc, free, xrealloc);
	json_source(&rdr, ctx->buf, sizeof(ctx->buf), ctx, refill);
	ctx->path = path;
	ctx->line = 1;
	char *key;
	parse_node(&rdr, root, &key);
	free(ctx);
	fclose(file);
	return 0;
}

int parse_json_tree(const char *path, struct json_node *root)
{
	FILE *file = fopen(path, "r");
	if (!file) return -1;
	return parse_json_tree_file(path, file, root);
}

void free_json_tree(struct json_node *nd)
{
	switch (nd->kind) {
	case JN_MAP:
		table_each(&nd->d.map, free_json_pair);
		table_free(&nd->d.map);
		break;
	case JN_LIST:
		for (size_t i = 0; i < nd->d.list.n_vals; ++i) {
			free_json_tree(&nd->d.list.vals[i]);
		}
		free(nd->d.list.vals);
		break;
	case JN_STRING:
		free(nd->d.str);
	default:
		break;
	}
}

int parse_json_vec(d3d_vec_s *vec, const struct json_node_data_list *list)
{
	int retval = 0;
	vec->x = vec->y = 0;
	if (list->n_vals < 1) return -1;
	if (list->vals[0].kind == JN_NUMBER) {
		vec->x = list->vals[0].d.num;
	} else {
		retval |= -1;
	}
	if (list->n_vals >= 2 && list->vals[1].kind == JN_NUMBER) {
		vec->y = list->vals[1].d.num;
	} else {
		retval |= -1;
	}
	return retval;
}

#if CTF_TESTS_ENABLED

#	include "libctf.h"
#	include <assert.h>

#	define SOURCE(text) \
	struct json_node root; \
	do { \
		char str[] = (text); \
		FILE *source = fmemopen(str, sizeof(str), "r"); \
		parse_json_tree_file("(memory)", source, &root); \
	} while (0)

CTF_TEST(ts3d_printed_json_error,
	SOURCE("{\"a\":1,\"b\":2");
	assert(root.kind == JN_ERROR);
	free_json_tree(&root);
)

CTF_TEST(ts3d_json_vec,
	d3d_vec_s vec;
	SOURCE("[1,2]");
	assert(root.kind == JN_LIST);
	assert(!parse_json_vec(&vec, &root.d.list));
	assert(vec.x == 1);
	assert(vec.y == 2);
	free_json_tree(&root);
)

CTF_TEST(ts3d_json_tree_map,
	SOURCE("{\"a\":1,\"b\":2}");
	assert(json_map_get(&root, "a", JN_NUMBER)->num == 1);
	assert(json_map_get(&root, "b", JN_NUMBER)->num == 2);
	free_json_tree(&root);
)

CTF_TEST(ts3d_json_tree_list,
	SOURCE("[1,2]");
	assert(root.kind == JN_LIST);
	assert(root.d.list.n_vals == 2);
	assert(root.d.list.vals[0].d.num == 1);
	assert(root.d.list.vals[1].d.num == 2);
	free_json_tree(&root);
)

CTF_TEST(ts3d_json_tree_nested,
	SOURCE("{\"a\":1,\"b\":[2,3]}");
	assert(json_map_get(&root, "a", JN_NUMBER)->num == 1);
	union json_node_data *list = json_map_get(&root, "b", JN_LIST);
	assert(list);
	assert(list->list.n_vals == 2);
	assert(list->list.vals[0].d.num == 2);
	assert(list->list.vals[1].d.num == 3);
	free_json_tree(&root);
)

#endif /* CTF_TESTS_ENABLED */
