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
#include <system/debug.h>

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

	//init footer height
	cch_font 	= g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL];
	height 		= max(h, cch_font->getHeight());

	shadow		= has_shadow;
	col_frame	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;

	corner_rad	= RADIUS_LARGE;
	corner_type	= CORNER_BOTTOM;

	btn_contour	= false;
	ccf_btn_font	= NULL;
	chain		= NULL;

	addContextButton(buttons);
	initCCItems();
	initParent(parent);
}

void CComponentsFooter::setButtonLabels(const struct button_label_s * const content, const size_t& label_count, const int& chain_width, const int& label_width)
{
	//define required total width of button group, minimal width is >0, sensless values are nonsens! 
	int w_chain = chain_width > 0 ? chain_width : width;//TODO: alow and handle only with rational values >0, exit here
	if (w_chain < 100){
		dprintf(DEBUG_NORMAL, "[CComponentsFooter]   [%s - %d] stupid width of chain: width = %d, values < 100 are nonsens, buttons not painted!\n", __func__, __LINE__, w_chain);
		return;
	}

	//consider context button group on the right side of footer, if exist then subtract result from chain_width of button container
	if (cch_btn_obj)
		w_chain -= cch_btn_obj->getWidth();

	//calculate required position of button container
	//consider icon (inherited) width, if exist then set evaluated result as x position for button label container and ...
	int x_chain = 0;
	if (cch_icon_obj)
		x_chain = (cch_icon_obj->getXPos() + cch_offset + cch_icon_obj->getWidth());
	//... reduce also total width for button label container
	w_chain -= x_chain;

	//initialize container (chain object) as button label container: this contains all passed (as interleaved) button label items, with this container we can work inside
	//footer as primary container (in this context '=this') and the parent for the button label container (chain object),
	//button label container (chain object) itself is concurrent the parent object for button objects.
	if (chain == NULL){
		chain = new CComponentsFrmChain(x_chain, CC_CENTERED, w_chain, height, 0, CC_DIR_X, this);
		chain->setCorner(this->corner_rad, this->corner_type);
		chain->doPaintBg(false);
	}
	if (!chain->empty())
		chain->clear();

	//calculate default static width of button labels inside button object container related to available width of chain object
	int w_btn_fix = chain->getWidth() / label_count;
	int w_btn_min = min(label_width, w_btn_fix);

	int w_used = 0;

	//generate and add button objects passed from button label content with default width to chain object.
	for (size_t i= 0; i< label_count; i++){
		string txt = content[i].text;
		string btn_name = string(content[i].button);

		//ignore item, if no text and icon are defined;
		if (txt.empty() && btn_name.empty()){
			dprintf(DEBUG_INFO, "[CComponentsFooter]   [%s - %d]  ignore item [%d], no icon and text defined!\n", __func__, __LINE__, i);
			continue;
		}

		CComponentsButton *btn = new CComponentsButton(0, CC_CENTERED, w_btn_min, height-height/4, txt, btn_name);
		btn->setButtonFont(ccf_btn_font);
		btn->doPaintBg(btn_contour);

		if (btn_name == NEUTRINO_ICON_BUTTON_RED)
			btn->setColorFrame(COL_DARK_RED);
		if (btn_name == NEUTRINO_ICON_BUTTON_GREEN)
			btn->setColorFrame(COL_DARK_GREEN);
		if (btn_name == NEUTRINO_ICON_BUTTON_YELLOW)
			btn->setColorFrame(COL_OLIVE);
		if (btn_name == NEUTRINO_ICON_BUTTON_BLUE)
			btn->setColorFrame(COL_DARK_BLUE);

		chain->addCCItem(btn);

		//set x position of next button object
		if (i != 0)
			btn->setXPos(CC_APPEND);

		//collect used button width inside chain object
		w_used += btn->getWidth();
	}

	//calculate offset between button objects inside chain object
	int w_rest = max(w_chain - w_used, 0);
	int btn_offset = w_rest / chain->size();
	chain->setAppendOffset(btn_offset, 0);
	dprintf(DEBUG_INFO, "[CComponentsFooter]   [%s - %d]  btn_offset = %d, w_rest = %d, w_chain  = %d, w_used = %d, chain->size() = %u\n", __func__, __LINE__, btn_offset, w_rest, w_chain, w_used, chain->size());

	//set x position of 1st button object inside chain, this is centering button objects inside chain
	int x_1st_btn = btn_offset/2;
	chain->getCCItem(0)->setXPos(x_1st_btn);

	//check used width of generated buttons, if required then use dynamic font, and try to fit buttons into chain container, dynamic font is used if ccf_btn_font==NULL
	//NOTE: user should be set not too small window size and not too large fontsize, at some point this possibility will be depleted and it's no more space for readable caption
	if (w_used > width && ccf_btn_font != NULL){
		chain->clear();
		ccf_btn_font = NULL;
		setButtonLabels(content, label_count, chain_width, label_width);
	}
}

void CComponentsFooter::setButtonLabels(const struct button_label_l * const content, const size_t& label_count, const int& chain_width, const int& label_width)
{
	button_label_s buttons[label_count];
	
	for (size_t i= 0; i< label_count; i++){
		buttons[i].button = content[i].button;
		buttons[i].text = content[i].locale != NONEXISTANT_LOCALE ? g_Locale->getText(content[i].locale) : "";
	}

	setButtonLabels(buttons, label_count, chain_width, label_width);
}

void CComponentsFooter::setButtonLabels(const struct button_label * const content, const size_t& label_count, const int& chain_width, const int& label_width)
{
	setButtonLabels((button_label_l*)content, label_count, chain_width, label_width);
}

void CComponentsFooter::setButtonLabel(const char *button_icon, const std::string& text, const int& chain_width, const int& label_width)
{
	button_label_s button[1];

	button[0].button = button_icon;
	button[0].text = text;

	setButtonLabels(button, 1, chain_width, label_width);
}

void CComponentsFooter::setButtonLabel(const char *button_icon, const neutrino_locale_t& locale, const int& chain_width, const int& label_width)
{
	string txt = locale != NONEXISTANT_LOCALE ? g_Locale->getText(locale) : "";

	setButtonLabel(button_icon, txt, chain_width, label_width);
}

void CComponentsFooter::showButtonContour(bool show)
{
	btn_contour = show;
	if (chain) {
		for (size_t i= 0; i< chain->size(); i++)
			chain->getCCItem(i)->doPaintBg(btn_contour);
	}
}


void CComponentsFooter::paintButtons(const int& x_pos,
				     const int& y_pos,
				     const int& w,
				     const int& h,
				     const size_t& label_count,
				     const struct button_label * const content,
				     const int& label_width,
				     const int& context_buttons,
				     Font* font,
				     bool do_save_bg)
{
	this->setDimensionsAll(x_pos, y_pos, w, h);
	this->setButtonFont(font);
	this->setContextButton(context_buttons);
	this->setButtonLabels(content, label_count, 0, label_width);

	this->paint(do_save_bg);
}
