#include "load-texture.h"
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include "d3d.h"
#include "json-util.h"
#include "map.h"
#include "ent.h"
#include "pixel.h"
#include "ticker.h"
#include "util.h"
#include "xalloc.h"
#include <ctype.h>
//#include <curses.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define LINES 40
#define COLS 150

#ifndef M_PI
#	define M_PI 3.14159265358979323846
#endif

#define CAM_RADIUS 0.1

#define PIXEL_ASPECT 0.625
#define FOV_X 2.0

#define FORWARD_COEFF 0.025
#define BACKWARD_COEFF 0.015
#define TURN_COEFF 0.055
#define SIDEWAYS_COEFF 0.02

void display_frame(d3d_camera *cam)
{
	printf("\x1B[H");
	for (size_t y = 0; y < d3d_camera_height(cam); ++y) {
		d3d_pixel last_wrote = pixel_letter('\0');
		for (size_t x = 0; x < d3d_camera_width(cam); ++x) {
			d3d_pixel pix = *d3d_camera_get(cam, x, y);
			if (pixel_is_letter(pix)) {
				if (!pixel_is_letter(last_wrote))
					printf("\x1B[0m");
				putchar(pix);
			} else {
				if (last_wrote != pix) {
					printf("\x1B[%s3%d;4%dm",
						pixel_is_bold(pix) ? "1;" : "",
						pixel_fg(pix), pixel_bg(pix));
				}
				putchar('#');
			}
			last_wrote = pix;
		}
		puts("\x1B[0m");
	}
	fflush(stdout);
}

void end_win(void) { endwin(); }

void init_entities(struct ent **entsp, d3d_sprite_s **spritesp, size_t *n_ents,
	struct map *map)
{
	*n_ents = map->n_ents;
	d3d_sprite_s *sprites = xmalloc(*n_ents * sizeof(*sprites) * 2);
	struct ent *ents = xmalloc(*n_ents * sizeof(*ents) * 2);
	for (size_t i = 0; i < *n_ents; ++i) {
		struct ent_type *type = map->ents[i].type;
		ent_init(&ents[i], type, &sprites[i], &map->ents[i].pos);
	}
	*spritesp = sprites;
	*entsp = ents;
}

