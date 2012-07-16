/*
	GUI window component classes - Neutrino-GUI

	Copyright(C) 2012, Thilo Graf (dbt)

	This class contains generic components for GUI-related parts.

	License: GPL

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Library General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Library General Public License for more details.

	You should have received a copy of the GNU Library General Public
	License along with this library; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gui/widget/components.h>

#include <global.h>
#include <neutrino.h>


//basic class CComponents
CComponents::CComponents(const int x_pos, const int y_pos, const int h, const int w)
{
	x = x_pos;
	y = y_pos;
	height = h;
	width = w;
	frameBuffer = CFrameBuffer::getInstance();	
}

CComponents::~CComponents()
{
	
}

//-------------------------------------------------------------------------------------------------------
//sub class CComponentsDetailLine
CComponentsDetailLine::CComponentsDetailLine(const int x_pos, const int y_pos_top, const int y_pos_down, const int h_mark_top_, const int h_mark_down_, fb_pixel_t color1, fb_pixel_t color2)
{
	x 		= x_pos;
	width 		= 16;
	thickness 	= 4;
	y 		= y_pos_top;
	y_down 		= y_pos_down;
	h_mark_top 	= h_mark_top_;
	h_mark_down 	= h_mark_down_;
	offs_up 	= offs_down = 0;
	col1 		= color1;
	col2		= color2;
}

//		y_top (=y)
//	xpos	+--|h_mark_up
//		|
//		|
//		|
//		|
//		|
//		|
//		|
//		|
//		|
//		+--|h_mark_down
//		y_down

//paint details line with current parameters 
void CComponentsDetailLine::paint()
{
	offs_up 	= h_mark_top/2-thickness+1;
	offs_down	= h_mark_down/2-thickness+2;
	int y_top	= y;
	
	/* vertical item mark | */
	frameBuffer->paintBoxRel(x+width-4, 		y_top-offs_up, 		thickness, 	h_mark_top, 			col1);
	frameBuffer->paintBoxRel(x+width-5+thickness,	y_top-offs_up, 		1, 		h_mark_top, 			col2);
	
	/* horizontal item line - */
	frameBuffer->paintBoxRel(x+width-15, 		y_top+1,		11, 		thickness, 			col1);
	frameBuffer->paintBoxRel(x+width-15+thickness,	y_top+1+thickness, 	11-thickness,	1, 				col2);
	
	/* vertical connect line [ */
	frameBuffer->paintBoxRel(x+width-15, 		y_top+2, 		thickness, 	y_down-y_top-1, 		col1);
	frameBuffer->paintBoxRel(x+width-15+thickness,	y_top+2+thickness,	1, 		y_down-y_top+3-2*thickness,	col2);
	
	/* horizontal info line - */
	frameBuffer->paintBoxRel(x+width-15, 		y_down, 		11, 		thickness, 			col1);
	frameBuffer->paintBoxRel(x+width-14+thickness,	y_down+thickness, 	11-thickness,	1, 				col2);
	
	/* vertical info mark | */
	frameBuffer->paintBoxRel(x+width-4, 		y_down-offs_down, 	thickness, 	h_mark_down, 			col1);
	frameBuffer->paintBoxRel(x+width-5+thickness,	y_down-offs_down, 	1, 		h_mark_down, 			col2);
}

//remove painted lines from screen
void CComponentsDetailLine::hide()
{
	//caching current colors
	fb_pixel_t c_tmp1, c_tmp2;
	c_tmp1 = col1;
	c_tmp2 = col2;
	
	//set background color
	col1 = col2 = COL_BACKGROUND;
	
	//paint with background and restore set last used colors
	paint();
	col1 = c_tmp1;
	col2 = c_tmp2;
}

