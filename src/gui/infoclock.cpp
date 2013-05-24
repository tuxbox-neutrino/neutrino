#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include <pthread.h>
#include <string>

#include <sys/timeb.h>
#include <time.h>
#include <unistd.h>
#include <sys/param.h>
#include <driver/volume.h>
#include <gui/volumebar.h>
#include <gui/infoclock.h>

#define YOFF 0

CInfoClock::CInfoClock()
{
	frameBuffer = CFrameBuffer::getInstance();
	x = y = clock_x = 0;
	time_height = time_width = thrTimer = 0;
	Init();
}

CInfoClock::~CInfoClock()
{
	if(thrTimer)
		pthread_cancel(thrTimer);
	thrTimer = 0;
}

void CInfoClock::Init()
{
	CVolumeHelper::getInstance()->refresh();
	CVolumeHelper::getInstance()->getInfoClockDimensions(&clock_x, &y, &time_width, &time_height, &digit_h, &digit_offset);
}

CInfoClock* CInfoClock::getInstance()
{
	static CInfoClock* InfoClock = NULL;
	if(!InfoClock)
		InfoClock = new CInfoClock();
	return InfoClock;
}

void CInfoClock::paintTime( bool show_dot)
{
	char timestr[20];
	int dummy, mute_dx, h_spacer;
	time_t tm = time(0);
	strftime((char*) &timestr, sizeof(timestr), "%H:%M:%S", localtime(&tm));
	timestr[2] = show_dot ? ':':'.';

	CVolumeHelper *cvh = CVolumeHelper::getInstance();
	cvh->getInfoClockDimensions(&clock_x, &y, &time_width, &time_height, &digit_h, &digit_offset);
	cvh->getMuteIconDimensions(&dummy, &dummy, &mute_dx, &dummy);
	cvh->getSpacer(&h_spacer, &dummy);
	if (CNeutrinoApp::getInstance()->isMuted())
		clock_x -= (mute_dx + h_spacer);

	int x_diff = (time_width - g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getRenderWidth(timestr)) / 2;
	frameBuffer->paintBoxRel(clock_x, y, time_width, time_height, COL_MENUCONTENT_PLUS_0, RADIUS_SMALL);
	g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->RenderString(clock_x + x_diff, y + digit_h + digit_offset + ((time_height - digit_h) / 2), time_width, timestr, COL_MENUCONTENT);
	frameBuffer->blit();
}

void* CInfoClock::TimerProc(void *arg)
{

	bool show_dot = false;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,0);
 	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS,0);

	CInfoClock *InfoClock = static_cast<CInfoClock*>(arg);
	InfoClock->paintTime(show_dot);
	while(1) {
		sleep(1);
		show_dot = !show_dot;
		InfoClock->paintTime(show_dot);
	}
	return 0;
}

void CInfoClock::ClearDisplay()
{
	frameBuffer->paintBackgroundBoxRel(clock_x, y, time_width, time_height);
	Init();
}

void CInfoClock::StartClock()
{
	Init();
	
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
		frameBuffer->paintBackgroundBoxRel(clock_x, y, time_width, time_height);
		frameBuffer->blit();
	}
}
