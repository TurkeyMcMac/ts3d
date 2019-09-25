#include "sim-worker.h"
#include "xalloc.h"
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#ifndef M_PI
#	define M_PI 3.14159265358979323846
#endif
#define CAM_RADIUS 0.1
#define FORWARD_COEFF 0.025
#define BACKWARD_COEFF 0.015
#define TURN_COEFF 0.055
#define SIDEWAYS_COEFF 0.02

static void destroy_sim_worker(void *arg)
{
	struct sim_worker *sim = arg;
	barrier_stop_using(&sim->cam_bar);
}

static void *run_sim_worker(void *arg)
{
	int cancel_junk; // This value is never used
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancel_junk);
	struct sim_worker *sim = arg;
	pthread_cleanup_push(destroy_sim_worker, sim);
	barrier_start_using(&sim->cam_bar);
	d3d_sprite_s *sprites = NULL;
	pthread_cleanup_push(free, sprites);
	struct ent *ents = NULL;
	struct map *map = sim->map;
	d3d_board *board = map->board;
	d3d_camera *cam = sim->cam;
	pthread_cleanup_push(free, ents);
	size_t n_ents = map->n_ents;
	sprites = xmalloc(n_ents * sizeof(*sprites) * 2);
	ents = xmalloc(n_ents * sizeof(*ents) * 2);
	for (size_t i = 0; i < n_ents; ++i) {
		struct ent_type *type = map->ents[i].type;
		ent_init(&ents[i], type, &sprites[i], &map->ents[i].pos);
	}
	d3d_vec_s *pos = d3d_camera_position(cam);
	*pos = map->player_pos;
	double *facing = d3d_camera_facing(cam);
	int translation = '\0';
	int key = '\0';
	*facing = M_PI / 2;
	for (;;) {
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancel_junk);
		switch (key) {
		case 'w': // Forward
		case 's': // Backward
		case 'a': // Left
		case 'd': // Right
			translation = translation != key ? key : '\0';
			break;
		case 'q': // Turn CCW
			*facing += TURN_COEFF;
			break;
		case 'e': // Turn CW
			*facing -= TURN_COEFF;
			break;
		}
		switch (translation) {
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
		map_check_walls(map, pos, CAM_RADIUS);
		barrier_wait(&sim->cam_bar);
		key = sim->input_char;
		d3d_draw_walls(cam, board);
		d3d_draw_sprites(cam, n_ents, sprites);
		barrier_wait(&sim->cam_bar);
		for (size_t i = 0; i < n_ents; ++i) {
			ent_tick(&ents[i]);
			d3d_vec_s *epos = ent_pos(&ents[i]);
			d3d_vec_s *evel = ent_vel(&ents[i]);
			epos->x += evel->x;
			epos->y += evel->y;
			d3d_vec_s disp;
			double dist;
			if (ents[i].type->turn_chance > rand()) {
				disp.x = epos->x - pos->x;
				disp.y = epos->y - pos->y;
				dist = hypot(disp.x, disp.y) /
					-ents[i].type->speed;
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
		for (size_t i = 0; i < n_ents; ++i) {
			if (ent_is_dead(&ents[i])) {
				--n_ents;
				ent_destroy(&ents[i]);
				ent_relocate(&ents[n_ents], &ents[i], &sprites[i]);
			}
		}
		size_t bullet_at = n_ents;
		for (size_t i = 0; i < n_ents; ++i) {
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
		if (bullet_at != n_ents) {
			n_ents = bullet_at;
			d3d_sprite_s *new_sprites = xrealloc(sprites, 2 * n_ents
				* sizeof(*sprites));
			if (new_sprites != sprites) {
				sprites = new_sprites;
				for (size_t i = 0; i < n_ents; ++i) {
					ents[i].sprite = &sprites[i];
				}
			}
			ents = xrealloc(ents, 2 * n_ents * sizeof(*ents));
		}
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &cancel_junk);
		pthread_testcancel();
	}
	pthread_cleanup_pop(true); // free(ents)
	pthread_cleanup_pop(true); // free(sprites)
	pthread_cleanup_pop(true); // destroy_sim_worker(sim)
}

int sim_worker_init(struct sim_worker *sim, d3d_camera *cam, struct map *map)
{
	int err = 0;
	sim->input_char = '\0';
	sim->cam = cam;
	sim->map = map;
	if (barrier_init(&sim->cam_bar)) goto error_barrier_init;
	if ((err = pthread_create(&sim->thread, NULL, run_sim_worker, sim)))
		goto error_thread;
	barrier_start_using(&sim->cam_bar);
	return 0;

error_thread:
	barrier_destroy(&sim->cam_bar);
error_barrier_init:
	errno = err;
	return -1;
}

int sim_worker_start_tick(struct sim_worker *sim, int input_char)
{
	sim->input_char = input_char;
	barrier_wait(&sim->cam_bar);
	return 0;
}

int sim_worker_finish_tick(struct sim_worker *sim)
{
	barrier_wait(&sim->cam_bar);
	return 0;
}

void sim_worker_destroy(struct sim_worker *sim)
{
	barrier_stop_using(&sim->cam_bar);
	pthread_cancel(sim->thread);
	pthread_join(sim->thread, NULL);
	barrier_destroy(&sim->cam_bar);
}
