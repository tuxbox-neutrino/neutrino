/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012-2014 Thilo Graf 'dbt'

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

#include "cc_base.h"
#include "cc_item.h"

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
		CComponentsShapeCircle(	const int x_pos, const int y_pos, const int diam,
					CComponentsForm *parent = NULL,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_FRAME_PLUS_0,
					fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0,
					fb_pixel_t color_shadow = COL_SHADOW_PLUS_0);

		///set property: diam
		inline void setDiam(const int& diam){d=width=height=diam, corner_rad=d/2;};
		///get property: diam
		inline int getDiam() const {return d;};

		///paint item
		void paint(const bool &do_save_bg = CC_SAVE_SCREEN_YES);
};

class CComponentsShapeSquare : public CComponentsItem
{
	public:
		CComponentsShapeSquare(	const int x_pos, const int y_pos, const int w, const int h,
					CComponentsForm *parent = NULL,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_FRAME_PLUS_0,
					fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0,
					fb_pixel_t color_shadow = COL_SHADOW_PLUS_0);

		void paint(const bool &do_save_bg = CC_SAVE_SCREEN_YES);
};


		/**Small and easy to apply box paint methode without expilcit object declaration
		* @return 			bool, true = painted
		*
		* @param[in] x			expects type int, x position on screen
		* @param[in] y			expects type int, y position on screen modes are:
		* @param[in] dx			expects type int, width of scrollbar object
		* @param[in] dy			expects type int, height of scrollbar object
		* @param[in] col		expects type fb_pixel_t, as body color
		*
		* usual paraemters:
		* @param[in] radius		optional, expects type int as corner radius, default = 0
		* @param[in] corner_type	optional, expects type int as cornar type, default CORNER_ALL
		* @param[in] shadow_mode	optional, expects type int defined by shadow mode enums, default CC_SHADOW_OFF
		*/
bool PaintBoxRel(const int& x,
		 const int& y,
		 const int& dx,
		 const int& dy,
		 const fb_pixel_t& col,
		 int radius = 0 /*RADIUS_NONE*/,
		 int corner_type = CORNER_ALL,
		 int shadow_mode = CC_SHADOW_OFF);

		/**Small and easy to apply box clear methode
		* @return 			void
		*
		* @param[in] x			expects type int, x position on screen
		* @param[in] y			expects type int, y position on screen modes are:
		* @param[in] dx			expects type int, width of scrollbar object
		* @param[in] dy			expects type int, height of scrollbar object
		*
		* usual paraemters:
		* @param[in] shadow_mode	optional, expects type int defined by shadow mode enums, default CC_SHADOW_OFF
		*/
void ClearBoxRel(const int& x,
		 const int& y,
		 const int& dx,
		 const int& dy,
		 int shadow_mode = CC_SHADOW_OFF);

#endif
