#include "translate-json-key.h"
#include "table.h"
#include <stdbool.h>
#include <stdint.h>

static bool json_key_tab_created = false;
static table json_key_tab;
int create_json_key_tab(void)
{
	if (json_key_tab_created) return 0;
	if (table_init(&json_key_tab, JKEY_COUNT)) return -1;
#define JKEY(name, key) \
	if (table_add(&json_key_tab, key, (void *)(intptr_t)JKEY_##name)) \
		goto error_table_add;
#include "json-keys.h"
#undef JKEY
	table_freeze(&json_key_tab);
	json_key_tab_created = true;
	return 0;

error_table_add:
	table_free(&json_key_tab);
	return -1;
}

enum json_key_code translate_json_key(const char *key)
{
	if (!key) return JKEY_NULL;
	intptr_t *codep = (void *)table_get(&json_key_tab, key);
	if (!codep) return JKEY_NOT_FOUND;
	return *codep;
}

void free_json_key_tab(void)
{
	if (!json_key_tab_created) return;
	table_free(&json_key_tab);
	json_key_tab_created = false;
}
