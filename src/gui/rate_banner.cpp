/*
	 Based up Neutrino-GUI - Tuxbox-Project
	 Copyright (C) 2001 by Steffen Hehn 'McClean'

	 Class for rate banner.
	 Copyright (C) 2020, Thilo Graf 'dbt'

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

#include "rate_banner.h"
#include <math.h>

CRateBanner::CRateBanner(	const int &x_pos,
				const int &y_pos,
				const float& quote,
				const float& quote_max,
				const std::string& provider_logo,
				const std::string& quote_icon,
				const std::string& quote_icon_bg,
				int shadow_mode,
				fb_pixel_t color_frame,
				fb_pixel_t color_body,
				fb_pixel_t color_shadow,
				CComponentsForm *parent)
					:CComponentsIconForm(x_pos, y_pos, 0, 0, std::vector<std::string>(), parent, shadow_mode, color_frame, color_body, color_shadow)
{
	init(quote_icon, quote_icon_bg, quote, quote_max, provider_logo);
}

CRateBanner::~CRateBanner()
{

}

void CRateBanner::init(const std::string& quote_icon, const std::string& quote_icon_bg, const float& quote, const float& quote_max, const std::string& provider_logo)
{
	cc_item_type.name 	= "rate_banner";
	append_x_offset = OFFSET_INNER_SMALL;
	paint_bg = false;
	rat_prov_logo 	= provider_logo;
	rat_icon 	= quote_icon;
	rat_icon_bg 	= quote_icon_bg;
	rate		= quote;
	rat_max 	= quote_max;
}

void CRateBanner::paint(const bool &do_save_bg)
{
	clear();
	addIcon(rat_prov_logo);
	addIcons(rat_icon, rat_max);
	paintForm(do_save_bg);

	int x_base = getCCItem(rat_icon)->getRealXPos();
	float w_rate_space = static_cast<float>(width - (getCCItem(rat_prov_logo) ? getCCItem(rat_prov_logo)->getWidth() - append_x_offset : 0));
	float w_rate_size = w_rate_space / rat_max * rate;
	int w_tmp = static_cast<int>(round(w_rate_size)) - append_x_offset;
	fb_pixel_t* pixbuf = getScreen(x_base, y, w_tmp, height);

	removeAllIcons();
	addIcon(rat_prov_logo);
	addIcons(rat_icon_bg, rat_max);

	paintCCItems();
	frameBuffer->RestoreScreen(x_base, y, w_tmp, height, pixbuf);
	delete[] pixbuf;
	pixbuf = NULL;
}
