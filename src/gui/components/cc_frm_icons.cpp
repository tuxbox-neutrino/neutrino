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

	chn_direction	= CC_DIR_X;

	append_y_offset = 2;

	initChainItems();
	initParent(parent);
}

void CComponentsIconForm::addIcon(const std::string& icon_name)
{
	//create new cc-picture item object
	CComponentsPicture *ccp = new CComponentsPicture(chn_direction == CC_DIR_X ? CC_APPEND : CC_CENTERED,
							chn_direction == CC_DIR_Y ? CC_APPEND : CC_CENTERED,
							0, 0,
							icon_name,
							this);
	ccp->doPaintBg(false);

	initChainItems();
}

void CComponentsIconForm::addIcon(std::vector<std::string> icon_name)
{
	for (size_t i= 0; i< icon_name.size(); i++)
		addIcon(icon_name[i]);
}

void CComponentsIconForm::insertIcon(const uint& icon_id, const std::string& icon_name)
{
	//create new cc-picture item object
	CComponentsPicture *ccp = new CComponentsPicture(chn_direction == CC_DIR_X ? CC_APPEND : CC_CENTERED,
							chn_direction == CC_DIR_Y ? CC_APPEND : CC_CENTERED,
							0, 0,
							icon_name);
	ccp->doPaintBg(false);

	insertCCItem(icon_id, ccp);
	initChainItems();
}

void CComponentsIconForm::removeIcon(const uint& icon_id)
{
	removeCCItem(icon_id);
	initChainItems();
}

//For existing instances it's recommended
//to remove old items before add new icons, otherwise icons will be appended.
void CComponentsIconForm::removeAllIcons()//TODO
{
	clear();
	v_icons.clear();
	initChainItems();
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

