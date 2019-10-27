/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012-2018 Thilo Graf 'dbt'
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
#include "cc_frm_header.h"
#include <system/debug.h>
#include <driver/fontrenderer.h>
#include <driver/pictureviewer/pictureviewer.h>

extern CPictureViewer * g_PicViewer;


using namespace std;

//-------------------------------------------------------------------------------------------------------

//	x/y	    width
//	+---------------------------------------------------------+
//	||icon |caption                    |clock|context buttons||height
//	+---------------------------------------------------------+

//sub class CComponentsHeader inherit from CComponentsForm
CComponentsHeader::CComponentsHeader(CComponentsForm* parent)
{
	//CComponentsHeader
	initVarHeader(1, 1, 0, 0, "", "", 0, parent, CC_SHADOW_OFF, COL_FRAME_PLUS_0, COL_MENUHEAD_PLUS_0, COL_SHADOW_PLUS_0, CC_HEADER_SIZE_LARGE);
}

CComponentsHeader::CComponentsHeader(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					int sizeMode,
					neutrino_locale_t caption_locale,
					CComponentsForm* parent
				)
{
	initVarHeader(x_pos, y_pos, w, h, g_Locale->getText(caption_locale), "", 0, parent, CC_SHADOW_OFF, COL_FRAME_PLUS_0, COL_MENUHEAD_PLUS_0, COL_SHADOW_PLUS_0, sizeMode);
}

CComponentsHeader::CComponentsHeader(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					int sizeMode,
					const std::string& caption,
					CComponentsForm* parent
				)
{
	initVarHeader(x_pos, y_pos, w, h, caption, "", 0, parent, CC_SHADOW_OFF, COL_FRAME_PLUS_0, COL_MENUHEAD_PLUS_0, COL_SHADOW_PLUS_0, sizeMode);
}

CComponentsHeader::CComponentsHeader(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const std::string& caption,
					const std::string& icon_name,
					const int& buttons,
					CComponentsForm* parent,
					int shadow_mode,
					fb_pixel_t color_frame,
					fb_pixel_t color_body,
					fb_pixel_t color_shadow,
					int sizeMode
				)
{
	initVarHeader(x_pos, y_pos, w, h, caption, icon_name, buttons, parent, shadow_mode, color_frame, color_body, color_shadow, sizeMode);
}

CComponentsHeader::CComponentsHeader(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					neutrino_locale_t caption_locale,
					const std::string& icon_name,
					const int& buttons,
					CComponentsForm* parent,
					int shadow_mode,
					fb_pixel_t color_frame,
					fb_pixel_t color_body,
					fb_pixel_t color_shadow,
					int sizeMode
				)
{
	initVarHeader(x_pos, y_pos, w, h, g_Locale->getText(caption_locale), icon_name, buttons, parent, shadow_mode, color_frame, color_body, color_shadow, sizeMode);
};

