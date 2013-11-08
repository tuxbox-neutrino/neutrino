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
CComponentsShapeSquare::CComponentsShapeSquare(const int x_pos, const int y_pos, const int w, const int h, bool has_shadow, fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	//CComponentsItem
	initVarItem();
	cc_item_type 	= CC_ITEMTYPE_SHAPE_SQUARE;

	x 		= x_pos;
	y 		= y_pos;
	width 		= w;
	height	 	= h;
	shadow		= has_shadow;
	shadow_w	= SHADOW_OFFSET;
	col_frame 	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;
}

void CComponentsShapeSquare::paint(bool do_save_bg)
{
	paintInit(do_save_bg);
}


//-------------------------------------------------------------------------------------------------------
//sub class CComponentsShapeCircle from CComponentsItem
CComponentsShapeCircle::CComponentsShapeCircle(	int x_pos, int y_pos, int diam, bool has_shadow,
					fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	//CComponents, CComponentsItem
	initVarItem();
	cc_item_type 	= CC_ITEMTYPE_SHAPE_CIRCLE;

	//CComponents
	x 		= x_pos;
	y 		= y_pos;
	//width = height	= d = diam;
	shadow		= has_shadow;
	shadow_w	= SHADOW_OFFSET;
	col_frame 	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;

	//CComponentsShapeCircle
	width = height	= d = diam;

	//CComponentsItem
	corner_rad	= d/2;
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

void CComponentsShapeCircle::paint(bool do_save_bg)
{
	paintInit(do_save_bg);
}
