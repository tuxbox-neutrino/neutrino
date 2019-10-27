/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Class for epg window navigation bar.
	Copyright (C) 2017, Thilo Graf 'dbt'

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


#ifndef __C_NAVIBAR__
#define __C_NAVIBAR__

#include <gui/components/cc.h>
#include <driver/fontrenderer.h>


/**
	CNaviBar is sub class of CComponentsFrmChain. 
	Shows a navigation bar with text and navigation icons.
	You can enable/disable predefined icons and texts
	on the left and/or right side of bar.
*/
class CNaviBar : public CComponentsFrmChain
{
	private:
		CComponentsPicture	 	*nb_lpic, *nb_rpic;
		CComponentsText 		*nb_lText, *nb_rText;

		Font				*nb_font;

		bool nb_lpic_enable;
		bool nb_rpic_enable;

		std::string nb_l_text;
		std::string nb_r_text;

		void initCCItems();

	public:
		/**CNaviBar Constructor
		* @param[in]	x_pos
		* 	@li 	expects type int, x position
		* @param[in]	y_ypos
		* 	@li 	expects type int, y position
		* @param[in]	dx
		* 	@li 	expects type int, width
		* @param[in]	dy
		* 	@li 	expects type int, height
		* @param[in]	parent
		* 	@li 	optional: expects type CComponentsForm or derivates, allows usage as item inside CComponentsForm container, default = NULL
		* @param[in]	shadow_mode
		* 	@li 	optional: expects type fb_pixel_t, defines shadow mode, default CC_SHADOW_OFF
		* @param[in]	color_frame
		* 	@li 	optional: expects type fb_pixel_t, defines frame color, default value =  COL_FRAME_PLUS_0
		* @param[in]	color_body
		* 	@li 	optional: expects type fb_pixel_t, defines body color, default value =  COL_MENUFOOT_PLUS_0
		* @param[in]	color_shadow
		* 	@li 	optional: expects type fb_pixel_t, defines shadow color, default value =  COL_SHADOW_PLUS_0
		*
		* 	@see	class CComponentsFrmChain()
		*/
		CNaviBar(	const int& x_pos,
				const int& y_pos,
				const int& dx,
				const int& dy,
				CComponentsForm* parent = NULL,
				int shadow_mode = CC_SHADOW_OFF,
				fb_pixel_t& color_frame = COL_FRAME_PLUS_0,
				fb_pixel_t& color_body = COL_MENUFOOT_PLUS_0,
				fb_pixel_t& color_shadow = COL_SHADOW_PLUS_0);

		//~CNaviBar(); //is inherited

		/**
		* Enable or disable left icon
		* @param[in]	enable
		* 	@li 	expects type bool, default = true
		*/
		void enableLeftArrow(bool enable = true){nb_lpic_enable = enable; initCCItems();}

		/**
		* Enable or disable right icon
		* @param[in]	enable
		* 	@li 	expects type bool, default = true
		*/
		void enableRightArrow(bool enable = true){nb_rpic_enable = enable; initCCItems();}

		/**
		* disable left icon
		* no parameter
		*/
		void disableLeftArrow(){enableLeftArrow(false);}

		/**
		* disable right icon
		* no parameter
		*/
		void disableRightArrow(){enableRightArrow(false);}

		/**
		* Enable or disable both icons at once.
		* @param[in]	enable_left
		* 	@li 	expects type bool, default = true
		* @param[in]	enable_right
		* 	@li 	expects type bool, default = true
		*/
		void enableArrows(bool enable_left = true, bool enable_right = true){enableLeftArrow(enable_left); enableRightArrow(enable_right);}

		/**
		* Disable all icons.
		* no parameter
		*/
		void disableArrows(){disableLeftArrow(); disableRightArrow();}

		/**
		* Sets font type for texts.
		* @param[in]	font
		* 	@li 	expects type Font*
		*/
		void setFont(Font *font) {nb_font = font; initCCItems();}

		/**
		* Sets left text.
		* @param[in]	text
		* 	@li 	expects type std::string
		*/
		void setLeftText(const std::string& text) {nb_l_text = text; initCCItems();}

		/**
		* Sets right text
		* @param[in]	text
		* 	@li 	expects type std::string
		*/
		void setRightText(const std::string& text) {nb_r_text = text; initCCItems();}

		/**
		* Sets left and right text at once.
		* @param[in]	left
		* 	@li 	expects type std::string
		* @param[in]	right
		* 	@li 	expects type std::string
		*/
		void setText(const std::string& left, const std::string& right) {setLeftText(left); setRightText(right);}

		/**
		* Paint bar on screen.
		* @param[in]	do_save_bg
		* 	@li 	optional: expects type bool, default = CC_SAVE_SCREEN_YES.
		*/
		void paint(const bool &do_save_bg = CC_SAVE_SCREEN_YES);
};

#endif

