/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012, 2013, Thilo Graf 'dbt'

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

#ifndef __CC_ITEM_SHAPES_H__
#define __CC_ITEM_SHAPES_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "cc.h"

//! Sub class of CComponentsItem. Shows a shape with given dimensions and color on screen.
/*!
Paint of simple shapes on screen.
*/

class CComponentsShapeCircle : public CComponentsItem
{
	private:
		///property: diam
		int d;
	public:
		CComponentsShapeCircle(	const int x_pos, const int y_pos, const int diam, bool has_shadow = CC_SHADOW_ON,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);

		///set property: diam
		inline void setDiam(const int& diam){d=width=height=diam, corner_rad=d/2;};
		///get property: diam
		inline int getDiam() const {return d;};

		///paint item
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
};

class CComponentsShapeSquare : public CComponentsItem
{
	public:
		CComponentsShapeSquare(	const int x_pos, const int y_pos, const int w, const int h, bool has_shadow = CC_SHADOW_ON,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);

		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
};

#endif
