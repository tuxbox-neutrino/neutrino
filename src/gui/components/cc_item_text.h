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
#include "cc_item.h"
#include "cc_text_screen.h"
#include <gui/widget/textbox.h>
#include <string>

//! Sub class of CComponentsItem. Shows a text box.
/*!
Usable like all other CCItems and provides paint of text with different properties for format, alignment, color, fonts etc.
The basic background of textboxes based on frame, body and shadow, known from othe CCItems.
Handling of text parts based up CTextBox attributes and methodes.
CComponentsText provides a interface to the embedded CTextBox object.
*/

class CComponentsText : public CCTextScreen, public CComponentsItem
{
	protected:
		///object: Fontrenderer object
		Font		*ct_font;
		///property: current text string
		std::string 	ct_text;
		///property: text color
		fb_pixel_t ct_col_text;
		///property: send to CTextBox object enableBackgroundPaint(true)
		bool ct_paint_textbg;
		///property: cached text color
		fb_pixel_t ct_old_col_text;
		///property: text display modes, see textbox.h for possible modes
		int ct_text_mode;

	private:
		///object: CTextBox object
		CTextBox 	* ct_textbox;
		CBox		ct_box;
		///property: font style
		int 		ct_text_style;
		///property: horizontal text border width (left and right)
		int ct_text_Hborder;
		///property: vertical text border width (top and buttom)
		int ct_text_Vborder;
		///status: cached text string, mainly required to compare with current text
		std::string ct_old_text;
		bool ct_utf8_encoded;
		///status: current text string is sent to CTextBox object
		bool ct_text_sent;
		///property: force sending text to the CTextBox object, false= text only sended, if text was changed, see also textChanged()
		bool ct_force_text_paint;
		///helper: convert int to string
		static std::string iToString(int int_val); //helper to convert int to string

		///initialize all required attributes
		void initVarText(	const int x_pos, const int y_pos, const int w, const int h,
					std::string text,
					const int mode,
					Font* font_text,
					const int& font_style,
					CComponentsForm *parent,
					int shadow_mode,
					fb_pixel_t color_text,
					fb_pixel_t color_frame,
					fb_pixel_t color_body,
					fb_pixel_t color_shadow);

		///destroy current CTextBox and CBox objects
		void clearCCText();

		///init internal CBox object required by CTextBox object
		void initCBox();

		///paint CCItem backckrond (if paint_bg=true), apply initCCText() and send paint() to the CTextBox object
		void paintText(const bool &do_save_bg = CC_SAVE_SCREEN_YES);
	public:
		enum {
			FONT_STYLE_REGULAR	= 0,
			FONT_STYLE_BOLD		= 1,
			FONT_STYLE_ITALIC	= 2
		};

		CComponentsText(	const int x_pos = 10, const int y_pos = 10, const int w = 0, const int h = 0,
					std::string text = "",
					const int mode = CTextBox::AUTO_WIDTH,
					Font* font_text = NULL,
					const int& font_style = CComponentsText::FONT_STYLE_REGULAR,
					CComponentsForm *parent = NULL,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t color_text = COL_MENUCONTENT_TEXT,
					fb_pixel_t color_frame = COL_FRAME_PLUS_0,
					fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0,
					fb_pixel_t color_shadow = COL_SHADOW_PLUS_0);

		CComponentsText(	CComponentsForm *parent,
					const int x_pos = 10, const int y_pos = 10, const int w = 150, const int h = 50,
					std::string text = "",
					const int mode = CTextBox::AUTO_WIDTH,
					Font* font_text = NULL,
					const int& font_style = CComponentsText::FONT_STYLE_REGULAR,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t color_text = COL_MENUCONTENT_TEXT,
					fb_pixel_t color_frame = COL_FRAME_PLUS_0,
					fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0,
					fb_pixel_t color_shadow = COL_SHADOW_PLUS_0);

		virtual ~CComponentsText();

		///default members to paint a text box and hide painted text
		///hide textbox
		void hide();
		///remove textbox from screen
		void kill(const fb_pixel_t& bg_color = COL_BACKGROUND_PLUS_0, const int& corner_radius = -1, const int& fblayer_type = CC_FBDATA_TYPES);
		///paint text box, parameter do_save_bg: default = true, causes fill of backckrond pixel buffer
		void paint(const bool &do_save_bg = CC_SAVE_SCREEN_YES);

		///send options for text font (size and type), color and mode (allignment)
		 void setTextFont(Font* font_text);
		///set text color
		 void setTextColor(const fb_pixel_t& color_text);
		///set all basic framebuffer element colors at once
		///Note: Possible color values are defined in "gui/color.h" and "gui/color_custom.h"
		 void setColorAll(fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow = COL_SHADOW_PLUS_0, fb_pixel_t color_text = COL_MENUCONTENT_TEXT)
					{
						CCDraw::setColorAll(color_frame, color_body, color_shadow);
						setTextColor(color_text);
					};
		///get text color
		 fb_pixel_t getTextColor(){return ct_col_text;};
		///set text alignment, also see textbox.h for possible alignment modes
		 void setTextMode(const int mode);
                ///set text border width
		 void setTextBorderWidth(const int Hborder, const int Vborder = 0);

		///send option to CTextBox object to paint background box behind text or not
		 void doPaintTextBoxBg(bool do_paintbox_bg);

		///initialize all required attributes for text and send to the CTextBox object
		void initCCText();

