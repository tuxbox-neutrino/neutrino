/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2013, 2014, Thilo Graf 'dbt'

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

#ifndef __CC_FORM_EXT_TEXT_H__
#define __CC_FORM_EXT_TEXT_H__

#include "cc_frm.h"
#include "cc_item_text.h"


class CComponentsExtTextForm : public CComponentsForm, public CCTextScreen
{
	private:
		///property: content of label, see also setLabelAndText()
		std::string ccx_label_text;
		///property: content of text, see also setLabelAndText()
		std::string ccx_text;
		///property: color of label text, see also setLabelAndTextColor()
		fb_pixel_t ccx_label_color;
		///property: color of text, see also setLabelAndTextColor()
		fb_pixel_t ccx_text_color;
		///property: mode of label text, see also setTextModes()
		int ccx_label_align;
		///property: mode of text, see also setTextModes()
		int ccx_text_align;
		///property: width of label, see also setLabelWidthPercent()
		int ccx_label_width;
		///property: width of text, see also setLabelWidthPercent()
		int ccx_text_width;
		///property: font type of both items (label and text), see also setLabelAndText()
		Font* ccx_font;
		///centered y position of label and text
		int y_text;

		///object: label object
		CComponentsLabel *ccx_label_obj;
		///object: text object
		CComponentsText *ccx_text_obj;

		///initialize of properties for all objects
		void initCCTextItems();
		///initialize the label object
		void initLabel();
		///initialize the text object
		void initText();

	protected:
		///initialize basic variables
		void initVarExtTextForm(const int& x_pos, const int& y_pos, const int& w, const int& h,
					const std::string& label_text, const std::string& text,
					Font* font_text,
					CComponentsForm* parent,
					int shadow_mode,
					fb_pixel_t label_color,
					fb_pixel_t text_color,
					fb_pixel_t color_frame,
					fb_pixel_t color_body,
					fb_pixel_t color_shadow);

	public:
		///advanced constructor for CComponentsExtTextForm, provides parameters for the most required properties, and caption as string
		CComponentsExtTextForm(CComponentsForm* parent = NULL);

		CComponentsExtTextForm(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const std::string& label_text = "", const std::string& text = "",
					Font* font_text = NULL,
					CComponentsForm *parent = NULL,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t label_color = COL_MENUCONTENTINACTIVE_TEXT,
					fb_pixel_t text_color = COL_MENUCONTENT_TEXT,
					fb_pixel_t color_frame = COL_FRAME_PLUS_0,
					fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0,
					fb_pixel_t color_shadow = COL_SHADOW_PLUS_0);

		CComponentsExtTextForm(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					neutrino_locale_t l_text = NONEXISTANT_LOCALE, const std::string& text = "",
					Font* font_text = NULL,
					CComponentsForm *parent = NULL,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t label_color = COL_MENUCONTENTINACTIVE_TEXT,
					fb_pixel_t text_color = COL_MENUCONTENT_TEXT,
					fb_pixel_t color_frame = COL_FRAME_PLUS_0,
					fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0,
					fb_pixel_t color_shadow = COL_SHADOW_PLUS_0);
// 		~CComponentsExtTextForm(); //inherited from CComponentsForm

		///assigns texts for label and text, parameter as string, parameter Font is optional for required font type, default font is dependently from defined item height
		void setLabelAndText(const std::string& label_text, const std::string& text,  Font* font_text = NULL);
		///assigns texts for label and text, parameter as neutrino_locale_t, parameter Font is optional for required font type, default font is dependently from defined item height
		void setLabelAndText(const neutrino_locale_t& locale_label_text, const neutrino_locale_t& locale_text,  Font* font_text = NULL);
		///assigns text Font type
		void setLabelAndTextFont(Font* font);

		///assigns texts for label and text, parameter as struct (cc_locale_ext_txt_t), parameters provide the same properties like setLabelAndText()
		void setLabelAndTexts(const cc_locale_ext_txt_t& texts);
		///assigns texts for label and text, parameter as struct (cc_string_ext_txt_t), parameters provide the same properties like setLabelAndText()
		void setLabelAndTexts(const cc_string_ext_txt_t& locale_texts);

		///assigns colors for text for label text, parameter as fb_pixel_t
		void setLabelAndTextColor(const fb_pixel_t label_color , const fb_pixel_t text_color);

		///assigns width of label and text related to width, parameter as uint8_t in percent of width, fits text automatically into the available remaining size of item
		void setLabelWidthPercent(const uint8_t& percent_val);

		///returns a pointer to the internal label object, use this to get access to its most properties
		CComponentsLabel*getLabelObject(){return ccx_label_obj;};
		///returns a pointer to the internal text object, use this to get access to its most properties
		CComponentsText*getTextObject(){return ccx_text_obj;};

		/**Member to modify background behavior of embeded label and text objects
		* @param[in]  mode
		* 	@li bool, default = true
		* @return
		*	void
		* @see
		* 	Parent member: CCTextScreen::enableTboxSaveScreen()
		* 	CTextBox::enableSaveScreen()
		* 	disableTboxSaveScreen()
		*/
		void enableTboxSaveScreen(bool mode){
			if (cc_txt_save_screen == mode)
				return;
			cc_txt_save_screen = mode;
			for(size_t i=0; i<v_cc_items.size(); i++)
				static_cast<CComponentsText*>(v_cc_items[i])->enableTboxSaveScreen(cc_txt_save_screen);
		};

		///sets the text modes (mainly text alignment) to the label and text object, see /gui/widget/textbox.h for possible modes
		void setTextModes(const int& label_mode, const int& text_mode);

		///return current font
		Font* getFont(){return ccx_font;}

		///paint this item/form
		void paint(const bool &do_save_bg = CC_SAVE_SCREEN_YES);
};

#endif
