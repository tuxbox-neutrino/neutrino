/*
	Neutrino-GUI  -   DBoxII-Project

	TimeOSD by Zwen

	Copyright (C) 2013, Thilo Graf 'dbt'
	Copyright (C) 2013, Michael Liebmann 'micha-bbg'
	Copyright (C) 2013, martii
	
	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include <gui/volumebar.h>
#include <gui/timeosd.h>



CTimeOSD::CTimeOSD():CComponentsFrmClock( 0, 0, 0, 50, "%H:%M:%S", true, CC_SHADOW_ON, COL_LIGHT_GRAY, COL_MENUCONTENT_PLUS_0,COL_MENUCONTENTDARK_PLUS_0)
{
	Init();
}

void CTimeOSD::Init()
{
	static int oldSize = 0;

	m_mode = MODE_HIDE;
	m_time_show = 0;

	if (oldSize != g_settings.infoClockFontSize) {
		oldSize = g_settings.infoClockFontSize;
		setClockFontSize(g_settings.infoClockFontSize);
	}

	//use current theme colors
	syncSysColors();

	paint_bg = g_settings.infoClockBackground;

	setClockFormat("%H:%M:%S");

	int x_old = x, y_old = y, width_old = width, height_old = height;
	CVolumeHelper::getInstance()->refresh(cl_font);
	CVolumeHelper::getInstance()->getTimeDimensions(&x, &y, &width, &height);
	if ((x_old != x) || (y_old != y) || (width_old != width) || (height_old != height))
		clear();

	// set corner radius depending on clock height
	corner_rad = (g_settings.rounded_corners) ? std::max(height/10, CORNER_RADIUS_SMALL) : 0;

	initCCLockItems();
}

#if 0 //if hide() or kill() required, it's recommended to use it separately
CTimeOSD::~CTimeOSD()
{
	CComponents::kill();
	clear();
}
#endif

void CTimeOSD::initTimeString()
{
	struct tm t;
	strftime((char*) &cl_timestr, sizeof(cl_timestr), cl_format_str.c_str(), gmtime_r(&m_time_show, &t));
}

void CTimeOSD::show(time_t time_show, bool force)
{
	time_show /= 1000;
	if (!force && (m_mode == MODE_HIDE || m_time_show == time_show))
		return;
	m_time_show = time_show;
	paint(false);
}

void CTimeOSD::updatePos(int position, int duration)
{
	int percent = (duration && duration > 100) ? (position * 100 / duration) : 0;
	if(percent > 100)
		percent = 100;
	else if(percent < 0)
		percent = 0;

	timescale.setProgress(x, y + height/4, width, height/2, percent, 100);
	timescale.setBlink();
	timescale.setRgb(0, 100, 70);
	timescale.paint();
	frameBuffer->blit();
}

void CTimeOSD::update(int position, int duration)
{
	switch(m_mode) {
		case MODE_ASC:
			show(position, false);
			break;
		case MODE_DESC:
			show(duration - position, false);
			break;
		case MODE_BAR:
			updatePos(position, duration);
			break;
		default:
			;
	}
}

void CTimeOSD::switchMode(int position, int duration)
{
	switch (m_mode) {
		case MODE_ASC:
			m_mode = MODE_DESC;
			break;
		case MODE_DESC:
			m_mode = MODE_BAR;
			CComponents::kill();
			break;
		case MODE_BAR:
			KillAndResetTimescale();
			frameBuffer->blit();
			return;
		default:
			m_mode = MODE_ASC;
	}

	update(position, duration);
}

void CTimeOSD::kill()
{
	if (m_mode != MODE_HIDE) {
		KillAndResetTimescale();
		CComponents::kill();
		frameBuffer->blit();
	}
}

void CTimeOSD::KillAndResetTimescale()
{
	m_mode = MODE_HIDE;
	timescale.kill();
	timescale.reset();
}
