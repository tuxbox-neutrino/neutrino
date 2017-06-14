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

CCButtonSelect::CCButtonSelect(CComponentsFrmChain *chain_obj)
{
	chain = chain_obj;
}

CComponentsFrmChain* CCButtonSelect::getButtonChainObject()
{
	return chain;
}

CComponentsButton* CCButtonSelect::getSelectedButtonObject()
{
	CComponentsButton* ret = static_cast<CComponentsButton*>(chain->getSelectedItemObject());
	return ret;
}

int CCButtonSelect::getSelectedButton()
{
	if (chain)
		return chain->getSelectedItem();
	return -1;
}

void CCButtonSelect::setSelectedButton(size_t item_id,
			const fb_pixel_t& fr_col,
			const fb_pixel_t& sel_fr_col,
			const fb_pixel_t& bg_col,
			const fb_pixel_t& sel_bg_col,
			const fb_pixel_t& text_col,
			const fb_pixel_t& sel_text_col,
			const int& frame_width,
			const int& sel_frame_width)
{
	if (chain){
		for (size_t i= 0; i< chain->size(); i++){
			CComponentsButton *btn = static_cast<CComponentsButton*>(chain->getCCItem(i));
			btn->setButtonTextColor(text_col);
		}
		fb_pixel_t sel_col = fr_col;
		if (chain->size() > 1)
			sel_col = sel_fr_col; //TODO: make it configurable
		chain->setSelectedItem(item_id, sel_col, fr_col, sel_bg_col, bg_col, frame_width, sel_frame_width);

		getSelectedButtonObject()->setButtonTextColor(sel_text_col);
	}
}


