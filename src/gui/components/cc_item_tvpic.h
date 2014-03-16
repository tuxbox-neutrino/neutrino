/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012-2014, Thilo Graf 'dbt'

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

#ifndef __CC_ITEM_TVPIC_H__
#define __CC_ITEM_TVPIC_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "cc_base.h"
#include <string>

//! Sub class of CComponentsItem. Shows a mini tv box, similar to a PIP-Feature with current tv-channel.
/*!
Usable like all other CCItems and provides a simple display of a mini tv box on screen.
A real PIP-Feature is only provided with correct hardware support and in soon in newer versions.
*/

class CComponentsPIP : public CComponentsItem
{
	private:
		///property: width of tv box
		int screen_w;
		///property: height of tv box
		int screen_h;
		///property: path to image that displayed, if no tv-mode is active, default: start.jpg
		std::string pic_name;
	public:
		///constructor: initialize of position like all other items with x and y values, but dimensions in percent
		CComponentsPIP(	const int x_pos = 0, const int y_pos = 0, const int percent = 30,
				CComponentsForm *parent = NULL,
				bool has_shadow = CC_SHADOW_OFF,
				fb_pixel_t color_frame = COL_BLACK, fb_pixel_t color_body = COL_BACKGROUND, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
		~CComponentsPIP();

		///set property: width of tv box in pixel
		void setScreenWidth(int screen_width){screen_w = screen_width;};
		///set property: height of tv box in pixel
		void setScreenHeight(int screen_heigth){screen_h = screen_heigth;};
		///property: path to image that displayed, if no tv-mode is active
		void setPicture(const std::string& image){pic_name = image;};

		///show tv box
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
		///remove tv box from screen
		void hide(bool no_restore = false);
};

#endif
