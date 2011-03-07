#include <stdio.h> /* for perror */
#include <time.h>

time_t time_monotonic_ms(void)
{
	struct timespec t;
	time_t ret;
	if (clock_gettime(CLOCK_MONOTONIC, &t))
	{
		perror("time_monotonic_ms clock_gettime");
		return -1;
	}
	ret = ((t.tv_sec + 604800)& 0x01FFFFF) * 1000; /* avoid overflow */
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
