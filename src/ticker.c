#include "ticker.h"

#if __APPLE__

void ticker_init(struct ticker *tkr, int interval)
{
	mach_timebase_info(&tkr->timebase);
	tkr->interval = interval * 1000000 * tkr->timebase.denom
		/ tkr->timebase.numer;
	tkr->last_tick = mach_continuous_time();
}

void tick(struct ticker *tkr)
{
	uint64_t now = mach_continuous_time();
	uint64_t deadline = tkr->last_tick + tkr->interval;
	if (now < deadline) {
		uint64_t delay = deadline - now;
		struct timespec ts = {0, delay * tkr->timebase.numer
			/ tkr->timebase.denom};
		nanosleep(&ts, NULL);
		tkr->last_tick = deadline;
	} else {
		tkr->last_tick = now;
	}
}

#else

void ticker_init(struct ticker *tkr, int interval)
{
	clock_gettime(CLOCK_MONOTONIC, &tkr->last_tick);
	tkr->interval = interval * 1000000;
}

void tick(struct ticker *tkr)
{
	tkr->last_tick.tv_nsec += tkr->interval;
	if (tkr->last_tick.tv_nsec >= 1000000000) {
		tkr->last_tick.tv_nsec -= 1000000000;
		++tkr->last_tick.tv_sec;
	}
	clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &tkr->last_tick, NULL);
	clock_gettime(CLOCK_MONOTONIC, &tkr->last_tick);
}

#endif /* !__APPLE__ */

#if CTF_TESTS_ENABLED

#	include "libctf.h"
#	include <assert.h>
#	include <sys/time.h>

CTF_TEST(tick_is_right_length,
	struct ticker tkr;
	struct timeval before, after;
	struct timespec sleep = { .tv_sec = 0, .tv_nsec = 51000000 };
	gettimeofday(&before, NULL);
	ticker_init(&tkr, 500);
	nanosleep(&sleep, NULL);
	tick(&tkr);
	gettimeofday(&after, NULL);
	long delay = after.tv_usec - before.tv_usec
		+ 1000000 * (after.tv_sec - before.tv_sec);
	assert(delay > 450000);
	assert(delay < 550000);
)

CTF_TEST(much_missed_tick_time_ignored,
	struct ticker tkr;
	struct timeval before, after;
	struct timespec sleep = { .tv_sec = 0, .tv_nsec = 500000000 };
	ticker_init(&tkr, 20);
	nanosleep(&sleep, NULL);
	gettimeofday(&before, NULL);
	tick(&tkr);
	tick(&tkr);
	gettimeofday(&after, NULL);
	long delay = after.tv_usec - before.tv_usec
		+ 1000000 * (after.tv_sec - before.tv_sec);
	assert(delay > 20000);
	assert(delay < 23000);
)

#endif /* CTF_TESTS_ENABLED */
