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
		if (node->d.list.n_vals < 2
		 || node->d.list.vals[1].kind != JN_NUMBER) break;
		duration = node->d.list.vals[1].d.num;
		break;
	default:
		break;
	}
	d3d_texture *txtr = load_texture(ldr, txtr_name);
	frame->txtr = txtr ? txtr : loader_empty_texture(ldr);
	frame->duration = duration;
}

struct npc_type *load_npc_type(struct loader *ldr, const char *name)
{
	struct logger *log = loader_logger(ldr);
	FILE *file;
	struct npc_type *npc, **npcp;
	npcp = loader_npc(ldr, name, &file);
	if (!npcp) return NULL;
	npc = *npcp;
	if (npc) return npc;
	npc = malloc(sizeof(*npc));
	struct json_node jtree;
	npc->name = str_dup(name);
	npc->frames = NULL;
	npc->n_frames = 0;
	npc->death_spawn = NULL;
	npc->lifetime = -1;
	if (parse_json_tree(name, file, log, &jtree)) return NULL;
	if (jtree.kind != JN_MAP) {
		if (jtree.kind != JN_ERROR)
			logger_printf(log, LOGGER_WARNING,
				"NPC type \"%s\" is not a JSON dictionary\n",
				name);
		goto end;
	}
	union json_node_data *got;
	if ((got = json_map_get(&jtree, "name", JN_STRING))) {
		free(npc->name);
		npc->name = got->str;
		got->str = NULL;
	} else {
		logger_printf(log, LOGGER_WARNING,
			"NPC type \"%s\" does not have a \"name\" attribute\n",
			name);
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
	if ((got = json_map_get(&jtree, "death_spawn", JN_STRING))) {
		// To prevent infinite recursion with infinite cycles:
		*npcp = npc;
		npc->death_spawn = load_npc_type(ldr, got->str);
	}
	if ((got = json_map_get(&jtree, "lifetime", JN_NUMBER)))
		npc->lifetime = got->num;
end:
	if (npc->n_frames == 0) {
		npc->frames = xrealloc(npc->frames, sizeof(*npc->frames));
		npc->frames[0].txtr = loader_empty_texture(ldr);
		npc->frames[0].duration = 0;
	}
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
	if (!npc) return;
	free(npc->name);
	free(npc->frames);
	free(npc);
}

void npc_init(struct npc *npc, struct npc_type *type, d3d_sprite_s *sprite,
	const d3d_vec_s *pos)
{
	npc->type = type;
	npc->sprite = sprite;
	npc->lifetime = type->lifetime;
	npc->frame = 0;
	npc->frame_duration = type->frames[0].duration;
	sprite->txtr = type->frames[0].txtr;
	sprite->transparent = type->transparent;
	sprite->pos = *pos;
	sprite->scale.x = type->width;
	sprite->scale.y = type->height;
}

void npc_tick(struct npc *npc)
{
	if (npc->type->lifetime < 0 || --npc->lifetime > 0) {
		if (npc->frame_duration-- <= 0) {
			if (++npc->frame >= npc->type->n_frames) npc->frame = 0;
			npc->frame_duration =
				npc->type->frames[npc->frame].duration;
			npc->sprite->txtr = npc->type->frames[npc->frame].txtr;
		}
	} else if (npc->type->death_spawn) {
		npc_destroy(npc);
		npc_init(npc, npc->type->death_spawn, npc->sprite,
			&npc->sprite->pos);
	} else {
		npc->lifetime = 0;
	}
}

void npc_relocate(struct npc *npc, struct npc *to_npc, d3d_sprite_s *to_sprite)
{
	*to_npc = *npc;
	*to_sprite = *to_npc->sprite;
	to_npc->sprite = to_sprite;
}

bool npc_is_dead(const struct npc *npc)
{
	return npc->type->lifetime >= 0 && npc->lifetime <= 0;
}

void npc_destroy(struct npc *npc)
{
	(void)npc;
}
