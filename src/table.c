#include "table.h"
#include "grow.h"
#include <stdlib.h>
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
	struct item *slot = GROW(&tbl->items, &tbl->len, &tbl->cap, ITEM_SIZE);
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
	size_t start, end;
	if (tbl->len == 0) return NULL;
	start = 0;
	end = tbl->len - 1;
	while (start <= end) {
		size_t mid = (start + end) / 2;
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
