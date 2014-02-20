/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012, 2013, 2014, Thilo Graf 'dbt'

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

#ifndef __CC_FORM_FOOTER_H__
#define __CC_FORM_FOOTER_H__


#include "cc_frm_header.h"

/*!
CComponentsFooter, sub class of CComponentsHeader provides prepared container for footer
Is mostly usable like a header but without caption, and context button icons.
*/
class CComponentsFooter : public CComponentsHeader
{
	protected:
		void initVarFooter(	const int& x_pos, const int& y_pos, const int& w, const int& h = 0,
					const int& buttons = 0,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6,
					fb_pixel_t color_body = COL_INFOBAR_SHADOW_PLUS_1,
					fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
	public:
		CComponentsFooter();
		CComponentsFooter(	const int& x_pos, const int& y_pos, const int& w, const int& h = 0,
					const int& buttons = 0,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6,
					fb_pixel_t color_body = COL_INFOBAR_SHADOW_PLUS_1,
					fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
};

#endif