void CComponentsHeader::initVarHeader(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const std::string& caption,
					const std::string& icon_name,
					const int& buttons,
					CComponentsForm* parent,
					int shadow_mode,
					fb_pixel_t color_frame,
					fb_pixel_t color_body,
					fb_pixel_t color_shadow,
					int sizeMode
				)
{
	cc_item_type.id		= CC_ITEMTYPE_FRM_HEADER;
	cc_item_type.name 	= "cc_header";
	clear();
	cc_txt_save_screen	= false;
	x = cc_xr = x_old 	= x_pos;
	y = cc_yr = y_old 	= y_pos;

	//init header width
	width 	= width_old = w == 0 ? frameBuffer->getScreenWidth(true) : w;

	cch_font		= NULL;
	cch_size_mode		= sizeMode;

	//init font and height
	initSizeMode();
	if (h)
		setHeight(h);

	CNeutrinoApp::getInstance()->OnAfterSetupFonts.connect(sigc::mem_fun(this, &CComponentsHeader::resetFont));

	shadow		= shadow_mode;
	col_frame = col_frame_old 	= color_frame;
	col_body = col_body_old		= color_body;
	col_shadow = col_shadow_old	= color_shadow;

	cc_body_gradient_enable	 	= cc_body_gradient_enable_old	= (cch_size_mode == CC_HEADER_SIZE_SMALL ? g_settings.theme.menu_SubHead_gradient : g_settings.theme.menu_Head_gradient);
	cc_body_gradient_direction	= cc_body_gradient_direction_old = (cch_size_mode == CC_HEADER_SIZE_SMALL ? g_settings.theme.menu_SubHead_gradient_direction : g_settings.theme.menu_Head_gradient_direction);
	cc_body_gradient_mode		= CColorGradient::gradientLight2Dark;

	cch_text	= caption;
	cch_icon_name	= icon_name;

	corner_rad	= RADIUS_LARGE,
	corner_type	= CORNER_TOP;

	cch_icon_obj		= NULL;
	cch_text_obj		= NULL;
	cch_btn_obj		= NULL;
	cch_cl_obj		= NULL;
	cch_logo_obj		= NULL;
	cch_logo.Id		= 0;
	cch_logo.Name		= "";
	cch_logo.dy_max		= -1;
	cch_logo.Align		= DEFAULT_LOGO_ALIGN;
	cch_col_text		= COL_MENUHEAD_TEXT;
	cch_caption_align	= DEFAULT_TITLE_ALIGN;
	cch_items_y 		= CC_CENTERED;
	cch_offset		= OFFSET_INNER_MID;
	cch_icon_x 		= cch_offset;
	cch_icon_w		= 0;
	cch_clock_w		= 0;
	cch_text_x		= cch_offset;
	cch_buttons_space	= cch_offset;

	cch_cl_enable 		= false;
	cch_cl_format		= "%H:%M";
	cch_cl_sec_format 	= cch_cl_format;
	cch_cl_enable_run	= false;

	addContextButton(buttons);
	initCCItems();
	initParent(parent);

	//init repaint slot before re paint of body, if paint() is already done
	initRepaintSlot();
}

void CComponentsHeader::initRepaintSlot(){
	sl_form_repaint = sigc::bind(sigc::mem_fun(*this, &CComponentsHeader::kill), cc_parent ? col_body : 0, -1, CC_FBDATA_TYPES, false);
	OnBeforeRePaint.connect(sl_form_repaint);
}

CComponentsHeader::~CComponentsHeader()
{
	dprintf(DEBUG_DEBUG, "[~CComponentsHeader]   [%s - %d] delete...\n", __func__, __LINE__);
	v_cch_btn.clear();
}

void CComponentsHeader::setCaption(const std::string& caption, const cc_title_alignment_t& align_mode, const fb_pixel_t& text_color)
{
	if (cch_cl_obj)
		cch_cl_obj->Stop();
	cch_text		= caption;
	cch_caption_align 	= align_mode;
	cch_col_text 		= text_color;
}

void CComponentsHeader::setCaption(neutrino_locale_t caption_locale, const cc_title_alignment_t& align_mode, const fb_pixel_t& text_color)
{
	setCaption(string(g_Locale->getText(caption_locale)), align_mode, text_color);
}

void CComponentsHeader::setCaptionFont(Font* font)
{
	cch_font = font;
}

void CComponentsHeader::resetFont()
{
	if (cch_font){
		dprintf(DEBUG_DEBUG, "\033[33m[CComponentsHeader][%s - %d] reset header font \033[0m\n", __func__, __LINE__);
		cch_font = NULL;
	}
	initDefaultFonts();
}

void CComponentsHeader::initDefaultFonts()
{
	l_font 	= g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE];
	s_font 	= g_Font[SNeutrinoSettings::FONT_TYPE_MENU];
}

