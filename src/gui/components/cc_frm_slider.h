/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2013-2014, Thilo Graf 'dbt'

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __CC_FORM_SLIDER_H__
#define __CC_FORM_SLIDER_H__


#include <config.h>
#include "cc_frm.h"


//! Sub class of CComponentsForm. Shows a slider.
/*!
Paint a simple slider on screen.
You can set default form parameters like  position, size, colors etc. and additional values
to display current values with a slider icon.
*/

class CComponentsSlider : public CComponentsForm
{
	private:
		///names of slider icons
		std::string csl_body_icon, csl_slider_icon;

		///property: current value that should be displayed by slider button, see also setValuePos()
		int csl_current_value;
		
		///property: minimal scale value, see also setValueScale()
		int csl_min_value;
		///property: maximal scale value, see also setValueScale()
		int csl_max_value;

		///object: image objects for slider button and body
		CComponentsPicture *csl_body_obj, *csl_slider_obj;

		///init body image object
		void initCCSlBody();
		///init slider image object
		void initCCSlSlider();
		///init all items at once
		void initCCSlItems();

	public:
		CComponentsSlider(	const int& x_pos = 0, const int& y_pos = 0, const int& w = 120+16, const int& h = 32,
					const int& current_value = 0,
					const int& min_value = 0,
					const int& max_value = 100,
					CComponentsForm *parent = NULL,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t& color_frame = COL_FRAME_PLUS_0,
					fb_pixel_t& color_body = COL_MENUHEAD_PLUS_0,
					fb_pixel_t& color_shadow = COL_SHADOW_PLUS_0);
// 		~CComponentsSlider(); //inherited from CComponentsForm
		
		void setValuePos(const int& current_value);
		void setValueScale(const int& min_value, const int& max_value);
		void setSliderIcon(const std::string &icon_name);

// 		void paint(const bool &do_save_bg = CC_SAVE_SCREEN_YES);
};

#endif
