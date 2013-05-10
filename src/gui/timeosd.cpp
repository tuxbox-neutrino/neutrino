/*
	Neutrino-GUI  -   DBoxII-Project

	Timerliste by Zwen
	
	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

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
#include <gui/timeosd.h>
#include <driver/fontrenderer.h>
#include <system/settings.h>
#include <gui/infoclock.h>

#define TIMEOSD_FONT SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME
#define BARLEN 200

extern CInfoClock *InfoClock;

CTimeOSD::CTimeOSD()
{
	frameBuffer = CFrameBuffer::getInstance();
	visible=false;
	m_mode=MODE_ASC;
	GetDimensions();
	timescale = new CProgressBar();
	m_time_show = 0;
}

CTimeOSD::~CTimeOSD()
{
	hide();
	delete timescale;
}

void CTimeOSD::show(time_t time_show)
{
	if (g_settings.mode_clock)
		InfoClock->StartClock();

	GetDimensions();
	visible = true;
	SetMode(CTimeOSD::MODE_ASC);
	m_time_show = 0;
	frameBuffer->paintBoxRel(m_xstart-2, m_y, 2+BARLEN+2, m_height, COL_INFOBAR_SHADOW_PLUS_0); //border
	timescale->reset();
	update(time_show);
}

void CTimeOSD::GetDimensions()
{
	m_xstart = g_settings.screen_StartX + 10;
	m_xend = frameBuffer->getScreenWidth();
	m_height = g_Font[TIMEOSD_FONT]->getHeight();
	if(m_height < 10)
		m_height = 10;
	m_y = frameBuffer->getScreenY();
	m_width = g_Font[TIMEOSD_FONT]->getRenderWidth("00:00:00");
	t1 = g_Font[TIMEOSD_FONT]->getRenderWidth(widest_number);
	m_width += t1;
	if(g_settings.mode_clock)
		m_xend = m_xend - m_width - (m_width/4);
}

void CTimeOSD::update(time_t time_show)
{
	char cDisplayTime[8+1];
	fb_pixel_t color1, color2;

	if(!visible)
		return;

	time_show /= 1000;
	if(m_time_show == time_show)
		return;

	//printf("CTimeOSD::update time %ld -> %ld\n", m_time_show, time_show);
	m_time_show = time_show;

	if(m_mode == MODE_ASC) {
		color1 = COL_MENUCONTENT_PLUS_0;
		color2 = COL_MENUCONTENT;
	} else {
		color1 = COL_MENUCONTENTSELECTED_PLUS_0;
		color2 = COL_MENUCONTENTSELECTED;
	}
	strftime(cDisplayTime, 9, "%T", gmtime(&time_show));
	frameBuffer->paintBoxRel(m_xend - m_width - t1, m_y, m_width, m_height, color1,RADIUS_SMALL);
	g_Font[TIMEOSD_FONT]->RenderString(m_xend - m_width - (t1/2),  m_y + m_height, m_width,    cDisplayTime, color2);
	frameBuffer->blit();
}

void CTimeOSD::updatePos(short runningPercent)
{
	if(!visible)
		return;

	if(runningPercent > 100 || runningPercent < 0)
		runningPercent = 0;

	timescale->setProgress(m_xstart, m_y, BARLEN, m_height -5, runningPercent, 100);
	timescale->setBlink();
	timescale->setRgb(0, 100, 70);
	timescale->paint();
	frameBuffer->blit();
}

void CTimeOSD::update(int position, int duration)
{
	if(!visible)
		return;

	int percent = 0;
	if(duration > 100)
		percent = (unsigned char) (position / (duration / 100));
	if(m_mode == CTimeOSD::MODE_ASC)
		update(position /* / 1000*/);
	else
		update((duration - position)/* / 1000 */);
	updatePos(percent);
}

void CTimeOSD::hide()
{
	if(!visible)
		return;

	if (g_settings.mode_clock)
		InfoClock->StopClock();

	//GetDimensions();
	frameBuffer->paintBackgroundBoxRel(m_xend - m_width - t1, m_y, m_width, m_height);
	timescale->reset();
	frameBuffer->paintBackgroundBoxRel(m_xstart-2, m_y, 2+BARLEN+2, m_height); //clear border
	frameBuffer->blit();
	visible=false;
}

void CTimeOSD::switchMode(int position, int duration)
{
	if(visible) {
		if (GetMode() == CTimeOSD::MODE_ASC) {
			SetMode(CTimeOSD::MODE_DESC);
			update(position, duration);
		} else 
			hide();
	} else 
		show(position);
}
