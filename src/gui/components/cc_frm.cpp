/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012, 2013, 2014 Thilo Graf 'dbt'
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
#include "cc_frm.h"
#include <stdlib.h>
#include <algorithm>

using namespace std;

//-------------------------------------------------------------------------------------------------------
//sub class CComponentsForm from CComponentsItem
CComponentsForm::CComponentsForm(	const int x_pos, const int y_pos, const int w, const int h,
					CComponentsForm* parent,
					bool has_shadow,
					fb_pixel_t color_frame,
					fb_pixel_t color_body,
					fb_pixel_t color_shadow)
					:CComponentsItem(parent)
{
	cc_item_type 	= CC_ITEMTYPE_FRM;

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

	shadow_w	= SHADOW_OFFSET;
	corner_rad	= RADIUS_LARGE;
	corner_type 	= CORNER_ALL;
	cc_item_index	= 0;

	v_cc_items.clear();

	append_x_offset = 0;
	append_y_offset = 0;
}

CComponentsForm::~CComponentsForm()
{
	clear();
}


void CComponentsForm::clear()
{
 	if (v_cc_items.empty())
		return;
#ifdef DEBUG_CC
	printf("     [CComponentsForm] %s... delete %d cc-item(s) \n", __func__, (int)v_cc_items.size());
#endif

	for(size_t i=0; i<v_cc_items.size(); i++) {
		if (v_cc_items[i]){
#ifdef DEBUG_CC
	printf("     [CComponentsForm] %s... delete form cc-item %d of %d (type=%d)\n", __func__, (int)i+1, (int)v_cc_items.size(), v_cc_items[i]->getItemType());
#endif
			delete v_cc_items[i];
			v_cc_items[i] = NULL;
		}
	}
	v_cc_items.clear();
}


void CComponentsForm::addCCItem(CComponentsItem* cc_Item)
{
	if (cc_Item){
#ifdef DEBUG_CC
		printf("	[CComponentsForm]  %s-%d try to add cc_Item [type %d] to form [current index=%d] \n", __func__, __LINE__, cc_Item->getItemType(), cc_item_index);
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
		printf("			   %s-%d parent index = %d, assigned index ======> %d\n", __func__, __LINE__, cc_item_index, new_index);
#endif
	}
#ifdef DEBUG_CC
	else
		printf("	[CComponentsForm]  %s-%d tried to add an empty or invalide cc_item !!!\n", __func__, __LINE__);
#endif
}

void CComponentsForm::addCCItem(const std::vector<CComponentsItem*> &cc_Items)
{
	for (size_t i= 0; i< cc_Items.size(); i++)
		addCCItem(cc_Items[i]);
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
			CComponentsForm * old_parent = old_Item->getParent();
			new_cc_Item->setParent(old_parent);
			new_cc_Item->setIndex(old_parent->getIndex());
			delete old_Item;
			old_Item = NULL;
			v_cc_items[cc_item_id] = new_cc_Item;
		}
	}
#ifdef DEBUG_CC
	else
		printf("[CComponentsForm]  %s replace cc_Item not possible, v_cc_items is empty\n", __func__);
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
		printf("[CComponentsForm]  %s insert cc_Item not possible, v_cc_items is empty, cc_Item added\n", __func__);
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
		printf("[CComponentsForm]  %s removing cc_Item not possible, v_cc_items is empty...\n", __func__);
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

		//check item for corrupt position, skip current item if found problems
		//TODO: need a solution with possibility for scrolling
		if (ypos > height || xpos > width){
			printf("[CComponentsForm] %s: [form: %d] [item-index %d] [type=%d] WARNING: item position is out of form size:\ndefinied x=%d, defined width=%d \ndefinied y=%d, defined height=%d \n",
				__func__, cc_item_index, cc_item->getIndex(), cc_item->getItemType(), xpos, width, ypos, height);
			if (this->cc_item_type != CC_ITEMTYPE_FRM_CHAIN)
				continue;
		}

		//set required x-position to item:
		//append vertical
		if (xpos == CC_APPEND){
			auto_x += append_x_offset;
			cc_item->setRealXPos(auto_x + xpos);
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
			auto_y += append_y_offset;
			cc_item->setRealYPos(auto_y + ypos);
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
		int right_frm = (cc_parent ? cc_xr : x) + width - 2*fr_thickness;
		int right_item = cc_item->getRealXPos() + w_item;
		int w_diff = right_item - right_frm;
		int new_w = w_item - w_diff;
		//avoid of width error due to odd values (1 line only)
		right_item -= (new_w%2);
		w_item -= (new_w%2);
		if (right_item > right_frm){
			printf("[CComponentsForm] %s: [form: %d] [item-index %d] [type=%d] width is too large, definied width=%d, possible width=%d \n",
				__func__, cc_item_index, cc_item->getIndex(), cc_item->getItemType(), w_item, new_w);
			cc_item->setWidth(new_w);
		}

		//check height and adapt if required
		int bottom_frm = (cc_parent ? cc_yr : y) + height - 2*fr_thickness;
		int bottom_item = cc_item->getRealYPos() + h_item;
		int h_diff = bottom_item - bottom_frm;
		int new_h = h_item - h_diff;
		//avoid of height error due to odd values (1 line only)
		bottom_item -= (new_h%2);
		h_item -= (new_h%2);
		if (bottom_item > bottom_frm){
			printf("[CComponentsForm] %s: [form: %d] [item-index %d] [type=%d] height is too large, definied height=%d, possible height=%d \n",
			       __func__, cc_item_index, cc_item->getIndex(), cc_item->getItemType(), h_item, new_h);
			cc_item->setHeight(new_h);
		}

		//get current visibility mode from item, me must hold it and restore after paint
		bool item_visible = cc_item->paintAllowed();
		//set visibility mode
		if (!this->cc_allow_paint)
			cc_item->allowPaint(false);

		//finally paint current item
		cc_item->paint(CC_SAVE_SCREEN_NO);

		//restore defined old visibility mode of item after paint
		cc_item->allowPaint(item_visible);
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
