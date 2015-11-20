/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Scrollbar class based up CComponentsFrmChain.
	Copyright (C) 2014 Thilo Graf 'dbt'

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

#ifndef __CC_FORM_SCROLLBAR_H__
#define __CC_FORM_SCROLLBAR_H__

#include "cc_frm_chain.h"
#include "cc_item_picture.h"

class CComponentsScrollBar : public CComponentsFrmChain
{
	private:
		///scroll up navi icon object
		CComponentsPicture *sb_up_obj;
		///scroll down navi icon object
		CComponentsPicture *sb_down_obj;
		///container object for segments
		CComponentsFrmChain *sb_segments_obj;

		///names of navi icons
		std::string sb_up_icon, sb_down_icon;

		///count of segments
		int sb_segments_count;

		///mark id
		int sb_mark_id;

		///init top icon
		void initTopNaviIcon();
		///init bottom icon
		void initBottomNaviIcon();

		///init segements
		void initSegments();
		
		///init all items
		void initCCItems();

		void initVarSbForm(	const int& count);

	public:
		CComponentsScrollBar(	const int &x_pos, const int &y_pos, const int &w = 15, const int &h = 40,
					const int& count = 1,
					CComponentsForm *parent = NULL,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_3,
					fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
// 		~CComponentsScrollBar(); //inherited from CComponentsForm

		///set marked segment, 1st = 0, 2nd = 1 ...
		void setMarkID(const int& mark_id){sb_mark_id = mark_id; initSegments();};
		///get current assigned marked id
		int getMarkID(){return sb_mark_id;};

		///Sets count of scrollbar segments and is similar e.g. page count. Each segment is assigned to an id.  Starting with id 0...n see also setMarkID(), getMarkID().
		void setSegmentCount(const int& segment_count, const int& mark_id = 0);
		///Get count of current scrollbar segments
		int getSegmentCount(){return sb_segments_count;}
};

#endif
