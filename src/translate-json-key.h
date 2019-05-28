#ifndef TRANSLATE_JSON_KEY_H_
#define TRANSLATE_JSON_KEY_H_

enum json_key_code {
	JKEY_NULL = -1,
	JKEY_NOT_FOUND = 0
#define JKEY(name, _) , JKEY_##name
#include "json-keys.h"
#undef JKEY
};

enum {
	JKEY_COUNT =
#define JKEY(_, __) +1
#include "json-keys.h"
#undef JKEY
};

int create_json_key_tab(void);

enum json_key_code translate_json_key(const char *key);

void free_json_key_tab(void);

#endif /* TRANSLATE_JSON_KEY_H_ */
