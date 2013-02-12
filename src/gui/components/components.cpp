/*
	GUI window component classes - Neutrino-GUI

	Copyright(C) 2012, 2013, Thilo Graf
	Copyright(C) 2012, Michael Liebmann

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

#include <global.h>
#include <neutrino.h>
#include "cc.h"





//basic class CComponents
CComponents::CComponents(const int x_pos, const int y_pos, const int h, const int w)
{
	x = x_pos;
	y = y_pos;
	height = h;
	width = w;
	sw = 0; //shadow width
	frameBuffer = CFrameBuffer::getInstance();
	v_screen_val.clear();
}

CComponents::~CComponents()
{
	clear();
}

//paint framebuffer stuff and fill buffer
void CComponents::paintFbItems(struct comp_fbdata_t * fbdata, const int items_count, bool do_save_bg)
{
	int i;
	for(i=0; i< items_count ;i++){
		if (do_save_bg)
			fbdata[i].pixbuf = saveScreen(fbdata[i].x, fbdata[i].y, fbdata[i].dx, fbdata[i].dy);
		v_screen_val.push_back(fbdata[i]);
	}

	for(i=0; i< items_count ;i++){
		if (fbdata[i].is_frame)
			frameBuffer->paintBoxFrame(fbdata[i].x, fbdata[i].y, fbdata[i].dx, fbdata[i].dy, fbdata[i].frame_thickness, fbdata[i].color, fbdata[i].r);
		else
			frameBuffer->paintBoxRel(fbdata[i].x, fbdata[i].y, fbdata[i].dx, fbdata[i].dy, fbdata[i].color, fbdata[i].r);
	}
}

//screen area save
inline fb_pixel_t* CComponents::saveScreen(int ax, int ay, int dx, int dy)
{
	fb_pixel_t* pixbuf = new fb_pixel_t[dx * dy];
	frameBuffer->SaveScreen(ax, ay, dx, dy, pixbuf);
	return pixbuf;
}

//restore screen
inline void CComponents::restore()
{
	for(size_t i =0; i< v_screen_val.size() ;i++) {
		if (v_screen_val[i].pixbuf != NULL){
			frameBuffer->RestoreScreen(v_screen_val[i].x, v_screen_val[i].y, v_screen_val[i].dx, v_screen_val[i].dy, v_screen_val[i].pixbuf);
			delete[] v_screen_val[i].pixbuf;
		}
	}
	v_screen_val.clear();
}

//clean old screen buffer
inline void CComponents::clear()
{
	for(size_t i =0; i< v_screen_val.size() ;i++)
		if (v_screen_val[i].pixbuf != NULL)
			delete[] v_screen_val[i].pixbuf;
	v_screen_val.clear();
}

//-------------------------------------------------------------------------------------------------------
//sub class CComponentsDetailLine
CComponentsDetailLine::CComponentsDetailLine(const int x_pos, const int y_pos_top, const int y_pos_down, const int h_mark_top_, const int h_mark_down_, fb_pixel_t color_line, fb_pixel_t color_shadow)
{
	x 		= x_pos;
	width 		= 16;
	thickness 	= 4;
	sw		= 1; //shadow width
	y 		= y_pos_top;
	y_down 		= y_pos_down;
	h_mark_top 	= h_mark_top_;
	h_mark_down 	= h_mark_down_;
	col_line 	= color_line;
	col_shadow	= color_shadow;
}

CComponentsDetailLine::~CComponentsDetailLine()
{
	clear();
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


#define DLINE_ITEMS_COUNT 12
//paint details line with current parameters 
void CComponentsDetailLine::paint(bool do_save_bg)
{
	clear();

	int y_mark_top = y-h_mark_top/2+thickness/2;
	int y_mark_down = y_down-h_mark_down/2+thickness/2;
	
	comp_fbdata_t fbdata[DLINE_ITEMS_COUNT] =
	{
		/* vertical item mark | */
		{x+width-thickness-sw, 	y_mark_top, 		thickness, 		h_mark_top, 		col_line, 	0, NULL, NULL, false, 0},
		{x+width-sw,		y_mark_top, 		sw, 			h_mark_top, 		col_shadow, 	0, NULL, NULL, false, 0},
		{x+width-thickness-sw,	y_mark_top+h_mark_top, 	thickness+sw, 		sw	, 		col_shadow, 	0, NULL, NULL, false, 0},
		
		/* horizontal item line - */
		{x, 			y,			width-thickness-sw,	thickness, 		col_line, 	0, NULL, NULL, false, 0},
		{x+thickness,		y+thickness,		width-2*thickness-sw,	sw, 			col_shadow, 	0, NULL, NULL, false, 0},
		
		/* vertical connect line [ */
		{x,			y+thickness, 		thickness, 		y_down-y-thickness, 	col_line, 	0, NULL, NULL, false, 0},
		{x+thickness,		y+thickness+sw,		sw, 			y_down-y-thickness-sw,	col_shadow, 	0, NULL, NULL, false, 0},
		
		/* horizontal info line - */
		{x,			y_down, 		width-thickness-sw, 	thickness, 		col_line, 	0, NULL, NULL, false, 0},
		{x,			y_down+thickness, 	width-thickness-sw,	sw, 			col_shadow, 	0, NULL, NULL, false, 0},
		
		/* vertical info mark | */
		{x+width-thickness-sw,	y_mark_down, 		thickness, 		h_mark_down, 		col_line, 	0, NULL, NULL, false, 0},
		{x+width-sw,		y_mark_down, 		sw, 			h_mark_down, 		col_shadow, 	0, NULL, NULL, false, 0},
		{x+width-thickness-sw,	y_mark_down+h_mark_down,thickness+sw, 		sw,	 		col_shadow, 	0, NULL, NULL, false, 0},
	};
	
	paintFbItems(fbdata, DLINE_ITEMS_COUNT, do_save_bg);
}