void CComponentsHeader::initCaptionFont()
{
	initDefaultFonts();
	if (!cch_font)
		cch_font = (cch_size_mode == CC_HEADER_SIZE_LARGE ? l_font : s_font);
}

void CComponentsHeader::initSizeMode()
{
	initCaptionFont();
	int h_new = cch_font->getHeight();
	setHeight(h_new);
}

void CComponentsHeader::setIcon(const char* icon_name)
{
	if (icon_name){
		string s_icon = string(icon_name);
		setIcon(s_icon);
	}
	else
		setIcon("");
}

void CComponentsHeader::setIcon(const std::string& icon_name)
{
	cch_icon_name = icon_name;
	initIcon();
}

void CComponentsHeader::initIcon()
{
	//init cch_icon_obj only if an icon available
	if (cch_icon_name.empty()) {
		cch_icon_w = 0;
		if (cch_icon_obj){
			removeCCItem(cch_icon_obj);
			cch_icon_obj = NULL;
		}
		return;
	}

	//create instance for cch_icon_obj and add to container at once
	if (cch_icon_obj == NULL){
		dprintf(DEBUG_DEBUG, "[CComponentsHeader]\n    [%s - %d] init header icon: %s\n", __func__, __LINE__, cch_icon_name.c_str());
		cch_icon_obj = new CComponentsPicture(cch_icon_x, cch_items_y, 0, 0, cch_icon_name, this);
	}

	//set properties for icon object
	if (cch_icon_obj){
		//get dimensions of header icon
		int iw = 0;
		int ih = 0;
		cch_icon_obj->getSize(&iw, &ih);
		dprintf(DEBUG_INFO, "[CComponentsHeader]\n    [%s - %d] init icon size: iw = %d, ih = %d\n", __func__, __LINE__, iw, ih);
		cch_icon_obj->setWidth(iw);
		cch_icon_obj->setHeight(ih);
		cch_icon_obj->doPaintBg(false);

		//set corner mode of icon item
		int cc_icon_corner_type = CORNER_LEFT;
		if (corner_type & CORNER_TOP_LEFT || corner_type & CORNER_TOP)
			cc_icon_corner_type = CORNER_TOP_LEFT;

		cch_icon_obj->setCorner(corner_rad-fr_thickness, cc_icon_corner_type);

		//global set width of icon object
		cch_icon_w = cch_icon_obj->getWidth();

		//global adapt height
		height = max(height, cch_icon_obj->getHeight());

//		//re-assign height of icon object, for the case of changed height
// 		cch_icon_obj->setHeight(height);
	}
}

