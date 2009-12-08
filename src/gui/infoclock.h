#ifndef __infoclock__
#define __infoclock__


#include <driver/framebuffer.h>
#include <driver/fontrenderer.h>
#include <system/settings.h>

#include "gui/color.h"

#include <string>


class CInfoClock
{
 private:
	CFrameBuffer * 	frameBuffer;

	pthread_t	   	thrTimer;
	int 			time_width;
	int 			time_height;
	void 			paintTime( bool show_dot);

	static void		CleanUpProc(void* arg);
	static void*		TimerProc(void *arg);

 public:
	CInfoClock();
	~CInfoClock();

	void 			StartClock();
	void 			StopClock();

};

#endif
