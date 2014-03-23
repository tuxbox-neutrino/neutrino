/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012-2014, Thilo Graf 'dbt'

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

#ifndef __CC_ITEM_TEXT_H__
#define __CC_ITEM_TEXT_H__

#include "cc_base.h"
#include <gui/widget/textbox.h>
#include <string>

//! Sub class of CComponentsItem. Shows a text box.
/*!
Usable like all other CCItems and provides paint of text with different properties for format, alignment, color, fonts etc.
The basic background of textboxes based on frame, body and shadow, known from othe CCItems.
Handling of text parts based up CTextBox attributes and methodes.
CComponentsText provides a interface to the embedded CTextBox object.
*/

class CComponentsText : public CComponentsItem, public CBox
{
	protected:
		///object: CTextBox object
		CTextBox 	* ct_textbox;
		///object: Fontrenderer object
		Font		* ct_font;

		///property: text color
		fb_pixel_t ct_col_text;
		///property: cached text color
		fb_pixel_t ct_old_col_text;
		///property: text display modes, see textbox.h for possible modes
		int ct_text_mode;
		///property: horizontal text border width (left and right)
		int ct_text_Hborder;
		///property: vertical text border width (top and buttom)
		int ct_text_Vborder;
		///property: current text string
		std::string ct_text;
		///status: cached text string, mainly required to compare with current text
		std::string ct_old_text;

		///status: current text string is sent to CTextBox object
		bool ct_text_sent;
		///property: send to CTextBox object enableBackgroundPaint(true)
		bool ct_paint_textbg;

		///property: force sending text to the CTextBox object, false= text only sended, if text was changed, see also textChanged()
		bool ct_force_text_paint;

		///helper: convert int to string
		static std::string iToString(int int_val); //helper to convert int to string

		///initialize all required attributes
		void initVarText(	const int x_pos, const int y_pos, const int w, const int h,
					std::string text,
					const int mode,
					Font* font_text,
					CComponentsForm *parent,
					bool has_shadow,
					fb_pixel_t color_text, fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow);

		///destroy current CTextBox and CBox objects
		void clearCCText();

		///initialize all required attributes for text and send to the CTextBox object
		void initCCText();
		///paint CCItem backckrond (if paint_bg=true), apply initCCText() and send paint() to the CTextBox object
		void paintText(bool do_save_bg = CC_SAVE_SCREEN_YES);
	public:
		CComponentsText(	const int x_pos = 10, const int y_pos = 10, const int w = 150, const int h = 50,
					std::string text = "",
					const int mode = CTextBox::AUTO_WIDTH,
					Font* font_text = NULL,
					CComponentsForm *parent = NULL,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_text = COL_MENUCONTENT_TEXT, fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);

		CComponentsText(	CComponentsForm *parent,
					const int x_pos = 10, const int y_pos = 10, const int w = 150, const int h = 50,
					std::string text = "",
					const int mode = CTextBox::AUTO_WIDTH,
					Font* font_text = NULL,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_text = COL_MENUCONTENT_TEXT, fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);

		virtual ~CComponentsText();

		///default members to paint a text box and hide painted text
		///hide textbox
		void hide(bool no_restore = false);
		///paint text box, parameter do_save_bg: default = true, causes fill of backckrond pixel buffer
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);

		///send options for text font (size and type), color and mode (allignment)
		virtual inline void setTextFont(Font* font_text){ct_font = font_text;};
		///set text color
		virtual inline void setTextColor(fb_pixel_t color_text){ ct_col_text = color_text;};
		///get text color
		virtual inline fb_pixel_t getTextColor(){return ct_col_text;};
		///set text alignment, also see textbox.h for possible alignment modes
		virtual inline void setTextMode(const int mode){ct_text_mode = mode;};
                ///set text border width
		virtual inline void setTextBorderWidth(const int Hborder, const int Vborder = 0){ct_text_Hborder = Hborder; ct_text_Vborder = Vborder;};

		///send option to CTextBox object to paint background box behind text or not
		virtual inline void doPaintTextBoxBg(bool do_paintbox_bg){ ct_paint_textbg = do_paintbox_bg;};

		///set text as string also possible with overloades members for loacales, const char and text file
		virtual void setText(const std::string& stext, const int mode = ~CTextBox::AUTO_WIDTH, Font* font_text = NULL);
		///set text with const char*
		virtual	void setText(const char* ctext, const int mode = ~CTextBox::AUTO_WIDTH, Font* font_text = NULL);
		///set text from locale
		virtual void setText(neutrino_locale_t locale_text, const int mode = ~CTextBox::AUTO_WIDTH, Font* font_text = NULL);
		///set text from digit, digit is integer
		virtual void setText(const int digit, const int mode = ~CTextBox::AUTO_WIDTH, Font* font_text = NULL);
		///set text directly from a textfile, path as string is required
		virtual bool setTextFromFile(const std::string& path_to_textfile, const int mode = ~CTextBox::AUTO_WIDTH, Font* font_text = NULL);
		///get text directly from a textfile, path as string is required
		virtual std::string getTextFromFile(const std::string& path_to_textfile);

		///helper to remove linebreak chars from a string if needed
		virtual void removeLineBreaks(std::string& str);

		///returns true, if text was changed
		virtual bool textChanged(){return ct_old_text != ct_text;};
		///force paint of text even if text was changed or not
		virtual void forceTextPaint(bool force_text_paint = true){ct_force_text_paint = force_text_paint;};

		///gets the embedded CTextBox object, so it's possible to get access directly to its methods and properties
		virtual CTextBox* getCTextBoxObject() { return ct_textbox; };

		///returns count of lines from a text box page
		virtual int getTextLinesAutoHeight(const int& textMaxHeight, const int& textWidth, const int& mode);
};


//! Sub class of CComponentsText. Shows text as label, text color=inactive mode, depending from color settings.
/*!
Usable like all other CCItems and provides paint of text with different properties for format, alignment, color, fonts etc.
The basic background of textboxes based on frame, body and shadow, known from othe CCItems.
Handling of text parts based up CTextBox attributes and methodes.
CComponentsLbel provides a interface to the embedded CTextBox object.
*/

class CComponentsLabel : public CComponentsText
{
	public:
		CComponentsLabel(	const int x_pos = 10, const int y_pos = 10, const int w = 150, const int h = 50,
					std::string text = "",
					const int mode = CTextBox::AUTO_WIDTH,
					Font* font_text = NULL,
					CComponentsForm *parent = NULL,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_text = COL_MENUCONTENTINACTIVE_TEXT,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6,
					fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0,
					fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0)
					:CComponentsText(x_pos, y_pos, w, h, text, mode, font_text, parent, has_shadow, color_text, color_frame, color_body, color_shadow)
		{
			cc_item_type 	= CC_ITEMTYPE_LABEL;
		};
};

#endif
