#define _POSIX_C_SOURCE 200112L
#include "ticker.h"

#if __APPLE__

#	include <mach/mach_time.h>

void ticker_init(struct ticker *tkr, int interval)
{
	mach_timebase_info_data_t tb;
	mach_timebase_info(&tb);
	tkr->interval = interval * 1000000 * tb.denom / tb.numer;
	tkr->last_tick = mach_continuous_time();
}

void tick(struct ticker *tkr)
{
	uint64_t now = mach_continuous_time();
	uint64_t deadline = tkr->last_tick + tkr->interval;
	if (now < deadline) {
		uint64_t delay = deadline - now;
		mach_timebase_info_data_t tb;
		mach_timebase_info(&tb);
		struct timespec ts = {0, delay * tb.numer / tb.denom};
		nanosleep(&ts, NULL);
	}
	tkr->last_tick = deadline;
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
}

#endif /* !__APPLE__ */

#if CTF_TESTS_ENABLED

#	include "libctf.h"
#	include <assert.h>
#	include <sys/time.h>

CTF_TEST(ts3d_tick_is_right_length,
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

#endif /* CTF_TESTS_ENABLED */
