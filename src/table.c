#include "table.h"
#include "grow.h"
#include "string.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct item {
	const char *key;
	void *val;
};

#define ITEM_SIZE sizeof(struct item)

int table_init(table *tbl, size_t size)
{
	tbl->cap = size;
	tbl->len = 0;
	tbl->items = malloc(size * ITEM_SIZE);
	if (!tbl->items) return -1;
	return 0;
}

int table_add(table *tbl, const char *key, void *val)
{
	struct item *slot = GROWE(&tbl->items, &tbl->len, &tbl->cap, ITEM_SIZE);
	if (!slot) return -1;
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
	tbl->items = realloc(tbl->items, tbl->len * ITEM_SIZE);
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

void table_free(table *tbl)
{
	free(tbl->items);
}

char *table_to_string(const table *tbl, char *(*item)(void *))
{
	size_t str_cap = 64;
	struct string str;
	str.text = malloc(str_cap);
	if (!str.text) goto error_malloc;
	str.len = 1;
	memcpy(str.text, "{", str.len);
	for (size_t i = 0; i < tbl->len; ++i) {
		char *val = item(tbl->items[i].val);
		if (!val) goto error_item;
		size_t key_len = strlen(tbl->items[i].key);
		size_t val_len = strlen(val);
		size_t len = key_len + val_len + 5;
		char *place = string_grow(&str, &str_cap, len);
		if (!place) {
			free(val);
			goto error_string_grow;
		}
		snprintf(place, len + 1, "\n\t%s: %s,", tbl->items[i].key, val);
		free(val);
	}
	char *end = string_grow(&str, &str_cap, 2);
	if (!end) goto error_string_grow_end;
	memcpy(end, "\n}", 2);
	return str.text;

error_string_grow:
error_item:
error_string_grow_end:
	free(str.text);
error_malloc:
	return NULL;
}
