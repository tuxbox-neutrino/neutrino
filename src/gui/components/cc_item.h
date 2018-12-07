/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012-2015, Thilo Graf 'dbt'

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

#ifndef __CC_ITEMS__
#define __CC_ITEMS__

#include "cc_types.h"
#include "cc_base.h"
#include "cc_draw.h"
#include "cc_signals.h"
#include <vector>
#include <string>
#include <driver/colorgradient.h>


class CComponentsItem : public CComponents
{
	protected:
		///property: define of item index, all bound items get an index,
		///default: CC_NO_INDEX as identifer for not embedded item and default index=0 for form as main parent
		///see also getIndex(), setIndex()
		int cc_item_index;
		///property: default enabled
		bool cc_item_enabled;
		///property: default not selected
		bool cc_item_selected;
		///property: page number, this defines current item page location, means: this item is embedded in a parent container on page number n, see also setPageNumber()
		///default value is 0 for page one, any value > 0 causes handling for mutilple pages at parent container
		uint8_t cc_page_number;
		///specifies that some certain operations especially eg. exec events for that item are possible, see also setFocus(), hasFocus()
		bool cc_has_focus;

		///Pointer to the form object in which this item is embedded.
		///Is typically the type CComponentsForm or derived classes, default intialized with NULL
		CComponentsForm *cc_parent;

		///initialze of basic framebuffer elements with shadow, background and frame.
		///must be called first in all paint() members before paint any item,
		///If backround is not required, it's possible to override this with variable paint_bg=false, use doPaintBg(true/false) to set this!
		///arg do_save_bg=false avoids using of unnecessary pixel memory, eg. if no hide with restore is provided. This is mostly the case  whenever
		///an item will be hide or overpainted with other methods, or it's embedded  (bound)  in a parent form.
		void paintInit(bool do_save_bg);

		///add "this" current item to parent
		void initParent(CComponentsForm* parent);


	public:
		CComponentsItem(CComponentsForm *parent = NULL);

		///sets pointer to the form object in which this item is embedded.
		void setParent(CComponentsForm *parent){cc_parent = parent;};
		///returns pointer to the form object in which this item is embedded.
		CComponentsForm* getParent(){return cc_parent;};
		///property: returns true if item is added to a form
		bool isAdded();
		///indicates wether item has focus
		bool hasFocus(){return cc_has_focus;}
		///set or unset focus of item, stand alone items without parent have always set focus to true, inside of a parent form object, always the last added item has focus
		void setFocus(bool focus);

		/**Erase or paint over rendered objects without restore of background, it's similar to paintBackgroundBoxRel() known
		 * from CFrameBuffer but with possiblity to define color, default color is COL_BACKGROUND_PLUS_0 (empty background)
		 *
		 * @return void
		 *
		 * @param[in] bg_color		optional, color, default color is current screen
		 * @param[in] ignore_parent	optional, default = false, defines the behavior inside a form, if item is embedded current background color is used instead blank screen
		 * @param[in] fblayer_type	optional, defines layer that to remove, default all layers (cc_fbdata_t) will remove
		 * 				possible layer types are:
		 * 				@li CC_FBDATA_TYPE_BGSCREEN,
		 * 				@li CC_FBDATA_TYPE_BOX,
		 * 				@li CC_FBDATA_TYPE_SHADOW_BOX,
		 * 				@li CC_FBDATA_TYPE_FRAME,
		 * 				@li CC_FBDATA_TYPE_BACKGROUND,
		 * @see
		 * 	cc_types.h
		 * 	gui/color.h
		 * 	driver/framebuffer.h
		*/
		void kill(const fb_pixel_t& bg_color = COL_BACKGROUND_PLUS_0, bool ignore_parent = false, const int& fblayer_type = ~CC_FBDATA_TYPES);

		///syncronizes item colors with current color settings if required, NOTE: overwrites internal values!
		void syncSysColors();
		
		///set select mode
		void setSelected(bool selected,
					const fb_pixel_t& sel_frame_col = COL_MENUCONTENTSELECTED_PLUS_0,
					const fb_pixel_t& frame_col = COL_FRAME_PLUS_0,
					const fb_pixel_t& sel_body_col = COL_MENUCONTENT_PLUS_0,
					const fb_pixel_t& body_col = COL_MENUCONTENT_PLUS_0,
					const int& frame_w = 3,
					const int& sel_frame_w = 3);
		///set enable mode, see also cc_item_enabled
		void setEnable(bool enabled){cc_item_enabled = enabled;};
		
		///get select mode, see also setSelected() above
		bool isSelected(){return cc_item_selected;};
		///get enable mode, see also setEnable() above
		bool isEnabled(){return cc_item_enabled;};

		///get current index of item, see also attribut cc_item_index
		int getIndex(){return cc_item_index;};
		///set an index to item, see also attribut cc_item_index.
		///To generate an index, use genIndex()
		void setIndex(const int& index){cc_item_index = index;};

		///sets page location of current item, parameter as uint8_t, see: cc_page_number
		void setPageNumber(const uint8_t& on_page_number){cc_page_number = on_page_number;};
		///returns current number of page location of current item, see: cc_page_number
		uint8_t getPageNumber();

		///set screen x-position, parameter as int
		void setXPos(const int& xpos);
		///set screen y-position, parameter as int
		void setYPos(const int& ypos);

		///set screen x-position, parameter as uint8_t, percent x value related to current width of parent form or screen
		void setXPosP(const uint8_t& xpos_percent);
		///set screen y-position, parameter as uint8_t, percent y value related to current height of parent form or screen
		void setYPosP(const uint8_t& ypos_percent);
		///set x and y position as percent value related to current parent form or screen dimensions at once
		void setPosP(const uint8_t& xpos_percent, const uint8_t& ypos_percent);

		///sets real x position on screen. Use this, if item is added to a parent form
		void setRealXPos(const int& xr){cc_xr = xr;}
		///sets real y position on screen. Use this, if item is added to a parent form
		void setRealYPos(const int& yr){cc_yr = yr;}
		///sets real x and y position on screen at once. Use this, if item is added to a parent form
		void setRealPos(const int& xr, const int& yr){cc_xr = xr; cc_yr = yr;}
		///get real x-position on screen. Use this, if item contains own render methods and item is bound to a form
		int getRealXPos(){return cc_parent ? cc_xr : x;}
		///get real y-position on screen. Use this, if item contains own render methods and item is bound to a form
		int getRealYPos(){return cc_parent ? cc_yr : y;}

		///do center item on screen or within a parent form, parameter along_mode assigns direction of centering
		void setCenterPos(int along_mode = CC_ALONG_X | CC_ALONG_Y);

		///set item height, parameter as uint8_t, as percent value related to current height of parent form or screen
		void setHeightP(const uint8_t& h_percent);
		///set item width, parameter as uint8_t, as percent value related to current width of parent form or screen
		void setWidthP(const uint8_t& w_percent);
};

#endif
