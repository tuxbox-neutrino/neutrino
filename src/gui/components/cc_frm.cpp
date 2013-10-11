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
#include "cc_frm.h"
#include <stdlib.h>
#include <algorithm>

using namespace std;

//-------------------------------------------------------------------------------------------------------
//sub class CComponentsForm from CComponentsItem
CComponentsForm::CComponentsForm()
{
	//CComponentsForm
	initVarForm();
}

CComponentsForm::CComponentsForm(const int x_pos, const int y_pos, const int w, const int h, bool has_shadow,
				fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	//CComponentsForm
	initVarForm();

	//CComponents
	x		= x_pos;
	y 		= y_pos;
	cc_xr 		= x;
	cc_yr 		= y;
	width 		= w;
	height	 	= h;

	shadow		= has_shadow;
	col_frame 	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;
}

CComponentsForm::~CComponentsForm()
{
	cleanCCForm();
}

void CComponentsForm::cleanCCForm()
{
#ifdef DEBUG_CC
	printf("[CComponentsForm]   [%s - %d] clean up...\n", __FUNCTION__, __LINE__);
#endif

	clearCCItems();
	clearSavedScreen();
	clear();
}



void CComponentsForm::clearCCItems()
{
 	if (v_cc_items.empty())
		return;
#ifdef DEBUG_CC
	printf("     [CComponentsForm] %s... delete %d cc-item(s) \n", __FUNCTION__, v_cc_items.size());
#endif

	for(size_t i=0; i<v_cc_items.size(); i++) {
		if (v_cc_items[i]){
#ifdef DEBUG_CC
	printf("     [CComponentsForm] %s... delete form cc-item %d of %d (type=%d)\n", __FUNCTION__, i+1, v_cc_items.size(), v_cc_items[i]->getItemType());
#endif
			delete v_cc_items[i];
			v_cc_items[i] = NULL;
		}
	}
	v_cc_items.clear();
}


void CComponentsForm::initVarForm()
{
	//CComponentsItem
	initVarItem();


	//simple default dimensions
	x 		= 0;
	y 		= 0;
	width 		= 150;
	height	 	= 150;
	shadow		= CC_SHADOW_OFF;
	shadow_w	= SHADOW_OFFSET;
	col_frame 	= COL_MENUCONTENT_PLUS_6;
	col_body	= COL_MENUCONTENT_PLUS_0;
	col_shadow	= COL_MENUCONTENTDARK_PLUS_0;
	corner_rad	= RADIUS_LARGE;
	corner_type 	= CORNER_ALL;
	cc_item_index	= 0;

	//CComponentsForm
	v_cc_items.clear();
	cc_item_type 	= CC_ITEMTYPE_FRM;
	append_h_offset = 0;
	append_v_offset = 0;
}

void CComponentsForm::addCCItem(CComponentsItem* cc_Item)
{
	if (cc_Item){
#ifdef DEBUG_CC
		printf("	[CComponentsForm]  %s-%d try to add cc_Item [type %d] to form [current index=%d] \n", __FUNCTION__, __LINE__, cc_Item->getItemType(), cc_item_index);
#endif
		cc_Item->setParent(this);
		v_cc_items.push_back(cc_Item);
#ifdef DEBUG_CC
		printf("			   added cc_Item [type %d] to form [current index=%d] \n", cc_Item->getItemType(), cc_item_index);
#endif

		//assign item index
		int new_index = genIndex();
		cc_Item->setIndex(new_index);
#ifdef DEBUG_CC
		printf("			   %s-%d parent index = %d, assigned index ======> %d\n", __FUNCTION__, __LINE__, cc_item_index, new_index);
#endif
	}
#ifdef DEBUG_CC
	else
		printf("	[CComponentsForm]  %s-%d tried to add an empty or invalide cc_item !!!\n", __FUNCTION__, __LINE__);
#endif
}

