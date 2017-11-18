/*
 * shutdown_helper - switch off the box with a possible timer wakeup
 * this is needed because the drivers do not implement a proper
 * hwclock interface :-(
 *
 * (C) 2013-2014 Stefan Seyfried
 *
 * License: GPLv2+
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <cs_frontpanel.h>

int main(int argc, char **argv)
{
	int fd;
	int leds;
	int brightness;
	fp_standby_data_t fp;
	time_t timer_seconds;
	time_t now = time(NULL);
	struct tm *tmnow = localtime(&now);
	time_t fp_timer = 0;

	if (argc < 3) {
		fprintf(stderr, "need timer_seconds, led and brightness values\n");
		return 1;
	}

	timer_seconds = atol(argv[1]);
	leds = atoi(argv[2]);
	brightness = atoi(argv[3]);

	if (timer_seconds) {
		fp_timer = (timer_seconds - now) / 60;
		if (fp_timer < 1) {
			fprintf(stderr, "warning: interval less than 1 minute (%d s) correcting\n",
					(int)(timer_seconds - now));
			fp_timer = 1;
		}
	}
	fprintf(stderr, "now: %ld, timer %ld, FP timer %ldmin\n\n", now / 60, timer_seconds / 60, fp_timer);

	fp.brightness		= brightness;
	fp.flags		= leds;
	fp.current_hour		= tmnow->tm_hour;
	fp.current_minute	= tmnow->tm_min;
	fp.timer_minutes_hi	= fp_timer >> 8;;
	fp.timer_minutes_lo	= fp_timer & 0xFF;

	system("/etc/init.d/rcK");

	fd = open("/dev/display", O_RDONLY);
	if (fd < 0)
		perror("/dev/display");
	else {
		if (ioctl(fd, IOC_FP_STANDBY, (fp_standby_data_t *) &fp))
			perror("IOC_FP_STANDBY");
		else {
			while (1)
				sleep(1);
		}
	}
	return 1;
}
