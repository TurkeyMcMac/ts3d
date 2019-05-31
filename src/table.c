#include "table.h"
#include "grow.h"
#include "string.h"
#include "xalloc.h"
#include <stdio.h>
#include <string.h>

struct item {
	const char *key;
	void *val;
};

#define ITEM_SIZE sizeof(struct item)

void table_init(table *tbl, size_t size)
{
	tbl->cap = size;
	tbl->len = 0;
	tbl->items = xmalloc(size * ITEM_SIZE);
}

int table_add(table *tbl, const char *key, void *val)
{
	struct item *slot = GROWE(tbl->items, tbl->len, tbl->cap);
	slot->key = key;
	slot->val = val;
	return 0;
}

static int item_compar(const void *item1, const void *item2)
{
	return strcmp(*(char * const *)item1, *(char * const *)item2);
}

void table_freeze(table *tbl)
{
	tbl->items = xrealloc(tbl->items, tbl->len * ITEM_SIZE);
	qsort(tbl->items, tbl->len, ITEM_SIZE, item_compar);
}

void **table_get(table *tbl, const char *key)
{
	long start, end;
	start = 0;
	end = tbl->len - 1;
	while (start <= end) {
		long mid = (start + end) / 2;
		int cmp = strcmp(key, tbl->items[mid].key);
		if (cmp > 0) {
			start = mid + 1;
		} else if (cmp < 0) {
			end = mid - 1;
		} else {
			return &tbl->items[mid].val;
		}
	}
	return NULL;
}

int table_each(table *tbl, int (*item)(const char *, void **))
{
	for (size_t i = 0; i < tbl->len; ++i) {
		int retval = item(tbl->items[i].key, &tbl->items[i].val);
		if (retval) return retval;
	}
	return 0;
}

void table_free(table *tbl)
{
	free(tbl->items);
}

char *table_to_string(const table *tbl, char *(*item)(void *))
{
	size_t str_cap = 64;
	struct string str;
	str.text = xmalloc(str_cap);
	str.len = 1;
	memcpy(str.text, "{", str.len);
	for (size_t i = 0; i < tbl->len; ++i) {
		char *val = item(tbl->items[i].val);
		if (!val) goto error_item;
		size_t key_len = strlen(tbl->items[i].key);
		size_t val_len = strlen(val);
		size_t len = key_len + val_len + 5;
		snprintf(string_grow(&str, &str_cap, len), len + 1,
			"\n\t%s: %s,", tbl->items[i].key, val);
		free(val);
	}
	memcpy(string_grow(&str, &str_cap, 3), "\n}", 3);
	return str.text;

error_item:
	free(str.text);
	return NULL;
}
