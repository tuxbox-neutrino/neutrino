/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic for GUI-related components.
	Copyright (C) 2012, 2013, Thilo Graf 'dbt'
	Copyright (C) 2012, Michael Liebmann 'micha-bbg'

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


using namespace std;

//abstract basic class CComponents
CComponents::CComponents()
{
	//basic CComponents
	x = 0;
	y = 0;
	height 		= CC_HEIGHT_MIN;
	width 		= CC_WIDTH_MIN;
	col_body 	= COL_MENUCONTENT;
	col_shadow 	= COL_MENUCONTENTDARK_PLUS_0;
	col_frame 	= COL_MENUCONTENT_PLUS_6;
	corner_type 	= CORNER_ALL;
	shadow		= CC_SHADOW_OFF;
	shadow_w	= SHADOW_OFFSET;
	firstPaint	= true;
	frameBuffer = CFrameBuffer::getInstance();
	v_fbdata.clear();
	saved_screen	= NULL;
}

CComponents::~CComponents()
{
	clear();
}

//paint framebuffer stuff and fill buffer
void CComponents::paintFbItems(struct comp_fbdata_t * fbdata, const int items_count, bool do_save_bg)
{
	for(int i=0; i< items_count ;i++){
		int fbtype = fbdata[i].fbdata_type;
		
		if (firstPaint){
			
			if (do_save_bg && fbtype == CC_FBDATA_TYPE_LINE)
				fbdata[i].pixbuf = saved_screen = getScreen(fbdata[i].x, fbdata[i].y, fbdata[i].dx, fbdata[i].dy);
			v_fbdata.push_back(fbdata[i]);
			
			//ensure painting of all line fb items with saved screens
			if (fbtype == CC_FBDATA_TYPE_LINE)
				firstPaint = true;
			else
				firstPaint = false;
		}
		if (fbtype != CC_FBDATA_TYPE_BGSCREEN){
			if (fbtype == CC_FBDATA_TYPE_FRAME && fbdata[i].frame_thickness > 0)
				frameBuffer->paintBoxFrame(fbdata[i].x, fbdata[i].y, fbdata[i].dx, fbdata[i].dy, fbdata[i].frame_thickness, fbdata[i].color, fbdata[i].r);
			else
				frameBuffer->paintBoxRel(fbdata[i].x, fbdata[i].y, fbdata[i].dx, fbdata[i].dy, fbdata[i].color, fbdata[i].r, corner_type);
		}
	}
}

//screen area save
inline fb_pixel_t* CComponents::getScreen(int ax, int ay, int dx, int dy)
{
	fb_pixel_t* pixbuf = new fb_pixel_t[dx * dy];
	frameBuffer->SaveScreen(ax, ay, dx, dy, pixbuf);
	return pixbuf;
}

//restore screen from buffer
inline void CComponents::hide()
{
	for(size_t i =0; i< v_fbdata.size() ;i++) {
		if (v_fbdata[i].pixbuf != NULL){
			frameBuffer->RestoreScreen(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy, v_fbdata[i].pixbuf);
			delete[] v_fbdata[i].pixbuf;
			v_fbdata[i].pixbuf = NULL;
		}
	}
	v_fbdata.clear();
}

//clean old screen buffer
inline void CComponents::clear()
{
	for(size_t i =0; i< v_fbdata.size() ;i++)
		if (v_fbdata[i].pixbuf != NULL)
			delete[] v_fbdata[i].pixbuf;
	v_fbdata.clear();
}


//-------------------------------------------------------------------------------------------------------
//abstract sub class CComponentsContainer from CComponents
CComponentsContainer::CComponentsContainer()
{
	//CComponentsContainer
	corner_rad	= 0;
	fr_thickness	= 2;
}

// 	 y
// 	x+------f-r-a-m-e-------+
// 	 |			|
//     height	  body		|
// 	 |			|
// 	 +--------width---------+


