/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2022 TangoCash

	License: GPLv2

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include <driver/rcinput.h>
#include <driver/slide.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_HEIGHT 576
#define MAX_WIDTH 720
#define MIN_HEIGHT 0
#define STEPSIZE 10
#define SLIDETIME 2*5000

COSDSlider::COSDSlider(int percent, int mode)
{
	slideTimer = 0;
	slideIn = false;
	slideOut = false;
	slideMode = mode;
	if (mode == MID2SIDE)
		slideMax = MAX_WIDTH;
	else
		slideMax = MAX_HEIGHT * percent / 100;
}

COSDSlider::~COSDSlider()
{
	StopSlide();
}

int COSDSlider::proc_put(const char *path, char *value, int len)
{
	int ret, ret2;
	int pfd = open(path, O_WRONLY);

	if (pfd < 0)
		return pfd;

	ret = write(pfd, value, len);
	ret2 = close(pfd);

	if (ret2 < 0)
		return ret2;

	return ret;
}

void COSDSlider::setValue(int val)
{
	//printf("setvalue: %d\n",val);
	char buffer[10];
	if (slideMode == MID2SIDE)
	{
		int w = MAX_WIDTH - val;
		int l = (MAX_WIDTH - w) / 2;
		sprintf(buffer, "%x\n", w);
		proc_put("/proc/stb/fb/dst_width", buffer, strlen(buffer));
		sprintf(buffer, "%x\n", l);
		proc_put("/proc/stb/fb/dst_left", buffer, strlen(buffer));
	}
	else
	{
		sprintf(buffer, "%x\n", val);
		proc_put("/proc/stb/fb/dst_top", buffer, strlen(buffer));
	}
	sprintf(buffer, "%x\n", 1);
	proc_put("/proc/stb/fb/dst_apply", buffer, strlen(buffer));
}

void COSDSlider::StartSlideIn()
{
	slideIn = true;
	slideOut = false;
	slideValue = slideMax;
	setValue(slideMax);
	slideTimer = g_RCInput->addTimer(SLIDETIME, false);
}

/* return true if slide out started */
bool COSDSlider::StartSlideOut()
{
	bool ret = false;

	if (slideIn)
	{
		g_RCInput->killTimer(slideTimer);
		slideIn = false;
	}

	if (!slideOut)
	{
		slideOut = true;
		g_RCInput->postMsg(NeutrinoMessages::EVT_SLIDER, BEFORE_SLIDEOUT);
		setValue(MIN_HEIGHT);
		slideTimer = g_RCInput->addTimer(SLIDETIME, false);
		ret = true;
	}

	return ret;
}

void COSDSlider::StopSlide()
{
	if (slideIn || slideOut)
	{
		g_RCInput->killTimer(slideTimer);
		//usleep(SLIDETIME * 3);
		setValue(MIN_HEIGHT);
		slideIn = slideOut = false;
	}
}

/* return true, if slide out done */
bool COSDSlider::SlideDone()
{
	bool ret = false;

	if (slideOut) // disappear
	{
		slideValue += STEPSIZE;

		if (slideValue >= slideMax)
		{
			slideValue = slideMax;
			g_RCInput->killTimer(slideTimer);
			ret = true;
		}
		else
			setValue(slideValue);
	}
	else // appear
	{
		slideValue -= STEPSIZE;

		if (slideValue <= MIN_HEIGHT)
		{
			slideValue = MIN_HEIGHT;
			g_RCInput->killTimer(slideTimer);
			slideIn = false;
			g_RCInput->postMsg(NeutrinoMessages::EVT_SLIDER, AFTER_SLIDEIN);
		}
		else
			setValue(slideValue);
	}

	return ret;
}
