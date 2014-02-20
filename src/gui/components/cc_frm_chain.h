/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2013, Thilo Graf 'dbt'

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

#ifndef __CC_FORM_CHAIN_H__
#define __CC_FORM_CHAIN_H__


#include <config.h>
#include "cc_frm.h"


//! Sub class of CComponentsForm. Creates a dynamic form with chained items.
/*!
Paint chained cc-items on screen.
You can set default form parameters like  position, size, colors etc. and additional values
to display with defined direction.
*/

class CComponentsFrmChain : public CComponentsForm
{
	private:
		///property: defined arrangement mode of items, can be vertical or horizontal
		int chn_horizontal;
		
		///property: defines height from sum of all contained items
		bool chn_dyn_height;
		///property: defines width from sum of all contained items
		bool chn_dyn_width;
		
		///init all required variables
		void initVarChain(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const std::vector<CComponentsItem*> *v_items,
					bool horizontal,
					bool dynamic_width,
					bool dynamic_height,
					bool has_shadow,
					fb_pixel_t& color_frame,
					fb_pixel_t& color_body,
					fb_pixel_t& color_shadow);
		
		void initCChainItems();
	protected:


	public:
		CComponentsFrmChain(	const int& x_pos = 1, const int& y_pos = 1, const int& w = 720, const int& h = 32,
					const std::vector<CComponentsItem*> *v_items = NULL,
					bool horizontal = true,
					bool dynamic_width = false,
					bool dynamic_height = false,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t& color_frame = COL_MENUCONTENT_PLUS_6,
					fb_pixel_t& color_body = COL_MENUHEAD_PLUS_0,
					fb_pixel_t& color_shadow = COL_MENUCONTENTDARK_PLUS_0);
// 		~CComponentsSlider(); //inherited from CComponentsForm
};

#endif
