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
	// An input element was entered. This is the same as WENT except that
	// the entered item is always an INPUT.
	ACTION_INPUT,
	// The item had a tag.
	ACTION_TAG
};

// A kind of menu item.
enum menu_kind {
	// An item which does nothing.
	ITEM_INERT,
	// An item which contains a list of other items.
	ITEM_LINKS,
	// An item which displays text from a file.
	ITEM_TEXT,
	// An item for user text input.
	ITEM_INPUT,
	// An item which has a string tag.
	ITEM_TAG
};

// An item in a list in the menu.
struct menu_item {
	// The parent menu item.
	struct menu_item *parent;
	// The displayed title of the item.
	char *title;
	// The kind of item.
	enum menu_kind kind;
	// Some item-specific data. For TAGs, this is the tag string. For TEXTs,
	// it is the path of a text file relative to the data directory. For
	// inputs, it is the pointer to the current text buffer, For other
	// kinds, it is NULL. This pointer is allocated or NULL unless the item
	// is an INPUT.
	char *tag;
	// Sub-items for LINKSs. For other kinds, this is NULL.
	struct menu_item *items;
	// The number of sub-items for LINKSs. For TEXTs, this is set to the
	// number of scrollable lines in the file when it is loaded, or 0 before
	// it has ever been loaded. For INPUTs, this is the space in characters
	// of the current input buffer. For other kinds, this is 0.
	size_t n_items;
	// The place of the cursor in the menu, from 0 to n_items minus 1. This
	// is only relevant for TEXTs and LINKSs.
	int place;
	// The position in a LINKS menu where the user is viewing. This is used
	// for scrolling so that the selected item at place is always on-screen.
	int frame;
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

// Get the currently looked-at menu item. This will never be NULL.
struct menu_item *menu_get_current(struct menu *menu);

// Get the currently selected item within the looked-at item. This is NULL if
// the looked-at item is not a LINKS.
struct menu_item *menu_get_selected(struct menu *menu);

// Scroll the viewed menu by the number of lines. Positive means down. The
// return value is the number of lines actually scrolled, which may be less than
// the requested if there are no more lines to look at.
int menu_scroll(struct menu *menu, int lines);

// Enter the currently selected menu. See enum menu_action above for more
// information. If the action is TAG, the currently selected menu item (see
// menu_get_selected) will have a non-NULL tag field.
enum menu_action menu_enter(struct menu *menu);

// Teleport to the location of the given menu item. The item can be external and
// manually constructed. It is valid for into to have a parent pointer to some
// place even if it is not part of the menu tree.
enum menu_action menu_redirect(struct menu *menu, struct menu_item *into);

// Exit the current menu to the parent. true is returned unless the menu that is
// currently viewed is the root menu.
bool menu_escape(struct menu *menu);

// Delete the selected menu item in a LINKS. This will shift over menu items
// after it in memory. Returned is whether an item was deleted. If no item was
// selected (i.e. the current item was not a LINKS) then false is returned.
// move_to is set to the removed item, which is not freed or modified. move_to
// must be freed manually.
bool menu_delete_selected(struct menu *menu, struct menu_item *move_to);

// Set the input buffer with the given size. This only applies to INPUTS, and
// returns true and does stuff only if the current item is an INPUT. The buffer
// can later be modified and changes will show up when the menu is redrawn. The
// text of the buffer must ALWAYS BE NUL TERMINATED.
bool menu_set_input(struct menu *menu, char *buf, size_t size);

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
