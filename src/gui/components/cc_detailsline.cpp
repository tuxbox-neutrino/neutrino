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
#include "cc_draw.h"

using namespace std;

//sub class CComponentsDetailsLine from CComponents
CComponentsDetailsLine::CComponentsDetailsLine(	const int& x_pos, const int& y_pos_top,
						const int& y_pos_down, const int& h_mark_top_, const int& h_mark_down_,
						fb_pixel_t color_line, fb_pixel_t color_shadow)
{
	initVarDline(x_pos, y_pos_top, y_pos_down, h_mark_top_, h_mark_down_, color_line, color_shadow);
}

void CComponentsDetailsLine::initVarDline(	const int& x_pos, const int& y_pos_top,
						const int& y_pos_down, const int& h_mark_top_, const int& h_mark_down_,
						fb_pixel_t color_line, fb_pixel_t color_shadow)
{
	//CComponents
	x 		= x_pos;
	y 		= y_pos_top;
	col_shadow	= color_shadow;
	col_body	= color_line;

	//CComponentsDetailsLine
	y_down 		= y_pos_down;
	h_mark_top 	= h_mark_top_;
	h_mark_down 	= h_mark_down_;

	shadow_w	= 1;

	//CComponentsDetailsLine
	dl_w 	= CFrameBuffer::getInstance()->scale2Res(3);

	cc_body_gradient_enable = false;
}

CComponentsDetailsLine::~CComponentsDetailsLine()
{
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
void CComponentsDetailsLine::paint(bool do_save_bg)
{
	hide();
	if (hasChanges())
		clearFbData();

	if (v_fbdata.empty()){

		int sw = shadow_w;

		// reduce two times the shadow width, to avoid shadow overlaps
		h_mark_down -= 2*sw;

		int y_mark_top = y-h_mark_top/2;
		int y_mark_down = y_down-h_mark_down/2;
		int dx_c = dl_w%2; //correction for odd values

		cc_fbdata_t fbdata[] =
		{
			/*buffered bg full width and height */
			{true, CC_FBDATA_TYPE_BGSCREEN,	x,			y_mark_top, 		width,			y_mark_down-y_mark_top+h_mark_down+sw,	0, 0, 0, 0, NULL, NULL, NULL, false},

			/* vertical item mark | */
			{true, CC_FBDATA_TYPE_BOX, 	x+width-dl_w-sw, 	y_mark_top, 		dl_w, 			h_mark_top, 		col_body, 	0, 0, 0, NULL, NULL, NULL, false},
			{true, CC_FBDATA_TYPE_BOX, 	x+width-sw,		y_mark_top+sw, 		sw, 			h_mark_top-sw, 		col_shadow, 	0, 0, 0, NULL, NULL, NULL, false},
			{true, CC_FBDATA_TYPE_BOX, 	x+width-dl_w,		y_mark_top+h_mark_top, 	dl_w, 			sw,	 		col_shadow, 	0, 0, 0, NULL, NULL, NULL, false},

			/* horizontal item line - */
			{true, CC_FBDATA_TYPE_BOX, 	x, 			y-dl_w/2,		width-dl_w-sw,		dl_w, 			col_body, 	0, 0, 0, NULL, NULL, NULL, false},
			{true, CC_FBDATA_TYPE_BOX, 	x+dl_w,			y+dl_w/2+dx_c,		width-2*dl_w-sw,	sw, 			col_shadow, 	0, 0, 0, NULL, NULL, NULL, false},

			/* vertical connect line [ */
			{true, CC_FBDATA_TYPE_BOX, 	x,			y+dl_w/2+dx_c, 		dl_w, 			y_down-y-dl_w, 		col_body, 	0, 0, 0, NULL, NULL, NULL, false},
			{true, CC_FBDATA_TYPE_BOX, 	x+dl_w,			y+dl_w/2+dx_c,		sw, 			y_down-y-dl_w,		col_shadow, 	0, 0, 0, NULL, NULL, NULL, false},

			/* horizontal info line - */
			{true, CC_FBDATA_TYPE_BOX, 	x,			y_down-dl_w/2,		width-dl_w-sw, 		dl_w, 			col_body, 	0, 0, 0, NULL, NULL, NULL, false},
			{true, CC_FBDATA_TYPE_BOX, 	x+sw,			y_down+dl_w/2+dx_c,	width-dl_w-2*sw,	sw, 			col_shadow, 	0, 0, 0, NULL, NULL, NULL, false},

			/* vertical info mark | */
			{true, CC_FBDATA_TYPE_BOX, 	x+width-dl_w-sw,	y_mark_down, 		dl_w, 			h_mark_down, 		col_body, 	0, 0, 0, NULL, NULL, NULL, false},
			{true, CC_FBDATA_TYPE_BOX, 	x+width-sw,		y_mark_down+sw,		sw, 			h_mark_down-sw,		col_shadow, 	0, 0, 0, NULL, NULL, NULL, false},
			{true, CC_FBDATA_TYPE_BOX, 	x+width-dl_w,		y_mark_down+h_mark_down,dl_w, 			sw,	 		col_shadow, 	0, 0, 0, NULL, NULL, NULL, false},
		};

		for(size_t i =0; i< (sizeof(fbdata) / sizeof(fbdata[0])) ;i++)
			v_fbdata.push_back(fbdata[i]);
	}
	paintFbItems(do_save_bg);
}

//synchronize colors for details line
//This is usefull if the system colors are changed during runtime
//so you can ensure correct applied system colors in relevant objects with unchanged instances.
void CComponentsDetailsLine::syncSysColors()
{
	col_body 	= COL_FRAME_PLUS_0;
	col_shadow 	= COL_SHADOW_PLUS_0;
}