void CComponentsContainer::paint(bool do_save_bg)
{
	int items_cnt = 0;
 	clear();
	
	int sw = shadow ? shadow_w : 0;
	int th = fr_thickness;
	
	comp_fbdata_t fbdata[] =
	{
		{CC_FBDATA_TYPE_BGSCREEN, 	x,	y, 	width+sw, 	height+sw, 	0, 		0, 		0, 	NULL,	NULL},
		{CC_FBDATA_TYPE_SHADOW, 	x+sw,	y+sw, 	width, 		height, 	col_shadow, 	corner_rad, 	0, 	NULL,	NULL},
		{CC_FBDATA_TYPE_FRAME, 		x, 	y, 	width, 		height, 	col_frame, 	corner_rad, 	th, 	NULL,	NULL},
		{CC_FBDATA_TYPE_BOX, 		x+th,	y+th, 	width-2*th, 	height-2*th, 	col_body, 	corner_rad-th,  0, 	NULL,	NULL},
	};
	
	items_cnt = sizeof(fbdata) / sizeof(fbdata[0]);
	
	if (firstPaint && do_save_bg)	{
		for(int i=0; i<items_cnt; i++){
			if (fbdata[i].fbdata_type == CC_FBDATA_TYPE_BGSCREEN){
				fbdata[i].pixbuf = getScreen(fbdata[i].x, fbdata[i].y, fbdata[i].dx, fbdata[i].dy);
				break;
			}
		}
	}
	
	paintFbItems(fbdata, items_cnt, do_save_bg);
}

//restore last saved screen behind form box,
//Do use parameter 'no restore' to override temporarly the restore funtionality.
//This could help to avoid ugly flicker efffects if it is necessary e.g. on often repaints, without changed contents.
void CComponentsContainer::hide(bool no_restore)
{
	if (no_restore)
		return;
	
	for(size_t i =0; i< v_fbdata.size() ;i++) {
		if (v_fbdata[i].pixbuf != NULL && v_fbdata[i].fbdata_type == CC_FBDATA_TYPE_BGSCREEN)
			frameBuffer->RestoreScreen(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy, v_fbdata[i].pixbuf);
		delete[] v_fbdata[i].pixbuf;
	}
	v_fbdata.clear();
	firstPaint = true;
}

//hide rendered objects
void CComponentsContainer::paintBackground()
{
	//save current colors
	fb_pixel_t c_tmp1, c_tmp2, c_tmp3;
	c_tmp1 = col_body;
	c_tmp2 = col_shadow;
	c_tmp3 = col_frame;

	//set background color
	col_body = col_frame = col_shadow = COL_BACKGROUND;

	//paint with background and restore last used colors
	paint(CC_SAVE_SCREEN_NO);
	col_body = c_tmp1;
	col_shadow = c_tmp2;
	col_frame = c_tmp3;
	firstPaint = true;
}

//-------------------------------------------------------------------------------------------------------
//sub class CComponentsInfoBox from CComponentsContainer
CComponentsInfoBox::CComponentsInfoBox(const int x_pos, const int y_pos, const int w, const int h, bool has_shadow, fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	//CComponents
	x 		= x_pos;
	y 		= y_pos;
	width 		= w;
	height	 	= h;
	shadow		= has_shadow;
	shadow_w	= SHADOW_OFFSET;
	col_frame 	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;
	firstPaint	= true;
	v_fbdata.clear();
	
	//CComponentsContainer
	corner_rad	= RADIUS_LARGE;
	fr_thickness	= 2;
}


