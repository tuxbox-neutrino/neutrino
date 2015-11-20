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

//direction types
enum
{
	CC_DIR_X 	= 0x1,
	CC_DIR_Y 	= 0x2
};

class CComponentsFrmChain : public CComponentsForm
{
	private:
		///init all required variables
		void initVarChain(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const std::vector<CComponentsItem*> *v_items,
					int direction,
					CComponentsForm* parent,
					bool has_shadow,
					fb_pixel_t& color_frame,
					fb_pixel_t& color_body,
					fb_pixel_t& color_shadow);
	protected:
		///property: mode for arrangement direction of items, see also setDirection(), getDirection()
		int chn_direction;

		void initChainItems();

	public:
		CComponentsFrmChain(	const int& x_pos = 1, const int& y_pos = 1, const int& w = 720, const int& h = 32,
					const std::vector<CComponentsItem*> *v_items = NULL,
					int direction = CC_DIR_X,
					CComponentsForm* parent = NULL,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t& color_frame = COL_MENUCONTENT_PLUS_6,
					fb_pixel_t& color_body = COL_MENUHEAD_PLUS_0,
					fb_pixel_t& color_shadow = COL_MENUCONTENTDARK_PLUS_0);
// 		~CComponentsFrmChain(); //inherited from CComponentsForm

		///defines mode for arrangement direction of items, see also chn_direction
		virtual void setDirection(int direction);
		///gets the mode of arrangment direction
		virtual int getDirection(){return chn_direction;};
};

#endif