int CComponentsForm::getCCItemId(CComponentsItem* cc_Item)
{
	if (cc_Item){
		for (size_t i= 0; i< v_cc_items.size(); i++)
			if (v_cc_items[i] == cc_Item)
				return i;
	}
	return -1;
}

bool CComponentsForm::isAdded(CComponentsItem* cc_item)
{
	bool ret = false;
	if (getCCItemId(cc_item) != -1)
		ret = true;
	return ret;
}

int CComponentsForm::genIndex()
{
	int count = v_cc_items.size();
	char buf[64];
	snprintf(buf, sizeof(buf), "%d%d", cc_item_index, count);
	buf[63] = '\0';
	int ret = atoi(buf);
	return ret;
}

CComponentsItem* CComponentsForm::getCCItem(const uint& cc_item_id)
{
	if (v_cc_items[cc_item_id])
		return v_cc_items[cc_item_id];
	return NULL;
}

void CComponentsForm::replaceCCItem(const uint& cc_item_id, CComponentsItem* new_cc_Item)
{
	if (!v_cc_items.empty()){
		CComponentsItem* old_Item = v_cc_items[cc_item_id];
		if (old_Item){
			CComponentsItem * old_parent = old_Item->getParent();
			new_cc_Item->setParent(old_parent);
			new_cc_Item->setIndex(old_parent->getIndex());
			delete old_Item;
			old_Item = NULL;
			v_cc_items[cc_item_id] = new_cc_Item;
		}
	}
#ifdef DEBUG_CC
	else
		printf("[CComponentsForm]  %s replace cc_Item not possible, v_cc_items is empty\n", __FUNCTION__);
#endif

}

void CComponentsForm::replaceCCItem(CComponentsItem* old_cc_Item, CComponentsItem* new_cc_Item)
{
	replaceCCItem(getCCItemId(old_cc_Item), new_cc_Item);
}

void CComponentsForm::insertCCItem(const uint& cc_item_id, CComponentsItem* cc_Item)
{

	if (cc_Item == NULL){
#ifdef DEBUG_CC
		printf("[CComponentsForm]  %s parameter: cc_Item = %p...\n", __func__, cc_Item);
#endif
		return;
	}

	if (v_cc_items.empty()){
		addCCItem(cc_Item);
#ifdef DEBUG_CC
		printf("[CComponentsForm]  %s insert cc_Item not possible, v_cc_items is empty, cc_Item added\n", __FUNCTION__);
#endif
	}else{
		v_cc_items.insert(v_cc_items.begin()+cc_item_id, cc_Item);
		cc_Item->setParent(this);
		//assign item index
		int index = genIndex();
		cc_Item->setIndex(index);
	}
}

void CComponentsForm::removeCCItem(const uint& cc_item_id)
{
	if (!v_cc_items.empty()){
		if (v_cc_items[cc_item_id]) {
			delete v_cc_items[cc_item_id];
			v_cc_items[cc_item_id] = NULL;
			v_cc_items.erase(v_cc_items.begin()+cc_item_id);
		}
	}
#ifdef DEBUG_CC
	else
		printf("[CComponentsForm]  %s removing cc_Item not possible, v_cc_items is empty...\n", __FUNCTION__);
#endif
}

void CComponentsForm::removeCCItem(CComponentsItem* cc_Item)
{
	uint id = getCCItemId(cc_Item);	
	removeCCItem(id);
}

void CComponentsForm::exchangeCCItem(const uint& cc_item_id_a, const uint& cc_item_id_b)
{
	if (!v_cc_items.empty())
		swap(v_cc_items[cc_item_id_a], v_cc_items[cc_item_id_b]);
}

void CComponentsForm::exchangeCCItem(CComponentsItem* item_a, CComponentsItem* item_b)
{
	exchangeCCItem(getCCItemId(item_a), getCCItemId(item_b));
}

void CComponentsForm::paintForm(bool do_save_bg)
{
	//paint body
	paintInit(do_save_bg);

	//paint
	paintCCItems();
}

