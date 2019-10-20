#ifndef MENU_H_
#define MENU_H_

#include <curses.h>
#include "logger.h"

enum menu_action {
	ACTION_BLOCKED,
	ACTION_WENT,
	ACTION_MAP,
	ACTION_QUIT
};

enum menu_kind {
	ITEM_INERT,
	ITEM_LINKS,
	ITEM_TEXT,
	ITEM_LEVEL,
	ITEM_QUIT
};

struct menu_item {
	struct menu_item *parent;
	char *title;
	enum menu_kind kind;
	char *data_src;
	struct menu_item *items;
	size_t n_items;
	int place;
};

struct menu {
	struct menu_item *root;
	struct menu_item *current;
	WINDOW *win;
	const char *root_dir;
	const char *msg;
	struct string *lines;
	size_t n_lines;
};

int menu_init(struct menu *menu, const char *data_dir, WINDOW *win,
	struct logger *log);

int menu_scroll(struct menu *menu, int lines);

enum menu_action menu_enter(struct menu *menu, const char **mapp);

bool menu_escape(struct menu *menu);

void menu_draw(struct menu *menu);

void menu_set_message(struct menu *menu, const char *msg);

void menu_clear_message(struct menu *menu);

#endif /* MENU_H_ */
