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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <neutrino.h>
#include "cc_frm_scrollbar.h"
#include "system/debug.h"

using namespace std;

/* base schema

		  x,y              width (w)
		/(x_pos, y_pos)       ^
		+---------------------+
		| +-----------------+ |
		| | sb_up_obj (icon)| |/color_frame
		| |       /\        | |
		| +-----------------+ | |/color_shadow
		|      col_body       | |
		|                     |
		| +-sb_segments_obj+  |
		| |                 | |
		| | +---segment---+ | |
		| | |    id 0     | | |
		| | |   active    | | |
		| | | color_select| | |
		| | +-------------+ | |
		| | append_y_offset | |
		| | +---segment---+ | |
		| | |     id 1    | | |
		| | |   passive   | | |
		| | |color_passive| | |
		| | +-------------+ | |
		| |   (count = 2)   | |
		| | other segments  | |
		| |  are possible   | |
		| +-----------------+ |
		|                     |
		| +-----------------+ |
		| | sb_up_obj (icon)| |
		| |       \/        | |
		| +-----------------+ |
		+---------------------+-> height (h)
*/

//sub class CComponentsScrollBar inherit from CComponentsFrmChain
CComponentsScrollBar::CComponentsScrollBar(	const int &x_pos, const int &y_pos, const int &w, const int &h,
						const int& count,
						CComponentsForm* parent,
						int shadow_mode,
						fb_pixel_t color_frame,
						fb_pixel_t color_body,
						fb_pixel_t color_shadow,
						fb_pixel_t color_select,
						fb_pixel_t color_passive)
						:CComponentsFrmChain(x_pos, y_pos, w, h, NULL, CC_DIR_Y, parent, shadow_mode, color_frame, color_body, color_shadow)
{
	initVarSbForm(count, color_select, color_passive);
}

void CComponentsScrollBar::initVarSbForm(const int& count, const fb_pixel_t& color_select, const fb_pixel_t& color_passive)
{
	cc_item_type 	= CC_ITEMTYPE_FRM_SCROLLBAR;
	fr_thickness	= 0;

	append_x_offset = OFFSET_INNER_MIN;
	append_y_offset = OFFSET_INNER_MIN;

	sb_up_obj 	= NULL;
	sb_down_obj	= NULL;
	sb_segments_obj = NULL;

	corner_type = CORNER_NONE;

	sb_up_icon	= frameBuffer->getIconPath(NEUTRINO_ICON_BUTTON_UP) ;
	sb_down_icon	= frameBuffer->getIconPath(NEUTRINO_ICON_BUTTON_DOWN);

	sb_segments_count = count;
	sb_mark_id	= 0;

	sb_visual_enable = false;
	sb_segment_col_sel = color_select;
	sb_segment_col	= color_passive;

	initCCItems();
}

void CComponentsScrollBar::setSegmentCount(const int& segment_count, const int& mark_id)
{
	sb_segments_count = segment_count; 
	sb_mark_id = mark_id;
	initSegments();
}


void CComponentsScrollBar::initCCItems()
{
	initTopNaviIcon();
	initSegments();
	initBottomNaviIcon();
}

void CComponentsScrollBar::initTopNaviIcon()
{
	//initialize icon object
	if (sb_up_obj == NULL){
		sb_up_obj = new CComponentsPicture(CC_CENTERED, fr_thickness, width-2*fr_thickness, width-2*fr_thickness, sb_up_icon, this);
		sb_up_obj->SetTransparent(CFrameBuffer::TM_BLACK);
		sb_up_obj->doPaintBg(false);
	}
}

void CComponentsScrollBar::initBottomNaviIcon()
{
	//initialize icon object
	if (sb_down_obj == NULL){
		sb_down_obj = new CComponentsPicture(CC_CENTERED, height - width-2*fr_thickness, width-2*fr_thickness, 0, sb_down_icon, this);
		sb_down_obj->SetTransparent(CFrameBuffer::TM_BLACK);
		sb_down_obj->doPaintBg(false);
	}
}