//-------------------------------------------------------------------------------------------------------
//sub class CComponentsDetailLine from CComponents
CComponentsDetailLine::CComponentsDetailLine(const int x_pos, const int y_pos_top, const int y_pos_down, const int h_mark_top_, const int h_mark_down_, fb_pixel_t color_line, fb_pixel_t color_shadow)
{
	//CComponents
	x 		= x_pos;
	y 		= y_pos_top;
	width 		= CC_WIDTH_MIN;
	col_shadow	= color_shadow;
	col_body	= color_line;
// 	col_frame	= COL_BACKGROUND; // not used in this class
//	shadow		= CC_SHADOW_OFF; // not used in this class
	shadow_w	= 1;
	firstPaint	= true;
	v_fbdata.clear();

	//CComponentsDetailLine
	y_down 		= y_pos_down;
	h_mark_top 	= h_mark_top_;
	h_mark_down 	= h_mark_down_;
	thickness 	= 4;
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


//paint details line with current parameters
void CComponentsDetailLine::paint(bool do_save_bg)
{
	int items_cnt = 0;
	
	clear();
	
	int y_mark_top = y-h_mark_top/2+thickness/2;
	int y_mark_down = y_down-h_mark_down/2+thickness/2;

	int sw = shadow_w;
	
	comp_fbdata_t fbdata[] =
	{
		/* vertical item mark | */
		{CC_FBDATA_TYPE_LINE, x+width-thickness-sw, 	y_mark_top, 		thickness, 		h_mark_top, 		col_body, 	0, 0, NULL, NULL},
		{CC_FBDATA_TYPE_LINE, x+width-sw,		y_mark_top, 		sw, 			h_mark_top, 		col_shadow, 	0, 0, NULL, NULL},
		{CC_FBDATA_TYPE_LINE, x+width-thickness-sw,	y_mark_top+h_mark_top, 	thickness+sw, 		sw	, 		col_shadow, 	0, 0, NULL, NULL},
		
		/* horizontal item line - */
		{CC_FBDATA_TYPE_LINE, x, 			y,			width-thickness-sw,	thickness, 		col_body, 	0, 0, NULL, NULL},
		{CC_FBDATA_TYPE_LINE, x+thickness,		y+thickness,		width-2*thickness-sw,	sw, 			col_shadow, 	0, 0, NULL, NULL},
		
		/* vertical connect line [ */
		{CC_FBDATA_TYPE_LINE, x,			y+thickness, 		thickness, 		y_down-y-thickness, 	col_body, 	0, 0, NULL, NULL},
		{CC_FBDATA_TYPE_LINE, x+thickness,		y+thickness+sw,		sw, 			y_down-y-thickness-sw,	col_shadow, 	0, 0, NULL, NULL},
		
		/* horizontal info line - */
		{CC_FBDATA_TYPE_LINE, x,			y_down, 		width-thickness-sw, 	thickness, 		col_body, 	0, 0, NULL, NULL},
		{CC_FBDATA_TYPE_LINE, x,			y_down+thickness, 	width-thickness-sw,	sw, 			col_shadow, 	0, 0, NULL, NULL},
		
		/* vertical info mark | */
		{CC_FBDATA_TYPE_LINE, x+width-thickness-sw,	y_mark_down, 		thickness, 		h_mark_down, 		col_body, 	0, 0, NULL, NULL},
		{CC_FBDATA_TYPE_LINE, x+width-sw,		y_mark_down, 		sw, 			h_mark_down, 		col_shadow, 	0, 0, NULL, NULL},
		{CC_FBDATA_TYPE_LINE, x+width-thickness-sw,	y_mark_down+h_mark_down,thickness+sw, 		sw,	 		col_shadow, 	0, 0, NULL, NULL},
	};
	
	items_cnt = sizeof(fbdata) / sizeof(fbdata[0]);
	
	paintFbItems(fbdata, items_cnt, do_save_bg);
}

//remove painted fb items from screen
void CComponentsDetailLine::paintBackground()
{
	//save current colors
	fb_pixel_t c_tmp1, c_tmp2;
	c_tmp1 = col_body;
	c_tmp2 = col_shadow;

	//set background color
	col_body = col_shadow = COL_BACKGROUND;

	//paint with background and restore, set last used colors
	paint(CC_SAVE_SCREEN_NO);
	col_body = c_tmp1;
	col_shadow = c_tmp2;
	firstPaint = true;
}



