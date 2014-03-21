/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2013-2014, Thilo Graf 'dbt'

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

#include <global.h>
#include <neutrino.h>
#include "cc_frm_chain.h"

using namespace std;

//-------------------------------------------------------------------------------------------------------
//sub class CComponentsFrmChain
CComponentsFrmChain::CComponentsFrmChain(	const int& x_pos, const int& y_pos, const int& w, const int& h,
						const std::vector<CComponentsItem*> *v_items,
						int direction,
						CComponentsForm* parent,
						bool has_shadow,
						fb_pixel_t& color_frame,
						fb_pixel_t& color_body,
						fb_pixel_t& color_shadow)
{
	initVarChain(x_pos, y_pos, w, h, v_items, direction, parent, has_shadow, color_frame, color_body, color_shadow);
}


void CComponentsFrmChain::initVarChain(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const std::vector<CComponentsItem*> *v_items,
					int direction,
					CComponentsForm* parent,
					bool has_shadow,
					fb_pixel_t& color_frame,
					fb_pixel_t& color_body,
					fb_pixel_t& color_shadow)
{
	cc_item_type 	= CC_ITEMTYPE_FRM_CHAIN;
	corner_rad	= 0;

	x 		= x_pos;
	y 		= y_pos;
	width 		= w;
	height 		= h;

	shadow		= has_shadow;
	col_frame	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;
	
	chn_direction	= direction;

	if (v_items)
		addCCItem(*v_items);

	initChainItems();
	initParent(parent);
}

void CComponentsFrmChain::setDirection(int direction)
{
	chn_direction = direction;
	initChainItems();
};

void CComponentsFrmChain::initChainItems()
{
	//init required dimensions, preferred are current width and height
	int w_tmp = width;
	int h_tmp = height;

	//exit if no item available
	if (v_cc_items.empty())
		return;

	//set new values
	w_tmp = append_x_offset;
	h_tmp = append_y_offset;

	for (size_t i= 0; i< v_cc_items.size(); i++){
		if (chn_direction & CC_DIR_X){
			w_tmp += v_cc_items[i]->getWidth();
			w_tmp += append_x_offset;
			v_cc_items[i]->setPos(CC_APPEND, CC_CENTERED);
		}

		if (chn_direction & CC_DIR_Y){
			h_tmp += v_cc_items[i]->getHeight();
			h_tmp += append_y_offset;
			v_cc_items[i]->setPos(CC_CENTERED, CC_APPEND);
		}
	}
	width 	= max (w_tmp, width);
	height 	= max (h_tmp, height);
}

