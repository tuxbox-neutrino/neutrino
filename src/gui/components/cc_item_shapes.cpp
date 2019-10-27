/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012-2014, Thilo Graf 'dbt'
	Copyright (C) 2012, Michael Liebmann 'micha-bbg'

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Library General Public License for more details.

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
#include "cc_item_shapes.h"

using namespace std;

//sub class CComponentsShapeSquare from CComponentsItem
CComponentsShapeSquare::CComponentsShapeSquare(	const int x_pos, const int y_pos, const int w, const int h,
						CComponentsForm *parent,
						int shadow_mode,
						fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	//CComponentsItem
	cc_item_type.id 	= CC_ITEMTYPE_SHAPE_SQUARE;
	cc_item_type.name	="cc_box";

	x 		= x_old = x_pos;
	y 		= y_old = y_pos;
	width 		= width_old = w;
	height	 	= height_old = h;
	shadow		= shadow_mode;
	shadow_w	= OFFSET_SHADOW;
	col_frame 	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;
	initParent(parent);
}

void CComponentsShapeSquare::paint(const bool &do_save_bg)
{
	paintInit(do_save_bg);
}


//-------------------------------------------------------------------------------------------------------
//sub class CComponentsShapeCircle from CComponentsItem
CComponentsShapeCircle::CComponentsShapeCircle(	int x_pos, int y_pos, int diam,
						CComponentsForm *parent,
						int shadow_mode,
						fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	//CComponents, CComponentsItem
	cc_item_type.id 	= CC_ITEMTYPE_SHAPE_CIRCLE;
	cc_item_type.name	="cc_circle";

	//CComponents
	x 		= x_pos;
	y 		= y_pos;

	corner_type = corner_type_old		= CORNER_ALL;

	//width = height	= d = diam;
	shadow		= shadow_mode;
	shadow_w	= OFFSET_SHADOW;
	col_frame 	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;

	//CComponentsShapeCircle
	width = height	= d = diam;

	//CComponentsItem
	corner_rad	= d/2;
	initParent(parent);
}

// 	 y
// 	x+	 -	 +
//
//
//
// 	 |----d-i-a-m----|
//
//
//
// 	 +	 -	 +

void CComponentsShapeCircle::paint(const bool &do_save_bg)
{
	paintInit(do_save_bg);
}

bool PaintBoxRel(const int& x, const int& y, const int& dx, const int& dy, const fb_pixel_t& col, int radius, int corner_type, int shadow_mode)
{
	CComponentsShapeSquare box(x, y, dx, dy, NULL,  shadow_mode, COL_SHADOW_PLUS_0, col);
	box.setCorner(radius, corner_type);
	box.paint(CC_SAVE_SCREEN_NO);
	return box.isPainted();
}

void ClearBoxRel(const int& x, const int& y, const int& dx, const int& dy, int shadow_mode)
{
	int w = dx + (shadow_mode ? OFFSET_SHADOW : 0);
	int h = dy + (shadow_mode ? OFFSET_SHADOW : 0);
	CFrameBuffer::getInstance()->paintBackgroundBoxRel(x, y, w, h);
}
