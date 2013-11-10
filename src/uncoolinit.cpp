/*
 * (C) 2013 Stefan Seyfried, License: GPL v3+
 *
 * uncoolinit -- work around driver initalization bug on
 * (at least) Coolstream HD1 "first fifty edition".
 * When starting up with 720p, the HDMI output is pink-tinted
 * and without sound until switching resolution once.
 *
 * work around the issue by initializing the videodecoder in
 * 1080i mode.
 *
 * This is really just a workaround for crappy drivers, but
 * better than nothing :-)
 */

#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <time.h>

#include <cs_api.h>
#include <video_cs.h>
#include <dmx_cs.h>

cVideo *v = NULL;
cDemux *d = NULL;

int main (int argc, char **argv)
{
	struct timespec s, e;
	clock_gettime(CLOCK_MONOTONIC, &s);
	printf("\nuncoolinit by seife -- working around crappy drivers since 2009!\n");
	/* initialize everything */
	cs_api_init();
	d = new cDemux();		/* dummy demux for getChannel() and getBuffer() */
	d->Open(DMX_VIDEO_CHANNEL);	/* do we really need to open it? */

	/* this enables output in 1080i mode */
	v = new cVideo(VIDEO_STD_1080I50, d->getChannel(), d->getBuffer());

	/* should we slow down booting even more by showing a picture? */
	if (argc > 1) {
		v->ShowPicture(argv[1]);
		if (argc > 2)
			sleep(atoi(argv[2]));
		v->StopPicture();
	}

	/* clean up after ourselves, or the drivers will be *very* unhappy */
	delete v;
	delete d;
	cs_api_exit();
	clock_gettime(CLOCK_MONOTONIC, &e);
	printf("uncoolinit ends, took %lld ms\n",
		(e.tv_sec*1000 + e.tv_nsec/1000000LL) - (s.tv_sec*1000 + s.tv_nsec/1000000LL));
	return 0;
}
