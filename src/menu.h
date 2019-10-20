#ifndef MENU_H_
#define MENU_H_

#include <curses.h>
#include "logger.h"

// An action taken when a menu item is entered.
enum menu_action {
	// The item could not be entered.
	ACTION_BLOCKED,
	// The item was entered to reveal a submenu.
	ACTION_WENT,
	// The item triggered the loading of a map.
	ACTION_MAP,
	// The item triggered the quitting of the game.
	ACTION_QUIT
};

// A kind of menu item.
enum menu_kind {
	// An item which does nothing.
	ITEM_INERT,
	// An item which contains a list of other items.
	ITEM_LINKS,
	// An item which displays text from a file.
	ITEM_TEXT,
	// An item which links to a map.
	ITEM_LEVEL,
	// An item which triggers quitting.
	ITEM_QUIT
};

// An item in a list in the menu.
struct menu_item {
	// The parent menu item.
	struct menu_item *parent;
	// The displayed title of the item.
	char *title;
	// The kind of item.
	enum menu_kind kind;
	// Some item-specific data. For LEVELs, this is the name of a map. For
	// TEXTs, it is the path of a text file relative to the data directory.
	// For other kinds, it is NULL.
	char *data_src;
	// Sub-items for LINKSs. For other kinds, this is NULL.
	struct menu_item *items;
	// The number of sub-items for LINKSs. For LEVELs, this is set to the
	// number of scrollable lines in the file when it is loaded, or 0 before
	// it has ever been loaded. For other kinds, this is 0.
	size_t n_items;
	// The place of the cursor in the menu, from 0 to n_items minus 1. This
	// is only relevant for LEVELs and LINKSs.
	int place;
};

// An object for traversing a menu tree, holding the relevant state inside.
struct menu {
	// The allocated root menu item, initially displayed.
	struct menu_item *root;
	// The currently displayed menu item.
	struct menu_item *current;
	// The window on which the menu is drawn.
	WINDOW *win;
	// The root directory of files loaded by the menu.
	const char *root_dir;
	// The message displayed at the bottom of the menu.
	const char *msg;
	// If a file is being viewed, the lines of that file. NULL otherwise.
	struct string *lines;
	// If a file is being viewed, the line count of that file. 0 otherwise.
	size_t n_lines;
};

// Initialize a menu from the file called "menu.json" in the root directory. The
// window will be used for drawing the menu. The logger is only used during
// menu_init. Success and failure return 0 or -1, respectively.
int menu_init(struct menu *menu, const char *data_dir, WINDOW *win,
	struct logger *log);

// Scroll the viewed menu by the number of lines. Positive means down. The
// return value is the number of lines actually scrolled, which may be less than
// the requested if there are no more lines to look at.
int menu_scroll(struct menu *menu, int lines);

// Enter the currently selected menu. See enum menu_action above for more
// information. If the action is MAP, *mapp is set to the name of the map to be
// loaded at the request of the user.
enum menu_action menu_enter(struct menu *menu, const char **mapp);

// Exit the current menu to the parent. true is returned unless the menu that is
// currently viewed is the root menu.
bool menu_escape(struct menu *menu);

// Draw the viewed menu on menu->win.
void menu_draw(struct menu *menu);

// Set the message to be displayed next time the menu is drawn.
void menu_set_message(struct menu *menu, const char *msg);

// Display an empty message next time the menu is drawn.
void menu_clear_message(struct menu *menu);

// Deallocate menu resources. This doesn't touch the window, the root directory,
// or the message.
void menu_destroy(struct menu *menu);

#endif /* MENU_H_ */
