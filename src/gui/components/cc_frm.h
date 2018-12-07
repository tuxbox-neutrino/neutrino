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

#ifndef __CC_FORM_H__
#define __CC_FORM_H__



#include "config.h"
#include "cc_base.h"
#include "cc_item.h"

#define DEFAULT_SEL_FRAME_WIDTH 1

class CComponentsForm : public CComponentsItem
{
	protected:
		std::vector<CComponentsItem*>	v_cc_items;
		void paintForm(bool do_save_bg);
		///generates next possible index for an item, see also cc_item_index, getIndex(), setIndex()
		int genIndex();

		///scrollbar object
		CComponentsScrollBar *sb;

		int append_x_offset;
		int append_y_offset;

		///property: count of pages of form
		uint8_t page_count;
		///property: id of current page, default = 0 for 1st page
		uint8_t cur_page;
		///scrollbar width
		int w_sb;
		///returns true, if current page is changed, see also: setCurrentPage() 
		bool isPageChanged();

		///enable/disable page scrolling, default enabled with page scroll mode up/down keys, see also enablePageScroll()
		int page_scroll_mode;

		///container for exit keys, default exit keys are CRCInput::RC_home and CRCInput::RC_setup
		std::vector <neutrino_msg_t> v_exit_keys;

		///initialize basic properties
		void Init(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const fb_pixel_t& color_frame,
					const fb_pixel_t& color_body,
					const fb_pixel_t& color_shadow);

	public:
		CComponentsForm(	const int x_pos = 0, const int y_pos = 0, const int w = 800, const int h = 600,
					CComponentsForm *parent = NULL,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_FRAME_PLUS_0,
					fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0,
					fb_pixel_t color_shadow = COL_SHADOW_PLUS_0);
		virtual ~CComponentsForm();

