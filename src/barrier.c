#include "barrier.h"
#include <errno.h>
#include <pthread.h>

int barrier_init(struct barrier *bar)
{
	int err;
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
	if ((err = pthread_mutex_init(&bar->mtxs[0], &attr)))
		goto error_mutex_0;
	if ((err = pthread_mutex_init(&bar->mtxs[1], &attr)))
		goto error_mutex_1;
	pthread_mutexattr_destroy(&attr);
	return 0;

error_mutex_1:
	pthread_mutex_destroy(&bar->mtxs[0]);
error_mutex_0:
	pthread_mutexattr_destroy(&attr);
	errno = err;
	return -1;
}

int barrier_start_using(struct barrier *bar)
{
	int err;
	if ((err = pthread_mutex_trylock(&bar->mtxs[0]))) {
		if (err != EBUSY) goto error;
		if ((err = pthread_mutex_trylock(&bar->mtxs[1]))
		 && err != EBUSY)
			goto error;
		return 0;
	} else {
		return 0;
	}
	errno = EBUSY;
	return -1;

error:
	errno = err;
	return -1;
}

int barrier_wait(struct barrier *bar)
{
	int err;
	size_t owned;
	for (owned = 0; owned < 2; ++owned) {
		if ((err = pthread_mutex_unlock(&bar->mtxs[owned]))) {
			if (err != EPERM) goto error;
		} else {
			break;
		}
	}
	if (owned >= 2) {
		errno = EPERM;
		return -1;
	}
	if ((err = pthread_mutex_lock(&bar->mtxs[!owned]))) goto error;
	return 0;

error:
	errno = err;
	return -1;
}

int barrier_stop_using(struct barrier *bar)
{
	pthread_mutex_unlock(&bar->mtxs[0]);
	pthread_mutex_unlock(&bar->mtxs[1]);
	return 0;
}

int barrier_destroy(struct barrier *bar)
{
	return 0;
}
