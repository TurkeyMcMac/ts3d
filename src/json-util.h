#ifndef JSON_UTIL_H_
#define JSON_UTIL_H_

#include "json.h"

int init_json_reader(const char *path, json_reader *rdr);

void free_json_reader(json_reader *rdr);

void print_json_error(const json_reader *rdr, const struct json_item *item);

#endif /* JSON_UTIL_H_ */