void CComponentsHeader::initLogo()
{
	// init logo with required height and logo
	int h_logo = cch_logo.dy_max == -1 ? height - 2*OFFSET_INNER_MIN : cch_logo.dy_max;

	if(!cch_logo_obj)
		cch_logo_obj = new CComponentsChannelLogoScalable(1, height/2 - h_logo/2, cch_logo.Name, cch_logo.Id, this);
	else
		cch_logo_obj->setChannel(cch_logo.Id, cch_logo.Name);

	// use value 1 as initial value for logo width, ensures downscale with stupid available logo space
	cch_logo_obj->setHeight(1, true);

	//ensure logo is not larger than original size if in auto mode
	if (cch_logo.dy_max == -1){
		int dx_orig = 0, dy_orig = 0 ;
		cch_logo_obj->getRealSize(&dx_orig, &dy_orig);
		if (h_logo > dy_orig)
			h_logo = dy_orig;
	}

	// manage logo position
	if (cch_logo_obj->hasLogo()){
		cch_logo_obj->setHeight(h_logo, true);

		/* Detect next and previous items,
		 * current item is logo item.
		*/
		int logo_id = getCCItemId(cch_logo_obj);
		CComponentsItem *prev_item = getCCItem((cch_caption_align & CC_TITLE_RIGHT) ? logo_id - 2 : logo_id - 1);
		CComponentsItem *next_item = getCCItem((cch_caption_align & CC_TITLE_RIGHT) ? logo_id - 1 : min(logo_id + 1, (int)size()-1));

		/*
		 * FIXME: Workaround to fix next item in case of wrong order of items.
		*/
		if (next_item){
			if (next_item->getItemType() == CC_ITEMTYPE_FRM_ICONFORM){
				/*
				 * Either clock is present or buttons are enabled,
				 * different order of objects are required, not optimal
				 * but works at the moment.
				*/
				if (cch_cl_obj)
					next_item = cch_cl_obj;
				else
					next_item = cch_btn_obj;
			}
		}

		/*
		 * Adjust usable space for logo.
		*/
		int x_logo_left = prev_item ? prev_item->getXPos() + prev_item->getWidth() : cch_offset;
		int x_logo_right = next_item ? next_item->getXPos() - cch_offset : width - cch_offset;
		int logo_space = x_logo_right - x_logo_left;

		/*
		 * Reduce logo width if logo space too small
		 * and adjust logo new width if required.
		*/
		int w_logo = min(cch_logo_obj->getWidth(), logo_space);
		cch_logo_obj->setWidth(w_logo, true);

		/*
		 * Adjust logo x position depends of align parameters.
		*/
		int x_logo = x_logo_left;
		if (cch_logo.Align & CC_LOGO_RIGHT)
			x_logo = x_logo_right - w_logo;

		if (cch_logo.Align & CC_LOGO_LEFT)
			x_logo = x_logo_left;

		if (cch_logo.Align & CC_LOGO_CENTER){
			x_logo = logo_space/2 - w_logo/2;
			/*
			* We are using centered mode as default,
			* but we must notice possible overlapp
			* with previous or next item.
			*/
			if (cch_caption_align & CC_TITLE_LEFT){
				if (prev_item){
					int left_tag = prev_item->getXPos() + prev_item->getWidth();
					if (x_logo <= left_tag)
						x_logo = left_tag + logo_space/2 - w_logo/2;
				}
			}

			if (cch_caption_align & CC_TITLE_RIGHT){
				if (next_item){
					if (x_logo + w_logo >= next_item->getXPos())
						x_logo = next_item->getXPos() - logo_space/2 - w_logo/2;
				}
			}
		}

		/*
		 * Finally set logo x position
		*/
		cch_logo_obj->setXPos(x_logo);
		cch_logo_obj->setYPos(height/2 - cch_logo_obj->getHeight()/2);
	}
}

void CComponentsHeader::addContextButton(const std::string& button_name)
{
	v_cch_btn.push_back(button_name);
	dprintf(DEBUG_DEBUG, "[CComponentsHeader]  %s added %d default buttons...\n", __func__, (int)v_cch_btn.size());
}

void CComponentsHeader::addContextButton(const std::vector<std::string>& v_button_names)
{
	for (size_t i= 0; i< v_button_names.size(); i++)
		addContextButton(v_button_names[i]);
}

