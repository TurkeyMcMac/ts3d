#include "body.h"
#include "load-texture.h"
#include "loader.h"
#include "logger.h"
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

// Define pi in case it is not defined.
#ifndef M_PI
#	define M_PI 3.14159265358979323846
#endif

// The width:height ratio of each pixel. Curses seems not to have a way to
// determine this dynamically. The information is used when deciding camera
// dimensions.
#define PIXEL_ASPECT 0.625
// The field of view side-to-side, in radians. The field is smaller if the
// screen is taller than it is wide, which is unlikely. See camera_with_dims
// for details.
#define FOV_X 2.0

// The number of ticks a turn lasts, triggered by a single key press. This is to
// smooth out key detection speeds for different terminals, as some wait longer
// before registering another press.
#define TURN_DURATION 5

// The ASCII escape key.
#define ESC '\033'
// The ASCII delete key.
#define DEL '\177'

// The "name" of an anonymous player whose progress is not saved.
#define ANONYMOUS ""

// The prefix to menu item tags referring to save names, not including the final
// slash.
#define SAVE_PFX_NO_SLASH "save"
// The same as above, but with the final slash. Evaluates to "save/".
#define SAVE_PFX SAVE_PFX_NO_SLASH "/"
// The length of the above prefix, including the slash.
#define SAVE_PFX_LEN 5

// Register all the color pairs needed for drawing the screen.
static void set_up_colors(void)
{
	start_color();
	for (int fg = 0; fg < 8; ++fg) {
		for (int bg = 0; bg < 8; ++bg) {
			init_pair(pixel_pair(pixel(fg, bg)), fg, bg);
		}
	}
}

// Copy the current scene from the camera to the window.
static void display_frame(d3d_camera *cam, WINDOW *win)
{
	for (size_t x = 0; x < d3d_camera_width(cam); ++x) {
		for (size_t y = 0; y < d3d_camera_height(cam); ++y) {
			d3d_pixel pix = *d3d_camera_get(cam, x, y);
			mvwaddch(win, y, x, pixel_style(pix) | '#');
		}
	}
}

// Create all the entities specified by the entity start specifications
// (struct map_ent_start) in the given map.
static void init_entities(struct ents *ents, struct map *map)
{
	ents_init(ents, map->n_ents * 2);
	for (size_t i = 0; i < map->n_ents; ++i) {
		ent_id e = ents_add(ents, map->ents[i].type, map->ents[i].team,
			&map->ents[i].pos);
		*ents_worth(ents, e) = map->ents[i].team == TEAM_ENEMY;
	}
}

// atexit callback that calls endwin().
static void end_win(void) { endwin(); }

// Move the player based on the input key. translation and turn_duration are
// used as persistent state. translation is the last translation key pressed
// ('w', 'a', etc.) turn_duration is a number whose absolute value specifies
// the remaining turn ticks and whose sign indicates the turn direction.
static void move_player(struct player *player,
	int *translation, int *turn_duration, int key)
{
	key = tolower(key);
	switch (key) {
	case 'w': // Forward
	case 's': // Backward
	case 'a': // Left
	case 'd': // Right
		*translation = *translation != key ? key : '\0';
		break;
	case 'q': // Turn CCW
		*turn_duration = +TURN_DURATION;
		break;
	case 'e': // Turn CW
		*turn_duration = -TURN_DURATION;
		break;
	}
	switch (*translation) {
	case 'w': // Forward
		player_walk(player, 0);
		break;
	case 's': // Backward
		player_walk(player, M_PI);
		break;
	case 'a': // Left
		player_walk(player, M_PI / 2);
		break;
	case 'd': // Right
		player_walk(player, -M_PI / 2);
		break;
	default:
		break;
	}
	if (*turn_duration > 0) {
		player_turn_ccw(player);
		--*turn_duration;
	} else if (*turn_duration < 0) {
		player_turn_cw(player);
		++*turn_duration;
	}
}

// Move the given entities on the map. The entities will approach the player if
// they can turn and move. Entities that die upon hitting the wall will do so.
static void move_ents(struct ents *ents, struct map *map, struct player *player)
{
	map_check_walls(map, &player->body.pos, player->body.radius);
	ENTS_FOR_EACH(ents, e) {
		const struct ent_type *type = ents_type(ents, e);
		d3d_vec_s *epos = ents_pos(ents, e);
		d3d_vec_s *evel = ents_vel(ents, e);
		epos->x += evel->x;
		epos->y += evel->y;
		d3d_vec_s disp;
		double dist;
		if (chance_decide(type->turn_chance)) {
			disp.x = epos->x - player->body.pos.x;
			disp.y = epos->y - player->body.pos.y;
			// Normalize displacement to speed.
			dist = hypot(disp.x, disp.y) / -type->speed;
			disp.x /= dist;
			disp.y /= dist;
		} else {
			disp.x = disp.y = 0;
		}
		d3d_vec_s move = *epos; // Movement due to wall collision.
		map_check_walls(map, &move, ents_body(ents, e)->radius);
		if (type->wall_die && (move.x != epos->x || move.y != epos->y))
		{
			ents_kill(ents, e);
		} else if (type->wall_block) {
			disp.x += move.x - epos->x;
			disp.y += move.y - epos->y;
			*epos = move;
			// The entity will move from the wall later.
			if (disp.x != 0.0) evel->x = disp.x;
			if (disp.y != 0.0) evel->y = disp.y;
		}
	}
}

