/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2013-2017, Thilo Graf 'dbt'

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

#include <neutrino.h>
#include <gui/color_custom.h>
#include "cc_frm_footer.h"
#include <system/debug.h>
#include <driver/fontrenderer.h>

using namespace std;

//-------------------------------------------------------------------------------------------------------
//sub class CComponentsFooter inherit from CComponentsHeader
CComponentsFooter::CComponentsFooter(CComponentsForm* parent):CCButtonSelect()
{
	//CComponentsFooter
	initVarFooter(1, 1, 0, 0, 0, parent, CC_SHADOW_OFF, COL_FRAME_PLUS_0, COL_MENUFOOT_PLUS_0, COL_SHADOW_PLUS_0);
}

CComponentsFooter::CComponentsFooter(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const int& buttons,
					CComponentsForm* parent,
					int shadow_mode,
					fb_pixel_t color_frame,
					fb_pixel_t color_body,
					fb_pixel_t color_shadow ):CCButtonSelect()
{
	//CComponentsFooter
	initVarFooter(x_pos, y_pos, w, h, buttons, parent, shadow_mode, color_frame, color_body, color_shadow);
}

void CComponentsFooter::initVarFooter(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const int& buttons,
					CComponentsForm* parent,
					int shadow_mode,
					fb_pixel_t color_frame,
					fb_pixel_t color_body,
					fb_pixel_t color_shadow )
{
	cc_item_type 	= CC_ITEMTYPE_FOOTER;

	x	= x_old = x_pos;
	y	= y_old = y_pos;

	//init footer width
	width =	width_old = w == 0 ? frameBuffer->getScreenWidth(true) : w;

	//init default fonts
	initDefaultFonts();

	//init default button text font
	ccf_btn_font	= g_Font[SNeutrinoSettings::FONT_TYPE_BUTTON_TEXT];

	//init footer height
	initCaptionFont();
	height = height_old		= max(h, cch_font->getHeight());

	shadow		= shadow_mode;
	ccf_enable_button_shadow 	= false ;
	ccf_button_shadow_width  	= shadow ? OFFSET_SHADOW/2 : 0;
	ccf_button_shadow_force_paint 	= false;
	col_frame = col_frame_old	= color_frame;
	col_body = col_body_old		= color_body;
	col_shadow = col_shadow_old	= color_shadow;
	cc_body_gradient_enable		= cc_body_gradient_enable_old = CC_COLGRAD_OFF/*g_settings.theme.menu_ButtonBar_gradient*/; //TODO: not complete implemented at the moment
	cc_body_gradient_direction	= CFrameBuffer::gradientVertical;
	cc_body_gradient_mode		= CColorGradient::gradientDark2Light;
	btn_auto_frame_col	= false;

	corner_rad	= RADIUS_LARGE;
	corner_type	= CORNER_BOTTOM;

	ccf_enable_button_bg	= false /*g_settings.theme.Button_gradient*/; //TODO: not implemented at the moment

	addContextButton(buttons);
	initCCItems();
	initParent(parent);

	//init repaint slot before re paint of body, if paint() is already done
	initRepaintSlot();
}

