#include "do-ts3d-game.h"
#include "body.h"
#include "load-texture.h"
#include "loader.h"
#include "logger.h"
#include "play-level.h"
#include "player.h"
#include "d3d.h"
#include "grow.h"
#include "json-util.h"
#include "map.h"
#include "menu.h"
#include "ent.h"
#include "pixel.h"
#include "save-state.h"
#include "ticker.h"
#include "ui-util.h"
#include "util.h"
#include "xalloc.h"
#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// The "name" of an anonymous player whose progress is not saved.
#define ANONYMOUS ""

// Character cell width of menu. Must be enough to fit width 40 text.
#define MENU_WIDTH 41

// The prefix to menu item tags referring to save names, not including the final
// slash.
#define SAVE_PFX_NO_SLASH "save"
// The same as above, but with the final slash. Evaluates to "save/".
#define SAVE_PFX SAVE_PFX_NO_SLASH "/"
// The length of the above prefix, including the slash.
#define SAVE_PFX_LEN 5

struct title_state {
	d3d_camera *cam;
	d3d_vec_s pos;
	double facing;
	d3d_board *board;
	WINDOW *win;
	struct color_map *color_map;
	struct ticker *timer;
};

// Load the title screensaver map with the given loader, setting the camera and
// board in the title state. The window will be used to draw the scene later.
// -1 is returned for error.
static int load_title(struct title_state *state, WINDOW *win,
	struct ticker *timer, struct loader *ldr)
{
	struct map *map = load_map(ldr, "title");
	if (!map) return -1;
	int width, height;
	getmaxyx(win, height, width);
	state->cam = camera_with_dims(width, height);
	state->facing = 0.0;
	state->board = map->board;
	state->win = win;
	state->color_map = loader_color_map(ldr);
	state->timer = timer;
	return 0;
}

// Move the screensaver forward a tick and draw it on the window. Then wait
// until the tick time is up.
static void tick_title(struct title_state *state)
{
	if (state->cam && state->board && state->win) {
		// Produces a cool turning effect with the camera's position:
		double theta = state->facing + 1.0;
		state->pos.x = cos(PI * cos(theta))
			+ d3d_board_width(state->board) / 2.0;
		state->pos.y = sin(PI * sin(theta))
			+ d3d_board_height(state->board) / 2.0;
		state->facing -= 0.003;
		d3d_draw(state->cam, state->pos, state->facing, state->board,
			0, NULL);
		display_frame(state->cam, state->win, state->color_map);
		wrefresh(state->win);
		tick(state->timer);
	}
}

// Resize the camera according to the size of the window.
static void resize_title_camera(struct title_state *state)
{
	d3d_free_camera(state->cam);
	int width, height;
	getmaxyx(state->win, height, width);
	state->cam = camera_with_dims(width, height);
}

static void destroy_title_state(struct title_state *state)
{
	d3d_free_camera(state->cam);
}

// Load the save state list and log in as the one with the given name. If that
// name is not present, create it.
static struct save_state *get_save_state(const char *name,
	const char *state_file, struct save_states *saves, struct logger *log)
{
	FILE *from = fopen(state_file, "r");
	if (from && !save_states_init(saves, from, log)) {
		save_states_add(saves, ANONYMOUS);
		struct save_state *save = save_states_get(saves, name);
		if (!save) save = save_states_add(saves, name);
		return save;
	} else {
		logger_printf(log, LOGGER_WARNING,
			"Unable to load save state from %s; "
			"creating empty state\n", state_file);
		save_states_empty(saves);
		return save_states_add(saves, ANONYMOUS);
	}
	// from closed by save_state_init
}

// Add any links present in the save state list but absent in the items list to
// the items list. The count of items will be updated. The items must have the
// same ordering as the table.
static void add_save_links(struct menu_item **items, size_t *num,
	struct save_states *from)
{
	size_t cap = *num;
	size_t head = 0;
	const char *key;
	void **UNUSED_VAR(val);
	SAVE_STATES_FOR_EACH(from, key, val) {
		// Skip unnamed saves:
		if (!strcmp(key, ANONYMOUS)) continue;
		struct menu_item *item = (*items) + head;
		if (head >= *num || strcmp(key, item->title)) {
			GROWE(*items, *num, cap);
			// Update the item in case the list moved in memory:
			item = (*items) + head;
			// Shift over the ones after:
			memmove(item + 1, item, (*num - 1 - head)
				* sizeof(*item));
			item->parent = NULL;
			item->kind = ITEM_TAG;
			item->tag = mid_cat(SAVE_PFX_NO_SLASH, '/', key);
			// The title is not allocated, so should not be freed:
			item->title = item->tag + SAVE_PFX_LEN;
		}
		++head;
	}
}

