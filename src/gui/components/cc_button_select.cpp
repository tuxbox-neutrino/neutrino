/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
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

#include "config.h"
#include "cc_button_select.h"
#include <system/debug.h>

CCButtonSelect::CCButtonSelect()
{
	btn_container = NULL;
}

CComponentsFrmChain* CCButtonSelect::getButtonChainObject()
{
	return btn_container;
}

CComponentsButton* CCButtonSelect::getSelectedButtonObject()
{
	CComponentsButton* ret = static_cast<CComponentsButton*>(btn_container->getSelectedItemObject());
	return ret;
}

int CCButtonSelect::getSelectedButton()
{
	if (btn_container)
		return btn_container->getSelectedItem();
	return -1;
}

void CCButtonSelect::setSelectedButton( size_t item_id,
			const fb_pixel_t& sel_fr_col,
			const fb_pixel_t& fr_col,
			const fb_pixel_t& bg_col,
			const fb_pixel_t& sel_bg_col,
			const fb_pixel_t& text_col,
			const fb_pixel_t& sel_text_col,
			const int& frame_width,
			const int& sel_frame_width)
{
	CComponentsButton *btn = NULL;
	if (btn_container){
		for (size_t i= 0; i< btn_container->size(); i++){
			CComponentsItem *item = btn_container->getCCItem(i);
			if (item->getItemType() >= CC_ITEMTYPE_BUTTON && item->getItemType() <= CC_ITEMTYPE_BUTTON_BLUE){
				btn = static_cast<CComponentsButton*>(item);
				btn->setButtonTextColor(text_col);
			}
		}
		if (!btn)
			dprintf(DEBUG_NORMAL, "\033[33m[CCButtonSelect]\t[%s - %d], no button object found...\033[0m\n", __func__, __LINE__);

		fb_pixel_t sel_col = fr_col;
		if (btn_container->size() > 1)
			sel_col = sel_fr_col; //TODO: make it configurable
		btn_container->setSelectedItem(item_id, sel_col, fr_col, sel_bg_col, bg_col, frame_width, sel_frame_width);

		getSelectedButtonObject()->setButtonTextColor(sel_text_col);
	}
}


