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
	initVarFooter(1, 1, 0, 0, 0, parent, CC_SHADOW_OFF, COL_FRAME_PLUS_0, COL_MENUFOOT_PLUS_0, COL_SHADOW_PLUS_0, CC_HEADER_SIZE_LARGE);
}

CComponentsFooter::CComponentsFooter(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const int& buttons,
					CComponentsForm* parent,
					int shadow_mode,
					fb_pixel_t color_frame,
					fb_pixel_t color_body,
					fb_pixel_t color_shadow,
					int sizeMode):CCButtonSelect()
{
	//CComponentsFooter
	initVarFooter(x_pos, y_pos, w, h, buttons, parent, shadow_mode, color_frame, color_body, color_shadow, sizeMode);
}

void CComponentsFooter::initVarFooter(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const int& buttons,
					CComponentsForm* parent,
					int shadow_mode,
					fb_pixel_t color_frame,
					fb_pixel_t color_body,
					fb_pixel_t color_shadow,
					int sizeMode)
{
	cc_item_type.id 	= CC_ITEMTYPE_FOOTER;
	cc_item_type.name 	= "cc_footer";

	x = cc_xr = cc_xr_old = x_old	= x_pos;
	y = cc_yr = cc_yr_old = y_old	= y_pos;

	//init footer width
	width =	width_old = w == 0 ? frameBuffer->getScreenWidth(true) : w;

	cch_font		= NULL;
	cch_size_mode		= sizeMode;
	cch_offset		= OFFSET_INNER_MID;

	//init font and height
	initSizeMode();
	if (h)
		setHeight(h);

	//init default button text font
	ccf_btn_font	= g_Font[SNeutrinoSettings::FONT_TYPE_BUTTON_TEXT];

	shadow		= shadow_mode;
	ccf_enable_button_shadow 	= false ;
	ccf_button_shadow_width  	= shadow ? OFFSET_SHADOW/2 : 0;
	ccf_button_shadow_force_paint 	= false;
	ccf_button_container_y		= -1; //centered as default
	col_frame			= color_frame;
	col_body_std			= color_body;
	col_shadow			= color_shadow;
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

int CComponentsFooter::initButtonContainer(const int& chain_width)
{
	/*
	 * Button container contains all passed button label items and
	 * represents the parent for button labels and is working as single child object inside footer.
	 */

	/* clean up before init */
	if (btn_container){
		removeCCItem(btn_container);
		btn_container = NULL;
	}

	/*
	 * Check and init parameter 'chain_width':
	 *
	 * Set usable basic space for button container,
	 * Default width of container is footer width minus width of all containing items and offsets inside footer object.
	 */
	int dx_used = 0;
	for (size_t i = 0; i< size(); i++){
		if (getCCItem(i) != btn_container) // avoid to point on button container itself!
			dx_used -= getCCItem(i)->getWidth();
	}
	int dx_container_max = width - dx_used - 2*cch_offset;
	int dx_container = min(width - 2*cch_offset, dx_container_max);

	/*
	 * Initialize width and x position of button container
	 */
	int x_container = cch_offset;

	if (cch_icon_obj)
	{
		int dx_icon_obj = cch_icon_obj->getWidth();
		dx_container -= dx_icon_obj;
		x_container += dx_icon_obj + cch_offset;
	}

	if (cch_text_obj)
		if (!cch_text_obj->getText().empty())
		{
			dx_container -= cch_text_obj->getXPos() + cch_text_obj->getWidth();
			dx_container -= cch_offset;
		}

	if (cch_btn_obj)
	{
		dx_container -= cch_btn_obj->getWidth();
		dx_container -= cch_offset;
	}

	/*
	 * On defined parameter chain_width (means width of button container),
	 * compare and if required use current available space for button container depends of passed chain with parameter.
	 * Consider that chain_width is not too large. In this case parameter chain_width will be ignored.
	 */
	if (chain_width > 0)
	{
		if (chain_width > dx_container)
		{
			dprintf(DEBUG_INFO, "\033[33m[CComponentsFooter]\t[%s - %d], NOTE:  chain_width [%d] too large, fallback to max allowed width [%d]\033[0m\n", __func__, __LINE__, chain_width, dx_container);
		}
	}

	int dist = height/2-cch_offset;
	int dy_container = ccf_btn_font->getHeight() > height+dist ? height-dist : ccf_btn_font->getHeight()+dist;
	dy_container -= cc_parent ? (cc_parent->getFrameThickness()/2 - shadow_w) : 0; // if footer is embedded then consider possible frame around parent object (e.g. window)
	int y_container = ccf_button_container_y < 0 ? height/2 - dy_container/2 : ccf_button_container_y;

	if (btn_container == NULL){
		btn_container = new CComponentsFrmChain(x_container, y_container, dx_container, dy_container, 0, CC_DIR_X, this, CC_SHADOW_OFF, COL_MENUCONTENT_PLUS_6, col_body_std);
		btn_container->setItemName(cc_parent ? cc_parent->getItemName() + ":" + getItemName() + ":btn_container" : "");
		btn_container->setAppendOffset(0, 0);
		//btn_container->setCorner(this->corner_rad, this->corner_type);
		btn_container->doPaintBg(false);
	}

	return dx_container;
}

void CComponentsFooter::setButtonLabels(const struct button_label_cc * const content, const size_t& label_count, const int& chain_width, const int& label_width)
{
	/* init button container and get its usable width*/
	int dx_container = initButtonContainer(chain_width);

	if (label_count == 0)
		return;

	/* init global increments */
	size_t l_count = label_count;

	/*
	 * Evaluate parameter 'label_width':
	 *
	 * Button label width is default auto generated, if no label width is defined.
	 * If parameter label_width is too large, we use maximal possible value.
	*/
	int btn_offset = cch_offset/2;
	int dx_btn = (dx_container - btn_offset * (int)l_count-1) / (int)l_count;
	if (label_width)
		if (label_width < dx_btn)
			dx_btn = label_width;

	/*
	 * Primary x postion of buttons inside button container is fix,
	 * height and y position of button labels are calculated by button container
	 * dimensions and have a fix value.
	*/
	int x_btn = 0;
	int dy_btn = btn_container->getHeight()- 2*fr_thickness - ccf_button_shadow_width;
	int y_btn = ccf_button_container_y < 0 ? btn_container->getHeight()/2 - dy_btn/2 : ccf_button_container_y;

	/*
	 * Init button label objects
	*/
	for (size_t i = 0; i < l_count; i++){
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
		CComponentsButton *btn = new CComponentsButton(x_btn, y_btn, dx_btn, dy_btn, txt, icon_name, btn_container, false, true, ccf_enable_button_shadow);
		btn->setButtonFont(NULL/*ccf_btn_font*/); // ensure init font type by item itself
		btn->doPaintBg(ccf_enable_button_bg);
		
		x_btn += btn_container->getCCItem(i)->getWidth();
		x_btn += btn_offset;

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
				f_col = COL_DARK_YELLOW;
			if (icon_name == NEUTRINO_ICON_BUTTON_BLUE)
				f_col = COL_DARK_BLUE;
			btn->setColorFrame(f_col);
		}
	}
	
	//set width of button container and ensure its center position inside button area of footer
	int dx_cont_free = btn_container->getFreeDX();
	int dx_cont = btn_container->getWidth();
	int x_cont = btn_container->getXPos();
	if (dx_cont_free > 0)
	{
		btn_container->setWidth(dx_cont - dx_cont_free);
		btn_container->setXPos(x_cont + dx_cont_free/2);
	}

	// get the lowest font height for unified typeface
	int dy_font_min = btn_container->getHeight();
	int dy_font_max = 0;
	CComponentsButton *btn_smallest = NULL;
	
	for (size_t i = 0; i < l_count; i++){
		CComponentsButton *btn = static_cast<CComponentsButton*>(btn_container->getCCItem(i));
		int dy_font_tmp = btn->getButtonFont()->getHeight();
		if (dy_font_tmp < dy_font_min)
		{
			dy_font_min = dy_font_tmp;
			btn_smallest = btn;
		}
		dy_font_max = max(dy_font_max, dy_font_tmp);

// 		dprintf(DEBUG_NORMAL, "[CComponentsFooter]\t[%s - %d]\tbutton: %s dy_font_min [%d]\n", __func__, __LINE__, btn->getItemName().c_str(), dy_font_min);
	}

	// set smallest font height
	if (btn_smallest)
	{
		Font* tmp_font = btn_smallest->getButtonFont();
		for (size_t i = 0; i < l_count; i++){
			CComponentsButton *btn = static_cast<CComponentsButton*>(btn_container->getCCItem(i));
			btn->setButtonFont(tmp_font);
		}
// 		dprintf(DEBUG_NORMAL, "[CComponentsFooter]\t[%s - %d]\tsmallest button font height: dy_font_min [%d] dy_font_max [%d]\n", __func__, __LINE__, dy_font_min, dy_font_max);
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
	buttons = NULL;
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
	buttons = NULL;
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
				     const bool &do_save_bg)
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