void CComponentsScrollBar::initSegments()
{
	//init dimensions for segments
	int w_seg = width - 2*fr_thickness - 2*append_x_offset;
	if (w_seg < 0)
		w_seg = 0;

	//calculate height of segment container
	int h_seg_obj = height - 2*fr_thickness - 2*sb_up_obj->getHeight()  - 2*append_y_offset;
	if (h_seg_obj < 0)
		h_seg_obj = 0;

	//init segment container
	if (sb_segments_obj == NULL){
		sb_segments_obj = new CComponentsFrmChain(CC_CENTERED, CC_APPEND, w_seg, h_seg_obj, NULL, CC_DIR_Y, this, false);
		sb_segments_obj->setFrameThickness(0);
	}else
		sb_segments_obj->setDimensionsAll(CC_CENTERED, CC_APPEND, w_seg, h_seg_obj);

	//set current color for segment container
	sb_segments_obj->setColorBody(col_body);

	//clean up segment container before add new segments
	sb_segments_obj->clear();

	//set y position of 1st segment and set height of segments
	int y_seg = append_y_offset;
	int h_seg = sb_segments_obj->getHeight()/sb_segments_count - append_y_offset;
	if (h_seg < 0)
		h_seg = 0;

	fb_pixel_t passive_col = sb_visual_enable ? sb_segment_col : col_body;

	//create and add segments to segment container
	for(u_int32_t i=0; i<sb_segments_count; i++){
		CComponentsShapeSquare *item = new CComponentsShapeSquare(0, y_seg, w_seg, h_seg, sb_segments_obj, false);
		y_seg += h_seg + append_y_offset;

		int id = sb_segments_obj->getCCItemId(item);

		//set color for marked id
		if (sb_mark_id == id){
			item->setColorBody(sb_segment_col_sel);
#if 0
			item->enableColBodyGradient(CC_COLGRAD_COL_A_2_COL_B);
			item->setColBodyGradient(CColorGradient::gradientDark2Light2Dark, CFrameBuffer::gradientHorizontal);
#endif
		}
		else{
			item->setColorBody(passive_col);
#if 0
			item->disableColBodyGradient();
#endif
		}

		//set different corner types for segments with possible conditions
		if (passive_col == col_body){
			item->setCorner(RADIUS_MIN, CORNER_ALL);
			continue;
		}else if (sb_segments_count == 1){
			item->setCorner(RADIUS_MIN, CORNER_ALL);
			break;
		}else if(i == 0){
			item->setCorner(RADIUS_MIN, CORNER_TOP);
			continue;
		}else if(i == sb_segments_count - 1){
			item->setCorner(RADIUS_MIN, CORNER_BOTTOM);
			break;
		}else if((i > 0 && i < sb_segments_count - 1)){
			item->setCorner(RADIUS_MIN, CORNER_NONE);
		}else{
			item->setCorner(RADIUS_MIN, CORNER_NONE);
		}
	}
}

void getScrollBarData(int *total_pages, int *current_page, int total_items, int items_per_page, int selected_item)
{
	// avoid divison by zero and fix wrong values
	total_items	= std::max(total_items, 1);
	items_per_page	= std::max(items_per_page, 1);
	selected_item	= std::max(selected_item, 0);

	*total_pages	= ((total_items - 1) / items_per_page) + 1;
	*current_page	= selected_item / items_per_page;
}

void paintScrollBar(	const int &x_pos,
			const int &y_pos,
			const int &w,
			const int &h,
			const int& count,
			const int& current_num,
			int shadow_mode,
			fb_pixel_t color_frame,
			fb_pixel_t color_body,
			fb_pixel_t color_shadow,
			fb_pixel_t color_select,
			fb_pixel_t color_passive)
{
	CComponentsScrollBar scrollbar(x_pos, y_pos, w, h, count, NULL, shadow_mode, color_frame, color_body, color_shadow, color_select, color_passive);
	scrollbar.setMarkID(current_num);
	scrollbar.paint0();
}
