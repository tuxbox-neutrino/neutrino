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
#include "cc_frm_icons.h"

using namespace std;

//sub class CComponentsIconForm inherit from CComponentsForm
CComponentsIconForm::CComponentsIconForm(CComponentsForm* parent)
{
	initVarIconForm(1, 1, 0, 0, vector<string>(), parent);
}

CComponentsIconForm::CComponentsIconForm(	const int &x_pos, const int &y_pos, const int &w, const int &h,
						const std::vector<std::string> &v_icon_names,
						CComponentsForm* parent,
						bool has_shadow,
						fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	initVarIconForm(x_pos, y_pos, w, h, v_icon_names, parent, has_shadow, color_frame, color_body, color_shadow);
}

void CComponentsIconForm::initVarIconForm(	const int &x_pos, const int &y_pos, const int &w, const int &h,
						const std::vector<std::string> &v_icon_names,
						CComponentsForm* parent,
						bool has_shadow,
						fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	cc_item_type 	= CC_ITEMTYPE_FRM_ICONFORM;

	x 		= x_pos;
	y 		= y_pos;
	width 		= w;
	height 		= h;
	v_icons		= v_icon_names;
	shadow		= has_shadow;
	col_frame	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;

	ccif_offset 	= 2;
	ccif_icon_align = CC_ICONS_FRM_ALIGN_LEFT;
	initParent(parent);
}

void CComponentsIconForm::addIcon(const std::string& icon_name)
{
	v_icons.push_back(icon_name);
}

void CComponentsIconForm::addIcon(std::vector<std::string> icon_name)
{
	for (size_t i= 0; i< icon_name.size(); i++)
		v_icons.push_back(icon_name[i]);
}

void CComponentsIconForm::insertIcon(const uint& icon_id, const std::string& icon_name)
{
	v_icons.insert(v_icons.begin()+icon_id, icon_name);
}

void CComponentsIconForm::removeIcon(const uint& icon_id)
{
	v_icons.erase(v_icons.begin()+icon_id);
}

void CComponentsIconForm::removeIcon(const std::string& icon_name)
{
	int id = getIconId(icon_name);
	removeIcon(id);
}

int CComponentsIconForm::getIconId(const std::string& icon_name)
{
	for (size_t i= 0; i< v_icons.size(); i++)
		if (v_icons[i] == icon_name)
			return i;
	return -1;
}

//For existing instances it's recommended
//to remove old items before add new icons, otherwise icons will be appended.
void CComponentsIconForm::removeAllIcons()
{
	clear();
	v_icons.clear();
}

//get maximal form height depends of biggest icon height, but don't touch defined form height
void CComponentsIconForm::initMaxHeight(int *pheight)
{
	for (size_t i= 0; i< v_icons.size(); i++){
		int dummy, htmp;
		frameBuffer->getIconSize(v_icons[i].c_str(), &dummy, &htmp);
		*pheight = max(htmp, height)/*+2*fr_thickness*/;
	}
}

void CComponentsIconForm::initCCIcons()
{
	int ccp_y = 0;
	int ccp_h = 0;
	int ccp_w = 0;
	//calculate start pos of first icon
	int ccp_x = 0 + fr_thickness; //CC_ICONS_FRM_ALIGN_LEFT;
	
	if (ccif_icon_align == CC_ICONS_FRM_ALIGN_RIGHT)
		ccp_x += (width - fr_thickness);
	
	//get width of first icon
	frameBuffer->getIconSize(v_icons[0].c_str(), &ccp_w, &ccp_h);
		
	//get maximal form height
 	int h = 0;
	initMaxHeight(&h);

	//set xpos of first icon with right alignment, icon must positionized on the right border reduced with icon width
	if (ccif_icon_align == CC_ICONS_FRM_ALIGN_RIGHT)
		ccp_x -= ccp_w;

	//init and add item objects
	size_t i_cnt = 	v_icons.size();	//icon count
	
	for (size_t i= 0; i< i_cnt; i++){
		//create new cc-picture item object
		CComponentsPicture *ccp = new CComponentsPicture(ccp_x, ccp_y, ccp_w, h, v_icons[i]);
		ccp->setPictureAlign(CC_ALIGN_HOR_CENTER | CC_ALIGN_VER_CENTER);
 		ccp->doPaintBg(false);
		//add item to form
		addCCItem(ccp);

		//reset current width for next object
		ccp_w = 0;
		//get next icon size if available
 		size_t next_i = i+1;
		if (next_i != i_cnt)
			frameBuffer->getIconSize(v_icons[next_i].c_str(), &ccp_w, &ccp_h);

		//set next icon position
		int tmp_offset = ( i<i_cnt ? ccif_offset : 0 );

		if (ccif_icon_align == CC_ICONS_FRM_ALIGN_LEFT)
			ccp_x += (ccp->getWidth() + tmp_offset);
		if (ccif_icon_align == CC_ICONS_FRM_ALIGN_RIGHT)
			ccp_x -= (ccp_w + tmp_offset);
	}

	//calculate width and height of form
	int w_tmp = 0;
	int h_tmp = 0;
	for (size_t i= 0; i< i_cnt; i++){
		w_tmp += v_cc_items[i]->getWidth()+ccif_offset+fr_thickness;
		h_tmp = max(h_tmp, v_cc_items[i]->getHeight()+2*fr_thickness);

	}
	width = max(w_tmp, width);
	height = max(h_tmp, height);
}

void CComponentsIconForm::paint(bool do_save_bg)
{
	//init and add icons
	initCCIcons();
	
	//paint form contents
	paintForm(do_save_bg);
}
