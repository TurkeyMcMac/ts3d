#include "do-ts3d-game.h"
#include "body.h"
#include "config.h"
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

// Screen state, including the menu and screensaver. The fields are public.
struct screen_state {
	// Whether the screen state is the right size for the physical screen.
	bool sized;
	struct menu_state {
		bool initialized;
		struct menu menu;
		struct screen_area area;
	} menu;
	struct title_state {
		bool initialized;
		d3d_camera *cam;
		double facing;
		d3d_board *board;
		struct screen_area area;
		struct color_map *color_map;
	} title;
};

// The initializer for the screen state excluding the menu/title stuff.
#define SCREEN_STATE_PARTIAL_INITIALIZER {  .sized = false, \
	.menu = { .initialized = false }, .title = { .initialized = false } }

// Load the menu part of the screen state. This is pretty much just menu_init
// except the area is provided internally. The return value is the same as
// menu_init's for success/failure.
static int load_menu_state(struct menu_state *state, const char *data_dir,
	struct logger *log)
{
	// Screen area not yet initialized:
	state->area = (struct screen_area) { 0, 0, 1, 1 };
	int ret = menu_init(&state->menu, data_dir, &state->area, log);
	state->initialized = !ret;
	return ret;
}

// Load the title screensaver map with the given loader, setting the camera and
// board in the title state. The area will be used to draw the scene later.
// -1 is returned for error.
static int load_title_state(struct title_state *state, struct loader *ldr)
{
	struct map *map = load_map(ldr, TITLE_SCREEN_MAP_NAME);
	if (!map) return -1;
	// Screen area not yet initialized:
	state->area = (struct screen_area) { 0, 0, 1, 1 };
	state->cam = camera_with_dims(state->area.width, state->area.height);
	state->facing = 0.0;
	state->board = map->board;
	state->color_map = loader_color_map(ldr);
	state->initialized = true;
	return 0;
}

// Update the screen (the menu and the screensaver.)
static void do_screen_tick(struct screen_state *state)
{
	// Sync the screen size:
	if (!state->sized) {
		update_term_size();
		state->menu.area.width =
			COLS >= MENU_WIDTH ? MENU_WIDTH : COLS;
		state->menu.area.height = LINES;
		state->title.area.width = COLS - state->menu.area.width;
		state->title.area.height = LINES;
		state->title.area.x = state->menu.area.width;
		if (state->title.initialized) {
			d3d_free_camera(state->title.cam);
			state->title.cam = camera_with_dims(
				state->title.area.width,
				state->title.area.height);
		}
		if (state->menu.initialized)
			menu_mark_area_changed(&state->menu.menu);
		state->sized = true;
	}
	// Update the screen:
	if (state->title.initialized) {
		// Produces a cool turning effect with the camera's position:
		double theta = state->title.facing + 1.0;
		double x = cos(PI * cos(theta))
			+ d3d_board_width(state->title.board) / 2.0;
		double y = sin(PI * sin(theta))
			+ d3d_board_height(state->title.board) / 2.0;
		d3d_vec_s pos = { x, y };
		state->title.facing -= TITLE_SCREEN_CAM_ROTATION;
		d3d_draw(state->title.cam, pos, state->title.facing,
			state->title.board, 0, NULL);
		display_frame(state->title.cam, &state->title.area,
			state->title.color_map);
	}
	if (state->menu.initialized) menu_draw(&state->menu.menu);
	refresh();
}

static void destroy_screen_state(struct screen_state *state)
{
	if (state->title.initialized) d3d_free_camera(state->title.cam);
	if (state->menu.initialized) menu_destroy(&state->menu.menu);
}

