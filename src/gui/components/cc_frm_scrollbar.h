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

#include <global.h>

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

		///visualize count mode
		bool sb_visual_enable;

		///segment colors
		fb_pixel_t sb_segment_col, sb_segment_col_sel;

		///count of segments
		uint32_t sb_segments_count;

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

		void initVarSbForm(	const int& count, const fb_pixel_t& color_select, const fb_pixel_t& color_passive);

	public:
		/**Class constructor to generate individual scrollbar objects
		 *
		 * @param[in] x_pos		exepts type int, x position on screen
		 * @param[in] x_pos		exepts type int, y position on screen modes are:
		 * @param[in] w			exepts type int, width of scrollbar object
		 * @param[in] h			exepts type int, height of scrollbar object
		 * @param[in] count		optional, exepts type int, count of pages, default 1
		 *
		 * usual paraemters:
		 * @param[in] parent		optional, exepts type pointer to a parent CComponentsForm object, default NULL
		 * @param[in] shadow_mode	optional, exepts type int defined by shadow mode enums, default CC_SHADOW_OFF
		 * @param[in] color_frame	optional, exepts type fb_pixel_t, default COL_SCROLLBAR_PLUS_0
		 * @param[in] color_body	optional, exepts type fb_pixel_t, default COL_SCROLLBAR_PLUS_0
		 * @param[in] color_shadow	optional, exepts type fb_pixel_t, default COL_SHADOW_PLUS_0
		 * @param[in] color_select	optional, exepts type fb_pixel_t, default COL_SCROLLBAR_ACTIVE_PLUS_0
		 * @param[in] color_passive	optional, exepts type fb_pixel_t, default COL_SCROLLBAR_PASSIVE_PLUS_0
		*/
		CComponentsScrollBar(	const int &x_pos,
					const int &y_pos,
					const int &w 		= SCROLLBAR_WIDTH,
					const int &h 		= 0,
					const int& count 	= 1,
					CComponentsForm *parent = NULL,
					int shadow_mode 	= CC_SHADOW_OFF,
					fb_pixel_t color_frame 	= COL_SCROLLBAR_PLUS_0,
					fb_pixel_t color_body 	= COL_SCROLLBAR_PLUS_0,
					fb_pixel_t color_shadow = COL_SHADOW_PLUS_0,
					fb_pixel_t color_select = COL_SCROLLBAR_ACTIVE_PLUS_0,
					fb_pixel_t color_passive = COL_SCROLLBAR_PASSIVE_PLUS_0);
// 		~CComponentsScrollBar(); //inherited from CComponentsForm

		/**Set current page number
		 * @return 			void
		 *
		 * @param[in] mark_id		exepts type int, this sets the current selected page number.
		 *
		 * @see				getMarkID()
		*/
		void setMarkID(const int& mark_id){sb_mark_id = mark_id; initSegments();}

		/**Gets current page number
		 * @return 			int
		 *
		 * @see				setMarkID()
		*/
		int getMarkID(){return sb_mark_id;};

		/**Sets count of possible scrollbar segments (e.g. page count) and
		 * current selected page at once .
		 * Each segment is assigned to a page number. Starting with id 0...n
		 * @return 			void
		 *
		 * @param[in] segment_count	exepts type int, sets the current count of pages.
		 * @param[in] mark_id		optional, exepts type int, sets the current selected page number, default = 0
		 * @see				also setMarkID()
		 * 				getMarkID()
		*/
		void setSegmentCount(const int& segment_count, const int& mark_id = 0);

		/**Get count of current scrollbar segments (page count)
		 * @return 			int
		 *
		 * @see				setSegmentCount()
		*/
		int getSegmentCount(){return sb_segments_count;}

		/**Enable/disable vizualized count of possible scroll items
		 * @param[in] enable		optional, exepts type bool.
		 * @note			Default mode is disabled.
		*/
		void enableVisualize(bool enable = true){sb_visual_enable = enable;}

		/**Disable vizualized count of possible scroll items
		*/
		void disableVisualize(){enableVisualize(false);}
};

void getScrollBarData(int *total_pages, int *current_page, int total_items, int items_per_page, int selected_item);

		/**Small and easy to apply scrollbar paint methode without expilcit object declaration
		* @return 			void
		*
		* @param[in] x_pos		exepts type int, x position on screen
		* @param[in] x_pos		exepts type int, y position on screen modes are:
		* @param[in] w			exepts type int, width of scrollbar object
		* @param[in] h			exepts type int, height of scrollbar object
		* @param[in] count		exepts type int, count of pages, default 1
		* @param[in] current_num	exepts type int, current selected page, default 0
		*
		* usual paraemters:
		* @param[in] parent		optional, exepts type pointer to a parent CComponentsForm object, default NULL
		* @param[in] shadow_mode	optional, exepts type int defined by shadow mode enums, default CC_SHADOW_OFF
		* @param[in] color_frame	optional, exepts type fb_pixel_t, default COL_SCROLLBAR_PLUS_0
		* @param[in] color_body		optional, exepts type fb_pixel_t, default COL_SCROLLBAR_PLUS_0
		* @param[in] color_shadow	optional, exepts type fb_pixel_t, default COL_SHADOW_PLUS_0
		* @param[in] color_select	optional, exepts type fb_pixel_t, default COL_SCROLLBAR_ACTIVE_PLUS_0
		* @param[in] color_passive	optional, exepts type fb_pixel_t, default COL_SCROLLBAR_PASSIVE_PLUS_0
		*/
void paintScrollBar(	const int &x_pos,
			const int &y_pos,
			const int &w,
			const int &h,
			const int& count,
			const int& current_num,
			int shadow_mode 	= CC_SHADOW_OFF,
			fb_pixel_t color_frame 	= COL_SCROLLBAR_PLUS_0,
			fb_pixel_t color_body 	= COL_SCROLLBAR_PLUS_0,
			fb_pixel_t color_shadow = COL_SHADOW_PLUS_0,
			fb_pixel_t color_select = COL_SCROLLBAR_ACTIVE_PLUS_0,
			fb_pixel_t color_passive = COL_SCROLLBAR_PASSIVE_PLUS_0);

#endif