// Delete the selected menu item. The menu must be showing the list of saves.
// The given save set will also be updated.
static void delete_save_link(struct menu *menu, struct save_states *saves)
{
	struct menu_item freed;
	if (menu_delete_selected(menu, &freed)) {
		save_states_remove(saves, freed.tag + SAVE_PFX_LEN);
		free(freed.tag);
	}
}

// Free the list of save links that is links->items. These must have been added
// by add_save_links.
static void free_save_links(struct menu_item *links)
{
	for (size_t i = 0; i < links->n_items; ++i) {
		free(links->items[i].tag);
	}
	free(links->items);
}

// Sync the save state set with the file.
static int write_save_states(struct save_states *saves, const char *state_file,
	struct logger *log)
{
	FILE *to = fopen(state_file, "w");
	int ret = 0;
	if (!to || save_states_write(saves, to)) {
		int errnum = errno;
		FILE *print = logger_get_output(log, LOGGER_ERROR);
		if (print) {
			static const char *const lines[] = {
				"Failed to write state information to %s: %s\n",
				"The information is below. Copy this to %s or"
				" to another location (see ts3d -h output):\n"
			};
			logger_printf(log, LOGGER_ERROR, lines[0],
				state_file, strerror(errnum));
			logger_printf(log, LOGGER_ERROR, lines[1], state_file);
			save_states_write(saves, print);
		}
		ret = -1;
	}
	if (to) fclose(to);
	return ret;
}

// Get an input that is the name of a save from the menu. The menu must be in
// the input element from which the input is to be read. The name_buf is where
// the name will be put. The name will be at most buf_size - 1 characters, and
// will be NUL-terminated. title_state is for running the screensaver while
// waiting for input. An empty name signifies entry cancellation.
static void get_input(char *name_buf, size_t buf_size, struct menu *menu,
	struct title_state *title_state)
{
	int key;
	--buf_size; // Make space for the NUL-terminator from now on.
	menu_set_input(menu, name_buf, buf_size);
	for (;;) {
		key = wgetch(menu->win);
		// The name is entered:
		if (key == KEY_ENTER || key == '\n') break;
		// The entry is cancelled:
		if (key == ESC || key == KEY_LEFT) {
			*name_buf = '\0';
			return;
		}
		menu_draw(menu);
		wrefresh(menu->win);
		tick_title(title_state);
		size_t len = strlen_max(name_buf, buf_size);
		if (key == KEY_BACKSPACE || key == KEY_DC || key == DEL) {
			// Delete a character.
			if (len > 0)
				name_buf[len - 1] = '\0';
		} else if (key == ERR || !isprint(key)){
			// Key is not to be appended to the name.
			continue;
		} else if (len < buf_size) {
			// Key appended to name.
			name_buf[len] = key;
			name_buf[len + 1] = '\0';
		}
	}
}