void CComponentsFooter::setButtonLabels(const struct button_label_cc * const content, const size_t& label_count, const int& chain_width, const int& label_width)
{
	/* clean up before init*/
	if (btn_container)
		btn_container->clear();

	if (label_count == 0)
		return;

	/* set general available full basic space for button chain,
	 * in this case this is footer width
	*/
	int w_chain = width - 2*cch_offset;

	/* calculate current available space for button container depends
	 * of already enbedded footer objects.
	 * If already existing some items then subtract those width from footer width.
	 * ...so we have the possible usable size for button container.
	*/
	if(!v_cc_items.empty()){
		for (size_t j= 0; j< size(); j++)
			w_chain -= getCCItem(j)->getWidth();
	}

	/* On defined parameter chain_width
	 * calculate current available space for button container depends
	 * of passed chain with parameter
	 * Consider that chain_width is not too large.
	*/
	if (chain_width > 0 && chain_width <= w_chain){
		if (chain_width <= w_chain){
			w_chain = chain_width;
		}
	}

	/* initialize button container (chain object): this contains all passed (as interleaved) button label items,
	 * With this container we can work inside footer as primary container (in this context '=this') and the parent for the button label container (chain object).
	 * Button label container (chain object) itself is concurrent to the parent object for button objects.
	*/
	int dist = height/2-cch_offset;
	int h_chain = ccf_btn_font->getHeight() > height+dist ? height-dist : ccf_btn_font->getHeight()+dist;
	int x_chain = width/2 - w_chain/2;
	int y_chain = height/2 - h_chain/2;
	if (cch_icon_obj)
		 x_chain = cch_offset+cch_icon_obj->getWidth()+cch_offset;
	if (btn_container == NULL){
		btn_container = new CComponentsFrmChain(x_chain, y_chain, w_chain, h_chain, 0, CC_DIR_X, this, CC_SHADOW_OFF, COL_MENUCONTENT_PLUS_6, col_body);
		btn_container->setAppendOffset(0, 0);
		btn_container->setCorner(this->corner_rad, this->corner_type);
		btn_container->doPaintBg(false);
	}

	/* Calculate usable width of button labels inside button object container
	 * related to available width of chain object and passed
	 * label_width parameter.
	 * Parameter is used as minimal value and will be reduced
	 * if it is too large.
	 * Too small label_width parameter will be compensated by
	 * button objects itself.
	*/
	int w_offset = int((label_count-1)*cch_offset);
	int w_btn = btn_container->getWidth()/label_count - w_offset;
	if (label_width){
		int w_label = label_width;
		int w_defined = int(label_width*label_count);
		int w_max = btn_container->getWidth() - w_offset;
		while (w_defined > w_max){
			w_label--;
			w_defined = int(w_label*label_count) - w_offset;
		}
		w_btn = w_label;
	}

	/* generate button objects passed from button label content
	 * with default width to chain object.
	*/
	vector<CComponentsItem*> v_btns;
	int h_btn = /*(ccf_enable_button_bg ? */btn_container->getHeight()-2*fr_thickness/*-OFFSET_INNER_SMALL*//* : height)*/-ccf_button_shadow_width;
	for (size_t i= 0; i< label_count; i++){
		string txt 		= content[i].locale == NONEXISTANT_LOCALE ? content[i].text : g_Locale->getText(content[i].locale);
		string icon_name 	= string(content[i].button);

		//ignore item, if no text and icon are defined;
		if (txt.empty() && icon_name.empty()){
			dprintf(DEBUG_INFO, "[CComponentsFooter]   [%s - %d]  ignore item [%zu], no icon and text defined!\n", __func__, __LINE__, i);
			continue;
		}

		int y_btn = btn_container->getHeight()/2 - h_btn/2;
		dprintf(DEBUG_INFO, "[CComponentsFooter]   [%s - %d]  y_btn [%d] ccf_button_shadow_width [%d]\n", __func__, __LINE__, y_btn, ccf_button_shadow_width);
		CComponentsButton *btn = new CComponentsButton(0, y_btn, w_btn, h_btn, txt, icon_name, NULL, false, true, ccf_enable_button_shadow);

		btn->doPaintBg(ccf_enable_button_bg);
		btn->setButtonDirectKeys(content[i].directKeys);
		btn->setButtonResult(content[i].btn_result);
		btn->setButtonAlias(content[i].btn_alias);
		btn->setButtonFont(ccf_btn_font);

		//set button frames to icon color, predefined for available color buttons
		if (btn_auto_frame_col){
			fb_pixel_t f_col = btn->getColorFrame();
			if (icon_name == NEUTRINO_ICON_BUTTON_RED)
				f_col = COL_DARK_RED;
			if (icon_name == NEUTRINO_ICON_BUTTON_GREEN)
				f_col = COL_DARK_GREEN;
			if (icon_name == NEUTRINO_ICON_BUTTON_YELLOW)
				f_col = COL_OLIVE;
			if (icon_name == NEUTRINO_ICON_BUTTON_BLUE)
				f_col = COL_DARK_BLUE;
			btn->setColorFrame(f_col);
		}

		v_btns.push_back(btn);

		if (w_btn < btn->getWidth()){
			btn->setWidth(w_btn);
			btn->setButtonFont(NULL);
		}
			
		dprintf(DEBUG_INFO, "[CComponentsFooter]   [%s - %d]  button %s [%u]  btn->getWidth() = %d w_btn = %d,  (chain->getWidth() = %d)\n", __func__, __LINE__,  txt.c_str(), (uint32_t)i, btn->getWidth(), w_btn, btn_container->getWidth());
	}

	/* add generated button objects to chain object.
	*/
	if (!v_btns.empty()){
		/*add all buttons into button container*/
		btn_container->addCCItem(v_btns);

		/* set position of labels, as centered inside button container*/
		int w_chain_used = 0;
		for (size_t a= 0; a< btn_container->size(); a++)
			w_chain_used += btn_container->getCCItem(a)->getWidth();
		w_chain_used += (btn_container->size()-1)*cch_offset;

		int x_btn = btn_container->getWidth()/2 - w_chain_used/2;
		btn_container->getCCItem(0)->setXPos(x_btn);

		for (size_t c= 1; c< btn_container->size(); c++){
			x_btn += btn_container->getCCItem(c-1)->getWidth()+ cch_offset;
			btn_container->getCCItem(c)->setXPos(x_btn);
		}
	}
}

