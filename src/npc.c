#include "npc.h"
#include "dir-iter.h"
#include "grow.h"
#include "json.h"
#include "json-util.h"
#include "pixel.h"
#include "load-texture.h"
#include "string.h"
#include "util.h"
#include "xalloc.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void parse_frame(struct json_node *node, struct npc_frame *frame,
	table *txtrs)
{
	const char *txtr_name = EMPTY_TXTR_KEY;
	long duration = 1;
	switch (node->kind) {
	case JN_STRING:
		txtr_name = node->d.str;
		node->d.str = NULL;
		break;
	case JN_LIST:
		if (node->d.list.n_vals < 1
		 || node->d.list.vals[0].kind != JN_STRING) break;
		txtr_name = node->d.list.vals[0].d.str;
		node->d.list.vals[0].d.str = NULL;
		if (node->d.list.n_vals < 2
		 || node->d.list.vals[1].kind != JN_NUMBER) break;
		duration = node->d.list.vals[1].d.num;
		break;
	default:
		break;
	}
	void **txtrp = table_get(txtrs, txtr_name);
	if (!txtrp) txtrp = table_get(txtrs, EMPTY_TXTR_KEY);
	frame->txtr = *txtrp;
	frame->duration = duration;
}

int load_npc_type(const char *path, struct npc_type *npc, table *txtrs)
{
	struct json_node jtree;
	npc->flags = NPC_INVALID;
	npc->name = NULL;
	npc->frames = NULL;
	npc->n_frames = 0;
	if (parse_json_tree(path, &jtree)) return -1;
	if (jtree.kind != JN_MAP) goto end;
	union json_node_data *got;
	npc->flags = 0;
	if ((got = json_map_get(&jtree, "name", JN_STRING))) {
		npc->name = got->str;
		got->str = NULL;
	} else {
		npc->name = str_dup("");
	}
	npc->width = 1.0;
	if ((got = json_map_get(&jtree, "width", JN_NUMBER)))
		npc->width = got->num;
	npc->height = 1.0;
	if ((got = json_map_get(&jtree, "height", JN_NUMBER)))
		npc->height = got->num;
	npc->transparent = EMPTY_PIXEL;
	if ((got = json_map_get(&jtree, "transparent", JN_STRING))
			&& *got->str)
		npc->height = *got->str;
	if ((got = json_map_get(&jtree, "frames", JN_LIST))) {
		npc->n_frames = got->list.n_vals;
		npc->frames = xmalloc(npc->n_frames * sizeof(*npc->frames));
		for (size_t i = 0; i < npc->n_frames; ++i) {
			parse_frame(&got->list.vals[i], &npc->frames[i], txtrs);
		}
	}
end:
	free_json_tree(&jtree);
	return 0;
}

struct npc_type_iter_arg {
	table *npcs;
	table *txtrs;
	const char *dirpath;
};

static int npc_type_iter(struct dirent *ent, void *ctx)
{
	int retval = -1;
	struct npc_type_iter_arg *arg = ctx;
	table *npcs = arg->npcs;
	table *txtrs = arg->txtrs;
	size_t cap = 0;
	struct string path = {0};
	char *suffix = strstr(ent->d_name, ".json");
	if (!suffix) return 0;
	string_pushz(&path, &cap, arg->dirpath);
	string_pushc(&path, &cap, '/');
	string_pushz(&path, &cap, ent->d_name);
	string_pushc(&path, &cap, '\0');
	*suffix = '\0';
	struct npc_type *npc = xmalloc(sizeof(*npc));
	if (load_npc_type(path.text, npc, txtrs)) goto error_load_npc_type;
	if (npc->flags & NPC_INVALID) {
		retval = 0;
		goto invalid_npc;
	}
	if (table_add(npcs, str_dup(ent->d_name), npc)) goto error_table_add;
	return 0;

invalid_npc:
error_table_add:
	npc_type_free(npc);
error_load_npc_type:
	free(npc);
	free(path.text);
	return retval;
}

static int free_npc_type_entry(const char *key, void **val)
{
	free((char *)key);
	npc_type_free(*val);
	free(*val);
	return 0;
}

int load_npc_types(const char *dirpath, table *npcs, table *txtrs)
{
	struct npc_type_iter_arg arg = {
		.npcs = npcs,
		.txtrs = txtrs,
		.dirpath = dirpath
	};
	table_init(npcs, 32);
	if (dir_iter(dirpath, npc_type_iter, &arg)) {
		table_each(npcs, free_npc_type_entry);
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
	string_init(&str, cap);
	string_pushz(&str, &cap, "npc_type { name = \"");
	string_pushz(&str, &cap, npc->name);
	string_pushn(&str, &cap, fmt_buf, sbprintf(fmt_buf, sizeof(fmt_buf),
		"\", width = %f, height = %f", npc->width, npc->height));
	if (npc->transparent >= 0) {
		string_pushn(&str, &cap, fmt_buf, sbprintf(fmt_buf,
			sizeof(fmt_buf),
			", transparent = '%c'", npc->transparent));
	}
	string_pushz(&str, &cap, ", frames = ");
	if (npc->n_frames > 0) {
		const char *before = "[ ";
		for (size_t i = 0; i < npc->n_frames; ++i) {
			const d3d_texture *txtr = npc->frames[i].txtr;
			string_pushn(&str, &cap, before, 2);
			string_pushn(&str, &cap, fmt_buf, sbprintf(fmt_buf,
				sizeof(fmt_buf),
				"texture { width = %zu, height = %zu }",
				d3d_texture_width(txtr),
				d3d_texture_height(txtr)));
			before = ", ";
		}
		string_pushn(&str, &cap, " ]", 2);
	} else {
		string_pushn(&str, &cap, "[]", 2);
	}
	string_pushn(&str, &cap, " }", 3);
	string_shrink_to_fit(&str);
	return str.text;
}

void npc_type_free(struct npc_type *npc)
{
	free(npc->name);
	free(npc->frames);
}
