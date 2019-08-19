#include "npc.h"
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
	struct loader *ldr)
{
	const char *txtr_name = "";
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
	d3d_texture *txtr = load_texture(ldr, txtr_name);
	frame->txtr = txtr ? txtr : d3d_new_texture(0, 0);
	frame->duration = duration;
}

struct npc_type *load_npc_type(struct loader *ldr, const char *name)
{
	FILE *file;
	struct npc_type *npc, **npcp;
	npcp = loader_npc(ldr, name, &file);
	if (!npcp) return NULL;
	npc = *npcp;
	if (npc) return npc;
	npc = malloc(sizeof(*npc));
	struct json_node jtree;
	npc->flags = NPC_INVALID;
	npc->name = NULL;
	npc->frames = NULL;
	npc->n_frames = 0;
	if (parse_json_tree(name, file, &jtree)) return NULL;
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
			parse_frame(&got->list.vals[i], &npc->frames[i], ldr);
		}
	}
end:
	free_json_tree(&jtree);
	*npcp = npc;
	return npc;
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