		///set text as string also possible with overloades members for loacales, const char and text file, returns true if text was changed
		 bool setText(const std::string& stext, const int mode = ~CTextBox::AUTO_WIDTH, Font* font_text = NULL, const fb_pixel_t& color_text = 0, const int& style = FONT_STYLE_REGULAR);
		///set text with const char*, returns true if text was changed
			bool setText(const char* ctext, const int mode = ~CTextBox::AUTO_WIDTH, Font* font_text = NULL, const fb_pixel_t& color_text = 0, const int& style = FONT_STYLE_REGULAR);
		///set text from locale, returns true if text was changed
		 bool setText(neutrino_locale_t locale_text, const int mode = ~CTextBox::AUTO_WIDTH, Font* font_text = NULL, const fb_pixel_t& color_text = 0, const int& style = FONT_STYLE_REGULAR);
		///set text from digit, digit is integer, returns true if text was changed
		 bool setText(const int digit, const int mode = ~CTextBox::AUTO_WIDTH, Font* font_text = NULL, const fb_pixel_t& color_text = 0, const int& style = FONT_STYLE_REGULAR);
		///set text directly from a textfile, path as string is required, returns true if text was changed
		 bool setTextFromFile(const std::string& path_to_textfile, const int mode = ~CTextBox::AUTO_WIDTH, Font* font_text = NULL, const fb_pixel_t& color_text = 0, const int& style = FONT_STYLE_REGULAR);
		///get text directly from a textfile, path as string is required
		static std::string getTextFromFile(const std::string& path_to_textfile);
		///returns current text content of text/label object as std::string
		 std::string getText();

		///return current font
		Font* getFont();

		///set screen x-position, parameter as int
		 void setXPos(const int& xpos);
		///set screen y-position, parameter as int
		 void setYPos(const int& ypos);
		///set height of component on screen
		 void setHeight(const int& h);
		///set width of component on screen
		 void setWidth(const int& w);

		///helper to remove linebreak chars from a string if needed
		 void removeLineBreaks(std::string& str);

		///force paint of text even if text was changed or not
		 void forceTextPaint(bool force_text_paint = true);
		///gets the embedded CTextBox object, so it's possible to get access directly to its methods and properties
		 CTextBox* getCTextBoxObject();

		///returns count of lines from a text box page
		 int getTextLinesAutoHeight(const int& textMaxHeight, const int& textWidth, const int& mode);

		/**Member to modify background behavior of textbox object
		* @param[in]  mode
		* 	@li bool, default = true
		* @return
		*	void
		* @see
		* 	Parent member: CCTextScreen::enableTboxSaveScreen()
		* 	CTextBox::enableSaveScreen()
		* 	disableTboxSaveScreen()
		*/
		void enableTboxSaveScreen(bool mode)
		{
			cc_txt_save_screen = mode;
			if (ct_textbox)
				ct_textbox->enableSaveScreen(cc_txt_save_screen);
		}
		///enable/disable utf8 encoding
		void enableUTF8(bool enable = true);
		void disableUTF8(bool enable = false);
		/*!Clean up screen buffers from background layers.
		 * Paint cache and gradient cache not touched.
		 * The default basic methode CCDraw::clearSavedScreen() doesn't considering text bg screen, therefore
		 * we ensure also clean up the background of textbox object.
		 * Returns true if any buffer was cleane
		*/
		 bool clearSavedScreen();
// 		///set color gradient on/off, returns true if gradient mode was changed
// 		 bool enableColBodyGradient(const int& enable_mode, const fb_pixel_t& sec_color = 255 /*=COL_BACKGROUND*/);
};


//! Sub class of CComponentsText. Shows text with transparent background
class CComponentsTextTransp : public CComponentsText
{
	public:
		CComponentsTextTransp(	CComponentsForm *parent,
					const int x_pos = 10, const int y_pos = 10, const int w = 150, const int h = 50,
					std::string text = "",
					const int mode = CTextBox::AUTO_WIDTH,
					Font* font_text = NULL,
					const int& font_style = CComponentsText::FONT_STYLE_REGULAR,
					fb_pixel_t color_text = COL_MENUCONTENT_TEXT)
					:CComponentsText(x_pos, y_pos, w, h, text, mode, font_text, font_style, parent, CC_SHADOW_OFF, color_text)
					{
						doPaintBg(false);
						enableTboxSaveScreen(true);
					};
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
					const int& font_style = CComponentsText::FONT_STYLE_REGULAR,
					CComponentsForm *parent = NULL,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t color_text = COL_MENUCONTENTINACTIVE_TEXT,
					fb_pixel_t color_frame = COL_FRAME_PLUS_0,
					fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0,
					fb_pixel_t color_shadow = COL_SHADOW_PLUS_0)
					:CComponentsText(x_pos, y_pos, w, h, text, mode, font_text, font_style, parent, shadow_mode, color_text, color_frame, color_body, color_shadow)
		{
			cc_item_type.id 	= CC_ITEMTYPE_LABEL;
			cc_item_type.name	="cc_label_box";
		};

		CComponentsLabel(	CComponentsForm *parent,
					const int x_pos = 10, const int y_pos = 10, const int w = 150, const int h = 50,
					std::string text = "",
					const int mode = CTextBox::AUTO_WIDTH,
					Font* font_text = NULL,
					const int& font_style = CComponentsText::FONT_STYLE_REGULAR,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t color_text = COL_MENUCONTENTINACTIVE_TEXT,
					fb_pixel_t color_frame = COL_FRAME_PLUS_0,
					fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0,
					fb_pixel_t color_shadow = COL_SHADOW_PLUS_0)
					:CComponentsText(x_pos, y_pos, w, h, text, mode, font_text, font_style, parent, shadow_mode, color_text, color_frame, color_body, color_shadow)
		{
			cc_item_type.id		= CC_ITEMTYPE_LABEL;
			cc_item_type.name	="cc_label_box";
		};
};

#endif