void CComponentsForm::paint(bool do_save_bg)
{
	paintForm(do_save_bg);
}


void CComponentsForm::paintCCItems()
{
	size_t items_count 	= v_cc_items.size();

	//using of real x/y values to paint items if this text object is bound in a parent form
	int this_x = x, auto_x = x, this_y = y, auto_y = y;
	if (cc_parent){
		this_x = auto_x = cc_xr;
		this_y = auto_y = cc_yr;
	}

	for(size_t i=0; i<items_count; i++){
		//assign item object
		CComponentsItem *cc_item = v_cc_items[i];

		//get current dimension of item
		int w_item = cc_item->getWidth();
		int h_item = cc_item->getHeight();

		//get current position of item
		int xpos = cc_item->getXPos();
		int ypos = cc_item->getYPos();

		//set required x-position to item:
		//append vertical
		if (xpos == CC_APPEND){
			auto_x += append_h_offset;
			cc_item->setRealXPos(auto_x + xpos + 1);
			auto_x += w_item;
		}
		//positionize vertical centered
		else if (xpos == CC_CENTERED){
			auto_x =  width/2 - cc_item->getWidth()/2;
			cc_item->setRealXPos(this_x + auto_x);
		}
		else{
			cc_item->setRealXPos(this_x + xpos);
			auto_x = (cc_item->getRealXPos() + w_item);
		}

		//set required y-position to item
		//append hor
		if (ypos == CC_APPEND){
			auto_y += append_v_offset;
			cc_item->setRealYPos(auto_y + ypos + 1);
			auto_y += h_item;
		}
		//positionize hor centered
		else if (ypos == CC_CENTERED){
			auto_y =  height/2 - cc_item->getHeight()/2;
			cc_item->setRealYPos(this_y + auto_y);
		}
		else{
			cc_item->setRealYPos(this_y + ypos);
			auto_y = (cc_item->getRealYPos() + h_item);
		}


		//These steps check whether the element can be painted into the container.
		//Is it too wide or too high, it will be shortened and displayed in the log.
		//This should be avoid!
		//checkwidth and adapt if required
		int right_frm = (cc_parent ? cc_xr : x) + width - fr_thickness;
		int right_item = cc_item->getRealXPos() + w_item;
		int w_diff = right_item - right_frm;
		int new_w = w_item - w_diff;
		if (right_item > right_frm){
			printf("[CComponentsForm] %s: [form: %d] [item-index %d] [type=%d] width is too large, definied width=%d, possible width=%d \n",
				__FUNCTION__, cc_item_index, cc_item->getIndex(), cc_item->getItemType(), w_item, new_w);
			cc_item->setWidth(new_w);
		}

		//check height and adapt if required
		int bottom_frm = (cc_parent ? cc_yr : y) + height - fr_thickness;
		int bottom_item = cc_item->getRealYPos() + h_item;
		int h_diff = bottom_item - bottom_frm;
		int new_h = h_item - h_diff;
		if (bottom_item > bottom_frm){
			printf("[CComponentsForm] %s: [form: %d] [item-index %d] [type=%d] height is too large, definied height=%d, possible height=%d \n",
			       __FUNCTION__, cc_item_index, cc_item->getIndex(), cc_item->getItemType(), h_item, new_h);
			cc_item->setHeight(new_h);
		}

		//finally paint current item
		cc_item->paint(CC_SAVE_SCREEN_NO);
	}
}

void CComponentsForm::hide(bool no_restore)
{
	// hack: ensure hiding of minitv during hide of forms and inherited classes,
	// because the handling of minitv items are different to other item types
	// and need an explizit call of hide()
	for(size_t i=0; i<v_cc_items.size(); i++) {
		if (v_cc_items[i]->getItemType() == CC_ITEMTYPE_PIP){
			v_cc_items[i]->hide();
			break;
		}
	}

	//hide body
	hideCCItem(no_restore);
}