// Have entities collide with each other.
static void hit_ents(struct ents *ents)
{
	ENTS_FOR_EACH_PAIR(ents, ea, eb) {
		enum team ta = ents_team(ents, ea), tb = ents_team(ents, eb);
		if (teams_can_collide(ta, tb)) {
			if (bodies_collide(ents_body(ents, ea),
				ents_body(ents, eb))
			 && (ta == TEAM_ALLY || tb == TEAM_ALLY))
				beep();
		}
	}
}

// Create a camera with the given positive dimensions.
static d3d_camera *camera_with_dims(int width, int height)
{
	if (width <= 0) width = 1;
	if (height <= 0) height = 1;
	if (width >= height) {
		return d3d_new_camera(FOV_X,
			FOV_X / PIXEL_ASPECT * height / width, width, height);
	} else {
		return d3d_new_camera(FOV_X * width / height,
			FOV_X / PIXEL_ASPECT, width, height);
	}
}

// Shoot the bullets of the entities who want to shoot, adding them to the
// entity pool.
static void shoot_bullets(struct ents *ents)
{
	ENTS_FOR_EACH(ents, e) {
		struct ent_type *type = ents_type(ents, e);
		if (type->bullet && chance_decide(type->shoot_chance)) {
			ent_id bullet = ents_add(ents, type->bullet,
				ents_team(ents, e), ents_pos(ents, e));
			d3d_vec_s *bvel = ents_vel(ents, bullet);
			*bvel = *ents_vel(ents, e);
			double speed = hypot(bvel->x, bvel->y) /
				ents_type(ents, bullet)->speed;
			if (!isnan(speed)) {
				bvel->x += bvel->x / speed;
				bvel->y += bvel->y / speed;
			}
		}
	}
}

// Count the remaining number of targets standing in the way of level winning.
static int get_remaining(struct ents *ents)
{
	int remaining = 0;
	ENTS_FOR_EACH(ents, e) {
		remaining += *ents_worth(ents, e);
	}
	return remaining;
}

// Display a popup window mid-screen asking if the player wants to quit. The
// decision is returned when it is made.
static bool prompt_quit_popup(struct ticker *timer)
{
	WINDOW *quit_popup = popup_window(
		"Are you sure you want to quit?\n"
		"Press Y to confirm or N to cancel.");
	touchwin(quit_popup);
	wrefresh(quit_popup);
	delwin(quit_popup);
	int key;
	while ((key = tolower(getch())) != 'n') {
		if (key == 'y') return true;
		tick(timer);
	}
	return false;
}

// Display a popup mind-screen saying the game is paused. The game will resume
// when the user is ready. If the player quits from the pause menu, true is
// returned, or false otherwise.
static bool do_pause_popup(struct ticker *timer)
{
	WINDOW *pause_popup = popup_window("Game paused.\nPress P to resume.");
	touchwin(pause_popup);
	wrefresh(pause_popup);
	int key;
	while ((key = tolower(getch())) != 'p') {
		if (key == 'x' || key == ESC) {
			touchwin(stdscr);
			refresh();
			if (prompt_quit_popup(timer)) {
				delwin(pause_popup);
				return true;
			}
			touchwin(stdscr);
			refresh();
			touchwin(pause_popup);
			wrefresh(pause_popup);
		}
		tick(timer);
	}
	delwin(pause_popup);
	return false;
}