		///paints current form on screen, for paint a page use paintPage()
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);

		///same like CComponentsItem::kill(), but erases all embedded items inside of parent at once, this = parent
		///NOTE: Items always have parent bindings to "this" and use the parent background color as default! Set parameter 'ignore_parent=true' to ignore parent background color!
		void killCCItems(const fb_pixel_t& bg_color, bool ignore_parent);

		/**
		 Removes possible contained items and finally removes
		 current form from screen and
		 restores last displayed background before form was painted.
		*/
		void hide(){hideCCItems(); CCDraw::hide();}

		///restore background for all items inside form,
		void hideCCItems();

		///add an item to form collection, returns id
		int addCCItem(CComponentsItem* cc_Item);
		///add items from a vector to form collection, returns size/count of items
		int addCCItem(const std::vector<CComponentsItem*> &cc_items);
		void insertCCItem(const uint& cc_item_id, CComponentsItem* cc_Item);

		///removes item object from container and deallocates instance
		void removeCCItem(const uint& cc_item_id);
		///removes item object from container and deallocates instance
		void removeCCItem(CComponentsItem* cc_Item);

		void replaceCCItem(const uint& cc_item_id, CComponentsItem* new_cc_Item);
		void replaceCCItem(CComponentsItem* old_cc_Item, CComponentsItem* new_cc_Item);
		void exchangeCCItem(const uint& item_id_a, const uint& item_id_b);
		void exchangeCCItem(CComponentsItem* item_a, CComponentsItem* item_b);

		/**Function to get current item id from passed item.
		* @param[in]  cc_Item
		* 	@li CComponentsItem*
		* @return
		*	int, in case of not found item returns -1
		*/
		int getCCItemId(CComponentsItem* cc_Item);

		/**Function to get current item from item collection.
		* @param[in]  cc_item_id
		* 	@li item id as unsigned int
		* @return
		*	CComponentsItem*, in case of not found item returns NULL
		*/
		CComponentsItem* getCCItem(const uint& cc_item_id);

		/**Function to get previous item from item collection.
		* @param[in]  current_cc_item
		* 	@li CComponentsItem*
		* @return
		*	CComponentsItem*, in case of not found item returns NULL
		*/
		CComponentsItem* getPrevCCItem(CComponentsItem* current_cc_item);

		/**Function to get next item from item collection.
		* @param[in]  current_cc_item
		* 	@li CComponentsItem*
		* @return
		*	CComponentsItem*, in case of not found item returns NULL
		*/
		CComponentsItem* getNextCCItem(CComponentsItem* current_cc_item);

		void paintCCItems();

		///clean up and deallocate existant items from v_cc_items at once
		virtual	void clear();
		///return true, if no items available
		virtual	bool empty(){return v_cc_items.empty();};
		///return size (count) of available items
		virtual	size_t size(){return v_cc_items.size();};
		///return reference to first item
		virtual	CComponentsItem* front(){return v_cc_items.front();};
		///return reference to last item
		virtual	CComponentsItem* back(){return v_cc_items.back();};

		///sets alignment offset between items
		void setAppendOffset(const int &x_offset, const int& y_offset){append_x_offset = x_offset; append_y_offset = y_offset;};

		///sets count of pages, parameter as uint8_t
		///NOTE: page numbers are primary defined in items and this values have priority!! Consider that smaller values
		///than the current values can make problems and are not allowed, therefore smaller values than
		///current page count are ignored!
		void setPageCount(const uint8_t& pageCount);
		///returns current count of pages,
		///NOTE: page number are primary defined in items and secondary in form variable 'page_count'. This function returns the maximal value from both!
		uint8_t getPageCount();
		///sets current page
		void setCurrentPage(const uint8_t& current_page){cur_page = current_page;};
		///get current page
		uint8_t getCurrentPage(){return cur_page;};
		///paint defined page number 0...n
		void paintPage(const uint8_t& page_number, bool do_save_bg = CC_SAVE_SCREEN_NO);
		///enum page scroll modes
		enum
		{
			PG_SCROLL_M_UP_DOWN_KEY 	= 1,
			PG_SCROLL_M_LEFT_RIGHT_KEY 	= 2,
			PG_SCROLL_M_OFF		 	= 4
		};
		///enable/disable page scroll, parameter1 default enabled for up/down keys
		void enablePageScroll(const int& mode = PG_SCROLL_M_UP_DOWN_KEY){page_scroll_mode = mode;};

		///set width of scrollbar
		void setScrollBarWidth(const int& scrollbar_width){w_sb = scrollbar_width;};
		///returns id of selected item, return value as int, returns -1: if is nothing selected
		int getSelectedItem();

		/**Function to get consumed  space of items inside form in y direction.
		* @return
		*	int, used lines
		*/
		int getUsedDY();

		/**Function to get consumed  space of items inside form in x direction.
		* @return
		*	int, used lines
		*/
		int getUsedDX();

		/**Function to get free usable space of items inside form in y direction.
		* @return
		*	int, free lines
		*/
		int getFreeDY(){return height - getUsedDY();}

		/**Function to get free usable space of items inside form in x direction.
		* @return
		*	int, free lines
		*/
		int getFreeDX(){return width - getUsedDX();}

		///returns pointer to selected item, return value as CComponentsItem*, returns NULL: if is nothing selected
		CComponentsItem* getSelectedItemObject();
		///select a definied item, parameter1 as size_t
		void setSelectedItem(	int item_id,
						const fb_pixel_t& sel_frame_col = COL_MENUCONTENTSELECTED_PLUS_0,
						const fb_pixel_t& frame_col = COL_FRAME_PLUS_0,
						const fb_pixel_t& sel_body_col = COL_MENUCONTENT_PLUS_0,
						const fb_pixel_t& body_col = COL_MENUCONTENT_PLUS_0,
						const int& frame_w = DEFAULT_SEL_FRAME_WIDTH,
						const int& sel_frame_w = DEFAULT_SEL_FRAME_WIDTH);
		///select a definied item, parameter1 as CComponentsItem*
		void setSelectedItem(	CComponentsItem* cc_item,
						const fb_pixel_t& sel_frame_col = COL_MENUCONTENTSELECTED_PLUS_0,
						const fb_pixel_t& frame_col = COL_FRAME_PLUS_0,
						const fb_pixel_t& sel_body_col = COL_MENUCONTENT_PLUS_0,
						const fb_pixel_t& body_col = COL_MENUCONTENT_PLUS_0,
						const int& frame_w = DEFAULT_SEL_FRAME_WIDTH,
						const int& sel_frame_w = DEFAULT_SEL_FRAME_WIDTH);

		///exec main method, see also sub exec methods
		int exec();

		///adds additional exec key to current collection, default exit keys are CRCInput::RC_home and CRCInput::RC_setup
		void addExitKey(const neutrino_msg_t& key){v_exit_keys.push_back(key);}
		///remove all current exec keys from current collection, NOTE: use addExitKey() if new exec key is required
		void removeExitKeys(){v_exit_keys.clear();}

		///enum exec loop control
		enum
		{
			NO_EXIT	= 0,
			EXIT	= 1
		};
		///execKey() methods handle events for defined neutrino messages, see class CRCInput::, this methodes contains a signal handler named OnExecMsg, so it is possible to connect with any common function or method
		///exec sub method for pressed keys, parameters1/2 by rev, parameter3 msg_list as vector contains a list of possible RC-messages for defined message, parameter4 force_exit default = false
		void execKey(	neutrino_msg_t& msg,
					neutrino_msg_data_t& data,
					int& res,
					bool& cancel_exec,
					const std::vector<neutrino_msg_t>& msg_list,
					bool force_exit = false);
		///exec sub method for pressed key, parameters1/2 by rev, parameter3 force_exit default = false
		bool execKey(	neutrino_msg_t& msg,
					neutrino_msg_data_t& data,
					int& res,
					bool& cancel_exec,
					const neutrino_msg_t& msg_val,
					bool force_exit = false);

		///exec sub method for page scroll, parameter1 neutrino_msg_t by rev
		void execPageScroll(neutrino_msg_t& msg, neutrino_msg_data_t& data, int& res, bool& cancel_exec);

		///exec sub method for exit loop, parameters by rev
		void execExit(	neutrino_msg_t& msg,
					neutrino_msg_data_t& data,
					int& res, bool& cancel_exec,
					const std::vector<neutrino_msg_t>& v_msg_list);

		///enum scroll direction
		enum
		{
			SCROLL_P_DOWN	= 0,
			SCROLL_P_UP	= 1
		};
		///scroll page and paint current selected page, if parameter2 = true (default)
		void ScrollPage(int direction = SCROLL_P_DOWN, bool do_paint = true);

		bool enableColBodyGradient(const int& enable_mode, const fb_pixel_t& sec_colorconst, const int& direction = -1 /*CFrameBuffer::gradientVertical*/);
		///cleans saved screen buffer include from sub items, required by hide(), returns true if any buffer was deleted
		bool clearSavedScreen();
		///cleanup paint cache include from sub items, removes saved buffer contents from cached foreground layers, returns true if any buffer was removed
		bool clearPaintCache();
		///cleanup old gradient buffers include from sub items, returns true if any gradient buffer data was removed
		bool clearFbGradientData();

		///slot for repaint event, reserved for actions before repaint if paint() already was done.
		sigc::slot0<void> sl_form_repaint;
};

#endif
