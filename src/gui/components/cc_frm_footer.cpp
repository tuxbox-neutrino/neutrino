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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include "cc_frm_footer.h"

using namespace std;

//-------------------------------------------------------------------------------------------------------
//sub class CComponentsFooter inherit from CComponentsHeader
CComponentsFooter::CComponentsFooter(CComponentsForm* parent)
{
	//CComponentsFooter
	initVarFooter(1, 1, 0, 0, 0, parent);
}

CComponentsFooter::CComponentsFooter(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const int& buttons,
					CComponentsForm* parent,
					bool has_shadow,
					fb_pixel_t color_frame,
					fb_pixel_t color_body,
					fb_pixel_t color_shadow )
{
	//CComponentsFooter
	initVarFooter(x_pos, y_pos, w, h, buttons, parent, has_shadow, color_frame, color_body, color_shadow);
}

void CComponentsFooter::initVarFooter(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const int& buttons,
					CComponentsForm* parent,
					bool has_shadow,
					fb_pixel_t color_frame,
					fb_pixel_t color_body,
					fb_pixel_t color_shadow )
{
	cc_item_type 	= CC_ITEMTYPE_FOOTER;

	x		= x_pos;
	y		= y_pos;
	
	//init footer width
	width 	= w == 0 ? frameBuffer->getScreenWidth(true) : w;
		//init header height
	cch_font 	= g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE];
	if (h > 0)
		height 	= h;
	else
		height 	= cch_font->getHeight();
	
	cch_buttons	= buttons;
	shadow		= has_shadow;
	col_frame	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;
	
	corner_rad	= RADIUS_LARGE;
	corner_type	= CORNER_BOTTOM;

	initDefaultButtons();
	initCCItems();
	initParent(parent);
}
