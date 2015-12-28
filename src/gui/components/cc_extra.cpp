/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Extra functions based up GUI-related components.
	Copyright (C) 2015, Thilo Graf 'dbt'

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
#include "cc_extra.h"
#include <system/debug.h>

using namespace std;

bool paintBoxRel(	const int& x,
			const int& y,
			const int& dx,
			const int& dy,
			const fb_pixel_t& color_body,
			const int& radius,
			const int& corner_type,
			const int& gradient_mode,
			const int& gradient_sec_col,
			const int& gradient_direction,
			const int& gradient_intensity,
			const int& w_frame,
			const fb_pixel_t& color_frame,
			int shadow_mode,
			const fb_pixel_t& color_shadow)
{
	CComponentsShapeSquare box(x, y, dx, dy, NULL, shadow_mode, color_frame, color_body, color_shadow);
	box.setColBodyGradient(gradient_mode, gradient_direction, gradient_sec_col, gradient_intensity);
	box.setCorner(radius, corner_type);
	box.enableFrame(w_frame, w_frame);
	box.paint(CC_SAVE_SCREEN_NO);
	return box.isPainted();
}

bool paintBoxRel0(	const int& x,
			const int& y,
			const int& dx,
			const int& dy,
			const fb_pixel_t& color_body,
			const int& radius,
			const int& corner_type,
			const int& w_frame,
			const fb_pixel_t& color_frame,
			int shadow_mode,
			const fb_pixel_t& color_shadow)
{
	return paintBoxRel(x, y, dx, dy, color_body, radius, corner_type, w_frame, CC_COLGRAD_OFF, COL_MENUCONTENT_PLUS_0, CFrameBuffer::gradientVertical, CColorGradient::normal, color_frame, shadow_mode, color_shadow);
}

bool paintTextBoxRel(	const string& text,
			const int& x,
			const int& y,
			const int& dx,
			const int& dy,
			Font *font,
			const int& mode,
			const int& font_style,
			const fb_pixel_t& color_text,
			const fb_pixel_t& color_body,
			const int& radius,
			const int& corner_type,
			const int& gradient_mode,
			const int& gradient_sec_col,
			const int& gradient_direction,
			const int& gradient_intensity,
			const fb_pixel_t& color_frame,
			int shadow_mode,
			const fb_pixel_t& color_shadow)
{
	CComponentsText box(x, y, dx, dy, text, mode, font, font_style, NULL, shadow_mode, color_text, color_frame, color_body, color_shadow);
	box.setColBodyGradient(gradient_mode, gradient_direction, gradient_sec_col, gradient_intensity);
	box.doPaintBg(color_body !=0);
	box.enableTboxSaveScreen(false);
	box.setCorner(radius, corner_type);
	box.paint(CC_SAVE_SCREEN_NO);

	return box.isPainted();
}

bool paintImage(	const std::string& image_name,
			const int& x,
			const int& y,
			const int& dx,
			const int& dy,
			const int& transparent,
			const fb_pixel_t& color_body,
			const int& radius,
			const int& corner_type,
			const fb_pixel_t& color_frame,
			int shadow_mode,
			const fb_pixel_t& color_shadow)
{
	CComponentsPicture box( x, y, dx, dy, image_name, NULL, shadow_mode, color_frame, color_body, color_shadow, transparent);
	box.doPaintBg(color_body !=0);
	box.setCorner(radius, corner_type);
	box.paint(CC_SAVE_SCREEN_NO);

	return box.isPicPainted();
}