// Load the save state list and log in as the one with the given name. If that
// name is not present, add it. NULL is returned if the saves couldn't load.
static struct save_state *get_save_state(const char *name,
	const char *state_file, struct save_states *saves, struct logger *log)
{
	FILE *from = fopen(state_file, "r");
	// from closed by save_state_init:
	if (from && !save_states_init(saves, from, log)) {
		save_states_add(saves, ANONYMOUS);
		struct save_state *save = save_states_get(saves, name);
		if (!save) save = save_states_add(saves, name);
		return save;
	}
	logger_printf(log, LOGGER_ERROR,
		"Unable to load save state from %s\n", state_file);
	return NULL;
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

// Get an input that is the name of a save from the menu. The name_buf is where
// the name will be put. The name will be at most buf_size - 1 characters, and
// will be NUL-terminated. An empty name signifies entry cancellation.
// screen_state is used for accessing the menu and managing the screen while
// waiting for input. timer is used to do the waiting.
static void get_input(char *name_buf, size_t buf_size,
	struct screen_state *screen_state, struct ticker *timer)
{
	int key;
	--buf_size; // Make space for the NUL-terminator from now on.
	menu_set_input(&screen_state->menu.menu, name_buf, buf_size);
	for (;;) {
		key = getch();
		// The name is entered:
		if (key == KEY_ENTER || key == '\n') break;
		// The entry is cancelled:
		if (key == ESC || key == KEY_LEFT) {
			*name_buf = '\0';
			return;
		}
		if (key == KEY_RESIZE) screen_state->sized = false;
		tick(timer);
		do_screen_tick(screen_state);
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
	if (!save) goto error_save_state;
	// ncurses reads ESCDELAY and waits that many ms after an ESC key press.
	// This here is lowered from "1000":
	try_setenv("ESCDELAY", STRINGIFY(FRAME_DELAY), 0);
	initscr();
	start_color();
	timeout(0); // No delay for key presses.
	// For some reason, NetBSD Curses requires this to keep the program from
	// waiting for user input in the menu:
	getch();
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
	// The screen state including the menu and title screensaver:
	struct screen_state screen_state = SCREEN_STATE_PARTIAL_INITIALIZER;
	if (load_menu_state(&screen_state.menu, data_dir, log)) {
		logger_printf(log, LOGGER_ERROR, "Failed to load menu\n");
		goto error_menu;
	}
	if (load_title_state(&screen_state.title, &ldr)) {
		logger_printf(log, LOGGER_WARNING,
			"Failed to load title screensaver\n");
	} else {
		color_map_apply(loader_color_map(&ldr));
		loader_print_summary(&ldr);
	}
	// Maximum dynamically sized message length is 63 characters (msg_buf
	// is just some scratch space):
	char msg_buf[64];
	if (!play_as)
		menu_set_message(&screen_state.menu.menu,
			"Select a game BEFORE playing to save!");
	curs_set(0); // Hide the cursor.
	noecho(); // Hide input characters.
	keypad(stdscr, TRUE); // Automatically detect arrow keys and so on.
	struct ticker timer; // The ticker used throughout the game.
	ticker_init(&timer, FRAME_DELAY);
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
	for (;;) {
		// Alias for the menu, to make the code easier to read:
		struct menu *menu = &screen_state.menu.menu;
		// The selected menu item (used later):
		struct menu_item *selected;
		// The map prerequisite found (used later):
		char *prereq;
		tick(&timer);
		do_screen_tick(&screen_state);
		switch (key = getch()) {
		redirect: // Simulate entering the redirected-to item:
			redirect->title = selected->title;
			// Simulate childhood:
			redirect->parent = selected->parent;
			/* FALLTHROUGH */
		case 'd':
		case '\n':
		case KEY_ENTER:
		case KEY_RIGHT:
			menu_clear_message(menu);
			switch (redirect ? menu_redirect(menu, redirect)
				: menu_enter(menu))
			{
			case ACTION_INPUT:
				menu_clear_message(menu);
				for (;;) {
					get_input(name_buf, sizeof(name_buf),
						&screen_state, &timer);
					if (*name_buf
					 && save_states_get(&saves, name_buf)) {
						menu_set_message(menu,
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
				menu_escape(menu);
				menu_set_message(menu, msg_buf);
				break;
			case ACTION_TAG:
				selected = menu_get_selected(menu);
				if (!selected || !selected->tag) break;
				if (!strcmp(selected->tag, "act/resume-game")) {
					const char *name =
						save_state_name(save);
					if (strcmp(name, ANONYMOUS)) {
						// Not anonymous.
						snprintf(msg_buf,
							sizeof(msg_buf),
							"Playing as %s", name);
						menu_set_message(menu,
							msg_buf);
					} else {
						menu_set_message(menu,
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
					menu_set_message(menu,
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
								menu, &saves);
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
						menu_set_message(menu,
							msg_buf);
					}
					break;
				}
				// No action taken above; load the map:
				prereq = map_prereq(&ldr, selected->tag);
				if (prereq
				 && !save_state_is_complete(save, prereq)) {
					menu_set_message(menu, "Level locked");
					beep();
				} else if (play_level(data_dir, save,
					selected->tag, &timer, log))
				{
					menu_set_message(menu,
						"Error loading map");
					beep();
				} else {
					menu_clear_message(menu);
					// Reset colors:
					color_map_apply(loader_color_map(&ldr));
					// Resize the screen state in case the
					// screen changed size during the level:
					screen_state.sized = false;
				}
				free(prereq);
				break;
			default:
				break;
			}
			break;
		case 'a':
		case ESC:
		case KEY_LEFT:
			// Leave submenu.
			if (menu_escape(menu)) menu_clear_message(menu);
			break;
		case 'w':
		case KEY_UP:
		case KEY_BACKSPACE:
		case KEY_SR:
			// Scroll up.
			menu_scroll(menu, -1);
			break;
		case 's':
		case ' ':
		case KEY_DOWN:
		case KEY_SF:
			// Scroll down.
			menu_scroll(menu, 1);
			break;
		case 'g':
			// Scroll to beginning.
			menu_scroll(menu, -999);
			break;
		case 'G':
			// Scroll to end.
			menu_scroll(menu, 999);
			break;
		case KEY_HOME:
			// Return to root menu.
			while (menu_escape(menu)) {
				menu_clear_message(menu);
			}
			break;
		case 'x':
			// Quit shortcut.
			goto end;
		case KEY_RESIZE:
			screen_state.sized = false;
			break;
		default:
			if (isdigit(key)) {
				// Goto nth menu item.
				int to = key == '0' ? 9 : key - '0' - 1;
				menu_scroll(menu, -999);
				menu_scroll(menu, to);
			}
			break;
		}
		redirect = NULL;
	}
end:
	ret = 0; // Things are successful so far.
error_menu:
	destroy_screen_state(&screen_state);
	endwin();
	// Don't save the progress of the anonymous:
	save_states_remove(&saves, ANONYMOUS);
	if (write_save_states(&saves, state_file, log)) ret = -1;
	free_save_links(&game_list);
	save_states_destroy(&saves);
error_save_state:
	loader_free(&ldr);
	return ret;
}
