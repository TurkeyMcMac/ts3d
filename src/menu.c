#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "json-util.h"
#include "menu.h"
#include "read-lines.h"
#include "xalloc.h"

static bool has_items(const struct menu_item *item)
{
	return item->items && item->n_items > 0;
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
	if (item->kind != ITEM_INPUT) free(item->tag);
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
	item->tag = NULL;
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
		item->tag = got->str;
		item->n_items = 0;
		got->str = NULL;
	} else if ((got = json_map_get(json, "input", JN_BOOLEAN))
			&& got->boolean) {
		item->kind = ITEM_INPUT;
		item->n_items = 0;
		item->tag = "";
	} else if ((got = json_map_get(json, "tag", JN_STRING))) {
		item->kind = ITEM_TAG;
		item->tag = got->str;
		got->str = NULL;
	} else {
		item->kind = ITEM_INERT;
	}
	item->place = 0;
	item->frame = 0;
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

struct menu_item *menu_get_current(struct menu *menu)
{
	return menu->current;
}

struct menu_item *menu_get_selected(struct menu *menu)
{
	if (!menu->current
	 || menu->current->kind != ITEM_LINKS
	 || (size_t)menu->current->place >= menu->current->n_items)
		return NULL;
	return &menu->current->items[menu->current->place];
}

static void text_n_items(struct menu *menu, struct menu_item *text)
{
	int maxy, maxx;
	getmaxyx(menu->win, maxy, maxx);
	size_t lines = (size_t)maxy - 3;
	maxy = maxx; // Suppress unused warning.
	if (menu->n_lines > lines) {
		text->n_items = menu->n_lines - lines;
	} else {
		text->n_items = 1;
	}
}

int menu_scroll(struct menu *menu, int amount)
{
	struct menu_item *current = menu->current;
	if (current->kind == ITEM_INPUT) return 0;
	if (current->kind == ITEM_TEXT) text_n_items(menu, current);
	if (current->n_items <= 0) return 0;
	int last_place = current->place;
	current->place =
		CLAMP(current->place + amount, 0, (int)current->n_items - 1);
	if (current->kind == ITEM_LINKS) {
		int maxy, maxx;
		getmaxyx(menu->win, maxy, maxx);
		maxy -= 2;
		if ((int)current->n_items < maxy) {
			current->frame = 0;
		} else {
			current->frame = CLAMP(current->frame,
				current->place - maxy + 1, current->place);
		}
		maxy = maxx; // Suppress unused warning.
	}
	return current->place - last_place;
}

static void enter(struct menu *menu, struct menu_item *into)
{
	menu->current = into;
	werase(menu->win);
}

bool menu_set_input(struct menu *menu, char *buf, size_t size)
{
	struct menu_item *current = menu->current;
	if (current->kind != ITEM_INPUT) return false;
	current->tag = buf;
	current->n_items = size;
	return true;
}

enum menu_action menu_enter(struct menu *menu)
{
	struct menu_item *current = menu->current;
	if (!has_items(current)) return ACTION_BLOCKED;
	struct menu_item *into = &current->items[current->place];
	return menu_redirect(menu, into);
}

enum menu_action menu_redirect(struct menu *menu, struct menu_item *into)
{
	switch (into->kind) {
		FILE *txtfile;
		char *fname;
	case ITEM_INERT:
		return ACTION_BLOCKED;
	case ITEM_LINKS:
		enter(menu, into);
		return ACTION_WENT;
	case ITEM_TEXT:
		fname = mid_cat(menu->root_dir, '/', into->tag);
		if (!(txtfile = fopen(fname, "r"))
		 || !(menu->lines = read_lines(txtfile, &menu->n_lines))) {
			free(fname);
			menu_set_message(menu, "Failed to read file");
			return ACTION_BLOCKED;
		}
		free(fname);
		text_n_items(menu, into);
		enter(menu, into);
		return ACTION_WENT;
	case ITEM_INPUT:
		enter(menu, into);
		return ACTION_INPUT;
	case ITEM_TAG:
		return ACTION_TAG;
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

bool menu_delete_selected(struct menu *menu, struct menu_item *move_to)
{
	struct menu_item *current = menu_get_current(menu);
	struct menu_item *selected = menu_get_selected(menu);
	if (!selected) return false;
	*move_to = *selected;
	--current->n_items;
	memmove(selected, selected + 1,
		(current->n_items - (selected - current->items))
			* sizeof(*selected));
	menu_scroll(menu, 0);
	return true;
}

void menu_draw(struct menu *menu)
{
	int i = 0;
	struct menu_item *current = menu->current;
	wattron(menu->win, A_UNDERLINE);
	mvwaddstr(menu->win, 0, 0, current->title);
	wattroff(menu->win, A_UNDERLINE);
	int maxy, maxx;
	getmaxyx(menu->win, maxy, maxx);
	--maxy;
	switch (current->kind) {
	case ITEM_LINKS:
		for (int l = current->frame;
		     l < (int)current->n_items && ++i < maxy; ++l) {
			int reverse = l == current->place;
			wmove(menu->win, i, 0);
			if (reverse) wattron(menu->win, A_REVERSE);
			wprintw(menu->win,  "%2d. %s ",
				l + 1, current->items[l].title);
			if (reverse) wattroff(menu->win, A_REVERSE);
			wclrtoeol(menu->win);
		}
		break;
	case ITEM_TEXT:
		for (int l = current->place;
		     l < (int)menu->n_lines && ++i < maxy; ++l) {
			struct string *line = &menu->lines[l];
			wmove(menu->win, i, 0);
			waddnstr(menu->win, line->text, line->len);
			wclrtoeol(menu->win);
		}
		break;
	case ITEM_INPUT:
		wattron(menu->win, A_UNDERLINE);
		mvwprintw(menu->win, 2, 1, "%*.*s",
			-(int)current->n_items, (int)current->n_items,
			current->tag);
		wattroff(menu->win, A_UNDERLINE);
		i = 2;
		break;
	default:
		break;
	}
	wclrtobot(menu->win);
	if (++i >= maxy) i = maxy;
	mvwaddstr(menu->win, i, 0, menu->msg);
	maxy = maxx; // Suppress unused warning.
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
		free(menu->lines);
	}
}