void move_player(d3d_vec_s *pos, double *facing, int *translation, int key)
{
	switch (key) {
	case 'w': // Forward
	case 's': // Backward
	case 'a': // Left
	case 'd': // Right
		*translation = *translation != key ? key : '\0';
		break;
	case 'q': // Turn CCW
		*facing += TURN_COEFF;
		break;
	case 'e': // Turn CW
		*facing -= TURN_COEFF;
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
}

void move_ents(struct ent *ents, size_t n_ents, struct map *map,
	d3d_vec_s *cam_pos)
{
	for (size_t i = 0; i < n_ents; ++i) {
		ent_tick(&ents[i]);
		d3d_vec_s *epos = ent_pos(&ents[i]);
		d3d_vec_s *evel = ent_vel(&ents[i]);
		epos->x += evel->x;
		epos->y += evel->y;
		d3d_vec_s disp;
		double dist;
		if (ents[i].type->turn_chance > rand()) {
			disp.x = epos->x - cam_pos->x;
			disp.y = epos->y - cam_pos->y;
			dist = hypot(disp.x, disp.y) / -ents[i].type->speed;
			disp.x /= dist;
			disp.y /= dist;
		} else {
			disp.x = disp.y = 0;
		}
		d3d_vec_s move = *epos;
		map_check_walls(map, &move, CAM_RADIUS);
		if (ents[i].type->wall_die
		 && (move.x != epos->x || move.y != epos->y)) {
			ents[i].lifetime = 0;
		} else {
			disp.x += move.x - epos->x;
			disp.y += move.y - epos->y;
			*epos = move;
			if (disp.x != 0.0) evel->x = disp.x;
			if (disp.y != 0.0) evel->y = disp.y;
		}
	}
}

d3d_camera *make_camera(void)
{
	return d3d_new_camera(FOV_X, LINES * FOV_X / COLS / PIXEL_ASPECT,
		COLS, LINES);
}

void clean_up_dead(struct ent *ents, d3d_sprite_s *sprites, size_t *n_ents)
{
	for (size_t i = 0; i < *n_ents; ++i) {
		if (ent_is_dead(&ents[i])) {
			--*n_ents;
			ent_destroy(&ents[i]);
			ent_relocate(&ents[*n_ents], &ents[i], &sprites[i]);
		}
	}
}

void shoot_bullets(struct ent **entsp, d3d_sprite_s **spritesp, size_t *n_ents)
{
	struct ent *ents = *entsp;
	d3d_sprite_s *sprites = *spritesp;
	size_t bullet_at = *n_ents;
	for (size_t i = 0; i < *n_ents; ++i) {
		if (ents[i].type->bullet
		 && ents[i].type->shoot_chance > rand()) {
			size_t b = bullet_at++;
			ent_init(&ents[b], ents[i].type->bullet,
				&sprites[b], ent_pos(&ents[i]));
			d3d_vec_s *bvel = ent_vel(&ents[b]);
			*bvel = *ent_vel(&ents[i]);
			double speed = hypot(bvel->x, bvel->y) /
				ents[b].type->speed;
			bvel->x += bvel->x / speed;
			bvel->y += bvel->y / speed;
		}
	}
	if (bullet_at != *n_ents) {
		*n_ents = bullet_at;
		d3d_sprite_s *new_sprites = xrealloc(sprites, 2 * *n_ents
			* sizeof(*sprites));
		if (new_sprites != sprites) {
			sprites = new_sprites;
			for (size_t i = 0; i < *n_ents; ++i) {
				ent_use_moved_sprite(&ents[i],
					&sprites[i]);
			}
		}
		ents = xrealloc(ents, 2 * *n_ents * sizeof(*ents));
	}
	*entsp = ents;
	*spritesp = sprites;
}

int main(int argc, char *argv[])
{
	char stdout_buf[BUFSIZ];
	setbuf(stdout, stdout_buf);
	if (argc < 2) {
		fprintf(stderr, "Usage: %s map\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	d3d_malloc = xmalloc;
	d3d_realloc = xrealloc;
	struct loader ldr;
	srand(time(NULL)); // For random_start_frame
	loader_init(&ldr, "data");
	struct map *map = load_map(&ldr, argv[1]);
	if (!map) {
		logger_printf(loader_logger(&ldr), LOGGER_ERROR,
			"Failed to load map \"%s\"\n", argv[1]);
		loader_free(&ldr);
		exit(EXIT_FAILURE);
	}
	loader_print_summary(&ldr);
	size_t n_ents = map->n_ents;
	struct ent *ents;
	d3d_sprite_s *sprites;
	init_entities(&ents, &sprites, &n_ents, map);
	d3d_board *board = map->board;
	//initscr();
	//atexit(end_win);
	d3d_camera *cam = make_camera();
	d3d_vec_s *pos = d3d_camera_position(cam);
	*pos = map->player_pos;
	struct ticker timer;
	ticker_init(&timer, 15);
	double *facing = d3d_camera_facing(cam);
	int translation = '\0';
	int key;
	*facing = M_PI / 2;
	//curs_set(0);
	//timeout(0);
	struct termios termios;
	tcgetattr(STDIN_FILENO, &termios);
	termios.c_lflag &= ~(ECHO | ICANON);
	tcsetattr(STDIN_FILENO, TCSANOW, &termios);
//	fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
	while ((key = tolower(getchar())) != 'x') {
		move_player(pos, facing, &translation, key);
		map_check_walls(map, pos, CAM_RADIUS);
		d3d_draw_walls(cam, board);
		d3d_draw_sprites(cam, n_ents, sprites);
		display_frame(cam);
		move_ents(ents, n_ents, map, pos);
		clean_up_dead(ents, sprites, &n_ents);
		shoot_bullets(&ents, &sprites, &n_ents);
		tick(&timer);
	}
	d3d_free_camera(cam);
	free(ents);
	free(sprites);
	loader_free(&ldr);
}
