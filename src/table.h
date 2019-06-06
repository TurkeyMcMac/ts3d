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

// Add an item to the table. This must be called BEFORE table_freeze. Zero is
// returned on success, but negative is returned when the key is already in the
// table.
int table_add(table *tbl, const char *key, void *val);

// Freeze the table, so no more items can be added, but now it can be searched.
void table_freeze(table *tbl);

// Get an item. This must be called AFTER table_freeze. On success, a mutable
// pointer to the value is returned. When the key is not present, NULL is
// returned.
void **table_get(table *tbl, const char *key);

// Iterate through all the entries (AFTER table_freeze is called.) The first
// argument to the given function is the key, and the second is the mutable
// pointer to the corresponding value. The order is undefined.
int table_each(table *tbl, int (*item)(const char *, void **));

// Free all memory allocated for a table, before or after table_freeze
void table_free(table *tbl);

// Convert a table to an allocated string for debugging. The passed function
// takes one argument: the value. All values are visited using this function.
char *table_to_string(const table *tbl, char *(*item)(void *));

#endif /* TABLE_H_ */