void CComponentsHeader::addContextButton(const int& buttons)
{
	if (buttons & CC_BTN_EXIT)
		addContextButton(NEUTRINO_ICON_BUTTON_HOME);
	if (buttons & CC_BTN_HELP)
		addContextButton(NEUTRINO_ICON_BUTTON_HELP);
	if (buttons & CC_BTN_INFO)
		addContextButton(NEUTRINO_ICON_BUTTON_INFO);
	if (buttons & CC_BTN_MENU)
		addContextButton(NEUTRINO_ICON_BUTTON_MENU);
	if (buttons & CC_BTN_MUTE_ZAP_ACTIVE )
		addContextButton(NEUTRINO_ICON_BUTTON_MUTE_ZAP_ACTIVE);
	if (buttons & CC_BTN_MUTE_ZAP_INACTIVE )
		addContextButton(NEUTRINO_ICON_BUTTON_MUTE_ZAP_INACTIVE);
	if (buttons & CC_BTN_OKAY)
		addContextButton(NEUTRINO_ICON_BUTTON_OKAY);
	if (buttons & CC_BTN_MUTE)
		addContextButton(NEUTRINO_ICON_BUTTON_MUTE);
	if (buttons & CC_BTN_UP)
		addContextButton(NEUTRINO_ICON_BUTTON_UP);
	if (buttons & CC_BTN_DOWN)
		addContextButton(NEUTRINO_ICON_BUTTON_DOWN);
	if (buttons & CC_BTN_LEFT)
		addContextButton(NEUTRINO_ICON_BUTTON_LEFT);
	if (buttons & CC_BTN_RIGHT)
		addContextButton(NEUTRINO_ICON_BUTTON_RIGHT);
	if (buttons & CC_BTN_FORWARD)
		addContextButton(NEUTRINO_ICON_BUTTON_FORWARD);
	if (buttons & CC_BTN_BACKWARD)
		addContextButton(NEUTRINO_ICON_BUTTON_BACKWARD);
	if (buttons & CC_BTN_PAUSE)
		addContextButton(NEUTRINO_ICON_BUTTON_PAUSE);
	if (buttons & CC_BTN_PLAY)
		addContextButton(NEUTRINO_ICON_BUTTON_PLAY);
	if (buttons & CC_BTN_RECORD_ACTIVE)
		addContextButton(NEUTRINO_ICON_BUTTON_RECORD_ACTIVE);
	if (buttons & CC_BTN_RECORD_INACTIVE)
		addContextButton(NEUTRINO_ICON_BUTTON_RECORD_INACTIVE);
	if (buttons & CC_BTN_RECORD_STOP)
		addContextButton(NEUTRINO_ICON_BUTTON_STOP);
}

void CComponentsHeader::removeContextButtons()
{
	dprintf(DEBUG_DEBUG, "[CComponentsHeader]\t    [%s - %d] removing %zu context buttons...\n", __func__, __LINE__, v_cch_btn.size());
	v_cch_btn.clear();
	if (cch_btn_obj)
		cch_btn_obj->clear();
}

void CComponentsHeader::initButtons()
{
	//exit if no button defined
	if (v_cch_btn.empty()){
		if (cch_btn_obj)
			cch_btn_obj->clear(); //clean up, but hold instance
		return;
	}

	//create instance for header buttons chain object and add to container
	if (cch_btn_obj == NULL){
		dprintf(DEBUG_DEBUG, "[CComponentsHeader]\n    [%s - %d] init header buttons...\n", __func__, __LINE__);
		cch_btn_obj = new CComponentsIconForm(this);
	}

	//set button form properties
	if (cch_btn_obj){
		cch_btn_obj->setYPos(cch_items_y);
		cch_btn_obj->doPaintBg(false);;
		cch_btn_obj->setAppendOffset(cch_buttons_space, 0);
		cch_btn_obj->setRightOffset(cch_buttons_space);
		cch_btn_obj->removeAllIcons();
		cch_btn_obj->addIcon(v_cch_btn);

		//set corner mode of button item
		int cc_btn_corner_type = CORNER_RIGHT;
		if (corner_type == CORNER_TOP_RIGHT || corner_type == CORNER_TOP)
			cc_btn_corner_type = CORNER_TOP_RIGHT;

		cch_btn_obj->setCorner(corner_rad-fr_thickness, cc_btn_corner_type);

		//global adapt height
		height = max(height, cch_btn_obj->getHeight());

		//re-align height of button object
		cch_btn_obj->setHeight(height);

		//re-align height of icon object
		if (cch_icon_obj)
			cch_icon_obj->setHeight(height);
	}
}


