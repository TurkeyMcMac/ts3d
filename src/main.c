#include "body.h"
#include "load-texture.h"
#include "player.h"
#include "d3d.h"
#include "json-util.h"
#include "map.h"
#include "menu.h"
#include "ent.h"
#include "pixel.h"
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

#ifndef M_PI
#	define M_PI 3.14159265358979323846
#endif
#define PIXEL_ASPECT 0.625
#define FOV_X 2.0
#define TURN_DURATION 5
#define ESC '\033'

static void set_up_colors(void)
{
	start_color();
	for (int fg = 0; fg < 8; ++fg) {
		for (int bg = 0; bg < 8; ++bg) {
			init_pair(pixel_pair(pixel(fg, bg)), fg, bg);
		}
	}
}

static void display_frame(d3d_camera *cam, WINDOW *win)
{
	for (size_t x = 0; x < d3d_camera_width(cam); ++x) {
		for (size_t y = 0; y < d3d_camera_height(cam); ++y) {
			d3d_pixel pix = *d3d_camera_get(cam, x, y);
			mvwaddch(win, y, x, pixel_style(pix) | '#');
		}
	}
}

static void init_entities(struct ents *ents, struct map *map)
{
	ents_init(ents, map->n_ents * 2);
	for (size_t i = 0; i < map->n_ents; ++i) {
		ent_id e = ents_add(ents, map->ents[i].type, map->ents[i].team,
			&map->ents[i].pos);
		*ents_worth(ents, e) = map->ents[i].team == TEAM_ENEMY;
	}
}

static void end_win(void) { endwin(); }

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
			dist = hypot(disp.x, disp.y) / -type->speed;
			disp.x /= dist;
			disp.y /= dist;
		} else {
			disp.x = disp.y = 0;
		}
		d3d_vec_s move = *epos;
		map_check_walls(map, &move, ents_body(ents, e)->radius);
		if (type->wall_die && (move.x != epos->x || move.y != epos->y))
		{
			ents_kill(ents, e);
		} else if (type->wall_block) {
			disp.x += move.x - epos->x;
			disp.y += move.y - epos->y;
			*epos = move;
			if (disp.x != 0.0) evel->x = disp.x;
			if (disp.y != 0.0) evel->y = disp.y;
		}
	}
}

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

static int get_remaining(struct ents *ents)
{
	int remaining = 0;
	ENTS_FOR_EACH(ents, e) {
		remaining += *ents_worth(ents, e);
	}
	return remaining;
}

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

static int play_level(const char *root_dir, const char *map_name,
	struct ticker *timer)
{
	struct loader ldr;
	loader_init(&ldr, root_dir);
	struct map *map = load_map(&ldr, map_name);
	if (!map) {
		logger_printf(loader_logger(&ldr), LOGGER_ERROR,
			"Failed to load map \"%s\"\n", map_name);
		loader_free(&ldr);
		return -1;
	}
	srand(time(NULL)); // For random_start_frame
	struct ents ents;
	init_entities(&ents, map);
	d3d_board *board = map->board;
	initscr();
	atexit(end_win);
	struct meter health_meter = {
		.label = "HEALTH",
		.style = pixel_style(pixel(PC_BLACK, PC_GREEN)),
		.x = 0,
		.y = LINES - 1,
		.width = COLS / 2,
		.win = stdscr,
	};
	struct meter reload_meter = {
		.label = "RELOAD",
		.style = pixel_style(pixel(PC_BLACK, PC_RED)),
		.x = health_meter.width,
		.y = LINES - 1,
		.width = COLS - health_meter.width,
		.win = stdscr,
	};
	WINDOW *dead_popup = popup_window(
		"You died.\n"
		"Press Y to return to the menu.");
	d3d_camera *cam = camera_with_dims(COLS, LINES - 1);
	struct player player;
	player_init(&player, map);
	redrawwin(stdscr);
	timeout(0);
	keypad(stdscr, TRUE);
	int translation = '\0';
	int turn_duration = 0;
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
			goto quit;
		} else if (!won && player_is_dead(&player)) {
			if (lowkey == 'y') goto quit;
			touchwin(dead_popup);
			wrefresh(dead_popup);
		} else if (lowkey == 'p') {
			if (do_pause_popup(timer)) goto quit;
		} else if (lowkey == 'x' || key == ESC) {
			if (prompt_quit_popup(timer)) goto quit;
		} else {
			move_player(&player,
				&translation, &turn_duration, key);
		}
		move_ents(&ents, map, &player);
		player_collide(&player, &ents);
		hit_ents(&ents);
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
	loader_free(&ldr);
	return 0;
}

static void title_cam_pos(d3d_camera *cam, const d3d_board *board)
{
	d3d_vec_s *pos = d3d_camera_position(cam);
	double theta = *d3d_camera_facing(cam) + 1.0;
	pos->x = cos(M_PI * cos(theta)) + d3d_board_width(board) / 2.0;
	pos->y = sin(M_PI * sin(theta)) + d3d_board_height(board) / 2.0;
}

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

static void tick_title(d3d_camera *cam, d3d_board *board, WINDOW *win)
{
	d3d_draw_walls(cam, board);
	display_frame(cam, win);
	wrefresh(win);
	title_cam_pos(cam, board);
	*d3d_camera_facing(cam) -= 0.003;
}

int main(void)
{
	const char *data_dir = "data";
	initscr();
	atexit(end_win);
	set_up_colors();
	struct loader ldr;
	loader_init(&ldr, data_dir);
	WINDOW *menuwin = newwin(LINES, 41, 0, 0);
	WINDOW *titlewin = newwin(LINES, COLS - 41, 0, 41);
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
	curs_set(0);
	noecho();
	wtimeout(menuwin, 0);
	keypad(menuwin, TRUE);
	struct ticker timer;
	ticker_init(&timer, 30);
	int key;
	for (;;) {
		const char *map_name;
		tick_title(title_cam, title_board, titlewin);
		menu_draw(&menu);
		wrefresh(menuwin);
		tick(&timer);
		switch (key = wgetch(menuwin)) {
		case 'd':
		case '\n':
		case KEY_ENTER:
		case KEY_RIGHT:
			switch (menu_enter(&menu, &map_name)) {
			case ACTION_BLOCKED:
				beep();
				break;
			case ACTION_WENT:
				menu_clear_message(&menu);
				break;
			case ACTION_MAP:
				if (play_level(data_dir, map_name, &timer))
					menu_set_message(&menu,
						"Error loading map");
				redrawwin(menuwin);
				redrawwin(titlewin);
				break;
			case ACTION_QUIT:
				goto end;
			}
			break;
		case 'a':
		case ESC:
		case KEY_LEFT:
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
			if (!menu_scroll(&menu, -1)) beep();
			break;
		case 's':
		case ' ':
		case KEY_DOWN:
		case KEY_SF:
			if (!menu_scroll(&menu, 1)) beep();
			break;
		case 'g':
			menu_scroll(&menu, -999);
			break;
		case 'G':
			menu_scroll(&menu, 999);
			break;
		case KEY_HOME:
			while (menu_escape(&menu)) {
				menu_clear_message(&menu);
			}
			break;
		case 'x':
			goto end;
		default:
			if (isdigit(key)) {
				int to = key - '0' - 1;
				menu_scroll(&menu, -999);
				if (menu_scroll(&menu, to) != to) beep();
				break;
			}
			break;
		}
	}
end:
	d3d_free_camera(title_cam);
	loader_free(&ldr);
}
