/*
	Neutrino-HD GUI, COSDFader implementation
	Copyright (C) 2011 Stefan Seyfried

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <global.h>
#include <driver/rcinput.h>
#include <driver/fade.h>

COSDFader::COSDFader(int trans)
{
	fb = CFrameBuffer::getInstance();
	target_trans = trans;
	timer = 0;
	step = 0;
}

COSDFader::~COSDFader()
{
	Stop();
}

void COSDFader::StartFadeIn()
{
	transparency = 100;
	step = -FADE_STEP;
	Fade();
}

/* returns true if fade out was not started before and fade is enabled */
bool COSDFader::StartFadeOut()
{
	if (!g_settings.widget_fade) /* disabled */
		return false;
	if (step == FADE_STEP) /* already fading out... */
		return false;

	/* start fading */
	step = FADE_STEP;
	Fade();
	return true;
}

void COSDFader::Stop()
{
	if (!step)
		return;

	g_RCInput->killTimer(timer);
	fb->setBlendMode(1); /* per pixel alpha */
}

/* returns true if fade out is finished */
bool COSDFader::Fade()
{
	if (!g_settings.widget_fade)
		return false;

	if (timer == 0)
		timer = g_RCInput->addTimer(FADE_TIME, false);

	if (transparency == 100 || transparency == target_trans)
	{
		fb->setBlendMode(2); /* 2 == "global alpha x pixel alpha" */
		fb->setBlendLevel(transparency);
		transparency += step;
		return false;
	}
	transparency += step;
	if (step > 0 && transparency >= 100) /* finished fading out */
	{
		transparency = target_trans;
		g_RCInput->killTimer(timer);
		return true;
	}
	if (step < 0 && transparency <= target_trans) /* finished fading in */
	{
		transparency = target_trans;
		g_RCInput->killTimer(timer);
		fb->setBlendMode(1); /* 1 == "per pixel alpha" */
		return false;
	}
	fb->setBlendLevel(transparency);
	return false;
}
