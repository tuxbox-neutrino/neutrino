/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2013, Thilo Graf 'dbt'

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
						bool horizontal,
						bool dynamic_width,
						bool dynamic_height,
						bool has_shadow,
						fb_pixel_t& color_frame,
						fb_pixel_t& color_body,
						fb_pixel_t& color_shadow)
{
	initVarChain(x_pos, y_pos, w, h, v_items, horizontal, dynamic_width, dynamic_height, has_shadow, color_frame, color_body, color_shadow);
}


void CComponentsFrmChain::initVarChain(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const std::vector<CComponentsItem*> *v_items,
					bool horizontal,
					bool dynamic_width,
					bool dynamic_height,
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
	
	chn_horizontal	= horizontal;
	chn_dyn_height	= dynamic_height;
	chn_dyn_width	= dynamic_width;

	if (v_items){
		addCCItem(*v_items);
		initCChainItems();
	}
}

void CComponentsFrmChain::initCChainItems()
{
	if (!v_cc_items.empty()){
		if (chn_dyn_height)
			height = 0;
		if (chn_dyn_width)
			width = 0;
	}
	
	for (size_t i= 0; i< v_cc_items.size(); i++){
		//set general start position for all items
		if (i == 0)
			v_cc_items[i]->setPos(0, 0);

		//set arrangement with required direction
		if (chn_horizontal){
			if (i > 0)
				v_cc_items[i]->setPos(CC_APPEND, 0);
		}
		else{
			if (i > 0)
				v_cc_items[i]->setPos(0, CC_APPEND);
		}

		//assign size
		if (chn_horizontal){
			//assign dynamic width
			if (chn_dyn_width)
				width += v_cc_items[i]->getWidth();
			//assign dynamic height
			if (chn_dyn_height)
				height = max(v_cc_items[i]->getHeight(), height);
			else
				v_cc_items[i]->setHeight(height);
		}
		else{
			//assign dynamic height
			if (chn_dyn_height)
				height += v_cc_items[i]->getHeight();
			//assign dynamic width
			if (chn_dyn_width)
				width = max(v_cc_items[i]->getWidth(), width);
			else
				v_cc_items[i]->setWidth(width);
		}
	}
}

