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

#include <global.h>
#include <neutrino.h>
#include "cc_frm_scrollbar.h"
#include "system/debug.h"

using namespace std;

/* base schema
		x,y
		+-----------------+
		|+---------------+|
		||sb_up_obj 	 ||
		||          	 ||
		|+---------------+|
		|+---------------+|
		||sb_segments_obj||
		||+-------------+||
		||| segment	|||
		||| id 0	|||
		|||          	|||
		||+-------------+||
		||| segment	|||
		||| id 1	|||
		|||          	|||
		||+-------------+||
		|+---------------+|
		|+---------------+|
		||sb_up_obj 	 ||
		||          	 ||
		|+---------------+|
		+-----------------+
*/

//sub class CComponentsScrollBar inherit from CComponentsFrmChain
CComponentsScrollBar::CComponentsScrollBar(	const int &x_pos, const int &y_pos, const int &w, const int &h,
						const int& count,
						CComponentsForm* parent,
						bool has_shadow,
						fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
						:CComponentsFrmChain(x_pos, y_pos, w, h, NULL, CC_DIR_Y, parent, has_shadow, color_frame, color_body, color_shadow)
{
	initVarSbForm(count);
}

void CComponentsScrollBar::initVarSbForm(const int& count)
{
	cc_item_type 	= CC_ITEMTYPE_FRM_SCROLLBAR;
	fr_thickness	= 0;
	
	append_x_offset = 0;
	append_y_offset = 2;
	
	sb_up_obj 	= sb_down_obj = NULL;
	sb_segments_obj = NULL;
	sb_up_icon	= frameBuffer->getIconBasePath() + NEUTRINO_ICON_BUTTON_TOP;
	sb_up_icon	+= ".png";
	sb_down_icon	= frameBuffer->getIconBasePath() + NEUTRINO_ICON_BUTTON_DOWN;
	sb_down_icon	+= ".png";
	
	sb_segments_count = count;
	sb_mark_id	= 0;

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
		sb_up_obj = new CComponentsPicture(CC_CENTERED, fr_thickness, sb_up_icon, this);
		sb_up_obj->doPaintBg(false);
	}
	sb_up_obj->setWidth(width-2*fr_thickness);
}

void CComponentsScrollBar::initBottomNaviIcon()
{
	//initialize icon object
	if (sb_down_obj == NULL){
		sb_down_obj = new CComponentsPicture(CC_CENTERED, CC_APPEND, sb_down_icon, this);
		sb_down_obj->doPaintBg(false);
	}
	sb_down_obj->setWidth(width-2*fr_thickness);
}

void CComponentsScrollBar::initSegments()
{
	//init dimensions for segments
	int w_seg = width - 4*fr_thickness;
	int h_seg = height - (sb_segments_count-1)*append_y_offset;

	//calculate height of segment container
	int h_seg_obj = height - 2*sb_up_obj->getHeight() - 3*append_y_offset;

	//init segment container
	if (sb_segments_obj == NULL){
		sb_segments_obj = new CComponentsFrmChain(CC_CENTERED, CC_APPEND, w_seg, h_seg_obj, NULL, CC_DIR_Y, this, false);
		sb_segments_obj->setFrameThickness(0,0);
		sb_segments_obj->setAppendOffset(0, 3);
	}else
		sb_segments_obj->setDimensionsAll(CC_CENTERED, CC_APPEND, w_seg, h_seg_obj);

	//set current color for segment container
	sb_segments_obj->setColorBody(col_body);

	//clean up segment container before add new segments
	sb_segments_obj->clear();

	//set y position of 1st segment and set height of segments
	int y_seg = 1+ append_y_offset;
	h_seg = sb_segments_obj->getHeight()/sb_segments_count - append_y_offset;

	//create and add segments to segment container
	for(u_int8_t i=0; i<sb_segments_count; i++){
		CComponentsShapeSquare *item = new CComponentsShapeSquare(0, y_seg, w_seg, h_seg, sb_segments_obj, false);
		y_seg += h_seg + append_y_offset;

		int id = sb_segments_obj->getCCItemId(item);
		if (sb_mark_id > id){
			dprintf(DEBUG_NORMAL, "[CComponentsScrollBar] %s: sb_mark_id out of range current=%d allowed=%d\n", __func__, sb_mark_id, id);
		}

		//set color for marked id
		if (sb_mark_id == id)
			item->setColorBody(COL_MENUCONTENTSELECTED_PLUS_0);
		else
			item->setColorBody(COL_MENUCONTENT_PLUS_1);		
	}

	//set corner types
	sb_segments_obj->front()->setCorner(RADIUS_MIN, CORNER_TOP);
	sb_segments_obj->back()->setCorner(RADIUS_MIN, CORNER_BOTTOM);
}
