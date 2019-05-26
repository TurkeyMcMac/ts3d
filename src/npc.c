#include "npc.h"
#include "grow.h"
#include "json.h"
#include "string.h"
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static table json_table;
static int init_json_table(void)
{
	static bool json_table_created = false;
	if (json_table_created) return 0;
	if (table_init(&json_table, 8)) return -1;
#define ENTRY(name) if (table_add(&json_table, #name, \
		(void *)offsetof(struct npc_type, name))) goto error_table_add;
	ENTRY(name);
	ENTRY(width);
	ENTRY(height);
	ENTRY(transparent);
	ENTRY(frames);
#undef ENTRY
	table_freeze(&json_table);
	json_table_created = true;
	return 0;

error_table_add:
	table_free(&json_table);
	return -1;
}

static int parse_frames(struct npc_type *npc, json_reader *rdr, table *txtrs)
{
	size_t npc_cap = 0;
	npc->n_frames = 0;
	char *key;
	for (;;) {
		key = NULL;
		struct json_item item;
		if (json_read_item(rdr, &item) < 0) goto error;
		if (item.type == JSON_END_LIST) break;
		if (item.type != JSON_STRING) goto error;
		key = item.val.str.bytes;
		void **txtr = table_get(txtrs, key);
		if (!txtr) {
			npc->flags |= NPC_INVALID_TEXTURE;
			goto error;
		}
		const d3d_texture **place = GROWE(&npc->frames, &npc->n_frames,
			&npc_cap, sizeof(*npc->frames));
		if (!place) goto error;
		*place = *txtr;
	}
	return 0;

error:
	free(key);
	return -1;
}

int load_npc_type(const char *path, struct npc_type *npc, table *txtrs)
{
	int errnum;
	char buf[BUFSIZ];
	FILE *file = fopen(path, "r");
	if (!file) goto error_fopen;
	json_reader rdr;
	if (json_alloc(&rdr, NULL, 8, malloc, free, realloc))
		goto error_json_alloc;
	json_source_file(&rdr, buf, sizeof(buf), file);
	npc->flags = 0;
	npc->name = "";
	npc->width = 1.0;
	npc->height = 1.0;
	npc->transparent = ' ';
	npc->n_frames = 0;
	npc->frames = NULL;
	if (init_json_table()) goto error_init_json_table;
	struct json_item item;
	char *key = NULL;
	if (json_read_item(&rdr, &item) >= 0) {
		if (item.type != JSON_MAP) goto invalid_json;
	} else {
		goto error_json_read_item;
	}
	for (;;) {
		key = NULL;
		errno = 0;
		if (json_read_item(&rdr, &item) >= 0) {
			if (item.type == JSON_EMPTY) break;
			if (!item.key.bytes) goto invalid_json;
			key = item.key.bytes;
			intptr_t *field = (void *)table_get(&json_table, key);
			if (!field) continue;
			switch (*field) {
			case offsetof(struct npc_type, name):
				if (item.type != JSON_STRING) goto invalid_json;
				npc->name = item.val.str.bytes;
				break;
			case offsetof(struct npc_type, width):
				if (item.type != JSON_NUMBER) goto invalid_json;
				npc->width = item.val.num;
				break;
			case offsetof(struct npc_type, height):
				if (item.type != JSON_NUMBER) goto invalid_json;
				npc->height = item.val.num;
				break;
			case offsetof(struct npc_type, transparent):
				if (item.type == JSON_NULL) continue;
				if (item.type != JSON_STRING) goto invalid_json;
				if (item.val.str.len < 1) continue;
				npc->transparent = *item.val.str.bytes;
				break;
			case offsetof(struct npc_type, frames):
				if (item.type != JSON_LIST) goto invalid_json;
				errno = 0;
				if (parse_frames(npc, &rdr, txtrs)) {
					if (errno) goto error_parse_frames;
					goto invalid_json;
				}
				break;
			default:
				break;
			}
		} else if (errno) {
			goto error_json_read_item;
		} else {
			goto invalid_json;
		}
	}
	json_free(&rdr);
	fclose(file);
	return 0;

error_json_read_item:
error_parse_frames:
	free(key);
error_init_json_table:
	free(npc->frames);
	json_free(&rdr);
error_json_alloc:
	errnum = errno;
	fclose(file);
	errno = errnum;
error_fopen:
	return -1;

invalid_json:
	free(key);
	json_free(&rdr);
	fclose(file);
	npc->flags |= NPC_INVALID;
	return 0;
}

