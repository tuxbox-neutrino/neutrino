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


#ifndef __RATE_BANNER_H__
#define __RATE_BANNER_H__


#include "components/cc.h"

#include <string>


class CRateBanner : public CComponentsIconForm
{
	private:
		std::string rat_icon, rat_icon_bg, rat_prov_logo;
		float rate, rat_max;

		void init(	const std::string& quote_icon,
				const std::string& quote_icon_bg,
				const float& quote,
				const float& quote_max,
				const std::string& provider_logo);

	public:
		CRateBanner(	const int &x_pos,
				const int &y_pos,
				const float& quote,
				const float& quote_max = 10,
				const std::string& provider_logo = "",
				const std::string& quote_icon = NEUTRINO_ICON_STAR_ON,
				const std::string& quote_icon_bg = NEUTRINO_ICON_STAR_OFF,
				int shadow_mode = CC_SHADOW_OFF,
				fb_pixel_t color_frame = COL_FRAME_PLUS_0,
				fb_pixel_t color_body = COL_MENUHEAD_PLUS_0,
				fb_pixel_t color_shadow = COL_SHADOW_PLUS_0,
				CComponentsForm *parent = NULL);

		virtual ~CRateBanner();

		void paint(const bool &do_save_bg = CC_SAVE_SCREEN_YES);
};

#endif //__RATE_BANNER_H__
