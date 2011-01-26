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
#include <gui/widget/progressbar.h>

static CProgressBar *timescale = 0;

#define TIMEOSD_FONT SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME
#define BARLEN 200
CTimeOSD::CTimeOSD()
{
	frameBuffer = CFrameBuffer::getInstance();
	visible=false;
	m_mode=MODE_ASC;
	GetDimensions();
	if (! timescale ){
		if(m_height < 5)
			m_height = 10;
		timescale = new CProgressBar(true, BARLEN, m_height -5, 40, 100, 70, true);
	}
}

CTimeOSD::~CTimeOSD()
{
	hide();
	if(timescale) {
		delete timescale;
		timescale = 0;
	}
}

void CTimeOSD::show(time_t time_show)
{
	GetDimensions();
	visible = true;
	m_time_dis  = time(NULL);
	m_time_show = time_show;
	frameBuffer->paintBoxRel(m_xstart-2, m_y, 2+BARLEN+2, m_height, COL_INFOBAR_SHADOW_PLUS_0); //border
	if(timescale)
		timescale->reset();
	update();
}

void CTimeOSD::GetDimensions()
{
	m_xstart = g_settings.screen_StartX + 10;
	m_xend = frameBuffer->getScreenWidth();
	m_height = g_Font[TIMEOSD_FONT]->getHeight();
	m_y = frameBuffer->getScreenY();
	m_width = g_Font[TIMEOSD_FONT]->getRenderWidth("00:00:00");
	t1 = g_Font[TIMEOSD_FONT]->getRenderWidth(widest_number);
	m_width += t1;
	if(g_settings.mode_clock) {
		m_xend = m_xend - m_width - (m_width/4);
	}
}

void CTimeOSD::update(time_t time_show)
{
	time_t tDisplayTime;
	static time_t oldDisplayTime = 0;
	char cDisplayTime[8+1];
	fb_pixel_t color1, color2;

//printf("CTimeOSD::update time %ld\n", time_show);
	if(!visible)
		return;
	if(m_mode == MODE_ASC) {
		color1 = COL_MENUCONTENT_PLUS_0;
		color2 = COL_MENUCONTENT;
	} else {
		color1 = COL_MENUCONTENTSELECTED_PLUS_0;
		color2 = COL_MENUCONTENTSELECTED;
		if(!time_show) time_show = 1;
	}
	if(time_show) {
		m_time_show = time_show;
		tDisplayTime = m_time_show;
	} else {
		if(m_mode == MODE_ASC) {
			tDisplayTime = m_time_show + (time(NULL) - m_time_dis);
		} else {
			tDisplayTime = m_time_show + (m_time_dis - time(NULL));
		}
	}
	if(tDisplayTime < 0)
		tDisplayTime=0;
	if(tDisplayTime != oldDisplayTime) {
		oldDisplayTime = tDisplayTime;
		strftime(cDisplayTime, 9, "%T", gmtime(&tDisplayTime));
		frameBuffer->paintBoxRel(m_xend - m_width - t1, m_y, m_width, m_height, color1,RADIUS_SMALL);
		g_Font[TIMEOSD_FONT]->RenderString(m_xend - m_width - (t1/2),  m_y + m_height, m_width,    cDisplayTime, color2);
	}
}

void CTimeOSD::updatePos(short runningPercent)
{
  	if(timescale){
		if(runningPercent > 100 || runningPercent < 0){
			runningPercent = 0;
		}
		timescale->paintProgressBar2(m_xstart, m_y, runningPercent);
	}
}

void CTimeOSD::hide()
{
	GetDimensions();
	if(!visible)
		return;
	frameBuffer->paintBackgroundBoxRel(m_xend - m_width - t1, m_y, m_width, m_height);
	visible=false;
	if(timescale)
		timescale->reset();
	frameBuffer->paintBackgroundBoxRel(m_xstart-2, m_y, 2+BARLEN+2, m_height); //clear border

}
