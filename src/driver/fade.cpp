/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2011 CoolStream International Ltd

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
#include <driver/rcinput.h>
#include <driver/fade.h>

COSDFader::COSDFader(unsigned char & alpha)
	: max_alpha(alpha)
{
	frameBuffer = CFrameBuffer::getInstance();
	fadeTimer = 0;
	fadeIn = false;
	fadeOut = false;
}

COSDFader::~COSDFader()
{
	Stop();
}

void COSDFader::StartFadeIn()
{
	if(!g_settings.widget_fade)
		return;

	fadeIn = true;
	fadeOut = false;
	fadeValue = 100;
	frameBuffer->setBlendMode(2); // Global alpha multiplied with pixel alpha
	frameBuffer->setBlendLevel(fadeValue);
	fadeTimer = g_RCInput->addTimer( FADE_TIME, false );
}

/* return true if fade out started */
bool COSDFader::StartFadeOut()
{
	bool ret = false;
	if ( fadeIn ) {
		g_RCInput->killTimer(fadeTimer);
		fadeIn = false;
	}
	if ((!fadeOut) && g_settings.widget_fade) {
		fadeOut = true;
		fadeTimer = g_RCInput->addTimer( FADE_TIME, false );
		frameBuffer->setBlendMode(2); // Global alpha multiplied with pixel alpha
		ret = true;
	}
	return ret;
}

void COSDFader::Stop()
{
	if ( fadeIn || fadeOut ) {
		g_RCInput->killTimer(fadeTimer);
		frameBuffer->setBlendMode(1); // Set back to per pixel alpha
		fadeIn = fadeOut = false;
	}
}

/* return true, if fade out done */
bool COSDFader::Fade()
{
	bool ret = false;

	if (fadeOut) { // disappear
		fadeValue += FADE_STEP;
		if (fadeValue >= 100) {
			fadeValue = max_alpha;
			g_RCInput->killTimer (fadeTimer);
			ret = true;
		} else
			frameBuffer->setBlendLevel(fadeValue);
	} else { // appears
		fadeValue -= FADE_STEP;
		if (fadeValue <= max_alpha) {
			fadeValue = max_alpha;
			g_RCInput->killTimer (fadeTimer);
			fadeIn = false;
			frameBuffer->setBlendMode(1); // Set back to per pixel alpha
		} else
			frameBuffer->setBlendLevel(fadeValue);
	}
	return ret;
}
