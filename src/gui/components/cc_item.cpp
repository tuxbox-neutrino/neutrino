/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012-2015, Thilo Graf 'dbt'
	Copyright (C) 2012, Michael Liebmann 'micha-bbg'

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
#include "cc_item.h"
#include <driver/screen_max.h>
#include <system/debug.h>
#include <cs_api.h>
using namespace std;

// 	 y
// 	x+------f-r-a-m-e-------+
// 	 |			|
//     height	  body		|
// 	 |			|
// 	 +--------width---------+

//abstract sub class CComponentsItem from CComponents
CComponentsItem::CComponentsItem(CComponentsForm* parent)
{
	cc_item_type 		= CC_ITEMTYPE_GENERIC;
	cc_item_index 		= CC_NO_INDEX;
	cc_item_enabled 	= true;
	cc_item_selected 	= false;
	cc_page_number		= 0;
	cc_has_focus		= true;
	initParent(parent);
}

void CComponentsItem::initParent(CComponentsForm* parent)
{
	if (cc_parent == parent)
		return;
	cc_parent = parent;
	if (cc_parent)
		cc_parent->addCCItem(this);
}

// init container properties in cc-items for shadow, background and frame.
// This member must be called first in all paint() members before paint other items into the container.
// If backround is not required, it's possible to override this with variable paint_bg=false, use doPaintBg(true/false) to set this!
void CComponentsItem::paintInit(bool do_save_bg)
{
	if (hasChanges()){
		clearFbData();
		is_painted = false; //force repaint if required
	}

	if (v_fbdata.empty()){
		int th = fr_thickness;
		fb_pixel_t col_frame_cur = col_frame;

		//calculate current needed frame thickeness and color, if item selected or not
		if (cc_item_selected){
			col_frame_cur = col_frame_sel;
			th = max(fr_thickness_sel, fr_thickness);
		}

		//calculate current needed corner radius for body box, depends of frame thickness
		int rad = (corner_rad>th) ? corner_rad-th : corner_rad;
		int sw = (shadow) ? shadow_w : 0;

		//evaluate shadow mode
		bool sh_r = (shadow & CC_SHADOW_ON) || (shadow & CC_SHADOW_RIGHT);
		bool sh_b = (shadow & CC_SHADOW_ON) || (shadow & CC_SHADOW_BOTTOM);

		//if item is bound on a parent form,...
		int ix = x, iy = y;
		fb_pixel_t col_shadow_clean = 0;
		if (cc_parent){
			//we must use real x/y values and from parent form as reference
			ix = cc_xr;
			iy = cc_yr;
			//we must use color of parent body instead screen background
			col_shadow_clean = cc_parent->getColorBody();
		}

// 		//handle shadow width
// 		if (width <= sw || height <= sw){ //don't use shadow, if item dimensions too small
// 			dprintf(DEBUG_NORMAL, "\033[33m[CComponentsItem]\t[%s - %d] shadow dimensions too small sw=%d, shadow is disabled set dimension to 0\033[0m\n",__func__, __LINE__, sw);
// 			shadow = CC_SHADOW_OFF;
// 			sw = 0;
// 		}

		//init paint layers
		cc_fbdata_t fbdata[] =
		{
			 //buffered bg
			{true, CC_FBDATA_TYPE_BGSCREEN,		ix,			iy, 			width+sw, 		height+sw, 		0, 		0, 		0,					0, 		NULL, NULL, NULL, false},

			//shadow bottom corner left
			{sh_b, CC_FBDATA_TYPE_SHADOW_BOX, 	ix+sw,			iy+height-corner_rad, 	corner_rad+sw, 		corner_rad+sw, 		col_shadow, 		corner_rad,	CORNER_BOTTOM_LEFT,			0, 		NULL, NULL, NULL, false},
			//clean up
			{sh_b, CC_FBDATA_TYPE_SHADOW_BOX, 	ix,			iy+height-corner_rad-sw, corner_rad+2*sw, 	corner_rad+sw, 		col_shadow_clean, 	corner_rad,	CORNER_BOTTOM_LEFT,			0, 		NULL, NULL, NULL, false},

			//shadow bottom corner right
			{sh_r, CC_FBDATA_TYPE_SHADOW_BOX, 	ix+width-corner_rad,	iy+height-corner_rad, 	corner_rad+sw, 		corner_rad+sw, 		col_shadow, 		corner_rad,	corner_type & CORNER_BOTTOM_RIGHT,	0, 		NULL, NULL, NULL, false},
			//clean up
			{sh_r, CC_FBDATA_TYPE_SHADOW_BOX, 	ix+width-corner_rad-sw,iy+height-corner_rad-sw, corner_rad+sw, 		corner_rad+sw, 		col_shadow_clean, 	corner_rad,	corner_type & CORNER_BOTTOM_RIGHT,	0, 		NULL, NULL, NULL, false},

			//shadow top corner right
			{sh_r, CC_FBDATA_TYPE_SHADOW_BOX, 	ix+width-corner_rad,	iy+sw, 			corner_rad+sw, 		corner_rad+sw, 		col_shadow, 		corner_rad,	corner_type & CORNER_TOP_RIGHT,			0, 		NULL, NULL, NULL, false},
			//clean up
			{sh_r, CC_FBDATA_TYPE_SHADOW_BOX, 	ix+width-corner_rad-sw,	iy, 			corner_rad+sw, 		corner_rad+2*sw, 	col_shadow_clean, 	corner_rad,	corner_type & CORNER_TOP_RIGHT,			0, 		NULL, NULL, NULL, false},

			//shadow bar bottom
			{sh_b, CC_FBDATA_TYPE_SHADOW_BOX, 	ix+2*sw+corner_rad,	iy+height, 		max(width-2*corner_rad-2*sw,0), 	sw, 					col_shadow, 	0,		CORNER_NONE,	0, 	NULL, NULL, NULL, false},
			//shadow bar right
			{sh_r, CC_FBDATA_TYPE_SHADOW_BOX, 	ix+width,		iy+2*sw+corner_rad,	sw, 					max(height-2*corner_rad-2*sw,0), 	col_shadow, 	0,		CORNER_NONE,	0, 	NULL, NULL, NULL, false},

			//body box
			{true, CC_FBDATA_TYPE_BOX,		ix+th,  		iy+th,  		width-2*th,     			height-2*th,    			col_body,       rad,		corner_type,	0, 	NULL, NULL, NULL, false},
			//body frame
			{true, CC_FBDATA_TYPE_FRAME,		ix,			iy, 			width, 					height, 				col_frame_cur, 	corner_rad,	corner_type,	th, 	NULL, NULL, NULL, false}
		};

		for(size_t i =0; i< (sizeof(fbdata) / sizeof(fbdata[0])) ;i++) {
			if ((fbdata[i].fbdata_type == CC_FBDATA_TYPE_FRAME) && !fr_thickness)
				continue;
			v_fbdata.push_back(fbdata[i]);
		}

		dprintf(DEBUG_DEBUG, "[CComponentsItem] %s:\ncc_item_type: %d\ncc_item_index = %d\nheight = %d\nwidth = %d\n", __func__, cc_item_type,  cc_item_index, height, width);
	}
	paintFbItems(do_save_bg);
}

