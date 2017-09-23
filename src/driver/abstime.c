#include <stdint.h>
#include <stdio.h> /* for perror */
#include <time.h>

int64_t time_monotonic_ms(void)
{
	struct timespec t;
	time_t ret;
	if (clock_gettime(CLOCK_MONOTONIC, &t))
	{
		perror("time_monotonic_ms clock_gettime");
		return -1;
	}
	ret = (t.tv_sec + 604800) * (int64_t)1000; /* avoid overflow */
	ret += t.tv_nsec / 1000000;
	return ret;
}

time_t time_monotonic(void)
{
	struct timespec t;
	if (clock_gettime(CLOCK_MONOTONIC, &t))
	{
		perror("time_monotonic clock_gettime");
		return -1;
	}
	/* CLOCK_MONOTONIC is often == uptime, so starts at 0. Bad for relative
	   time comparisons if the uptime is low, so add 7 days */
	return t.tv_sec + 604800;
}

uint64_t time_monotonic_us(void)
{
	struct timespec t;
	if (clock_gettime(CLOCK_MONOTONIC, &t))
	{
		perror("time_monotonic_us clock_gettime");
		return -1;
	}
	return ((int64_t)t.tv_sec + (uint64_t)604800) * (uint64_t) 1000000 + (uint64_t) t.tv_nsec / (uint64_t)1000;
}
