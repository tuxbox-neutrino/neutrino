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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include "cc_frm_footer.h"

using namespace std;

//-------------------------------------------------------------------------------------------------------
//sub class CComponentsFooter inherit from CComponentsHeader
CComponentsFooter::CComponentsFooter(CComponentsForm* parent)
{
	//CComponentsFooter
	initVarFooter(1, 1, 0, 0, 0, parent);
}

CComponentsFooter::CComponentsFooter(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const int& buttons,
					CComponentsForm* parent,
					bool has_shadow,
					fb_pixel_t color_frame,
					fb_pixel_t color_body,
					fb_pixel_t color_shadow )
{
	//CComponentsFooter
	initVarFooter(x_pos, y_pos, w, h, buttons, parent, has_shadow, color_frame, color_body, color_shadow);
}

void CComponentsFooter::initVarFooter(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const int& buttons,
					CComponentsForm* parent,
					bool has_shadow,
					fb_pixel_t color_frame,
					fb_pixel_t color_body,
					fb_pixel_t color_shadow )
{
	cc_item_type 	= CC_ITEMTYPE_FOOTER;

	x		= x_pos;
	y		= y_pos;

	//init footer width
	width 	= w == 0 ? frameBuffer->getScreenWidth(true) : w;
		//init header height
	cch_font 	= g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE];
	if (h > 0)
		height 	= h;
	else
		height 	= cch_font->getHeight();

	shadow		= has_shadow;
	col_frame	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;

	corner_rad	= RADIUS_LARGE;
	corner_type	= CORNER_BOTTOM;

	btn_contour	= false;

	addContextButton(buttons);
	initCCItems();
	initParent(parent);
}

void CComponentsFooter::setButtonLabels(const struct button_label_s * const content, const size_t& label_count, const int& total_width, const int& label_width)
{
	//define required total width of button group, minimal width is >0, sensless values are nonsens! 
	int w_total = total_width > 0 ? total_width : width;//TODO: alow and handle only with rational values >0
	//consider context button group on the right side of footer, if exist then subtract result from total width of button container
	if (cch_btn_obj)
		w_total -= cch_btn_obj->getWidth();

	//calculate required position of button container
	//consider icon (inherited) width, if exist then set evaluated result as x position for button label container and ...
	int x_btnchain = 0;
	if (cch_icon_obj)
		x_btnchain = (cch_icon_obj->getXPos() + cch_offset + cch_icon_obj->getWidth());
	//... reduce also total width for button label container
	w_total -= x_btnchain;

	//initialize a sub form (item chain as button label container): this contains all passed (as interleaved) button label items, with this container we can work within
	//footer as primary container (in this context '=this') and the parent for the button label container and
	//button label container itself is concurrent the parent object for button objects
	CComponentsFrmChain *btnchain = new CComponentsFrmChain(x_btnchain, CC_CENTERED, w_total, height, 0, CC_DIR_X, this);
	btnchain->doPaintBg(false);

	//calculate matching maximal possible width of button lable within button label container
	int w_btn_max = btnchain->getWidth() / label_count;
	int w_btn = min(label_width, w_btn_max);

	//unused size, usable for offset calculation
	int w_btn_unused = w_total - w_btn * label_count;

	//consider offset of button labels
	int w_btn_offset = w_btn_unused / (label_count+1);
	btnchain->setAppendOffset(w_btn_offset, 0);

	//finally generate button objects passed from button label content.
	for (size_t i= 0; i< label_count; i++){
		CComponentsButton *btn = new CComponentsButton(CC_APPEND, CC_CENTERED, w_btn, height-height/4, content[i].text, content[i].button);
		btn->doPaintBg(btn_contour);
		btnchain->addCCItem(btn);
	}
}

void CComponentsFooter::setButtonLabels(const struct button_label_l * const content, const size_t& label_count, const int& total_width, const int& label_width)
{
	button_label_s buttons[label_count];
	
	for (size_t i= 0; i< label_count; i++){
		buttons[i].button = content[i].button;
		buttons[i].text = g_Locale->getText(content[i].locale);
	}

	setButtonLabels(buttons, label_count, total_width, label_width);
}

void CComponentsFooter::setButtonLabels(const struct button_label * const content, const size_t& label_count, const int& total_width, const int& label_width)
{
	setButtonLabels((button_label_l*)content, label_count, total_width, label_width);
}

void CComponentsFooter::setButtonLabel(const char *button_icon, const std::string& text, const int& total_width, const int& label_width)
{
	button_label_s button[1];

	button[0].button = button_icon;
	button[0].text = text;

	setButtonLabels(button, 1, total_width, label_width);
}

void CComponentsFooter::setButtonLabel(const char *button_icon, const neutrino_locale_t& locale, const int& total_width, const int& label_width)
{
	string txt = g_Locale->getText(locale);

	setButtonLabel(button_icon, txt, total_width, label_width);
}
