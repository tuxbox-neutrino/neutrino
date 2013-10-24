/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012, 2013, Thilo Graf 'dbt'
	Copyright (C) 2012, Michael Liebmann 'micha-bbg'

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
#include "cc_detailsline.h"

using namespace std;

//sub class CComponentsDetailLine from CComponents
CComponentsDetailLine::CComponentsDetailLine()
{
	initVarDline();

	//CComponents
	x 		= 0;
	y 		= 0;
	col_shadow	= COL_MENUCONTENTDARK_PLUS_0;
	col_body	= COL_MENUCONTENT_PLUS_6;

	//CComponentsDetailLine
	y_down 		= 0;
	h_mark_top 	= CC_HEIGHT_MIN;
	h_mark_down 	= CC_HEIGHT_MIN;
}

CComponentsDetailLine::CComponentsDetailLine(const int x_pos, const int y_pos_top, const int y_pos_down, const int h_mark_top_, const int h_mark_down_, fb_pixel_t color_line, fb_pixel_t color_shadow)
{
	initVarDline();

	//CComponents
	x 		= x_pos;
	y 		= y_pos_top;
	col_shadow	= color_shadow;
	col_body	= color_line;

	//CComponentsDetailLine
	y_down 		= y_pos_down;
	h_mark_top 	= h_mark_top_;
	h_mark_down 	= h_mark_down_;
}

void CComponentsDetailLine::initVarDline()
{
	//CComponents
	initVarBasic();

	shadow_w	= 1;

	//CComponentsDetailLine
	thickness 	= 4;
}

CComponentsDetailLine::~CComponentsDetailLine()
{
	hideCC(); //restore background
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
	clear();

	int y_mark_top = y-h_mark_top/2+thickness/2;
	int y_mark_down = y_down-h_mark_down/2+thickness/2;

	int sw = shadow_w;

	comp_fbdata_t fbdata[] =
	{
		/* vertical item mark | */
		{CC_FBDATA_TYPE_LINE, x+width-thickness-sw, 	y_mark_top, 		thickness, 		h_mark_top, 		col_body, 	0, 0, NULL, NULL},
		{CC_FBDATA_TYPE_LINE, x+width-sw,		y_mark_top+sw, 		sw, 			h_mark_top-sw, 		col_shadow, 	0, 0, NULL, NULL},
		{CC_FBDATA_TYPE_LINE, x+width-thickness,	y_mark_top+h_mark_top, 	thickness, 		sw,	 		col_shadow, 	0, 0, NULL, NULL},

		/* horizontal item line - */
		{CC_FBDATA_TYPE_LINE, x, 			y,			width-thickness-sw,	thickness, 		col_body, 	0, 0, NULL, NULL},
		{CC_FBDATA_TYPE_LINE, x+thickness,		y+thickness,		width-2*thickness-sw,	sw, 			col_shadow, 	0, 0, NULL, NULL},

		/* vertical connect line [ */
		{CC_FBDATA_TYPE_LINE, x,			y+thickness, 		thickness, 		y_down-y-thickness, 	col_body, 	0, 0, NULL, NULL},
		{CC_FBDATA_TYPE_LINE, x+thickness,		y+thickness+sw,		sw, 			y_down-y-thickness-sw,	col_shadow, 	0, 0, NULL, NULL},

		/* horizontal info line - */
		{CC_FBDATA_TYPE_LINE, x,			y_down, 		width-thickness-sw, 	thickness, 		col_body, 	0, 0, NULL, NULL},
		{CC_FBDATA_TYPE_LINE, x+sw,			y_down+thickness, 	width-thickness-2*sw,	sw, 			col_shadow, 	0, 0, NULL, NULL},

		/* vertical info mark | */
		{CC_FBDATA_TYPE_LINE, x+width-thickness-sw,	y_mark_down, 		thickness, 		h_mark_down, 		col_body, 	0, 0, NULL, NULL},
		{CC_FBDATA_TYPE_LINE, x+width-sw,		y_mark_down+sw,		sw, 			h_mark_down-sw,		col_shadow, 	0, 0, NULL, NULL},
		{CC_FBDATA_TYPE_LINE, x+width-thickness,	y_mark_down+h_mark_down,thickness, 		sw,	 		col_shadow, 	0, 0, NULL, NULL},
	};

	for(size_t i =0; i< (sizeof(fbdata) / sizeof(fbdata[0])) ;i++)
		v_fbdata.push_back(fbdata[i]);

	paintFbItems(do_save_bg);
}

//synchronize colors for details line
//This is usefull if the system colors are changed during runtime
//so you can ensure correct applied system colors in relevant objects with unchanged instances.
void CComponentsDetailLine::syncSysColors()
{
	col_body 	= COL_MENUCONTENT_PLUS_6;
	col_shadow 	= COL_MENUCONTENTDARK_PLUS_0;
}
