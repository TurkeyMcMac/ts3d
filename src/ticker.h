#ifndef TICKER_H_
#define TICKER_H_

#include <time.h>

#if __APPLE__ /* Mac OS doesn't support clock_gettime etc. */

#	include <mach/mach_time.h>
#	include <stdint.h>

struct ticker {
	mach_timebase_info_data_t timebase;
	uint64_t last_tick;
	uint64_t interval;
};

#elif defined(_WIN32)

#	include <stdint.h>

struct ticker {
	uint32_t last_tick;
	uint32_t interval;
};

#else

struct ticker {
	struct timespec last_tick;
	long interval;
};

#endif /* !__APPLE__ && !defined(_WIN32) */

// Initialize a given ticker with interval in milliseconds from 1 to 999.
void ticker_init(struct ticker *tkr, int interval);

// Go forward a tick, trying to account for time spent computing inbetween.
void tick(struct ticker *tkr);

#endif /* TICKER_H_ */