//erase or paint over rendered objects
void CComponentsItem::kill(const fb_pixel_t& bg_color, bool ignore_parent, const int& fblayer_type)
{
	if(cc_parent == NULL){
		CComponents::kill(bg_color, this->corner_rad, fblayer_type);
	}else{
		if(ignore_parent)
			CComponents::kill(bg_color, this->corner_rad, fblayer_type);
		else
			CComponents::kill(cc_parent->getColorBody(), cc_parent->getCornerRadius(), fblayer_type);
	}
}

//synchronize colors for forms
//This is usefull if the system colors are changed during runtime
//so you can ensure correct applied system colors in relevant objects with unchanged instances.
void CComponentsItem::syncSysColors()
{
	col_body 	= COL_MENUCONTENT_PLUS_0;
	col_shadow 	= COL_SHADOW_PLUS_0;
	col_frame 	= COL_FRAME_PLUS_0;
}

//returns current item element type, if no available, return -1 as unknown type
int CComponentsItem::getItemType()
{
	for(int i =0; i< (CC_ITEMTYPES) ;i++){
		if (i == cc_item_type)
			return i;
	}

	dprintf(DEBUG_DEBUG, "[CComponentsItem] %s: unknown item type requested...\n", __func__);

	return -1;
}

//returns true if current item is added to a form
bool CComponentsItem::isAdded()
{
	if (cc_parent)
		return true;

	return false;
}

void CComponentsItem::setXPosP(const uint8_t& xpos_percent)
{
	int x_tmp  = cc_parent ? xpos_percent*cc_parent->getWidth() : xpos_percent*frameBuffer->getScreenWidth();
	setXPos(x_tmp/100);
}

void CComponentsItem::setYPosP(const uint8_t& ypos_percent)
{
	int y_tmp  = cc_parent ? ypos_percent*cc_parent->getHeight() : ypos_percent*frameBuffer->getScreenHeight();
	setYPos(y_tmp/100);
}

void CComponentsItem::setPosP(const uint8_t& xpos_percent, const uint8_t& ypos_percent)
{
	setXPosP(xpos_percent);
	setYPosP(ypos_percent);
}

void CComponentsItem::setCenterPos(int along_mode)
{
	if (along_mode & CC_ALONG_X)
		x = cc_parent ? cc_parent->getWidth() - width/2 : getScreenStartX(width);
	if (along_mode & CC_ALONG_Y)
		y = cc_parent ? cc_parent->getHeight() - height/2 : getScreenStartY(height);
}

void CComponentsItem::setHeightP(const uint8_t& h_percent)
{
	height = cc_parent ? h_percent*cc_parent->getHeight()/100 : h_percent*frameBuffer->getScreenHeight(true)/100;
}

void CComponentsItem::setWidthP(const uint8_t& w_percent)
{
	width = cc_parent ? w_percent*cc_parent->getWidth()/100 : w_percent*frameBuffer->getScreenWidth(true)/100;
}

void CComponentsItem::setFocus(bool focus)
{
	if(cc_parent){
		for(size_t i=0; i<cc_parent->size(); i++){
			if (focus)
				cc_parent->getCCItem(i)->setFocus(false);
		}
	}
	cc_has_focus = focus;
}
