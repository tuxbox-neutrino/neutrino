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
	cc_item_type.id 	= CC_ITEMTYPE_FOOTER;
	cc_item_type.name 	= "cc_footer";

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
	btn_auto_frame_col		= false; //TODO: currently global disabled

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
	/* clean up before init */
	if (btn_container){
		removeCCItem(btn_container);
		btn_container = NULL;
	}

	if (label_count == 0)
		return;

	/* global increments */
	size_t i = 0;
	size_t l_count = label_count;

	/*
	* Evaluate parameter 'chain_width':
	* 
	* Set usable basic space for button container,
	* Default width this is footer width minus offset left and right of button container (btn_container).
	*/
	int w_container = max(0, width - 2*cch_offset);
	/*
	* Calculate current available space for button container depends of already embedded footer objects.
	* If already existing some items then subtract those width from footer width.
	* ...so we have the maximal possible usable size for button container.
	*/
	if(!v_cc_items.empty()){
		for (i = 0; i< size(); i++){
			if (getCCItem(i) != btn_container) // avoid to point on button container itself!
				w_container -= getCCItem(i)->getWidth();
		}
		w_container = max(0, w_container);
	}
	/*
	* On defined parameter chain_width (means width of button container),
	* compare and if required recalculate current available space for button container depends of passed chain with parameter.
	* Consider that chain_width is not too large. In this case parameter chain_width will be ignored.
	*/
	if (chain_width){
		if (chain_width <= w_container){
			w_container = chain_width;
		}else{
			dprintf(DEBUG_NORMAL, "\033[33m[CComponentsFooter]\t[%s - %d], NOTE: parameter chain_width is too large, defined value = %d, fallback to maximal value = %d => \033[0m\n",
				__func__, __LINE__, chain_width, w_container);
		}
	}

	/*
	 * Evaluate parameter 'label_width':
	 * 
	 * button label width is auto generated, if no label width is defined.
	 * If is parameter label_width too large, we use maximal possible value.
	*/
	int w_tmp = w_container - cch_offset * ((int)l_count-1);
	int w_btn = w_tmp / (int)l_count;
	if (label_width && (label_width <= w_btn))
		w_btn = label_width;
	w_container = min(w_container, (w_btn * (int)l_count) + cch_offset * ((int)l_count-1));

	/*
	 * Initialize button container: this object contains all passed button label items,
	 * Button container represents the parent for button labels and is working as single child object inside footer.
	*/
	int dist = height/2-cch_offset;
	int h_container = ccf_btn_font->getHeight() > height+dist ? height-dist : ccf_btn_font->getHeight()+dist;
	int x_container = width/2 - w_container/2; //FIXME: only centered position, other items will be overpainted
	int y_container = height/2 - h_container/2;
	if (cch_icon_obj)
		 x_container = cch_offset+cch_icon_obj->getWidth()+cch_offset;
	if (btn_container == NULL){
		btn_container = new CComponentsFrmChain(x_container, y_container, w_container, h_container, 0, CC_DIR_X, this, CC_SHADOW_OFF, COL_MENUCONTENT_PLUS_6, col_body);
		btn_container->setAppendOffset(0, 0);
		//btn_container->setCorner(this->corner_rad, this->corner_type);
		btn_container->doPaintBg(false);
	}

	/*
	 * Reassign current available container width after initialized button labels.
	*/
	w_container = btn_container->getWidth();

	/*
	 * Primary x postion of buttons inside button container is fix,
	 * height and y position of button labels are calculated by button container
	 * dimensions and have a fix value.
	*/
	int x_btn = 0;
	int h_btn = btn_container->getHeight()- 2*fr_thickness - ccf_button_shadow_width;
	int y_btn = btn_container->getHeight()/2 - h_btn/2;

	/*
	 * Init button label objects
	*/
	for (i = 0; i < l_count; i++){
		/*
		 * init button label face values
		*/
		string txt 		= content[i].locale == NONEXISTANT_LOCALE ? content[i].text : g_Locale->getText(content[i].locale);
		string icon_name 	= content[i].button ? string(content[i].button) : "";

		/*
		 * Ignore item, if no text and no icon is defined.
		*/
		if (txt.empty() && icon_name.empty()){
			//l_count -= 1;
			dprintf(DEBUG_NORMAL, "[CComponentsFooter]\t[%s - %d]\tignore item [%zu], no icon and text defined!\n", __func__, __LINE__, i);
			continue;
		}

		/*
		 * Create all button label objects and add to button container
		 * Set properties, like position, font, key values, coler etc...
		*/
		CComponentsButton *btn = new CComponentsButton(x_btn, y_btn, w_btn, h_btn, txt, icon_name, btn_container, false, true, ccf_enable_button_shadow);
		btn->setButtonFont(ccf_btn_font);
		btn->doPaintBg(ccf_enable_button_bg);
		
		x_btn += btn_container->getCCItem(i)->getWidth();
		x_btn += cch_offset;

		btn->setButtonDirectKeys(content[i].directKeys);
		btn->setButtonResult(content[i].btn_result);
		btn->setButtonAlias(content[i].btn_alias);

		btn->doPaintBg(true);

		/*
		 * Set button frames to icon color, predefined for available color buttons
		*/
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
	}

	/*
	 * Get the current required space for button labels after adding of all buttons.
	 * This is required to check possible changed dimensions after init and
	 * could be required if user has changed settings like font scale, font size or similar setting while runtime or
	 * reserved size of footer is too small. It is recommended to allocate enough space from the outset.
	*/
	int w_cont_used = 0;
	size_t c_btns = btn_container->size();
	for (i = 0; i < c_btns; i++){
		w_cont_used += btn_container->getCCItem(i)->getWidth();	
	}
	w_cont_used += cch_offset * (l_count -1);
	
	if (w_cont_used > w_container){
		/*
		* recalculate width of labels
		*/
		int w_used_tmp = w_cont_used;
		int w_btn_tmp = w_btn;
		if (w_used_tmp >= w_container){
			//w_btn_tmp = w_btn;
			for (i = 0; i < c_btns; i++){
				w_btn_tmp -= c_btns;
				btn_container->getCCItem(i)->setWidth(w_btn_tmp); // value = 0 forces recalculation, refresh is required
				static_cast<CComponentsButton*>(btn_container->getCCItem(i))->Refresh();
				w_used_tmp -= max(0, btn_container->getCCItem(i)->getWidth());
				dprintf(DEBUG_NORMAL, "\033[33m[CComponentsFooter]\t[%s - %d] item %d -> w_used_tmp [%d] :: w_btn_tmp [%d] w_container = %d\033[0m\n", __func__, __LINE__, (int)i, w_used_tmp, w_btn_tmp, w_container);
			}
		}

		/*
		* Trim position of labels, after possible changed width of button labels
		*/
		x_btn = 0;
		btn_container->front()->setXPos(x_btn);
		for (i = 1; i < c_btns; i++){
			x_btn += btn_container->getCCItem(i-1)->getWidth() + cch_offset;;
			if (i < c_btns)
				btn_container->getCCItem(i)->setXPos(x_btn);
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
		for (size_t j= 0; j< v_content[i].directKeys.size(); j++)
			buttons[i].directKeys.push_back(v_content[i].directKeys[j]);
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

CComponentsButton* CComponentsFooter::getButtonLabel(const uint& item_id)
{
	if (btn_container)
		return static_cast<CComponentsButton*>(btn_container->getCCItem(item_id));
	return NULL;
}
