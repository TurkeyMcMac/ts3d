#include "table.h"
#include "grow.h"
#include "string.h"
#include "xalloc.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

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

void *table_remove(table *tbl, const char *key)
{
	struct item *got;
	if (find(tbl, &got, key)) {
		void *val = got->val;
		--tbl->len;
		memmove(got, got + 1,
			(tbl->len - (got - tbl->items)) * ITEM_SIZE);
		return val;
	}
	return NULL;
}

size_t table_count(const table *tbl)
{
	return tbl->len;
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

#if CTF_TESTS_ENABLED

#	include "libctf.h"
#	include "util.h"
#	include <assert.h>
#	include <stdint.h>

static void set_up_table(table *tab, int val0, int val1, int val2)
{
	table_init(tab, 3);
	assert(!table_add(tab, "foo", (void *)(intptr_t)val0));
	assert(!table_add(tab, "bar", (void *)(intptr_t)val1));
	assert(!table_add(tab, "baz", (void *)(intptr_t)val2));
	assert(table_add(tab, "foo", (void *)(intptr_t)12345));
}

CTF_TEST(table_adds,
	table tab;
	set_up_table(&tab, 0, 0, 0);
	assert(table_count(&tab) == 3);
	table_free(&tab);
)

CTF_TEST(table_gets,
	table tab;
	set_up_table(&tab, 2, 3, 5);
	int product = 1;
	product *= *(intptr_t *)table_get(&tab, "foo");
	product *= *(intptr_t *)table_get(&tab, "bar");
	product *= *(intptr_t *)table_get(&tab, "baz");
	assert(product == 2 * 3 * 5);
	table_free(&tab);
)

CTF_TEST(table_removes,
	table tab;
	set_up_table(&tab, 2, 3, 5);
	int product = 1;
	product *= (intptr_t)table_remove(&tab, "foo");
	product *= (intptr_t)table_remove(&tab, "bar");
	product *= (intptr_t)table_remove(&tab, "baz");
	assert(product == 2 * 3 * 5);
	assert(table_count(&tab) == 0);
	assert(table_get(&tab, "foo") == NULL);
	table_free(&tab);
)

CTF_TEST(table_get_freeze,
	table tab;
	set_up_table(&tab, 2, 3, 5);
	table_freeze(&tab);
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

CTF_TEST(table_for_visits_all,
	table tab;
	set_up_table(&tab, 0, 0, 0);
	const char *key;
	void **val;
	TABLE_FOR_EACH(&tab, key, val) {
		set_value_for_key(key, val);
	}
	int product = 1;
	product *= *(intptr_t *)table_get(&tab, "foo");
	product *= *(intptr_t *)table_get(&tab, "bar");
	product *= *(intptr_t *)table_get(&tab, "baz");
	assert(product == 2 * 3 * 5);
	table_free(&tab);
)

CTF_TEST(table_empty_for_visits_all,
	table tab;
	table_init(&tab, 0);
	const char *key ATTRIBUTE(unused);
	int count = 0;
	TABLE_FOR_EACH(&tab, key, *(void ***)&count) {
		++count;
	}
	assert(count == 0);
	table_free(&tab);
)

CTF_TEST(table_each_visits_all,
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

CTF_TEST(table_each_returns_right,
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