void CComponentsHeader::enableClock(bool enable, const char* format, const char* sec_format_str, bool run)
{
	cch_cl_enable	= enable;
	cch_cl_format 	= format;
	if (cch_cl_obj && cch_cl_enable)
		cch_cl_obj->clear();
	if (sec_format_str)
		cch_cl_sec_format = sec_format_str;
	cch_cl_enable_run 	= run;
	if (!cch_cl_enable){
		if (cch_cl_obj){
			cch_cl_enable_run = false;
			removeCCItem(cch_cl_obj);
			cch_cl_obj = NULL;
		}
	}
	initCCItems();
}


void CComponentsHeader::disableClock()
{
	enableClock(false, cch_cl_format, cch_cl_sec_format, false);
}

void CComponentsHeader::initClock()
{
	//exit here if clock was disabled
	if (!cch_cl_enable){
		if (cch_cl_obj){
			removeCCItem(cch_cl_obj);
			cch_cl_obj = NULL;
		}
		return;
	}
	//create instance for header clock object and add to container
	if (cch_cl_obj == NULL){
		dprintf(DEBUG_DEBUG, "[CComponentsHeader]\n    [%s - %d] init clock...\n", __func__, __LINE__);
		cch_cl_obj = new CComponentsFrmClock(0, cch_items_y, cch_font, cch_cl_format, NULL, false, 1, this);
		cch_cl_obj->disableForceSegmentPaint();
		cch_cl_obj->doPaintBg(false);
	}

	//set clock form properties
	if (cch_cl_obj){
		cch_cl_obj->setHeight(height);

		//disallow paint of clock, if disabled and exit method
		if (!cch_cl_enable){
			cch_cl_obj->allowPaint(false);
			cch_cl_obj->Stop();
			return;
		}

		//assign time size and format
		cch_cl_obj->setClockFont(cch_font);
		cch_cl_obj->setClockFormat(cch_cl_format, cch_cl_sec_format);

		//set corner mode of button item
		int cc_btn_corner_type = CORNER_RIGHT;
		if (corner_type == CORNER_TOP_RIGHT || corner_type == CORNER_TOP)
			cc_btn_corner_type = CORNER_TOP_RIGHT;

		cch_cl_obj->setCorner(corner_rad-fr_thickness, cc_btn_corner_type);

		//global adapt height
		height = max(height, cch_cl_obj->getHeight());

		//re-assign height of clock object, for the case of changed height
		cch_cl_obj->setHeight(height);
	}
}

