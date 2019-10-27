/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012-2017, Thilo Graf 'dbt'

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

#ifndef __CC_FORM_FOOTER_H__
#define __CC_FORM_FOOTER_H__

#include "cc_frm_header.h"
#include "cc_frm_button.h"
#include "cc_button_select.h"

#include <global.h>
#include <gui/widget/buttons.h> //for compatibility with 'button_label' type


/*!
CComponentsFooter, sub class of CComponentsHeader provides prepared container for footer
It's usable like a header but without caption, and context button icons as default.
Additional implemented is a button container (chain) and some methods which can add
buttons via struct, vector or text and icon parameters. Also a compatible but limited methode
to add button labels known by older button handler, to find in gui/widget/buttons.h/cpp.
functionality. Why limited ?: old parameter 'struct button_label' doesn't provide newer parameters. 
Missing parameters are filled with default values and must be assigned afterward, if required.
*/
class CComponentsFooter : public CComponentsHeader, public CCButtonSelect
{
	private:
		void initVarFooter(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const int& buttons,
					CComponentsForm *parent,
					int shadow_mode,
					fb_pixel_t color_frame,
					fb_pixel_t color_body,
					fb_pixel_t color_shadow,
					int sizeMode
			);

		///show button with background, default false
		bool ccf_enable_button_bg;
		///enable button with shadow mode, default CC_SHADOW_OFF
		int ccf_enable_button_shadow;
		///set button shadow button width
		int ccf_button_shadow_width;
		///set button shadow button repaint mode
		bool ccf_button_shadow_force_paint;
		///enable/disable button frame in icon color, predefined for red, green, yellow and blue, default disabled
		bool btn_auto_frame_col;

		///property: set font for label caption, see also setButtonFont()
		Font* ccf_btn_font;

		///init default fonts for size modes
		virtual void initDefaultFonts();

	public:
		CComponentsFooter(CComponentsForm *parent = NULL);
		CComponentsFooter(	const int& x_pos, const int& y_pos, const int& w, const int& h = 0,
					const int& buttons = 0,
					CComponentsForm *parent = NULL,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_FRAME_PLUS_0,
					fb_pixel_t color_body = COL_MENUFOOT_PLUS_0,
					fb_pixel_t color_shadow = COL_SHADOW_PLUS_0,
					int sizeMode = CC_HEADER_SIZE_LARGE);
		
		virtual ~CComponentsFooter(){};

		///add button labels with string label type as content, count as size_t, chain_width as int, label width as int
		void setButtonLabels(const struct button_label_cc * const content, const size_t& label_count, const int& chain_width = 0, const int& label_width = 0);
		///add button labels with string label type as content, parameter 1 as vector, chain_width as int, label width as int
		void setButtonLabels(const std::vector<button_label_cc> &v_content, const int& chain_width, const int& label_width);

		///enable/disable button frame in icon color, predefined for red, green, yellow and blue
		void enableButtonFrameColor(bool enable = true){btn_auto_frame_col = enable;}

		///add button labels with old label type, count as size_t, chain_width as int, label width as int
		///NOTE: for compatibility with older button handler find in gui/widget/buttons.h, if possible, don't use this
		void setButtonLabels(const struct button_label * const content, const size_t& label_count, const int& chain_width = 0, const int& label_width = 0);

		///add single button label with string label type as content, chain_width as int, label width as int
		void setButtonLabel(	const char *button_icon,
					const std::string& text,
					const int& chain_width = 0,
					const int& label_width = 0,
					const neutrino_msg_t& msg = RC_NOKEY /*CRCInput::RC_nokey*/,
					const int& result_value = -1,
					const int& alias_value = -1);
		///add single button label with locale label type as content, chain_width as int, label width as int
		void setButtonLabel(	const char *button_icon,
					const neutrino_locale_t& locale,
					const int& chain_width = 0,
					const int& label_width = 0,
					const neutrino_msg_t& msg = RC_NOKEY /*CRCInput::RC_nokey*/,
					const int& result_value = -1,
					const int& alias_value = -1);
		
		///enables background of buttons, parameter bool show, default= true
		void enableButtonBg(bool enable = true);
		///disables background of buttons
		void disableButtonBg(){enableButtonBg(false);}

		/*!
		Sets a new text to an already predefined button.
		1st parameter 'btn_id' accepts current id of an already defined button. 2nd parameter sets the new text as std::string
		Usage:
		Buttons come with any text eg. 'Ok', 'No', 'Yes' ...whatever and this member allows to manipulate the text via button id.
		Button id means the showed button begins from the left position of button chain, starts with value=0, also to get via getButtonChainObject()->getCCItemId([CComponentsButton*])
		example: 1st buttons text is 'Hello', 2nd Button's text is 'You!',
		Now we want to change the text of 2nd button to 'World", so we must do this:
			setButtonText(1, "World");
		Wrong id's will be ignored.
		*/
		void setButtonText(const uint& btn_id, const std::string& text);

		///property: set font for label caption, parameter as font object, value NULL causes usage of dynamic font
		void setButtonFont(Font* font){ccf_btn_font = font;};

		///this is a nearly methode similar with the older button handler find in gui/widget/buttons.h, some parameters are different, but require minimalized input
		///this member sets some basic parameters and will paint concurrently on execute, explicit call of paint() is not required
		void paintButtons(	const int& x_pos,
					const int& y_pos,
					const int& w,
					const int& h,
					const size_t& label_count,
					const struct button_label * const content,
					const int& label_width = 0,
					const int& context_buttons = 0,
					Font* font = g_Font[SNeutrinoSettings::FONT_TYPE_BUTTON_TEXT],
					const bool &do_save_bg = CC_SAVE_SCREEN_NO
				);

		enum
		{
			CC_FOOTER_SIZE_LARGE 	= 0,
			CC_FOOTER_SIZE_SMALL 	= 1
		};
		///set size of footer, possible values are CC_FOOTER_SIZE_LARGE, CC_FOOTER_SIZE_SMALL
		virtual void setSizeMode(const int& size_mode){cch_size_mode = size_mode; initCCItems();}

		///enable and sets shadow properties for embedded buttons
		void enableButtonShadow(int mode = CC_SHADOW_ON, const int& shadow_width = OFFSET_SHADOW/2, bool force_paint = false);
		///disable shadow for embedded buttons
		void disbaleButtonShadow(){enableButtonShadow(CC_SHADOW_OFF);}
		///get button label object with defined item id
		CComponentsButton* getButtonLabel(const uint& item_id);
};



#endif
