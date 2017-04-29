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


/* general chain form shema

		   x/y
		 /
		+---------------------------------------width----------------------------------------+
		|                                    chn_t_offset                                    |
		|            +--------+               +--------+               +--------+            |
		|chn_l_offset| item   |append_x_offset| item   |append_x_offset| item   |chn_r_offset|
		|            +--------+               +--------+               +--------+            |
		|                                   append_y_offset                                  |height
		|            +--------+               +--------+               +--------+            |
		|            | item   |               | item   |               | item   |            |
		|            +--------+               +--------+               +--------+            |
		|                                    chn_t_offset                                    |
		+------------------------------------------------------------------------------------+
*/


//-------------------------------------------------------------------------------------------------------
//sub class CComponentsFrmChain
CComponentsFrmChain::CComponentsFrmChain(	const int& x_pos, const int& y_pos, const int& w, const int& h,
						const std::vector<CComponentsItem*> *v_items,
						int direction,
						CComponentsForm* parent,
						int shadow_mode,
						fb_pixel_t& color_frame,
						fb_pixel_t& color_body,
						fb_pixel_t& color_shadow)
{
	initVarChain(x_pos, y_pos, w, h, v_items, direction, parent, shadow_mode, color_frame, color_body, color_shadow);
}


void CComponentsFrmChain::initVarChain(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const std::vector<CComponentsItem*> *v_items,
					int direction,
					CComponentsForm* parent,
					int shadow_mode,
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

	chn_l_offset = chn_r_offset = 0;
	chn_t_offset = chn_b_offset = 0;

	shadow		= shadow_mode;
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
}

void CComponentsFrmChain::initChainItems()
{
	//exit if no item available
	if (v_cc_items.empty())
		return;

	//set new values
	int w_tmp = 0, h_tmp = 0;
	int w_item = 0, h_item = 0;
	size_t i_count = v_cc_items.size();


	for (size_t i= 0; i< i_count; i++){
// 		x_item = v_cc_items[i]->getXPos();
// 		y_item = v_cc_items[i]->getYPos();
		w_item = v_cc_items[i]->getWidth();
		h_item = v_cc_items[i]->getHeight();

		if (chn_direction & CC_DIR_X){
			if (i == 0){
				v_cc_items[i]->setXPos(chn_l_offset);
				w_tmp += chn_l_offset;
				w_tmp += w_item;
				if (i_count == 1)
					w_tmp += chn_r_offset;
			}

			if (i_count > 1){
				if (i == i_count-1){
					w_tmp += w_item;
					w_tmp += append_x_offset;
					v_cc_items[i]->setXPos(w_tmp - v_cc_items[i]->getWidth());
					w_tmp += chn_r_offset;
				}
			}

			if (i != 0 && i != i_count-1){
				w_tmp += w_item;
				w_tmp += append_x_offset;
				v_cc_items[i]->setXPos(w_tmp - v_cc_items[i]->getWidth());
			}
		}

		if (chn_direction & CC_DIR_Y){
			if (i == 0){
				v_cc_items[i]->setYPos(chn_t_offset);
				h_tmp += chn_t_offset;
				h_tmp += h_item;
				if (i_count == 1)
					h_tmp += chn_b_offset;
			}

			if (i_count > 1){
				if (i == i_count-1){
					h_tmp += h_item;
					h_tmp += append_y_offset;
					v_cc_items[i]->setYPos(h_tmp - v_cc_items[i]->getHeight());
					h_tmp += chn_b_offset;
				}
			}

			if (i != 0 && i != i_count-1){
				h_tmp += h_item;
				h_tmp += append_y_offset;
				v_cc_items[i]->setYPos(h_tmp - v_cc_items[i]->getHeight());
			}
		}
	}
	width 	= max (w_tmp, width);
	height 	= max (h_tmp, height);
}

