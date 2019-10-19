#include "body.h"
#include "load-texture.h"
#include "player.h"
#include "d3d.h"
#include "json-util.h"
#include "map.h"
#include "meter.h"
#include "ent.h"
#include "pixel.h"
#include "ticker.h"
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

static void set_up_colors(void)
{
	start_color();
	for (int fg = 0; fg < 8; ++fg) {
		for (int bg = 0; bg < 8; ++bg) {
			init_pair(pixel_pair(pixel(fg, bg)), fg, bg);
		}
	}
}

static void display_frame(d3d_camera *cam)
{
	for (size_t x = 0; x < d3d_camera_width(cam); ++x) {
		for (size_t y = 0; y < d3d_camera_height(cam); ++y) {
			d3d_pixel pix = *d3d_camera_get(cam, x, y);
			mvaddch(y, x, pixel_style(pix) | '#');
		}
	}
}

static void init_entities(struct ents *ents, struct map *map)
{
	ents_init(ents, map->n_ents * 2);
	for (size_t i = 0; i < map->n_ents; ++i) {
		ents_add(ents, map->ents[i].type, map->ents[i].team,
			&map->ents[i].pos);
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

static d3d_camera *make_camera(void)
{
	return d3d_new_camera(FOV_X, LINES * FOV_X / COLS / PIXEL_ASPECT,
		COLS, LINES - 1);
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

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s map\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	d3d_malloc = xmalloc;
	d3d_realloc = xrealloc;
	struct loader ldr;
	loader_init(&ldr, "data");
	struct map *map = load_map(&ldr, argv[1]);
	if (!map) {
		logger_printf(loader_logger(&ldr), LOGGER_ERROR,
			"Failed to load map \"%s\"\n", argv[1]);
		loader_free(&ldr);
		exit(EXIT_FAILURE);
	}
	loader_print_summary(&ldr);
	srand(time(NULL)); // For random_start_frame
	struct ents ents;
	init_entities(&ents, map);
	d3d_board *board = map->board;
	initscr();
	atexit(end_win);
	struct meter health_meter = {
		.label = "HEALTH",
		.style = A_BOLD | pixel_style(pixel(PC_BLACK, PC_GREEN)),
		.x = 0,
		.y = LINES - 1,
		.width = COLS / 2,
	};
	struct meter reload_meter = {
		.label = "RELOAD",
		.style = A_BOLD | pixel_style(pixel(PC_BLACK, PC_RED)),
		.x = health_meter.width,
		.y = LINES - 1,
		.width = COLS - health_meter.width,
	};
	d3d_camera *cam = make_camera();
	struct player player;
	player_init(&player, map);
	set_up_colors();
	struct ticker timer;
	ticker_init(&timer, 30);
	int translation = '\0';
	int turn_duration = 0;
	int key;
	curs_set(0);
	noecho();
	timeout(0);
	keypad(stdscr, TRUE);
	while (tolower(key = getch()) != 'x') {
		player_move_camera(&player, cam);
		d3d_draw_walls(cam, board);
		d3d_draw_sprites(cam, ents_num(&ents), ents_sprites(&ents));
		display_frame(cam);
		meter_draw(&health_meter, player_health_fraction(&player));
		meter_draw(&reload_meter, player_reload_fraction(&player));
		if (player_is_dead(&player)) {
			mvaddstr(LINES / 2, COLS / 2 - 2, "DEAD");
		} else {
			move_player(&player,
				&translation, &turn_duration, key);
		}
		refresh();
		move_ents(&ents, map, &player);
		player_collide(&player, &ents);
		hit_ents(&ents);
		if (isupper(key) || key == ' ')
			player_try_shoot(&player, &ents);
		shoot_bullets(&ents);
		player_tick(&player);
		ents_tick(&ents);
		ents_clean_up_dead(&ents);
		tick(&timer);
	}
	d3d_free_camera(cam);
	ents_destroy(&ents);
	loader_free(&ldr);
}