void CComponentsFooter::setButtonLabels(const struct button_label * const content, const size_t& label_count, const int& chain_width, const int& label_width)
{
	//conversion for compatibility with older paintButtons() methode, find in /gui/widget/buttons.h
	button_label_cc *buttons = new button_label_cc[label_count];
	for (size_t i = 0; i< label_count; i++){
		buttons[i].button = content[i].button;
		buttons[i].locale = content[i].locale;
		//NOTE: here are used default values, because old button label struct don't know about this,
		//if it possible, don't use this methode!
		buttons[i].directKeys.push_back(CRCInput::RC_nokey);
		buttons[i].btn_result = -1;
		buttons[i].btn_alias = -1;
	}
	setButtonLabels(buttons, label_count, chain_width, label_width);
	delete[] buttons;
}

void CComponentsFooter::setButtonLabels(const vector<button_label_cc> &v_content, const int& chain_width, const int& label_width)
{
	size_t label_count = v_content.size();
	button_label_cc *buttons = new button_label_cc[label_count];

	for (size_t i= 0; i< label_count; i++){
		buttons[i].button = v_content[i].button;
		buttons[i].text = v_content[i].text;
		buttons[i].locale = v_content[i].locale;
		buttons[i].directKeys = v_content[i].directKeys;
		buttons[i].btn_result = v_content[i].btn_result;
		buttons[i].btn_alias = v_content[i].btn_alias;
	}
	setButtonLabels(buttons, label_count, chain_width, label_width);
	delete[] buttons;
}

void CComponentsFooter::setButtonLabel(	const char *button_icon,
					const std::string& text,
					const int& chain_width,
					const int& label_width,
					const neutrino_msg_t& msg,
					const int& result_value,
					const int& alias_value)
{
	button_label_cc button[1];

	button[0].button = button_icon;
	button[0].text = text;
	button[0].directKeys.push_back(msg);
	button[0].btn_result = result_value;
	button[0].btn_alias = alias_value;

	setButtonLabels(button, 1, chain_width, label_width);
}

void CComponentsFooter::setButtonLabel(	const char *button_icon,
					const neutrino_locale_t& locale,
					const int& chain_width,
					const int& label_width,
					const neutrino_msg_t& msg,
					const int& result_value,
					const int& alias_value)
{
	string txt = locale != NONEXISTANT_LOCALE ? g_Locale->getText(locale) : "";

	setButtonLabel(button_icon, txt, chain_width, label_width, msg, result_value, alias_value);
}

void CComponentsFooter::enableButtonBg(bool enable)
{
	ccf_enable_button_bg = enable;
	if (btn_container) {
		for (size_t i= 0; i< btn_container->size(); i++)
			btn_container->getCCItem(i)->doPaintBg(ccf_enable_button_bg);
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

void CComponentsFooter::setButtonText(const uint& btn_id, const std::string& text)
{
	CComponentsItem *item = getButtonChainObject()->getCCItem(btn_id);
	if (item){
		CComponentsButton *button = static_cast<CComponentsButton*>(item);
		button->setCaption(text);
	}
	else
		dprintf(DEBUG_NORMAL, "[CComponentsForm]   [%s - %d]  Error: can't set button text, possible wrong btn_id=%u, item=%p...\n", __func__, __LINE__, btn_id, item);
}


void CComponentsFooter::enableButtonShadow(int mode, const int& shadow_width, bool force_paint)
{
	ccf_enable_button_shadow = mode;
	ccf_button_shadow_width = shadow_width;
	ccf_button_shadow_force_paint = force_paint;
	if (btn_container){
		for(size_t i=0; i<btn_container->size(); i++){
			btn_container->getCCItem(i)->enableShadow(ccf_enable_button_shadow, ccf_button_shadow_width, ccf_button_shadow_force_paint);
			//int y_btn = ccf_enable_button_shadow == CC_SHADOW_OFF ? CC_CENTERED : chain->getHeight()/2 - chain->getCCItem(i)->getHeight()/2 - ccf_button_shadow_width;
			int y_btn = btn_container->getHeight()/2 - btn_container->getCCItem(i)->getHeight()/2;
			btn_container->getCCItem(i)->setYPos(y_btn);
		}
	}
}

void CComponentsFooter::initDefaultFonts()
{
	l_font 	= g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE];
	s_font 	= g_Font[SNeutrinoSettings::FONT_TYPE_MENU_FOOT];
}