int do_ts3d_game(const char *play_as, const char *data_dir,
	const char *state_file, struct logger *log)
{
	int ret = -1;
	struct loader ldr;
	loader_init(&ldr, data_dir);
	logger_free(loader_set_logger(&ldr, log));
	struct save_states saves;
	// Log in with the given save name, or anonymously if NULL is given.
	struct save_state *save = get_save_state(play_as ? play_as : ANONYMOUS,
		state_file, &saves, log);
	// ncurses reads ESCDELAY and waits that many ms after an ESC key press.
	// This here is lowered from "1000":
	try_setenv("ESCDELAY", "30", 0);
	initscr();
	start_color();
	// For some reason, NetBSD Curses requires these two extra calls to keep
	// the program from waiting for user input in the menu:
	timeout(0);
	getch();
	// Windows will be resized at the beginning of the first tick:
	// Window to draw the menu on:
	WINDOW *menuwin = newwin(1, 1, 0, 0);
	// Window to draw the screensaver on:
	WINDOW *titlewin = newwin(1, 1, 0, 0);
	if (!menuwin || !titlewin) {
		logger_printf(log, LOGGER_ERROR, "Cannot allocate windows\n");
		goto error_windows;
	}
	// The item to menu_redirect to. NULL means not to redirect:
	struct menu_item *redirect = NULL;
	// A synthetic input element for the New Game link. This can only be
	// redirected to, and is not part of the menu tree:
	struct menu_item new_game = {
		.kind = ITEM_INPUT,
		// Initially empty text:
		.n_items = 0,
		.tag = ""
	};
	// Another synthetic element only accessible via redirection. The list
	// of saves for the Resume Game and Delete Game links:
	struct menu_item game_list = {
		.kind = ITEM_LINKS,
		.items = NULL,
		.n_items = 0
	};
	// Populate with saves:
	add_save_links(&game_list.items, &game_list.n_items, &saves);
	// The name entry buffer. The maximum name length is 15 characters. The
	// buffer is initially empty:
	char name_buf[16] = {'\0'};
	// The menu message buffer. The maximum length is 63 characters:
	char msg_buf[64];
	struct menu menu;
	if (menu_init(&menu, data_dir, menuwin, log)) {
		logger_printf(log, LOGGER_ERROR, "Failed to load menu\n");
		goto error_menu;
	}
	if (!play_as) {
		strcpy(msg_buf, "Select a game BEFORE playing to save!");
		menu_set_message(&menu, msg_buf);
	}
	struct ticker timer;
	ticker_init(&timer, 30); // The frame wait is 30ms.
	struct title_state title_state;
	if (load_title(&title_state, titlewin, &timer, &ldr)) {
		logger_printf(log, LOGGER_WARNING,
			"Failed to load title screensaver\n");
	}
	color_map_apply(loader_color_map(&ldr));
	loader_print_summary(&ldr);
	curs_set(0); // Hide the cursor.
	noecho(); // Hide input characters.
	wtimeout(menuwin, 0); // Don't wait for input in the menu.
	keypad(menuwin, TRUE); // Automatically detect arrow keys and so on.
	int key; // Input key.
	// The name of the save the user is deleting. It has been selected
	// once, and must be selected again to be deleted. NULL indicates
	// that deletion is not occurring:
	const char *delete_save = NULL;
	// The save managing state. SWITCHING means in the Resume Game
	// menu, and DELETING means in the Delete Game menu. The initial
	// value is irrelevant:
	enum {
		SWITCHING,
		DELETING
	} save_managing = SWITCHING;
	bool first_tick = true;
	int known_lines, known_cols;
	for (;;) {
		// The selected menu item (used later):
		struct menu_item *selected;
		// The map prerequisite found (used later):
		char *prereq;
		// Resize screen structures if needed:
		if (first_tick || sync_screen_size(known_lines, known_cols)) {
			known_lines = LINES;
			known_cols = COLS;
			int menu_width = COLS >= MENU_WIDTH ? MENU_WIDTH : COLS;
			int title_width = COLS - menu_width;
			wresize(menuwin, LINES, menu_width);
			if (titlewin) {
				wresize(titlewin, LINES, title_width);
				mvwin(titlewin, 0, menu_width);
				resize_title_camera(&title_state);
			}
			first_tick = false;
		}
		menu_draw(&menu);
		wrefresh(menuwin);
		tick_title(&title_state);
		switch (key = wgetch(menuwin)) {
		redirect: // Simulate entering the redirected-to item:
			redirect->title = selected->title;
			// Simulate childhood:
			redirect->parent = selected->parent;
			/* FALLTHROUGH */
		case 'd':
		case '\n':
		case KEY_ENTER:
		case KEY_RIGHT:
			menu_clear_message(&menu);
			switch (redirect ? menu_redirect(&menu, redirect)
				: menu_enter(&menu))
			{
			case ACTION_BLOCKED:
				beep();
				break;
			case ACTION_WENT:
				break;
			case ACTION_INPUT:
				menu_clear_message(&menu);
				for (;;) {
					get_input(name_buf, sizeof(name_buf),
						&menu, &title_state);
					if (*name_buf
					 && save_states_get(&saves, name_buf)) {
						menu_set_message(&menu,
							"Name already taken");
					} else {
						// Cancelled or accepted.
						break;
					}
				}
				if (*name_buf) {
					// Accepted.
					save = save_states_add(&saves,
						name_buf);
					snprintf(msg_buf, sizeof(msg_buf),
						"Switched to new save %s",
						name_buf);
					add_save_links(&game_list.items,
						&game_list.n_items, &saves);
					// Clear name buffer:
					name_buf[0] = '\0';
				} else {
					strcpy(msg_buf, "Save creation"
							" cancelled");
				}
				menu_escape(&menu);
				menu_set_message(&menu, msg_buf);
				break;
			case ACTION_TAG:
				selected = menu_get_selected(&menu);
				if (!selected || !selected->tag) break;
				if (!strcmp(selected->tag, "act/resume-game")) {
					const char *name =
						save_state_name(save);
					if (strcmp(name, ANONYMOUS)) {
						// Not anonymous.
						snprintf(msg_buf,
							sizeof(msg_buf),
							"Playing as %s", name);
						menu_set_message(&menu,
							msg_buf);
					} else {
						menu_set_message(&menu,
							"Playing anonymously");
					}
					save_managing = SWITCHING;
					redirect = &game_list;
					goto redirect;
				} else if (!strcmp(selected->tag,
						"act/new-game")) {
					redirect = &new_game;
					goto redirect;
				} else if (!strcmp(selected->tag,
						"act/delete-game")) {
					menu_set_message(&menu,
						"Select twice to delete");
					save_managing = DELETING;
					delete_save = NULL;
					redirect = &game_list;
					goto redirect;
				} else if (!strcmp(selected->tag, "act/quit")) {
					goto end;
				} else if (!strncmp(selected->tag, SAVE_PFX,
						SAVE_PFX_LEN))
				{
					// Save from the list was selected.
					const char *name = selected->tag
						+ SAVE_PFX_LEN;
					if (save_managing == DELETING) {
						if (delete_save && !strcmp(name,
								delete_save)) {
							// Delete game.
							if (!strcmp(name,
								save_state_name(
									save)))
								// Set save to
								// anonymous.
								save = // squish
								save_states_get(
								&saves,
								ANONYMOUS);
							delete_save_link(
								&menu, &saves);
						} else {
							// Confirmation needed.
							delete_save = name;
						}
						// Don't reset redirect to NULL:
						continue;
					} else if (save_managing == SWITCHING) {
						save = save_states_get(&saves,
							name);
						snprintf(msg_buf,
							sizeof(msg_buf),
							"Switched to %s", name);
						menu_set_message(&menu,
							msg_buf);
					}
					break;
				}
				// No action taken above; load the map:
				prereq = map_prereq(&ldr, selected->tag);
				if (prereq
				 && !save_state_is_complete(save, prereq)) {
					menu_set_message(&menu, "Level locked");
					beep();
				} else if (play_level(data_dir, save,
					selected->tag, &timer, log))
				{
					menu_set_message(&menu,
						"Error loading map");
					beep();
				} else {
					menu_clear_message(&menu);
				}
				free(prereq);
				// Reset colors:
				color_map_apply(loader_color_map(&ldr));
				// Redraw the screen next time:
				touchwin(menuwin);
				touchwin(titlewin);
				break;
			}
			break;
		case 'a':
		case ESC:
		case KEY_LEFT:
			// Leave submenu.
			if (menu_escape(&menu)) {
				menu_clear_message(&menu);
			} else {
				beep();
			}
			break;
		case 'w':
		case KEY_UP:
		case KEY_BACKSPACE:
		case KEY_SR:
			// Scroll up.
			if (!menu_scroll(&menu, -1)) beep();
			break;
		case 's':
		case ' ':
		case KEY_DOWN:
		case KEY_SF:
			// Scroll down.
			if (!menu_scroll(&menu, 1)) beep();
			break;
		case 'g':
			// Scroll to beginning.
			menu_scroll(&menu, -999);
			break;
		case 'G':
			// Scroll to end.
			menu_scroll(&menu, 999);
			break;
		case KEY_HOME:
			// Return to root menu.
			while (menu_escape(&menu)) {
				menu_clear_message(&menu);
			}
			break;
		case 'x':
			// Quit shortcut.
			goto end;
		default:
			if (isdigit(key)) {
				// Goto nth menu item.
				int to = key == '0' ? 9 : key - '0' - 1;
				menu_scroll(&menu, -999);
				if (menu_scroll(&menu, to) != to) beep();
				break;
			}
			break;
		}
		redirect = NULL;
	}
end:
	destroy_title_state(&title_state);
	menu_destroy(&menu);
	// Don't save the progress of the anonymous:
	save_states_remove(&saves, ANONYMOUS);
	ret = write_save_states(&saves, state_file, log);
error_menu:
	free_save_links(&game_list);
error_windows:
	if (menuwin) delwin(menuwin);
	if (titlewin) delwin(titlewin);
	endwin();
	save_states_destroy(&saves);
	loader_free(&ldr);
	return ret;
}
