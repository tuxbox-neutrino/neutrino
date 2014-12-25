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

using namespace std;

//-------------------------------------------------------------------------------------------------------
//sub class CComponentsSlider
CComponentsSlider::CComponentsSlider(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const int& current_value,
					const int& min_value,
					const int& max_value,
					CComponentsForm *parent,
					bool has_shadow,
					fb_pixel_t& color_frame,
					fb_pixel_t& color_body,
					fb_pixel_t& color_shadow)
{
	cc_item_type 	= CC_ITEMTYPE_SLIDER;
	corner_rad	= 0;

	x 		= x_pos;
	y 		= y_pos;
	width 		= w;
	height 		= h;

	csl_current_value 	= current_value;
	csl_min_value 		= min_value;
	csl_max_value 		= max_value;

	shadow		= has_shadow;
	col_frame	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;

	csl_body_obj	= NULL;
	csl_slider_obj	= NULL;

	csl_body_icon	= NEUTRINO_ICON_VOLUMEBODY;
	csl_slider_icon	 =NEUTRINO_ICON_VOLUMESLIDER2;

	initCCSlItems();
	initParent(parent);
}

//set current value
void CComponentsSlider::setValuePos(const int& current_value)
{
	csl_current_value = current_value;
	if (csl_slider_obj->isPicPainted())
		csl_slider_obj->hide();
	initCCSlItems();
}

//set current scale values
void CComponentsSlider::setValueScale(const int& min_value, const int& max_value)
{
	csl_min_value = min_value;
	csl_max_value = max_value;
	initCCSlItems();
}

//init slider body object and add to container
void CComponentsSlider::initCCSlBody()
{
	if (!csl_body_icon.empty()){
		if (csl_body_obj == NULL){
			csl_body_obj = new CComponentsPicture(0, 0, 0, 0, csl_body_icon);
			csl_body_obj->doPaintBg(false);
			addCCItem(csl_body_obj);
		}
		else
			csl_body_obj->setPicture(csl_body_icon);
	}
	else{
		printf("[CComponentsSlider]    [%s]  missing or undefinied slider body icon  %s\n", __func__, csl_body_icon.c_str());
		return;
	}

	//get first icon dimensions
	int icon_w = csl_body_obj->getWidth();
	int icon_h = csl_body_obj->getHeight();

	//position of icon default centered
	int icon_x = width/2-icon_w/2;
	int icon_y = height/2-icon_h/2;

	if (csl_body_obj){
		csl_body_obj->setDimensionsAll(icon_x, icon_y, icon_w, icon_h);
	}
}

//init slider caption object and add to container
void CComponentsSlider::initCCSlSlider()
{
	if (!csl_slider_icon.empty()){
		if (csl_slider_obj == NULL){
			csl_slider_obj = new CComponentsPicture(0, 0, 0, 0, csl_slider_icon);
			csl_slider_obj->doPaintBg(false);
			addCCItem(csl_slider_obj);
		}
		else
			csl_slider_obj->setPicture(csl_slider_icon);
	}
	else{
		printf("[CComponentsSlider]    [%s]  missing or undefinied slider icon  %s\n", __func__, csl_slider_icon.c_str());
		return;
	}

	//get first icon dimensions
	int slider_w = csl_slider_obj->getWidth();
	int slider_h = csl_slider_obj->getHeight();

	//position of slider icon
	int slider_x = csl_body_obj->getXPos() - slider_w/2 + csl_body_obj->getWidth() * (abs(csl_min_value) + csl_current_value) / (abs(csl_min_value) + abs(csl_max_value));
	int slider_y = height/2-slider_h/2;

	if (csl_slider_obj)
		csl_slider_obj->setDimensionsAll(slider_x, slider_y, slider_w, slider_h);
}

void CComponentsSlider::initCCSlItems()
{
	initCCSlBody();
	initCCSlSlider();
}

void CComponentsSlider::setSliderIcon(const std::string &icon_name)
{
	csl_slider_icon = icon_name;
	initCCSlSlider();
}

// void CComponentsSlider::paint(bool do_save_bg)
// {
// 	//prepare items before paint
// 	initCCSlItems();
// 
// 	//paint form contents
// 	paintForm(do_save_bg);
// }

