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
#include <gui/components/cc_base.h>


class CComponentsForm : public CComponentsItem
{
	private:
		void initVarForm();
	protected:
		std::vector<CComponentsItem*>	v_cc_items;
		void paintForm(bool do_save_bg);
		///generates next possible index for an item, see also cc_item_index, getIndex(), setIndex()
		int genIndex();

		int append_h_offset;
		int append_v_offset;
	public:
		
		CComponentsForm();
		CComponentsForm(const int x_pos, const int y_pos, const int w, const int h, bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
		virtual ~CComponentsForm();
		
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
		void hide(bool no_restore = false);
		virtual void addCCItem(CComponentsItem* cc_Item);
		virtual void addCCItem(const std::vector<CComponentsItem*> &cc_items);
		virtual void insertCCItem(const uint& cc_item_id, CComponentsItem* cc_Item);
		virtual void removeCCItem(const uint& cc_item_id);
		virtual void removeCCItem(CComponentsItem* cc_Item);
		virtual void replaceCCItem(const uint& cc_item_id, CComponentsItem* new_cc_Item);
		virtual void replaceCCItem(CComponentsItem* old_cc_Item, CComponentsItem* new_cc_Item);
		virtual void exchangeCCItem(const uint& item_id_a, const uint& item_id_b);
		virtual void exchangeCCItem(CComponentsItem* item_a, CComponentsItem* item_b);
		virtual int getCCItemId(CComponentsItem* cc_Item);
		virtual CComponentsItem* getCCItem(const uint& cc_item_id);
		virtual void paintCCItems();

		///clean up and deallocate existant items from v_cc_items at once
		virtual	void clear();
		///return true, if no items available
		virtual	bool empty(){return v_cc_items.empty();};
		///return size (count) of available items
		virtual	size_t size(){return v_cc_items.size();};

		virtual void setAppendOffset(const int &h_offset, const int& v_offset){append_h_offset = h_offset; append_v_offset = v_offset;};
};

#endif