//remove painted lines from screen
void CComponentsDetailLine::hide()
{
	//caching current colors
	fb_pixel_t c_tmp1, c_tmp2;
	c_tmp1 = col_line;
	c_tmp2 = col_shadow;
	
	//set background color
	col_line = col_shadow = COL_BACKGROUND;
	
	//paint with background and restore, set last used colors
	paint(false);
	col_line = c_tmp1;
	col_shadow = c_tmp2;
}


//-------------------------------------------------------------------------------------------------------
//sub class CComponentsInfoBox
CComponentsInfoBox::CComponentsInfoBox(const int x_pos, const int y_pos, const int w, const int h, bool has_shadow, fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	x 		= x_pos;
	y 		= y_pos;
	width 		= w;
	height	 	= h;
	rad 		= 0;
	shadow		= has_shadow;
	col_frame 	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;
	fr_thickness	= 2;
	firstPaint	= true;
	v_infobox_val.clear();
}

#define INFOBOX_ITEMS_COUNT 3	
void CComponentsInfoBox::paint(bool do_save_bg, bool fullPaint)
{
	clear();
	rad = RADIUS_LARGE;

	comp_fbdata_t fbdata[INFOBOX_ITEMS_COUNT] =
	{
		{x+SHADOW_OFFSET,	y+SHADOW_OFFSET, 	width, 			height, 		col_shadow, 	rad, NULL, NULL, false, 0},
		{x, 			y, 			width, 			height, 		col_frame, 	rad, NULL, NULL, false, 0},
		{x+fr_thickness,	y+fr_thickness, 	width-2*fr_thickness, 	height-2*fr_thickness, 	col_body, 	rad, NULL, NULL, false, 0},
	};

	int start = (shadow) ? 0 : 1;
	if (firstPaint) {
		if (do_save_bg) {
			v_infobox_val.clear();
			for(int i = start; i < INFOBOX_ITEMS_COUNT; i++) {
				fbdata[i].pixbuf = saveScreen(fbdata[i].x, fbdata[i].y, fbdata[i].dx, fbdata[i].dy);
				v_infobox_val.push_back(fbdata[i]);
				fbdata[i].pixbuf = NULL;
			}
		}
		// paint infobox full
		paintFbItems((comp_fbdata_t*)&fbdata[start], INFOBOX_ITEMS_COUNT - start, false);
		firstPaint = false;
	}
	else {
		if (fullPaint)
			// paint infobox full
			paintFbItems((comp_fbdata_t*)&fbdata[start], INFOBOX_ITEMS_COUNT - start, false);
		else
			// paint body only
			paintFbItems((comp_fbdata_t*)&fbdata[INFOBOX_ITEMS_COUNT - 1], 1, false);
	}
}

//restore infobox
void CComponentsInfoBox::restore(bool clear_)
{
	if (!v_infobox_val.empty()) {
		for(size_t i =0; i< v_infobox_val.size() ;i++) {
			if (v_infobox_val[i].pixbuf != NULL) {
				frameBuffer->RestoreScreen(v_infobox_val[i].x, v_infobox_val[i].y, v_infobox_val[i].dx, v_infobox_val[i].dy, v_infobox_val[i].pixbuf);
				if (clear_)
					delete[] v_infobox_val[i].pixbuf;
			}
		}
		if (clear_) {
			v_infobox_val.clear();
			firstPaint = true;
		}
	}
}

void CComponentsInfoBox::hide()
{
	//caching current colors
	fb_pixel_t c_tmp1, c_tmp2, c_tmp3;
	c_tmp1 = col_body;
	c_tmp2 = col_shadow;
	c_tmp3 = col_frame;

	//set background color
	col_body = col_frame = col_shadow = COL_BACKGROUND;

	//paint with background and restore, set last used colors
	paint(false, true);
	col_body = c_tmp1;
	col_shadow = c_tmp2;
	col_frame = c_tmp3;
}