void CComponentsHeader::initCaption()
{
	//recalc header text position if header icon is defined
	int cc_text_w = 0;
	if (!cch_icon_name.empty()){
		cch_text_x = cch_icon_x+cch_icon_w+cch_offset;
	}
	else /* if a icon was present before and was removed, reinit x position */
		cch_text_x = cch_offset;

	//calc width of text object in header
	cc_text_w = width-cch_text_x-cch_offset;

	//context buttons
	int buttons_w = 0;
	if (cch_btn_obj){
		//get width of buttons object
		buttons_w = cch_btn_obj->empty() ? 0 : cch_btn_obj->getWidth();
		dprintf(DEBUG_DEBUG, "[CComponentsHeader]\n    [%s - %d] init context buttons...x=%d, y=%d, width=%d, height=%d\n", __func__, __LINE__, cch_btn_obj->getXPos(), cch_btn_obj->getYPos(), cch_btn_obj->getWidth(), cch_btn_obj->getHeight());
		//set x position of buttons
		cch_btn_obj->setXPos(width - buttons_w);

		//set required width of caption object
		cc_text_w -= buttons_w;
	}

	//clock
	if (cch_cl_obj){
		//refresh clock properties
		cch_cl_obj->refresh();

		//get width of clock object
		int clock_w = cch_cl_enable ? cch_cl_obj->getWidth() : 0;

		//set x position of clock
		cch_cl_obj->setXPos(width - buttons_w - clock_w);

		//set required width of caption object
		cc_text_w -= clock_w;

		//stop clock if disabled or option run is disabled and clock is running
		if (cch_cl_enable){
			if (!cch_cl_enable_run  && cch_cl_obj->isRun()){
				cch_cl_obj->Stop();
				cch_cl_obj->allowPaint(false);
			}
		}

		//clock visible or not visible if run or not
		cch_cl_obj->allowPaint(cch_cl_enable);
	}


	//create cch_text_obj and add to collection
	if (cch_text_obj == NULL){
		dprintf(DEBUG_DEBUG, "[CComponentsHeader]\n    [%s - %d] init header text: %s [ x %d w %d ]\n", __func__, __LINE__, cch_text.c_str(), cch_text_x, cc_text_w);
		cch_text_obj = new CComponentsText(this);
		cch_text_obj->doPaintBg(false);
	}

	//set header text properties
	if (cch_text_obj){
		int w_free = cc_text_w;

		//recalc caption width
		cc_text_w = min(cc_text_w, cch_font->getRenderWidth(cch_text)) + cch_offset;

		//set alignment of text item in dependency from text alignment
		if (cch_caption_align & CC_TITLE_CENTER)
			cch_text_x = width/2 - cc_text_w/2;

		if (cch_caption_align & CC_TITLE_RIGHT){ //FIXME: does not work correct with some conditions, but still not used at the moment
			cch_text_x += w_free;
			cch_text_x -= max(cc_text_w, cch_font->getRenderWidth(cch_text));
		}

		//assign general properties
		cch_text_obj->setDimensionsAll(cch_text_x, cch_items_y, cc_text_w, height);
		cch_text_obj->setColorBody(col_body);
		if (cc_body_gradient_enable != cc_body_gradient_enable_old)
			cch_text_obj->getCTextBoxObject()->clearScreenBuffer();
		cch_text_obj->setText(cch_text, cch_caption_align, cch_font, cch_col_text);
		cch_text_obj->enableTboxSaveScreen(cc_body_gradient_enable || cc_txt_save_screen);

		//corner of text item
		cch_text_obj->setCorner(corner_rad-fr_thickness, corner_type);

		//synchronize clock color with caption color
		if (cch_cl_obj)
			cch_cl_obj->setTextColor(cch_col_text);

		/*
		   global adapt height not needed here again
		   because this object is initialized at last
		*/
		//height = max(height, cch_text_obj->getHeight());
	}
}

void CComponentsHeader::initCCItems()
{
	//set size
	initCaptionFont();

	//set basic properties
	Init(x, y, width, height, col_frame, col_body, col_shadow);

	//init icon
	initIcon();

	//init buttons
	initButtons();

	//init clock
	initClock();

	//init text
	initCaption();

	//init logo
	initLogo();
}

void CComponentsHeader::paint(const bool &do_save_bg)
{
	//prepare items
	initCCItems();

	//paint form contents
	CComponentsForm::paint(do_save_bg);

	//start clock if enabled
	if (cch_cl_obj){
		if (cch_cl_enable && cch_cl_enable_run)
			cch_cl_obj->Start();
	}
}


bool CComponentsHeader::enableColBodyGradient(const int& enable_mode, const fb_pixel_t& sec_color, const int& direction)
{
	int dir = direction == -1 ? g_settings.theme.menu_Head_gradient_direction : direction; //header mode is pre defined here

	if (cch_text_obj->getCTextBoxObject())
		cch_text_obj->getCTextBoxObject()->clearScreenBuffer();

	return CComponentsForm::enableColBodyGradient(enable_mode, sec_color, dir);
}

void CComponentsHeader::kill(const fb_pixel_t& bg_color, const int& corner_radius, const int& fblayer_type /*fbdata_type*/, bool disable_clock)
{
	if (disable_clock)
		disableClock();

	int rad = corner_radius;
	if (corner_radius == -1)
		rad = corner_rad;

	CComponentsForm::kill(bg_color, rad, fblayer_type);
}
