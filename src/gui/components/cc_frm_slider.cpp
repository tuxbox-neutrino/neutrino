/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2013-2014, Thilo Graf 'dbt'

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

#include <global.h>
#include <neutrino.h>
#include "cc_frm_slider.h"
#include <system/debug.h>

using namespace std;

//-------------------------------------------------------------------------------------------------------
//sub class CComponentsSlider
CComponentsSlider::CComponentsSlider(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const int& current_value,
					const int& max_value,
					CComponentsForm *parent,
					int shadow_mode,
					fb_pixel_t& color_frame,
					fb_pixel_t& color_body,
					fb_pixel_t& color_shadow)
{
	cc_item_type.id 	= CC_ITEMTYPE_SLIDER;
	cc_item_type.name 	= "cc_slider";
	corner_rad	= 0;

	x = cc_xr = x_old 	= x_pos;
	y = cc_yr = y_old 	= y_pos;
	width 	= width_old 	= w;
	height 	= height_old 	= h;

	csl_current_value 	= current_value;
	csl_max_value 		= max_value;

	shadow		= shadow_mode;
	col_frame	= color_frame;
	col_body_std	= color_body;
	col_shadow	= color_shadow;

	csl_slider_obj	= NULL;

	setBodyBGImageName(NEUTRINO_ICON_SLIDER_BODY);
	csl_slider_icon	 = frameBuffer->getIconPath(NEUTRINO_ICON_SLIDER_INACTIVE);

	initCCSlItems();
	initParent(parent);
}

CComponentsSlider::~CComponentsSlider()
{

}

//init slider caption object and add to container
void CComponentsSlider::initCCSlSlider()
{
	if (!csl_slider_icon.empty()){
		if (csl_slider_obj == NULL){
			int icon_h, icon_w;
			frameBuffer->getIconSize(csl_slider_icon.c_str(), &icon_w, &icon_h); //current size is required
			csl_slider_obj = new CComponentsPicture(0, 0, icon_w, icon_h, csl_slider_icon, this);
			csl_slider_obj->setColorBody(this->col_body_std); //FIXME: Background handling during current instance of slider object
			csl_slider_obj->doPaintBg(false);
			csl_slider_obj->enableSaveBg();
		}
		else
			csl_slider_obj->setPicture(frameBuffer->getIconPath(csl_slider_icon));
	}
	else{
		dprintf(DEBUG_NORMAL, "[CComponentsSlider]\t[%s - %d]: \033[35m WARNING:\033[0m missing or undefinied slider icon  %s\n", __func__, __LINE__, csl_slider_icon.c_str());
		return;
	}

	if (csl_current_value > csl_max_value)
		dprintf(DEBUG_NORMAL, "[CComponentsSlider]\t[%s - %d]: \033[35m WARNING:\033[0m current slider value [%d] is larger than maximal value of slider scale [%d]\n", __func__, __LINE__, csl_current_value, csl_max_value);

	//get first icon dimensions
	int slider_w = csl_slider_obj->getWidth();

	//position of slider icon
	int slider_space = width - slider_w;
	int slider_x = csl_current_value * slider_space/csl_max_value;
	printf("[CComponentsSlider]    [%s]  csl_current_value = %d slider_x =  %d csl_max_value = %d \n", __func__, csl_current_value, slider_x, csl_max_value);

	if (csl_slider_obj){
		printf("[CComponentsSlider]    [%s]  width =  %d\n", __func__, width);
		csl_slider_obj->setHeight(height, true);
		csl_slider_obj->setXPos(slider_x);
	}
}

void CComponentsSlider::initCCSlItems()
{
	initCCSlSlider();
}

void CComponentsSlider::setSliderIcon(const std::string &icon_name)
{
	csl_slider_icon = icon_name;
	initCCSlSlider();
}

// void CComponentsSlider::paint(const bool &do_save_bg)
// {
// 	//prepare items before paint
// 	initCCSlItems();
// 
// 	//paint form contents
// 	paintForm(do_save_bg);
// }

void CComponentsSlider::paint(const bool &do_save_bg)
{
	//prepare items before paint
	initCCSlItems();

	//paint form contents
	if (!is_painted)
		paintForm(do_save_bg);

	if (is_painted)
		paintMarker();

}


void CComponentsSlider::paintMarker()
{
	if(csl_slider_obj->isPainted())
		csl_slider_obj->hide();
// 			//if (csl_slider_obj->getXPos()>0)
// 			PaintBoxRel(cc_xr, cc_yr, csl_slider_obj->getXPos(), height, COL_GREEN);
	if(!csl_slider_obj->isPainted())
		csl_slider_obj->paint();

}

//set current value
void CComponentsSlider::setValue(const int& current_value, bool enable_paint)
{
	if (csl_current_value == current_value)
		return;

	csl_current_value = current_value;
	initCCSlItems();

	if(enable_paint)
		paintMarker();
}

//set current scale values
void CComponentsSlider::setValueMax(const int& max_value)
{
	csl_max_value = max_value;
	initCCSlItems();
}

