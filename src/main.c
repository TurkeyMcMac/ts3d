#include "load-texture.h"
#include "d3d.h"
#include "json-util.h"
#include "map.h"
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

#define CAM_RADIUS 0.1

#define PIXEL_ASPECT 0.625
#define FOV_X 2.0

#define FORWARD_COEFF 0.025
#define BACKWARD_COEFF 0.015
#define TURN_COEFF 0.038
#define SIDEWAYS_COEFF 0.02
#define RELOAD 50
#define TURN_DURATION 5

void set_up_colors(void)
{
	start_color();
	for (int fg = 0; fg < 8; ++fg) {
		for (int bg = 0; bg < 8; ++bg) {
			init_pair((fg << 3 | bg) + 1, fg, bg);
		}
	}
}

void display_frame(d3d_camera *cam)
{
	for (size_t x = 0; x < d3d_camera_width(cam); ++x) {
		for (size_t y = 0; y < d3d_camera_height(cam); ++y) {
			d3d_pixel pix = *d3d_camera_get(cam, x, y);
			int cell = COLOR_PAIR(pix + 1) | '#';
			if (pixel_is_bold(pix)) cell |= A_BOLD;
			mvaddch(y, x, cell);
		}
	}
	refresh();
}

void init_entities(struct ents *ents, struct map *map)
{
	ents_init(ents, map->n_ents * 2);
	for (size_t i = 0; i < map->n_ents; ++i) {
		ents_add(ents, map->ents[i].type, map->ents[i].team,
			&map->ents[i].pos);
	}
}

void end_win(void) { endwin(); }

void move_player(d3d_vec_s *pos, double *facing, int *translation,
	int *turn_duration, int key)
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
		double sideway;
	case 'w': // Forward
		pos->x += FORWARD_COEFF * cos(*facing);
		pos->y += FORWARD_COEFF * sin(*facing);
		break;
	case 's': // Backward
		pos->x -= FORWARD_COEFF * cos(*facing);
		pos->y -= FORWARD_COEFF * sin(*facing);
		break;
	case 'a': // Left
		sideway = *facing + M_PI / 2;
		pos->x += FORWARD_COEFF * cos(sideway);
		pos->y += FORWARD_COEFF * sin(sideway);
		break;
	case 'd': // Right
		sideway = *facing - M_PI / 2;
		pos->x += FORWARD_COEFF * cos(sideway);
		pos->y += FORWARD_COEFF * sin(sideway);
		break;
	default:
		break;
	}
	if (*turn_duration > 0) {
		*facing += TURN_COEFF;
		--*turn_duration;
	} else if (*turn_duration < 0) {
		*facing -= TURN_COEFF;
		++*turn_duration;
	}
}

void move_ents(struct ents *ents, struct map *map, d3d_vec_s *cam_pos)
{
	ENTS_FOR_EACH(ents, e) {
		const struct ent_type *type = ents_type(ents, e);
		d3d_vec_s *epos = ents_pos(ents, e);
		d3d_vec_s *evel = ents_vel(ents, e);
		epos->x += evel->x;
		epos->y += evel->y;
		d3d_vec_s disp;
		double dist;
		if (type->turn_chance > rand()) {
			disp.x = epos->x - cam_pos->x;
			disp.y = epos->y - cam_pos->y;
			dist = hypot(disp.x, disp.y) / -type->speed;
			disp.x /= dist;
			disp.y /= dist;
		} else {
			disp.x = disp.y = 0;
		}
		d3d_vec_s move = *epos;
		map_check_walls(map, &move, CAM_RADIUS);
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

bool touching(const d3d_vec_s *a, const d3d_vec_s *b)
{
	return fabs(a->x - b->x) < CAM_RADIUS * 2
	    && fabs(a->y - b->y) < CAM_RADIUS * 2;
}

void hit_ents(struct ents *ents, d3d_vec_s *cam_pos)
{
	ENTS_FOR_EACH(ents, e) {
		if (teams_can_collide(TEAM_PLAYER, ents_team(ents, e))
		 && touching(cam_pos, ents_pos(ents, e)))
			ents_kill(ents, e);
	}
	ENTS_FOR_EACH_PAIR(ents, ea, eb) {
		if (teams_can_collide(ents_team(ents, ea), ents_team(ents, eb))
		 && touching(ents_pos(ents, ea), ents_pos(ents, eb))) {
			ents_kill(ents, ea);
			ents_kill(ents, eb);
		}
	}
}

d3d_camera *make_camera(void)
{
	return d3d_new_camera(FOV_X, LINES * FOV_X / COLS / PIXEL_ASPECT,
		COLS, LINES);
}

void shoot_player_bullet(const d3d_vec_s *pos, double facing, struct ents *ents)
{
	ent_id bullet = ents_add(ents, ents_type(ents, 0)->bullet, TEAM_ALLY,
		pos);
	d3d_vec_s *bvel = ents_vel(ents, bullet);
	bvel->x = 2 * FORWARD_COEFF * cos(facing);
	bvel->y = 2 * FORWARD_COEFF * sin(facing);
}

void shoot_bullets(struct ents *ents)
{
	ENTS_FOR_EACH(ents, e) {
		struct ent_type *type = ents_type(ents, e);
		if (type->bullet && type->shoot_chance > rand()) {
			ent_id bullet = ents_add(ents, type->bullet,
				ents_team(ents, e), ents_pos(ents, e));
			d3d_vec_s *bvel = ents_vel(ents, bullet);
			*bvel = *ents_vel(ents, e);
			double speed = hypot(bvel->x, bvel->y) /
				ents_type(ents, bullet)->speed;
			bvel->x += bvel->x / speed;
			bvel->y += bvel->y / speed;
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
	d3d_camera *cam = make_camera();
	d3d_vec_s *pos = d3d_camera_position(cam);
	*pos = map->player_pos;
	set_up_colors();
	struct ticker timer;
	ticker_init(&timer, 30);
	double *facing = d3d_camera_facing(cam);
	int reload = 0;
	int translation = '\0';
	int turn_duration = 0;
	int key;
	*facing = M_PI / 2;
	curs_set(0);
	timeout(0);
	while (tolower(key = getch()) != 'x') {
		move_player(pos, facing, &translation, &turn_duration, key);
		map_check_walls(map, pos, CAM_RADIUS);
		d3d_draw_walls(cam, board);
		d3d_draw_sprites(cam, ents_num(&ents), ents_sprites(&ents));
		display_frame(cam);
		move_ents(&ents, map, pos);
		hit_ents(&ents, pos);
		ents_tick(&ents);
		ents_clean_up_dead(&ents);
		--reload;
		if ((isupper(key) || key == ' ') && reload < 0) {
			shoot_player_bullet(pos, *facing, &ents);
			reload = RELOAD;
		}
		shoot_bullets(&ents);
		tick(&timer);
	}
	d3d_free_camera(cam);
	ents_destroy(&ents);
	loader_free(&ldr);
}
