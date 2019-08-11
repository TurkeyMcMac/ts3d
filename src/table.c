#include "table.h"
#include "grow.h"
#include "string.h"
#include "xalloc.h"
#include <stdbool.h>
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

static bool find(table *tbl, struct item **found, const char *key)
{
	long start = 0, end = tbl->len - 1;
	while (start <= end) {
		long mid = (start + end) / 2;
		int cmp = (strcmp)(key, tbl->items[mid].key);
		if (cmp > 0) {
			start = mid + 1;
		} else if (cmp < 0) {
			end = mid - 1;
		} else {
			*found = &tbl->items[mid];
			return true;
		}
	}
	*found = &tbl->items[start];
	return false;
}

int table_add(table *tbl, const char *key, void *val)
{
	struct item *slot;
	GROWE(tbl->items, tbl->len, tbl->cap);
	--tbl->len;
	if (find(tbl, &slot, key)) return -1;
	memmove(slot + 1, slot, (tbl->len - (slot - tbl->items)) * ITEM_SIZE);
	slot->key = key;
	slot->val = val;
	++tbl->len;
	return 0;
}

void table_freeze(table *tbl)
{
	tbl->items = xrealloc(tbl->items, tbl->len * ITEM_SIZE);
}

void **table_get(table *tbl, const char *key)
{
	struct item *got;
	return find(tbl, &got, key) ? &got->val : NULL;
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

#if CTF_TESTS_ENABLED

#	include "libctf.h"
#	include <assert.h>
#	include <stdint.h>

static void set_up_table(table *tab, int val0, int val1, int val2)
{
	table_init(tab, 3);
	assert(!table_add(tab, "foo", (void *)(intptr_t)val0));
	assert(!table_add(tab, "bar", (void *)(intptr_t)val1));
	assert(!table_add(tab, "baz", (void *)(intptr_t)val2));
	assert(table_add(tab, "foo", (void *)(intptr_t)12345));
	table_freeze(tab);
}

CTF_TEST(ts3d_table_add,
	table tab;
	set_up_table(&tab, 0, 0, 0);
	table_free(&tab);
)

CTF_TEST(ts3d_table_get,
	table tab;
	set_up_table(&tab, 2, 3, 5);
	int product = 1;
	product *= *(intptr_t *)table_get(&tab, "foo");
	product *= *(intptr_t *)table_get(&tab, "bar");
	product *= *(intptr_t *)table_get(&tab, "baz");
	assert(product == 2 * 3 * 5);
	table_free(&tab);
)

static int set_value_for_key(const char *key, void **item_)
{
	intptr_t *item = (intptr_t *)item_;
	if (!strcmp(key, "foo")) {
		*item = 2;
	} else if (!strcmp(key, "bar")) {
		*item = 3;
	} else if (!strcmp(key, "baz")) {
		*item = 5;
	} else {
		assert(!"Bad key");
	}
	return 0;
}

CTF_TEST(ts3d_table_each_visits_all,
	table tab;
	set_up_table(&tab, 0, 0, 0);
	int product = 1;
	table_each(&tab, set_value_for_key);
	product *= *(intptr_t *)table_get(&tab, "foo");
	product *= *(intptr_t *)table_get(&tab, "bar");
	product *= *(intptr_t *)table_get(&tab, "baz");
	assert(product == 2 * 3 * 5);
	table_free(&tab);
)

static int set_value_for_key_return_1(const char *key, void **item)
{
	set_value_for_key(key, item);
	return 1;
}

CTF_TEST(ts3d_table_each_returns_right,
	table tab;
	set_up_table(&tab, 0, 0, 0);
	assert(!table_each(&tab, set_value_for_key));
	int product = 1;
	set_up_table(&tab, 0, 0, 0);
	assert(table_each(&tab, set_value_for_key_return_1) == 1);
	product *= *(intptr_t *)table_get(&tab, "foo");
	product *= *(intptr_t *)table_get(&tab, "bar");
	product *= *(intptr_t *)table_get(&tab, "baz");
	assert(product == 0);
	table_free(&tab);
)

#endif /* CTF_TESTS_ENABLED */