int load_npc_types(const char *dirpath, table *npcs, table *txtrs)
{
	int errnum;
	DIR *dir = opendir(dirpath);
	if (!dir) goto error_opendir;
	struct dirent *ent;
	size_t path_len = 0;
	size_t path_cap = 32;
	char *path = malloc(path_cap);
	if (!path) goto error_malloc;
	char *type_name = NULL;
	table_init(npcs, 16);
	errno = 0;
	while ((ent = readdir(dir))) {
		if (*ent->d_name == '.') continue;
		// XXX Not quite correct, but good enough if the name doesn't
		// have ".json" in the middle:
		char *suffix = strstr(ent->d_name, ".json");
		if (!suffix) continue;
		while ((path_len = snprintf(path, path_cap, "%s/%s", dirpath,
				ent->d_name) + 1) > path_cap) {
			path_cap = path_len;
			path = realloc(path, path_cap);
			if (!path) goto error_realloc;
		}
		size_t base_len = suffix - ent->d_name;
		type_name = malloc(base_len+1);
		if (!type_name) goto error_alloc_type_name;
		memcpy(type_name, ent->d_name, base_len);
		type_name[base_len] = '\0';
		struct npc_type *npc = malloc(sizeof(*npc));
		if (!npc) goto error_alloc_npc;
		if (load_npc_type(path, npc, txtrs)) goto error_load_npc_type;
		if (table_add(npcs, type_name, npc)) goto error_table_add;
	}
	if (errno) goto error_readdir;
	free(path);
	table_freeze(npcs);
	return 0;

error_table_add:
error_load_npc_type:
error_alloc_npc:
	free(type_name);
error_alloc_type_name:
error_realloc:
error_readdir:
	// TODO: free allocated keys and stuff
	table_free(txtrs);
	free(path);
error_malloc:
	errnum = errno;
	closedir(dir);
	errno = errnum;
error_opendir:
	return -1;
}

char *npc_type_to_string(const struct npc_type *npc)
{
	char fmt_buf[128];
	size_t cap = 64;
	struct string str;
	if (string_init(&str, cap)) goto error_string_init;
	if (string_pushz(&str, &cap, "npc_type { name = \"")
	 || string_pushz(&str, &cap, npc->name)) goto error_push;
	snprintf(fmt_buf, sizeof(fmt_buf), "\", width = %f, height = %f",
		npc->width, npc->height);
	if (string_pushz(&str, &cap, fmt_buf)) goto error_push;
	if (npc->transparent >= 0) {
		snprintf(fmt_buf, sizeof(fmt_buf), ", transparent = '%c'",
			npc->transparent);
		if (string_pushz(&str, &cap, fmt_buf)) goto error_push;
	}
	if (string_pushz(&str, &cap, ", frames = ")) goto error_push;
	if (npc->n_frames > 0) {
		const char *before = "[ ";
		for (size_t i = 0; i < npc->n_frames; ++i) {
			const d3d_texture *txtr = npc->frames[i];
			if (string_pushn(&str, &cap, before, 2))
				goto error_push;
			snprintf(fmt_buf, sizeof(fmt_buf),
				"texture { width = %lu, height = %lu }",
				d3d_texture_width(txtr),
				d3d_texture_height(txtr));
			if (string_pushz(&str, &cap, fmt_buf)) goto error_push;
			before = ", ";
		}
		if (string_pushn(&str, &cap, " ]", 2)) goto error_push;
	} else {
		if (string_pushn(&str, &cap, "[]", 2)) goto error_push;
	}
	if (string_pushn(&str, &cap, " }", 2)) goto error_push;
	string_shrink_to_fit(&str);
	return str.text;

error_push:
	free(str.text);
error_string_init:
	return NULL;
}
