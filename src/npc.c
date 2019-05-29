#include "npc.h"
#include "dir-iter.h"
#include "grow.h"
#include "json.h"
#include "json-util.h"
#include "string.h"
#include "translate-json-key.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
	json_reader rdr;
	if (init_json_reader(path, &rdr)) goto error_init_json_reader;
	npc->flags = 0;
	npc->name = "";
	npc->width = 1.0;
	npc->height = 1.0;
	npc->transparent = ' ';
	npc->n_frames = 0;
	npc->frames = NULL;
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
			key = item.key.bytes;
			switch (translate_json_key(key)) {
			case JKEY_NULL:
				goto invalid_json_fmt;
			case JKEY_name:
				if (item.type != JSON_STRING)
					goto invalid_json_fmt;
				npc->name = item.val.str.bytes;
				break;
			case JKEY_width:
				if (item.type != JSON_NUMBER)
					goto invalid_json_fmt;
				npc->width = item.val.num;
				break;
			case JKEY_height:
				if (item.type != JSON_NUMBER)
					goto invalid_json_fmt;
				npc->height = item.val.num;
				break;
			case JKEY_transparent:
				if (item.type == JSON_NULL) continue;
				if (item.type != JSON_STRING)
					goto invalid_json_fmt;
				if (item.val.str.len < 1) continue;
				npc->transparent = *item.val.str.bytes;
				break;
			case JKEY_frames:
				if (item.type != JSON_LIST)
					goto invalid_json_fmt;
				errno = 0;
				if (parse_frames(npc, &rdr, txtrs)) {
					if (errno) goto error_parse_frames;
					// TODO: invalid_json_fmt
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
	free_json_reader(&rdr);
	return 0;

error_json_read_item:
error_parse_frames:
	free(key);
	free(npc->frames);
	free_json_reader(&rdr);
error_init_json_reader:
	return -1;

invalid_json:
	print_json_error(&rdr, &item);
invalid_json_fmt:
	free(key);
	free_json_reader(&rdr);
	npc->flags |= NPC_INVALID;
	return 0;
}

struct npc_type_iter_arg {
	table *npcs;
	table *txtrs;
	const char *dirpath;
};

static int npc_type_iter(struct dirent *ent, void *ctx)
{
	struct npc_type_iter_arg *arg = ctx;
	table *npcs = arg->npcs;
	table *txtrs = arg->txtrs;
	size_t cap = 0;
	struct string path = {0};
	char *suffix = strstr(ent->d_name, ".json");
	if (!suffix) return 0;
	if (string_pushz(&path, &cap, arg->dirpath)
	 || string_pushc(&path, &cap, '/')
	 || string_pushz(&path, &cap, ent->d_name)
	 || string_pushc(&path, &cap, '\0'))
		goto error_push;
	size_t base_len = suffix - ent->d_name;
	char *type_name = malloc(base_len+1);
	if (!type_name) goto error_alloc_type_name;
	memcpy(type_name, ent->d_name, base_len);
	type_name[base_len] = '\0';
	struct npc_type *npc = malloc(sizeof(*npc));
	if (!npc) goto error_alloc_npc;
	if (load_npc_type(path.text, npc, txtrs)) goto error_load_npc_type;
	if (table_add(npcs, type_name, npc)) goto error_table_add;
	return 0;

error_table_add:
	npc_type_free(npc);
error_load_npc_type:
	free(npc);
error_alloc_npc:
	free(type_name);
error_alloc_type_name:
	free(path.text);
error_push:
	return -1;
}

int load_npc_types(const char *dirpath, table *npcs, table *txtrs)
{
	struct npc_type_iter_arg arg = {
		.npcs = npcs,
		.txtrs = txtrs,
		.dirpath = dirpath
	};
	if (table_init(npcs, 32)) return -1;
	if (dir_iter(dirpath, npc_type_iter, &arg)) {
		table_free(npcs);
		return -1;
	}
	table_freeze(npcs);
	return 0;
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

void npc_type_free(struct npc_type *npc)
{
	free(npc->name);
	free(npc->frames);
}
