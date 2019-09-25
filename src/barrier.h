#ifndef BARRIER_H_
#define BARRIER_H_

#include <pthread.h>

struct barrier {
	pthread_mutex_t mtxs[2];
};

int barrier_init(struct barrier *bar);

int barrier_start_using(struct barrier *bar);

int barrier_wait(struct barrier *bar);

int barrier_stop_using(struct barrier *bar);

int barrier_destroy(struct barrier *bar);

#endif /* BARRIER_H_ */
