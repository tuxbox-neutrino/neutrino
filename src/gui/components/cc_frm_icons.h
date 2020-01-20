/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012-2018, Thilo Graf 'dbt'

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

#ifndef __CC_FORM_ICONS_H__
#define __CC_FORM_ICONS_H__

#include "cc_frm_chain.h"

class CComponentsIconForm : public CComponentsFrmChain
{
	private:
		std::vector<std::string> v_icons;
		void initMaxHeight(int *pheight);

	protected:
		void initVarIconForm(	const int &x_pos, const int &y_pos, const int &w, const int &h,
					const std::vector<std::string> &v_icon_names,
					CComponentsForm* parent,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_FRAME_PLUS_0,
					fb_pixel_t color_body = COL_MENUHEAD_PLUS_0,
					fb_pixel_t color_shadow = COL_SHADOW_PLUS_0);

	public:
		CComponentsIconForm(CComponentsForm *parent = NULL);
		CComponentsIconForm(	const int &x_pos, const int &y_pos, const int &w, const int &h,
					const std::vector<std::string> &v_icon_names  = std::vector<std::string>(),
					CComponentsForm *parent = NULL,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_FRAME_PLUS_0,
					fb_pixel_t color_body = COL_MENUHEAD_PLUS_0,
					fb_pixel_t color_shadow = COL_SHADOW_PLUS_0);
		virtual ~CComponentsIconForm() {};

		void addIcon(const std::string& icon_name);
		void addIcon(std::vector<std::string> icon_name);
		void addIcons(const std::string& icon_name, const size_t& count = 1);
		void removeIcons(){v_icons.clear();};
		void insertIcon(const uint& icon_id, const std::string& icon_name);
		void removeIcon(const uint& icon_id);
		void removeAllIcons();
};

#endif
