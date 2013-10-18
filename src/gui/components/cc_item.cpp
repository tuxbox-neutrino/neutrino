/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012, 2013, Thilo Graf 'dbt'
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

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include "cc.h"

using namespace std;

//abstract sub class CComponentsItem from CComponents
CComponentsItem::CComponentsItem()
{
	//CComponentsItem
	initVarItem();
	cc_item_type = CC_ITEMTYPE_BASE;
}

// 	 y
// 	x+------f-r-a-m-e-------+
// 	 |			|
//     height	  body		|
// 	 |			|
// 	 +--------width---------+

void CComponentsItem::initVarItem()
{
	//CComponents
	initVarBasic();
	cc_item_index 		= CC_NO_INDEX;
	cc_item_enabled 	= true;
	cc_item_selected 	= false;
	cc_parent 		= NULL;
}

// Paint container background in cc-items with shadow, background and frame.
// This member must be called first in all paint() members before paint other items into the container.
// If backround is not required, it's possible to override this with variable paint_bg=false, use doPaintBg(true/false) to set this!
void CComponentsItem::paintInit(bool do_save_bg)
{
	clear();

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

	//if item is bound on a parent form, we must use real x/y values and from parent form as reference
	int ix = x, iy = y;
	if (cc_parent){
		ix = cc_xr + cc_parent->getFrameThickness();
		iy = cc_yr + cc_parent->getFrameThickness();
	}
	
	comp_fbdata_t fbdata[] =
	{
		{CC_FBDATA_TYPE_BGSCREEN,	ix,	iy, 	width+sw, 	height+sw, 	0, 		0, 		0, 	NULL,	NULL},
		{CC_FBDATA_TYPE_SHADOW_BOX, 	ix+sw,	iy+sw, 	width, 		height, 	col_shadow, 	corner_rad, 	0, 	NULL,	NULL},//shadow
		{CC_FBDATA_TYPE_FRAME,		ix,	iy, 	width, 		height, 	col_frame_cur, 	corner_rad, 	th, 	NULL,	NULL},//frame
		{CC_FBDATA_TYPE_BOX,		ix+th,  iy+th,  width-2*th,     height-2*th,    col_body,       rad, 		0, 	NULL, 	NULL},//body
	};

	for(size_t i =0; i< (sizeof(fbdata) / sizeof(fbdata[0])) ;i++) {
		if (((fbdata[i].fbdata_type == CC_FBDATA_TYPE_SHADOW_BOX) && !shadow) ||
		    ((fbdata[i].fbdata_type == CC_FBDATA_TYPE_FRAME) && !fr_thickness))
			continue;
		v_fbdata.push_back(fbdata[i]);
	}
#ifdef DEBUG_CC
	printf("[CComponentsItem] %s:\ncc_item_type: %d\ncc_item_index = %d\nheight = %d\nwidth = %d\n", __FUNCTION__, cc_item_type,  cc_item_index, height, width);
#endif
	paintFbItems(do_save_bg);
}

//restore last saved screen behind form box,
//Do use parameter 'no restore' to override temporarly the restore funtionality.
//This could help to avoid ugly flicker efffects if it is necessary e.g. on often repaints, without changed contents.
void CComponentsItem::hideCCItem(bool no_restore)
{
	is_painted = false;

	if (saved_screen.pixbuf) {
		frameBuffer->waitForIdle("CComponentsItem::hideCCItem()");
		frameBuffer->RestoreScreen(saved_screen.x, saved_screen.y, saved_screen.dx, saved_screen.dy, saved_screen.pixbuf);
		if (no_restore) {
			delete[] saved_screen.pixbuf;
			saved_screen.pixbuf = NULL;
			firstPaint = true;
		}
	}
}

void CComponentsItem::hide(bool no_restore)
{
	hideCCItem(no_restore);
}

//synchronize colors for forms
//This is usefull if the system colors are changed during runtime
//so you can ensure correct applied system colors in relevant objects with unchanged instances.
void CComponentsItem::syncSysColors()
{
	col_body 	= COL_MENUCONTENT_PLUS_0;
	col_shadow 	= COL_MENUCONTENTDARK_PLUS_0;
	col_frame 	= COL_MENUCONTENT_PLUS_6;
}

//returns current item element type, if no available, return -1 as unknown type
int CComponentsItem::getItemType()
{
	for(int i =0; i< (CC_ITEMTYPES) ;i++){
		if (i == cc_item_type)
			return i;
	}
#ifdef DEBUG_CC
	printf("[CComponentsItem] %s: unknown item type requested...\n", __FUNCTION__);
#endif
	return -1;
}

//returns true if current item is added to a form
bool CComponentsItem::isAdded(CComponentsItem *parent_frm)
{
	if (cc_parent)
		return true;

	return false;
}
