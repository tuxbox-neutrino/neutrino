/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012-2016, Thilo Graf 'dbt'
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
	cc_item_index 		= CC_NO_INDEX;
	cc_item_enabled 	= true;
	cc_item_selected 	= false;
	cc_page_number		= 0;
	cc_has_focus		= true;
	cc_parent		= NULL;
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

	//calculate current needed frame thickeness and color, if item selected or not
	fb_pixel_t col_frame_cur = col_frame;
	int th = cc_enable_frame ? fr_thickness : 0;

	if (v_fbdata.empty()){
		//set current position and dimensions
		int ix = x, iy = y, dx = width, dy = height;
		
		//and ensure sw is not larger than body dimensions, max x%
		int sw = (shadow) ? min(shadow_w, min(dx, dy)*50/100) : 0;
		/*ensure shadow is never < 0*/
		sw = max(0, sw);

		//set current needed corner main box radius
		int box_rad = corner_type ? corner_rad : 0;

		//and ensure max main box radius < dimensions, avoids possible fb artefacts on screen
		int box_rad2 = box_rad/2;
		if(box_rad2 > dy || box_rad2 > dx){
			int tmp_rad = box_rad;
			if (box_rad2 > dx)
				tmp_rad = (box_rad2-dx)*2;
			if (box_rad2 > dy)
				tmp_rad = (box_rad2-dy)*2;
			box_rad = tmp_rad;
		}

		//Workaround: ensure radius values >= 0, framebuffer methode paintBoxRel() gets confused
		box_rad = max(box_rad, 0);

		//if item is bound on a parent form,...
		if (cc_parent){
			//...we must use real x/y values and from parent form as reference
			ix = cc_xr;
			iy = cc_yr;
			//...we must use color of parent body instead screen background
			col_shadow_clean = cc_parent->getColorBody();
		}

		///evaluate shadow layer parts
		//handle general shadow corner dimensions
		int sh_cdx = box_rad+sw+th; //width
		int sh_cdy = box_rad+sw+th; //height

		//adapt shadow corner dimensions if body dimensions are too small, use an offset if required
		int /*sh_cdx_size_offset = 0,*/ sh_cdy_size_offset = 0;
		if (sh_cdy*2 > dy)
			sh_cdy_size_offset = sh_cdy*2-dy;
// 		if (sh_cdx*2 > dx)
// 			sh_cdx_size_offset = sh_cdx*2-dx;

		//handle shadow positions
		//...corner bottom right
		int sh_cbr_x = ix+dx-sh_cdx+sw;
		int sh_cbr_y = iy+dy-sh_cdy+sw;

		//...corner top right
		int sh_ctr_x = sh_cbr_x;
		int sh_ctr_y = iy+sw;

		//...corner bottom left
		int sh_cbl_x = ix+sw;
		int sh_cbl_y = sh_cbr_y;

		//handle general shadow bar dimensions
		int sh_bdx = max(0, dx-sh_cdx-sh_cdx); /*ensure value is never < 0*/
		int sh_rdy = dy-sh_cdy-sh_cdy;

		//...bar bottom
		int sh_bx = sh_cbl_x+sh_cdx;
		int sh_by = iy+dy;

		//...bar right
		int sh_rx = ix+dx;
		int sh_ry = sh_ctr_y+sh_cdy;

		//corners
		bool sh_ctr = (shadow & CC_SHADOW_CORNER_TOP_RIGHT);
		bool sh_cbr = (shadow & CC_SHADOW_CORNER_BOTTOM_RIGHT);
		bool sh_cbl = (shadow & CC_SHADOW_CORNER_BOTTOM_LEFT);

		//...shadow bar right
		bool sh_br = (shadow & CC_SHADOW_RIGHT);
		if (sh_rdy < 1)
 			sh_br = false;
		//...shadow bar bottom
		bool sh_bb = (shadow & CC_SHADOW_BOTTOM);
		if (sh_bdx < 1)
			sh_bx = false;

		//init paint layers
		cc_fbdata_t fbdata[] =
		{
			 //buffered bg
			{true, CC_FBDATA_TYPE_BGSCREEN,		ix,		iy, 		dx+sw, 		dy+sw, 				0, 			0, 		0,					0, NULL, NULL, NULL, false},

			//shadow corner bottom left
			{sh_cbl, CC_FBDATA_TYPE_SHADOW_BOX, 	sh_cbl_x,	sh_cbl_y, 	sh_cdx, 	sh_cdy, 			col_shadow, 		box_rad,	corner_type & CORNER_BOTTOM_LEFT,	0, NULL, NULL, NULL, false},
			//clean up inside body
			{sh_cbl, CC_FBDATA_TYPE_SHADOW_BOX, 	sh_cbl_x-sw+th,	sh_cbl_y-sw, 	sh_cdx+sw, 	sh_cdy, 			col_shadow_clean, 	box_rad,	corner_type & CORNER_BOTTOM_LEFT,	0, NULL, NULL, NULL, false},

			//shadow bar bottom
			{sh_bb, CC_FBDATA_TYPE_SHADOW_BOX, 	sh_bx,		sh_by, 		sh_bdx, 	sw, 				col_shadow, 		0,		CORNER_NONE,				0, NULL, NULL, NULL, false},

			//shadow corner bottom right
			{sh_cbr, CC_FBDATA_TYPE_SHADOW_BOX, 	sh_cbr_x,	sh_cbr_y, 	sh_cdx, 	sh_cdy, 			col_shadow, 		box_rad,	corner_type & CORNER_BOTTOM_RIGHT,	0, NULL, NULL, NULL, false},
			//clean up inside body
			{sh_cbr, CC_FBDATA_TYPE_SHADOW_BOX, 	sh_cbr_x-sw,	sh_cbr_y-sw, 	sh_cdx, 	sh_cdy, 			col_shadow_clean, 	box_rad,	corner_type & CORNER_BOTTOM_RIGHT,	0, NULL, NULL, NULL, false},

			//shadow bar right
			{sh_br, CC_FBDATA_TYPE_SHADOW_BOX, 	sh_rx,		sh_ry,		sw, 		sh_rdy, 			col_shadow, 		0,		CORNER_NONE,				0, NULL, NULL, NULL, false},

			//shadow corner top right
			{sh_ctr, CC_FBDATA_TYPE_SHADOW_BOX, 	sh_ctr_x,	sh_ctr_y, 	sh_cdx, 	sh_cdy-sh_cdy_size_offset, 	col_shadow, 		box_rad,	corner_type & CORNER_TOP_RIGHT,		0, NULL, NULL, NULL, false},
			//clean up inside body
			{sh_ctr, CC_FBDATA_TYPE_SHADOW_BOX, 	sh_ctr_x-sw,	sh_ctr_y-sw+th, sh_cdx, 	sh_cdy-sh_cdy_size_offset+sw, 	col_shadow_clean, 	box_rad,	corner_type & CORNER_TOP_RIGHT,		0, NULL, NULL, NULL, false},

			//main box
			{true, CC_FBDATA_TYPE_BOX,		ix+th,  	iy+th,  	dx-2*th,     	dy-2*th,    			col_body,       	max(0,box_rad-th),	corner_type,				0, NULL, NULL, NULL, false},

			//frame
			{true, CC_FBDATA_TYPE_FRAME,		ix,		iy, 		dx, 		dy, 				col_frame_cur,		box_rad,	corner_type,				th, NULL, NULL, NULL, false}
		};

		for(size_t i =0; i< (sizeof(fbdata) / sizeof(fbdata[0])) ;i++) {
			if ((fbdata[i].fbdata_type == CC_FBDATA_TYPE_FRAME) && !fr_thickness)
				continue;
			v_fbdata.push_back(fbdata[i]);
		}
	}

	//handle frame color for slected/not selected item
	if (fr_thickness) {
		for(size_t j =0; j< v_fbdata.size() ;j++) {
			if (v_fbdata[j].fbdata_type == CC_FBDATA_TYPE_FRAME){
				v_fbdata[j].color = col_frame_cur;
				v_fbdata[j].frame_thickness = th;
			}
		}
	}
	dprintf(DEBUG_DEBUG, "\033[1;32m[CComponentsItem]\t[%s - %d], init and paint item type = %d  [%s]...\033[0m\n", __func__, __LINE__, cc_item_type.id, cc_item_type.name.c_str());
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

//returns true if current item is added to a form
bool CComponentsItem::isAdded()
{
	if (cc_parent)
		return true;

	return false;
}

void CComponentsItem::setXPos(const int& xpos)
{
	CCDraw::setXPos(xpos);
	if (cc_parent)
		cc_xr = cc_parent->getRealXPos() + x;
}

void CComponentsItem::setYPos(const int& ypos)
{
	CCDraw::setYPos(ypos);
	if (cc_parent)
		cc_yr = cc_parent->getRealYPos() + y;
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

void CComponentsItem::setSelected(bool selected, const fb_pixel_t& sel_frame_col,  const fb_pixel_t& frame_col, const fb_pixel_t& sel_body_col, const fb_pixel_t& body_col, const int& frame_w, const int& sel_frame_w)
{
	cc_item_selected = selected;
	fr_thickness = cc_item_selected ? sel_frame_w : frame_w;
	col_body = cc_item_selected ? sel_body_col : body_col;
	col_frame = cc_item_selected ? sel_frame_col : frame_col;
}