// Play a level until death, completion, or quitting. root_dir is the root game
// data directory path. save is the save being used; it will be updated if the
// player wins. map_name is the name of the map to load. timer is the timepiece
// to measure by. log is the logger to print to. If the map is nonexistent or
// locked in the given save, -1 is returned, otherwise 0.
static int play_level(const char *root_dir, struct save_state *save,
	const char *map_name, struct ticker *timer, struct logger *log)
{
	struct loader ldr;
	loader_init(&ldr, root_dir);
	logger_free(loader_set_logger(&ldr, log));
	struct map *map = load_map(&ldr, map_name);
	if (!map) {
		logger_printf(loader_logger(&ldr), LOGGER_ERROR,
			"Failed to load map \"%s\"\n", map_name);
		goto error_map;
	}
	if (map->prereq && !save_state_is_complete(save, map->prereq))
		goto error_map; // Map not unlocked.
	loader_print_summary(&ldr);
	srand(time(NULL)); // For random_start_frame
	struct ents ents;
	init_entities(&ents, map);
	d3d_board *board = map->board;
	struct meter health_meter = {
		.label = "HEALTH",
		.style = pixel_style(pixel(PC_BLACK, PC_GREEN)),
		// Takes up the left half of the bottom:
		.x = 0,
		.y = LINES - 1,
		.width = COLS / 2,
		.win = stdscr,
	};
	struct meter reload_meter = {
		.label = "RELOAD",
		.style = pixel_style(pixel(PC_BLACK, PC_RED)),
		// Takes up the right half of the bottom:
		.x = health_meter.width,
		.y = LINES - 1,
		.width = COLS - health_meter.width,
		.win = stdscr,
	};
	WINDOW *dead_popup = popup_window(
		"You died.\n"
		"Press Y to return to the menu.");
	// LINES - 1 so that one is reserved for the health and reload meters:
	d3d_camera *cam = camera_with_dims(COLS, LINES - 1);
	struct player player;
	player_init(&player, map);
	timeout(0);
	keypad(stdscr, TRUE);
	int translation = '\0'; // No initial translation
	int turn_duration = 0; // No initial turning
	bool won = false;
	for (;;) {
		player_move_camera(&player, cam);
		d3d_draw_walls(cam, board);
		d3d_draw_sprites(cam, ents_num(&ents), ents_sprites(&ents));
		display_frame(cam, stdscr);
		health_meter.fraction = player_health_fraction(&player);
		meter_draw(&health_meter);
		reload_meter.fraction = player_reload_fraction(&player);
		meter_draw(&reload_meter);
		int remaining = get_remaining(&ents);
		// Player wins if all targets gone and they are not, or if they
		// won already:
		won = won || (remaining <= 0 && !player_is_dead(&player));
		attron(A_BOLD);
		if (won) {
			mvaddstr(0, 0, "YOU WIN! Press Y to return to menu.");
		} else {
			mvprintw(0, 0, "TARGETS LEFT: %d", remaining);
		}
		attroff(A_BOLD);
		int key = getch();
		int lowkey = tolower(key);
		refresh();
		if (won && lowkey == 'y') {
			// Player quits after winning.
			goto quit;
		} else if (!won && player_is_dead(&player)) {
			// Player lost, entities still simulated. Y to quit.
			if (lowkey == 'y') goto quit;
			touchwin(dead_popup);
			wrefresh(dead_popup);
		} else if (lowkey == 'p') {
			// Pause game
			if (do_pause_popup(timer)) goto quit;
		} else if (lowkey == 'x' || key == ESC) {
			// Quit game
			if (prompt_quit_popup(timer)) goto quit;
		} else {
			// Otherwise, let the player be controlled.
			move_player(&player,
				&translation, &turn_duration, key);
		}
		move_ents(&ents, map, &player);
		player_collide(&player, &ents);
		hit_ents(&ents);
		// Let the player shoot. This is blocked by player_try_shoot if
		// the player is dead:
		if (isupper(key) || key == ' ')
			player_try_shoot(&player, &ents);
		shoot_bullets(&ents);
		player_tick(&player);
		ents_tick(&ents);
		ents_clean_up_dead(&ents);
		tick(timer);
	}
quit:
	refresh();
	delwin(dead_popup);
	d3d_free_camera(cam);
	// Record the player's winning:
	if (won) save_state_mark_complete(save, map_name);
	ents_destroy(&ents);
	loader_free(&ldr);
	return 0;

error_map:
	loader_free(&ldr);
	return -1;
}

// Position the camera in the title screensaver.
static void title_cam_pos(d3d_camera *cam, const d3d_board *board)
{
	d3d_vec_s *pos = d3d_camera_position(cam);
	double theta = *d3d_camera_facing(cam) + 1.0;
	// Produces a cool turning effect:
	pos->x = cos(M_PI * cos(theta)) + d3d_board_width(board) / 2.0;
	pos->y = sin(M_PI * sin(theta)) + d3d_board_height(board) / 2.0;
}

// Load the title screensaver map with the given loader, putting the camera and
// board into the given pointers. The window will be used to draw the scene.
static int load_title(d3d_camera **cam, d3d_board **board, WINDOW *win,
	struct loader *ldr)
{
	struct map *map = load_map(ldr, "title-screen");
	if (!map) return -1;
	int width, height;
	getmaxyx(win, height, width);
	*cam = camera_with_dims(width, height);
	*board = map->board;
	title_cam_pos(*cam, *board);
	return 0;
}

