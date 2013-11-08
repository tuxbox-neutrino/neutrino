#ifndef __gui_widget_buttons_h__
#define __gui_widget_buttons_h__

/*
 * $Id: buttons.h,v 1.6 2010/07/12 08:24:55 dbt Exp $
 *
 * (C) 2003 by thegoodguy <thegoodguy@berlios.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
 
#include <driver/fontrenderer.h>
#include <driver/framebuffer.h>
#include <system/localize.h>
#include <gui/color.h>

#include <vector>

typedef struct button_label
{
	const char *      button;
	neutrino_locale_t locale;
} button_label_struct;

int paintButtons(	const int &x, 
			const int &y, 
			const int &footerwidth, 
			const uint &count, 
			const struct button_label * const content,
			const int &maxwidth,
			const int &footerheight = 0,
			std::string tmp = "", /* just to make sure compilation breaks */
			bool vertical_paint = false,
			const uint32_t fcolor = COL_INFOBAR_SHADOW_TEXT,
			const char * alt_buttontext = NULL,
			const uint &buttontext_id = 0,
			bool show = true,
			const std::vector<neutrino_locale_t>& all_buttontext_id = std::vector<neutrino_locale_t>());

#endif /* __gui_widget_buttons_h__ */
