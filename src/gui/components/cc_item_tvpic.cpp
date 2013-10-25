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
	Library General Public License for more details.

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
#include "cc_item_tvpic.h"

#include <video.h>

extern cVideo * videoDecoder;

using namespace std;

//-------------------------------------------------------------------------------------------------------
//sub class CComponentsPIP from CComponentsItem
CComponentsPIP::CComponentsPIP(	const int x_pos, const int y_pos, const int percent, bool has_shadow)
{
	//CComponents, CComponentsItem
	initVarItem();
	cc_item_type 	= CC_ITEMTYPE_PIP;

	//CComponentsPIP
	screen_w = frameBuffer->getScreenWidth(true);
	screen_h = frameBuffer->getScreenHeight(true);
	pic_name = DATADIR;
	pic_name += "/neutrino/icons/start.jpg";

	//CComponents
	x 		= x_pos;
	y 		= y_pos;
	width 		= percent*screen_w/100;
	height	 	= percent*screen_h/100;
	shadow		= has_shadow;
	shadow_w	= SHADOW_OFFSET;
	col_frame 	= COL_BACKGROUND;
	col_body	= COL_BACKGROUND;
	col_shadow	= COL_MENUCONTENTDARK_PLUS_0;
}

CComponentsPIP::~CComponentsPIP()
{
 	hide();
 	videoDecoder->Pig(-1, -1, -1, -1);
 	clearSavedScreen();
 	clear();
}

void CComponentsPIP::paint(bool do_save_bg)
{
	//NOTE: real values are reqiured, if we paint not bound items or an own render methodes
	int pig_x = (cc_parent ? cc_xr : x) + fr_thickness;
	int pig_y = (cc_parent ? cc_yr : y) + fr_thickness;
	int pig_w = width-2*fr_thickness;
	int pig_h = height-2*fr_thickness;
	
	paintInit(do_save_bg);
	
	if (videoDecoder->getAspectRatio() == 1){
		int tmpw = pig_w;
		pig_w -= pig_w*25/100;
		pig_x += tmpw/2-pig_w/2; 
	}
	
	if(CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_tv){
		videoDecoder->Pig(pig_x, pig_y, pig_w, pig_h, screen_w, screen_h);
	}
	else{ //paint an alternate image if no tv mode available
		CComponentsPicture pic = CComponentsPicture (pig_x, pig_y, pig_w, pig_h, pic_name, CC_ALIGN_HOR_CENTER | CC_ALIGN_VER_CENTER);
		pic.paint(CC_SAVE_SCREEN_NO);
	}
	
}


void CComponentsPIP::hide(bool no_restore)
{
	videoDecoder->Pig(-1, -1, -1, -1);
	hideCCItem(no_restore);
}
