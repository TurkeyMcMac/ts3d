#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include "json-util.h"
#include "menu.h"
#include "read-lines.h"
#include "xalloc.h"

static bool has_items(const struct menu_item *item)
{
	return item->items && item->n_items >= 0;
}

static void destroy_item(struct menu_item *item)
{
	if (has_items(item)) {
		for (size_t i = 0; i < item->n_items; ++i) {
			destroy_item(&item->items[i]);
		}
	}
	free(item->items);
	free(item->title);
	free(item->data_src);
}

static int construct(struct menu_item *item, struct menu_item *parent,
	struct json_node *json, struct logger *log)
{
	if (json->kind != JN_MAP) {
		logger_printf(log, LOGGER_ERROR,
			"Menu item is not a dictionary\n");
		goto error_kind;
	}
	union json_node_data *got;
	if ((got = json_map_get(json, "title", JN_STRING))) {
		item->title = got->str;
		got->str = NULL;
	} else {
		logger_printf(log, LOGGER_ERROR,
			"Menu item has no or invalid \"title\" attribute\n");
		goto error_title;
	}
	item->data_src = NULL;
	item->items = NULL;
	if ((got = json_map_get(json, "items", JN_LIST))) {
		item->kind = ITEM_LINKS;
		struct json_node_data_list *list = &got->list;
		item->items = xmalloc(list->n_vals * sizeof(*item->items));
		item->n_items = 0;
		for (size_t i = 0; i < list->n_vals; ++i) {
			if (construct(&item->items[i], item, &list->vals[i], log))
				goto error_item;
			++item->n_items;
		}
	} else if ((got = json_map_get(json, "text", JN_STRING))) {
		item->kind = ITEM_TEXT;
		item->data_src = got->str;
		item->n_items = 0;
		got->str = NULL;
	} else if ((got = json_map_get(json, "map", JN_STRING))) {
		item->kind = ITEM_LEVEL;
		item->data_src = got->str;
		got->str = NULL;
	} else if (table_get(&json->d.map, "quit")) {
		item->kind = ITEM_QUIT;
	} else {
		item->kind = ITEM_INERT;
	}
	item->place = 0;
	item->parent = parent;
	return 0;

error_item:
	for (size_t i = 0; i < item->n_items; ++i) {
		destroy_item(&item->items[i]);
	}
	free(item->title);
error_title:
error_kind:
	return -1;
}

int menu_init(struct menu *menu, const char *root_dir, WINDOW *win,
	struct logger *log)
{
	char *fname = mid_cat(root_dir, '/', "menu.json");
	FILE *file = fopen(fname, "r");
	if (!file) {
		logger_printf(log, LOGGER_ERROR,
			"Menu file \"%s\" not found\n", fname);
		goto error_fopen;
	}
	struct json_node jtree;
	if (parse_json_tree("menu", file, log, &jtree)) goto error_json_parse;
	menu->root = xmalloc(sizeof(*menu->root));
	if (construct(menu->root, NULL, &jtree, log)) goto error_json_format;
	menu->win = win;
	menu->current = menu->root;
	menu->root_dir = root_dir;
	menu->msg = "";
	menu->lines = NULL;
	menu->n_lines = 0;
	free_json_tree(&jtree);
	free(fname);
	return 0;

error_json_format:
	free(menu->root);
	free_json_tree(&jtree);
error_json_parse:
error_fopen:
	free(fname);
	return -1;
}

int menu_scroll(struct menu *menu, int amount)
{
	struct menu_item *current = menu->current;
	if (current->n_items <= 0) return 0;
	int last_place = current->place;
	current->place =
		CLAMP(current->place + amount, 0, (int)current->n_items - 1);
	return current->place - last_place;
}

static void enter(struct menu *menu, struct menu_item *into)
{
	menu->current = into;
	werase(menu->win);
}

enum menu_action menu_enter(struct menu *menu, const char **mapp)
{
	struct menu_item *current = menu->current;
	if (!has_items(current)) return ACTION_BLOCKED;
	struct menu_item *into = &current->items[current->place];
	switch (into->kind) {
		FILE *txtfile;
		char *fname;
		size_t lines;
	case ITEM_INERT:
		return ACTION_BLOCKED;
	case ITEM_LINKS:
		enter(menu, into);
		return ACTION_WENT;
	case ITEM_TEXT:
		fname = mid_cat(menu->root_dir, '/', into->data_src);
		if (!(txtfile = fopen(fname, "r"))
		 || !(menu->lines = read_lines(txtfile, &menu->n_lines))) {
			free(fname);
			menu_set_message(menu, "Failed to read file");
			return ACTION_BLOCKED;
		}
		free(fname);
		lines = (size_t)getmaxy(menu->win) - 2;
		if (menu->n_lines > lines) {
			into->n_items = menu->n_lines - lines;
		} else {
			into->n_items = 1;
		}
		enter(menu, into);
		return ACTION_WENT;
	case ITEM_LEVEL:
		*mapp = into->data_src;
		return ACTION_MAP;
	case ITEM_QUIT:
		return ACTION_QUIT;
	}
	return ACTION_BLOCKED;
}

bool menu_escape(struct menu *menu)
{
	if (menu->lines) {
		for (size_t i = 0; i < menu->n_lines; ++i) {
			free(menu->lines[i].text);
		}
		free(menu->lines);
		menu->lines = NULL;
	}
	if (!menu->current->parent) return false;
	menu->current = menu->current->parent;
	werase(menu->win);
	return true;
}

void menu_draw(struct menu *menu)
{
	int i = 0;
	struct menu_item *current = menu->current;
	wattron(menu->win, A_UNDERLINE);
	mvwaddstr(menu->win, 0, 0, current->title);
	wattroff(menu->win, A_UNDERLINE);
	switch (current->kind) {
	case ITEM_LINKS:
		for (i = 0; i < (int)current->n_items; ++i) {
			int n = i + 1;
			int reverse = i == current->place;
			if (reverse) wattron(menu->win, A_REVERSE);
			mvwprintw(menu->win, n, 0,
				"%2d. %s ", n, current->items[i].title);
			if (reverse) wattroff(menu->win, A_REVERSE);
		}
		break;
	case ITEM_TEXT:
		for (int maxy = getmaxy(menu->win), l = current->place;
		     l < (int)menu->n_lines && ++i < maxy; ++l) {
			struct string *line = &menu->lines[l];
			wmove(menu->win, i, 0);
			waddnstr(menu->win, line->text, line->len);
			wclrtoeol(menu->win);
		}
		wclrtobot(menu->win);
		break;
	default:
		break;
	}
	wmove(menu->win, i + 1, 0);
	wclrtoeol(menu->win);
	waddstr(menu->win, menu->msg);
}

void menu_set_message(struct menu *menu, const char *msg)
{
	menu->msg = msg;
}

void menu_clear_message(struct menu *menu)
{
	menu->msg = "";
}

void menu_destroy(struct menu *menu)
{
	destroy_item(menu->root);
	free(menu->root);
	if (menu->lines) {
		for (size_t i = 0; i < menu->n_lines; ++i) {
			free(menu->lines[i].text);
		}
	}
}
