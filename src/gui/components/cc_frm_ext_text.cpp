/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2013, Thilo Graf 'dbt'

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

#define DEF_HEIGHT 27
#define DEF_LABEL_WIDTH_PERCENT 30
#define DEF_WIDTH 300

using namespace std;

CComponentsExtTextForm::CComponentsExtTextForm()
{
	initVarExtTextForm();
	initCCTextItems();
}

CComponentsExtTextForm::CComponentsExtTextForm(	const int x_pos, const int y_pos, const int w, const int h,
						const std::string& label_text, const std::string& text,
						bool has_shadow,
						fb_pixel_t label_color,
						fb_pixel_t text_color,
						fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	initVarExtTextForm(x_pos, y_pos, w, h, has_shadow, label_color, text_color, color_frame, color_body, color_shadow);
	ccx_label_text 	= label_text;
	ccx_text 	= text;
	initCCTextItems();
}

CComponentsExtTextForm::CComponentsExtTextForm( const int x_pos, const int y_pos, const int w, const int h,
						const neutrino_locale_t& locale_label_text, const neutrino_locale_t& locale_text,
						bool has_shadow,
						fb_pixel_t label_color,
						fb_pixel_t text_color,
						fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	initVarExtTextForm(x_pos, y_pos, w, h, has_shadow, label_color, text_color, color_frame, color_body, color_shadow);
	ccx_label_text 	= g_Locale->getText(locale_label_text);
	ccx_text 	= g_Locale->getText(locale_text);

	initCCTextItems();
}

void CComponentsExtTextForm::initVarExtTextForm(const int x_pos, const int y_pos, const int w, const int h,
						bool has_shadow,
						fb_pixel_t label_color,
						fb_pixel_t text_color,
						fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	initVarForm();
	cc_item_type 	= CC_ITEMTYPE_FRM_EXT_TEXT;
	x = x_pos;
	y = y_pos;
	
	width = w;
	//init ccx_label_width and ccx_text_width
	//default ccx_label_width = 30% of form width
	ccx_percent_label_w = DEF_LABEL_WIDTH_PERCENT;
	ccx_label_width = ccx_percent_label_w * width/100;
	ccx_text_width	= width-ccx_label_width;
	
	height = h;
	
	ccx_label_text 	= "";
	ccx_text 	= "";
	shadow 		= has_shadow;
	ccx_label_color	= label_color;
	ccx_text_color	= text_color;
	col_frame	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;
	ccx_label_obj	= NULL;
	ccx_text_obj	= NULL;
	corner_type	= 0;
	int dx = 0, dy 	= DEF_HEIGHT;
	ccx_font 	= *(CNeutrinoFonts::getInstance()->getDynFont(dx, dy));
	ccx_label_align = ccx_text_align = CTextBox::NO_AUTO_LINEBREAK;
	

}

void CComponentsExtTextForm::initLabel()
{
	//initialize label object
	if (ccx_label_obj == NULL){
		ccx_label_obj = new CComponentsLabel();
		ccx_label_obj->doPaintBg(false);
	}	

	//add label object
	if (!ccx_label_obj->isAdded())
		addCCItem(ccx_label_obj);

	//set properties
	if (ccx_label_obj){
		ccx_label_width = (ccx_percent_label_w * width/100);
		ccx_label_obj->setText(ccx_label_text, ccx_label_align, ccx_font);
		ccx_label_obj->setTextColor(ccx_label_color);
		ccx_label_obj->setDimensionsAll(fr_thickness, 0, ccx_label_width-fr_thickness, height-2*fr_thickness);
		ccx_label_obj->setCorner(this->corner_rad, CORNER_LEFT);
	}
}

void CComponentsExtTextForm::initText()
{
	//initialize text object
	if (ccx_text_obj == NULL){
		ccx_text_obj = new CComponentsLabel();
		ccx_text_obj->doPaintBg(false);
	}

	//add text object
	if (!ccx_text_obj->isAdded())
		addCCItem(ccx_text_obj);

	//set properties
	if (ccx_text_obj){
		ccx_text_width = width-ccx_label_obj->getWidth();
		ccx_text_obj->setText(ccx_text, ccx_text_align, ccx_font);
		ccx_text_obj->setTextColor(ccx_text_color);
		ccx_text_obj->setDimensionsAll(CC_APPEND, 0, ccx_text_width-2*fr_thickness, height-2*fr_thickness);
		ccx_text_obj->setCorner(this->corner_rad, CORNER_RIGHT);
	}
}


void CComponentsExtTextForm::setLabelAndText(const std::string& label_text, const std::string& text, Font* font_text)
{
	ccx_label_text 	= label_text;
	ccx_text 	= text;
	if (font_text)
		ccx_font	= font_text;
	initCCTextItems();
}


void CComponentsExtTextForm::setLabelAndText(const neutrino_locale_t& locale_label_text, const neutrino_locale_t& locale_text, Font* font_text)
{
	setLabelAndText(g_Locale->getText(locale_label_text), g_Locale->getText(locale_text), font_text);
}

void CComponentsExtTextForm::setLabelAndTexts(const string_ext_txt_t& texts)
{
	setLabelAndText(texts.label_text, texts.text, texts.font);
}

void CComponentsExtTextForm::setLabelAndTexts(const locale_ext_txt_t& locale_texts)
{
	setLabelAndText(g_Locale->getText(locale_texts.label_text), g_Locale->getText(locale_texts.text), locale_texts.font);
}

void CComponentsExtTextForm::setLabelAndTextFont(Font* font)
{
	if (font == NULL)
		return;
	setLabelAndText(ccx_label_text, ccx_text, font);
}

void CComponentsExtTextForm::setTextModes(const int& label_mode, const int& text_mode)
{
	ccx_label_align = label_mode;
	ccx_text_align = text_mode;
	initCCTextItems();
}

void CComponentsExtTextForm::setLabelAndTextColor(const fb_pixel_t label_color , const fb_pixel_t text_color)
{
	ccx_label_color = label_color;
	ccx_text_color 	= text_color;
	initCCTextItems();
}

void CComponentsExtTextForm::initCCTextItems()
{
	initLabel();
	initText();
}

void CComponentsExtTextForm::setLabelWidthPercent(const uint8_t& percent_val)
{
	ccx_percent_label_w = (int)percent_val;
	initCCTextItems();
}

void CComponentsExtTextForm::paint(bool do_save_bg)
{
	//prepare items before paint
	initCCTextItems();

	//paint form contents
	paintForm(do_save_bg);
}
