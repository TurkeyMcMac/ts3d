#ifndef TICKER_H_
#define TICKER_H_

#if __APPLE__
#	include <time.h>
#	include <stdint.h>

struct ticker {
	uint64_t last_tick;
	uint64_t interval;
};
#elif __unix__
#	define TICKER_WITH_CLOCK
#	include <time.h>

struct ticker {
	struct timespec last_tick;
	long interval;
};
#else
#	error Unsupported OS
#endif

// Initialize a given ticker with interval in milliseconds from 1 to 999.
void ticker_init(struct ticker *tkr, int interval);

// Go forward a tick, trying to account for time spent computing inbetween.
void tick(struct ticker *tkr);

#endif /* TICKER_H_ */
