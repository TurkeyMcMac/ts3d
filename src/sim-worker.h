#ifndef SIM_WORKER_H_
#define SIM_WORKER_H_

#include "d3d.h"
#include "map.h"
#include <pthread.h>

struct sim_worker {
	pthread_t thread;
	pthread_mutex_t cam_mtx;
	d3d_camera *cam;
	struct map *map;
	int input_char;
};

int sim_worker_init(struct sim_worker *sim, d3d_camera *cam, struct map *map);

int sim_worker_start_tick(struct sim_worker *sim, int input_char);

int sim_worker_finish_tick(struct sim_worker *sim);

void sim_worker_destroy(struct sim_worker *sim);

#endif /* SIM_WORKER_H_ */
