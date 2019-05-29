#ifndef JSON_UTIL_H_
#define JSON_UTIL_H_

#include "json.h"

int init_json_reader(const char *path, json_reader *rdr);

void free_json_reader(json_reader *rdr);

void print_json_error(const json_reader *rdr, const struct json_item *item);

enum json_key_code {
	JKEY_NULL = -1,
	JKEY_NOT_FOUND = 0
#define JKEY(name) , JKEY_##name
#include "json-keys.h"
#undef JKEY
};

enum {
	JKEY_COUNT =
#define JKEY(_) +1
#include "json-keys.h"
#undef JKEY
};

void create_json_key_tab(void);

enum json_key_code translate_json_key(const char *key);

void free_json_key_tab(void);

#endif /* JSON_UTIL_H_ */
