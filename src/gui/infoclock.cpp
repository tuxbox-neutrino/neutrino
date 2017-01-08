/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Info Clock Window
	based up CComponentsFrmClock
	Copyright (C) 2013-2015, Thilo Graf 'dbt'
	Copyright (C) 2013, Michael Liebmann 'micha-bbg'

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
#include <gui/infoclock.h>



CInfoClock::CInfoClock():CComponentsFrmClock( 1, 1, NULL, "%H:%M:%S", NULL, false, 1, NULL, CC_SHADOW_ON)
{
	initCCLockItems();
}

CInfoClock* CInfoClock::getInstance()
{
	static CInfoClock* InfoClock = NULL;
	if(!InfoClock)
		InfoClock = new CInfoClock();
	return InfoClock;
}

void CInfoClock::initCCLockItems()
{
	paint_bg = g_settings.infoClockBackground;

	//use current theme colors
	setColorAll(COL_FRAME_PLUS_0, COL_MENUCONTENT_PLUS_0, COL_SHADOW_PLUS_0);

	//set text color
	if (paint_bg){
		cl_col_text = COL_MENUCONTENT_TEXT;
		setColorBody(COL_MENUCONTENT_PLUS_0);
		enableShadow(CC_SHADOW_ON, OFFSET_SHADOW/2);
	}else{
		cl_col_text = COL_INFOCLOCK_TEXT;
		setColorBody(COL_BACKGROUND_PLUS_0);
		disableShadow();
	}

	if (g_settings.infoClockSeconds)
		setClockFormat("%H:%M:%S");
	else
		setClockFormat("%H:%M", "%H %M");

	//set height, NOTE: height is strictly bound to settings
	height = g_settings.infoClockFontSize;
	initClockFont(0, height);

	// set corner radius depending on clock height
	corner_rad = (g_settings.rounded_corners) ? std::max(height/10, CORNER_RADIUS_SMALL) : 0;

	CVolumeHelper::getInstance()->refresh(cl_font);
	CVolumeHelper::getInstance()->getInfoClockDimensions(&x, &y, &width, &height);
}

void CInfoClock::ClearDisplay()
{
	bool run = isRun();
	this->kill();
	clearSavedScreen();
	initCCLockItems();
	//provokes full repaint for next activation, otherwise clock segments only repaints on changed content
	clear();
	if (run)
		Start();
}

bool CInfoClock::StartInfoClock()
{
	initCCLockItems();
	return Start();
}

bool CInfoClock::StopInfoClock()
{
	bool ret = Stop();
	kill();

	return ret;
}

bool CInfoClock::enableInfoClock(bool enable)
{
	bool ret = false;
	if (g_settings.mode_clock) {
		if (enable) {
			if (isBlocked()) //blocked
				ret = StartInfoClock();
		}
		else {
			if (!isBlocked()) //unblocked
				ret = StopInfoClock();
		}
	}
	return ret;
}

//switching clock on or off depends of current displayed or not
void CInfoClock::switchClockOnOff()
{
	if(g_settings.mode_clock)
		g_settings.mode_clock = false;
	else
		g_settings.mode_clock = true;
}
