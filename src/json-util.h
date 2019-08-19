#ifndef JSON_UTIL_H_
#define JSON_UTIL_H_

#include "d3d.h"
#include "json.h"
#include "table.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

// A node in a JSON tree
struct json_node {
	// The kind of the node. EMPTY is only possibly the top node, for an
	// empty file. ERROR is also only top level, indicating that the JSON
	// was invalid and an error was printed to stderr. Ignore END_. The
	// other kinds are clear.
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
	// The type-specific node data.
	union json_node_data {
		// A JSON map from allocated NUL-terminated keys to node
		// pointers.
		table map;
		// A JSON list of n_vals sub-nodes.
		struct json_node_data_list {
			size_t n_vals;
			struct json_node *vals;
		} list;
		// An allocated NUL-terminated string.
		char *str;
		// A number.
		double num;
		// A boolean.
		bool boolean;
	} d;
};

// Try to get the data from a node in a given JSON map. NULL is returned if:
//  1. The map node is not actually a map.
//  2. The key is not present in the map.
//  3. The node found is not of the desired kind.
union json_node_data *json_map_get(struct json_node *map, const char *key,
	enum json_node_kind kind);

// Parse a JSON tree from the file given into the root. Negative is returned if
// a system error occurred, but otherwise returned is zero. The name is used for
// errors. The file is closed.
int parse_json_tree(const char *name, FILE *file, struct json_node *root);

// Completely free a JSON tree, including all the strings and stuff.
void free_json_tree(struct json_node *root);

// Parse a vector in the [x, y] form. Zero is returned unless the format is
// invalid, in which case -1 is returned and unparseable coordinates are zero.
int parse_json_vec(d3d_vec_s *vec, const struct json_node_data_list *list);

#endif /* JSON_UTIL_H_ */