// Move the screensaver forward a tick and draw it on the window. The parameters
// must the same as from load_title above.
static void tick_title(d3d_camera *cam, d3d_board *board, WINDOW *win)
{
	d3d_draw_walls(cam, board);
	display_frame(cam, win);
	wrefresh(win);
	title_cam_pos(cam, board);
	*d3d_camera_facing(cam) -= 0.003;
}

// Load the save state list and log in as the one with the given name. If that
// name is not present, create it.
static struct save_state *get_save_state(const char *name,
	struct save_states *saves, struct logger *log)
{
	FILE *from = fopen("state.json", "r");
	if (from && !save_states_init(saves, from, log)) {
		struct save_state *save = save_states_get(saves, name);
		if (save) return save;
		fclose(from);
	} else {
		save_states_empty(saves);
	}
	return save_states_add(saves, name);
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
static void write_save_states(struct save_states *saves)
{
	FILE *to = fopen("state.json", "w");
	// Silently fail for now.
	if (to) {
		save_states_write(saves, to);
		fclose(to);
	}
}

// Get an input that is the name of a save from the menu. The menu must be in
// the input element from which the input is to be read. The name_buf is where
// the name will be put. The name will be at most buf_size - 1 characters, and
// will be NUL-terminated. The other parameters are for running the screensaver
// while waiting for input. An empty name signifies entry cancellation.
static void get_input(char *name_buf, size_t buf_size, struct menu *menu,
	d3d_camera *title_cam, d3d_board *title_board, WINDOW *title_win,
	struct ticker *timer)
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
		tick_title(title_cam, title_board, title_win);
		tick(timer);
		size_t len = strnlen(name_buf, buf_size);
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

int main(int argc, char *argv[])
{
	struct logger log;
	logger_init(&log);
	const char *data_dir = "data"; // Game data root directory.
	struct loader ldr;
	loader_init(&ldr, data_dir);
	logger_free(loader_set_logger(&ldr, &log));
	struct save_states saves;
	// Log in with the first argument as the name, or anonymously if no
	// argument is given:
	struct save_state *save = get_save_state(argc > 1 ? argv[1] : ANONYMOUS,
		&saves, loader_logger(&ldr));
	initscr();
	atexit(end_win);
	set_up_colors();
	// Window to draw the menu on:
	WINDOW *menuwin = newwin(LINES, 41, 0, 0);
	// Window to draw the screensaver on:
	WINDOW *titlewin = newwin(LINES, COLS - 41, 0, 41);
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
	if (menu_init(&menu, data_dir, menuwin, loader_logger(&ldr))) {
		logger_printf(loader_logger(&ldr), LOGGER_ERROR,
			"Failed to load menu\n");
		exit(EXIT_FAILURE);
	}
	d3d_malloc = xmalloc;
	d3d_realloc = xrealloc;
	d3d_camera *title_cam;
	d3d_board *title_board;
	if (load_title(&title_cam, &title_board, titlewin, &ldr)) {
		logger_printf(loader_logger(&ldr), LOGGER_ERROR,
			"Failed to load title screensaver\n");
		exit(EXIT_FAILURE);
	}
	loader_print_summary(&ldr);
	curs_set(0); // Hide the cursor.
	noecho(); // Hide input characters.
	wtimeout(menuwin, 0); // Don't wait for input in the menu.
	keypad(menuwin, TRUE); // Automatically detect arrow keys and so on.
	struct ticker timer;
	ticker_init(&timer, 30); // The frame wait is 30ms.
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
		// The selected menu item (used later):
		struct menu_item *selected;
		// The map prerequisite found (used later):
		char *prereq;
		tick_title(title_cam, title_board, titlewin);
		menu_draw(&menu);
		wrefresh(menuwin);
		tick(&timer);
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
						&menu, title_cam, title_board,
						titlewin, &timer);
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
					selected->tag, &timer, &log))
				{
					menu_set_message(&menu,
						"Error loading map");
					beep();
				} else {
					menu_clear_message(&menu);
				}
				free(prereq);
				// Mark redrawing as required:
				redrawwin(menuwin);
				redrawwin(titlewin);
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
				int to = key - '0' - 1;
				menu_scroll(&menu, -999);
				if (menu_scroll(&menu, to) != to) beep();
				break;
			}
			break;
		}
		redirect = NULL;
	}
end:
	d3d_free_camera(title_cam);
	menu_destroy(&menu);
	free_save_links(&game_list);
	// Don't save the progress of the anonymous:
	save_states_remove(&saves, ANONYMOUS);
	write_save_states(&saves);
	save_states_destroy(&saves);
	loader_free(&ldr);
	logger_free(&log);
}
