#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include <pthread.h>
#include <string>

#include <sys/timeb.h>
#include <time.h>
#include <sys/param.h>
#include "infoclock.h"

#define YOFF 0

CInfoClock::CInfoClock()
{
	frameBuffer      = CFrameBuffer::getInstance();
	x = frameBuffer->getScreenWidth();
	y = frameBuffer->getScreenY();

	time_height = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getHeight();
	int t1 = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getRenderWidth(widest_number);
	int t2 = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getRenderWidth(":");
	time_width = t1*6 + t2*2;
	thrTimer = 0;
}

CInfoClock::~CInfoClock()
{
	if(thrTimer)
		pthread_cancel(thrTimer);
	thrTimer = 0;
}

void CInfoClock::paintTime( bool show_dot)
{
	char timestr[20];
	time_t tm = time(0);
	strftime((char*) &timestr, sizeof(timestr), "%H:%M:%S", localtime(&tm));
	timestr[2] = show_dot ? ':':'.';

	frameBuffer->paintBoxRel(x - time_width - 10, y, time_width, time_height, COL_MENUCONTENT_PLUS_0, RADIUS_SMALL);
	g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->RenderString(x - time_width- 5, y+ time_height, time_width, timestr, COL_MENUCONTENT);
}

void* CInfoClock::TimerProc(void *arg)
{

	bool show_dot = false;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,0);
 	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS,0);

	CInfoClock *InfoClock = (CInfoClock*) arg;

	InfoClock->paintTime(show_dot);
	while(1) {
		sleep(1);
		show_dot = !show_dot;
		InfoClock->paintTime(show_dot);
	}
	return 0;
}

void CInfoClock::StartClock()
{
	if(!thrTimer) {
		pthread_create (&thrTimer, NULL, TimerProc, (void*) this) ;
		pthread_detach(thrTimer);
	}
}

void CInfoClock::StopClock()
{
	if(thrTimer) {
		pthread_cancel(thrTimer);
		thrTimer = 0;
		frameBuffer->paintBackgroundBoxRel(x - time_width - 10, y, time_width, time_height);
	}
}
