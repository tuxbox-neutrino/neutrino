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

#ifndef __CC_FORM_ICONS_H__
#define __CC_FORM_ICONS_H__

#include "cc_frm.h"

class CComponentsIconForm : public CComponentsForm
{
	private:
		std::vector<std::string> v_icons;
		int ccif_offset, ccif_icon_align;
		void initMaxHeight(int *pheight);

	protected:
		void initVarIconForm(	const int &x_pos, const int &y_pos, const int &w, const int &h,
					const std::vector<std::string> &v_icon_names,
					CComponentsForm* parent,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6,
					fb_pixel_t color_body = COL_MENUHEAD_PLUS_0,
					fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);

	public:
		CComponentsIconForm(CComponentsForm *parent = NULL);
		CComponentsIconForm(	const int &x_pos, const int &y_pos, const int &w, const int &h,
					const std::vector<std::string> &v_icon_names,
					CComponentsForm *parent = NULL,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6,
					fb_pixel_t color_body = COL_MENUHEAD_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
// 		~CComponentsIconForm(); //inherited from CComponentsForm

		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
		void initCCIcons();
		void addIcon(const std::string& icon_name);
		void addIcon(std::vector<std::string> icon_name);
		void removeIcons(){v_icons.clear();};
		void insertIcon(const uint& icon_id, const std::string& icon_name);
		void removeIcon(const uint& icon_id);
		void removeIcon(const std::string& icon_name);
		void removeAllIcons();
		void setIconOffset(const int offset){ccif_offset = offset;};

		enum //alignements
		{
			CC_ICONS_FRM_ALIGN_RIGHT ,
			CC_ICONS_FRM_ALIGN_LEFT
		};
		void setIconAlign(int alignment){ccif_icon_align = alignment;};

		int getIconId(const std::string& icon_name);
};

#endif
