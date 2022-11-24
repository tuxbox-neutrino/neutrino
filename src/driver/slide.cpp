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
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include <driver/rcinput.h>
#include <driver/slide.h>
#include <system/proc_tools.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

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

void COSDSlider::setValue(int val)
{
	//printf("setvalue: %d\n",val);

	if (slideMode == MID2SIDE)
	{
		int w = MAX_WIDTH - val;
		proc_put_hex("/proc/stb/fb/dst_width", w);

		int l = (MAX_WIDTH - w) / 2;
		proc_put_hex("/proc/stb/fb/dst_left", l);
	}
	else
	{
		proc_put_hex("/proc/stb/fb/dst_top", val);
	}

	proc_put_hex("/proc/stb/fb/dst_apply", 1);
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
