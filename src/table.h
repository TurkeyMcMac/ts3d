#ifndef TABLE_H_
#define TABLE_H_

#include <stddef.h>

// A map from strings to pointers
struct item;
typedef struct {
	size_t len;
	size_t cap;
	struct item *items;
} table;

// Initialize a table with a certain size suggestion. This need not be the exact
// size.
void table_init(table *tbl, size_t size);

// Add an item to the table. Zero is returned on success, but negative is
// returned when the key is already in the table.
int table_add(table *tbl, const char *key, void *val);

// Freeze the table so no more items can be added. This is not necessary.
void table_freeze(table *tbl);

// Get an item. pointer to the value is returned. When the key is not present,
// NULL is returned.
void **table_get(table *tbl, const char *key);

// Remove an item from the table and return it. If the item does not exist,
// NULL is returned. There is no way to tell between a NULL item and a
// nonexistent one.
void *table_remove(table *tbl, const char *key);

// Count the number of unique keys added to the table.
size_t table_count(const table *tbl);

// Iterate through all the entries. The first argument to the given function is
// the key, and the second is the mutable pointer to the corresponding value.
// The order is undefined.
int table_each(table *tbl, int (*item)(const char *, void **));

// Free all memory allocated for a table.
void table_free(table *tbl);

#endif /* TABLE_H_ */
