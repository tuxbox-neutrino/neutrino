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
#include "cc_frm.h"

using namespace std;

//-------------------------------------------------------------------------------------------------------
//sub class CComponentsFooter inherit from CComponentsHeader
CComponentsFooter::CComponentsFooter()
{
	//CComponentsFooter
	initVarFooter();
}

CComponentsFooter::CComponentsFooter(	const int x_pos, const int y_pos, const int w, const int h, const int buttons, bool has_shadow,
					fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow )
{
	//CComponentsFooter
	initVarFooter();

	x 		= x_pos;
	y 		= y_pos;
	width 		= w;
	height 		= h;
	shadow		= has_shadow;
	col_frame	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;
	
	cch_buttons	= buttons;

	initDefaultButtons();
	initCCItems();
}


void CComponentsFooter::initVarFooter()
{
	//CComponentsHeader
	initVarHeader();
	
	cc_item_type 	= CC_ITEMTYPE_FOOTER;
	corner_rad	= RADIUS_LARGE;
	corner_type	= CORNER_BOTTOM;
}
