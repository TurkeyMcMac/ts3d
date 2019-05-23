#ifndef TABLE_H_
#define TABLE_H_

#include <stddef.h>

struct item;
typedef struct {
	size_t len;
	size_t cap;
	struct item *items;
} table;

int table_init(table *tbl, size_t size);

int table_add(table *tbl, const char *key, void *val);

void table_freeze(table *tbl);

void **table_get(table *tbl, const char *key);

void table_free(table *tbl);

#endif /* TABLE_H_ */
