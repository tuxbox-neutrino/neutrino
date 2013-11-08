#ifndef __infoclock__
#define __infoclock__


#include <driver/framebuffer.h>
#include <driver/fontrenderer.h>
#include <system/settings.h>

#include <gui/color.h>

#include <string>


class CInfoClock
{
 private:
	CFrameBuffer * 	frameBuffer;

	pthread_t   	thrTimer;
	void 		paintTime( bool show_dot);
	int 		time_offset, digit_offset, digit_h;
	int 		x, y, clock_x;
	void		Init();
	static void	CleanUpProc(void* arg);
	static void*	TimerProc(void *arg);

 public:
	CInfoClock();
	~CInfoClock();
	static		CInfoClock* getInstance();

	void 		StartClock();
	void 		StopClock();
	void		ClearDisplay();

	int 		time_width, time_height;
};

#endif
