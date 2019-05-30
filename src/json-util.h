#ifndef JSON_UTIL_H_
#define JSON_UTIL_H_

#include "json.h"
#include "table.h"
#include <stdbool.h>
#include <stddef.h>

struct json_node {
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
	union json_node_data {
		table map;
		struct json_node_data_list {
			size_t n_vals;
			struct json_node *vals;
		} list;
		char *str;
		double num;
		bool boolean;
	} d;
};

union json_node_data *json_map_get(struct json_node *map, const char *key,
	enum json_node_kind kind);

int parse_json_tree(const char *path, struct json_node *root);

void free_json_tree(struct json_node *root);

#endif /* JSON_UTIL_H_ */
