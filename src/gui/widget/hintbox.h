/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean

	Hintbox based up initial code by
	Copyright (C) 2003 Ralf Gandy 'thegoodguy'
	Copyright (C) 2004 Sven Traenkle 'zwen'
	Copyright (C) 2008-2009, 2011, 2013 Stefan Seyfried

	Implementation of CComponent Window class.
	Copyright (C) 2014-2015 Thilo Graf 'dbt'

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


#ifndef __C_HINTBOX__
#define __C_HINTBOX__

#include <gui/components/cc.h>

#define HINTBOX_MIN_WIDTH 600
#define HINTBOX_MIN_HEIGHT 125
#define HINTBOX_MAX_HEIGHT 420
#define HINTBOX_DEFAULT_TIMEOUT 5
//frame around hint container as indent
#define W_FRAME 15
//frame color around hint/message box
#define HINTBOX_DEFAULT_FRAME_COLOR COL_MENUCONTENT_PLUS_6

//! Sub class of CComponentsWindow. Shows a window as a hintbox with text and optional icon beside of text.
/*!
CHintBox provides a small window with header and a text item,
optional you can add an icon in the header and/or beside left of
text and context buttons on the right site of header.
*/

class CHintBox : public CComponentsWindow
{
	protected:
		int y_hint_obj;
		int h_hint_obj;
		int w_indentation;

		///global count of lines
		uint lines;

		///timeout value, see also setTimeOut()
		int timeout;

		///content container object, contains the hint objects, it's a child of body object
		CComponentsFrmChain *obj_content;

		///scroll handler, default down and for the 1st hint item (=0), NOTE: exec() must be called! see also scroll_down()/scroll_up()
		void Scroll(bool down, const uint& hint_id = 0);

		///main init handler
		void init(	const std::string& Text,
				const int& Width,
				const std::string& Picon,
				const int& header_buttons,
				const int& text_mode,
				const int& indent);

		void ReSize();

	public:
		// Text is UTF-8 encoded
		CHintBox(	const neutrino_locale_t Caption,
				const char * const Text,
				const int Width = HINTBOX_MIN_WIDTH,
				const char * const Icon = NULL,
				const char * const Picon = NULL,
				const int& header_buttons = 0,
				const int& text_mode = 0,
				const int& indent = W_FRAME);

		CHintBox(	const char * const Caption,
				const char * const Text,
				const int Width = HINTBOX_MIN_WIDTH,
				const char * const Icon = NULL,
				const char * const Picon = NULL,
				const int& header_buttons = 0,
				const int& text_mode = 0,
				const int& indent = W_FRAME);

		CHintBox(	const neutrino_locale_t Caption,
				const neutrino_locale_t Text,
				const int Width = HINTBOX_MIN_WIDTH,
				const char * const Icon = NULL,
				const char * const Picon = NULL,
				const int& header_buttons = 0,
				const int& text_mode = 0,
				const int& indent = W_FRAME);

		CHintBox(	const char * const Caption,
				const neutrino_locale_t Text,
				const int Width = HINTBOX_MIN_WIDTH,
				const char * const Icon = NULL,
				const char * const Picon = NULL,
				const int& header_buttons = 0,
				const int& text_mode = 0,
				const int& indent = W_FRAME);

		//~CHintBox(); //inherited
		int exec();

		///define timeout, timeout is enabled if parameter1 > -1
		virtual void setTimeOut(const int& Timeout){timeout = Timeout;};

		///scroll handler for text objects: NOTE: exec() must be called ! see also Scroll()
		///scroll up handler, default for the 1st hint item (=0), item id arises from the order of added items with addHintItem(), default we have minimal one item with id=0
		void scroll_up(const uint& hint_id = 0);
		///scroll down handler, default for the 1st hint item (=0), item id arises from the order of added items with addHintItem(), default we have minimal one item with id=0
		void scroll_down(const uint& hint_id = 0);

		///specialized member to add a hint item, parameter1: UTF8 text that will be showed, parameter2: (optional) textmode, see gui/widget/textbox.h for possible modes, parameter3: (optional) = an icon that will be showed left beside of text
		void addHintItem(	const std::string& Text,
					const int& text_mode = 0,
					const std::string& Picon = std::string(),
					const u_int8_t& at_page_number = 0,
					const fb_pixel_t& color_text = COL_MENUCONTENT_TEXT,
					Font* font_text = NULL);
		///add any cc-item to body object, also usable is addWindowItem() to add items to the window body object
		void addHintItem(CComponentsItem* cc_Item){obj_content->addCCItem(cc_Item);}

		/*!
		 * Sets a new text to a hint item,
		   2nd parameter expects an item number, default = 0 (1st item). Mostly this should be the only one, but if more than one items are exist, it's possible to select a target item.
		   3rd parameter expects modes from CTextBox (default = CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH | CTextBox::CENTER)
			AUTO_WIDTH
			AUTO_HIGH
			SCROLL
			CENTER
			RIGHT
			TOP
			BOTTOM
			NO_AUTO_LINEBREAK
			AUTO_LINEBREAK_NO_BREAKCHARS
		   4th parameter font_text expects a font object, if default value = NULL, then g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO] is used
		   5th parameter expects value from /gui/color.h or compatible color numbers
		   6th parameter expects
			FONT_STYLE_REGULAR (default)
			FONT_STYLE_BOLD
			FONT_STYLE_ITALIC
		*/
		void setMsgText(const std::string& Text,
				const uint& hint_id = 0,
				const int& mode = CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH | CTextBox::CENTER,
				Font* font_text = NULL,
				const fb_pixel_t& color_text = COL_MENUCONTENT_TEXT,
				const int& style = CComponentsText::FONT_STYLE_REGULAR);
};

// Text is UTF-8 encoded
int ShowHint(const neutrino_locale_t Caption, const char * const Text, const int Width = HINTBOX_MIN_WIDTH, int timeout = HINTBOX_DEFAULT_TIMEOUT, const char * const Icon = NULL, const char * const Picon = NULL, const int& header_buttons = 0);
int ShowHint(const neutrino_locale_t Caption, const neutrino_locale_t Text, const int Width = HINTBOX_MIN_WIDTH, int timeout = HINTBOX_DEFAULT_TIMEOUT, const char * const Icon = NULL, const char * const Picon = NULL, const int& header_buttons = 0);
int ShowHint(const char * const Caption, const char * const Text, const int Width = HINTBOX_MIN_WIDTH, int timeout = HINTBOX_DEFAULT_TIMEOUT, const char * const Icon = NULL, const char * const Picon = NULL, const int& header_buttons = 0);
int ShowHint(const char * const Caption, const neutrino_locale_t Text, const int Width = HINTBOX_MIN_WIDTH, int timeout = HINTBOX_DEFAULT_TIMEOUT, const char * const Icon = NULL, const char * const Picon = NULL, const int& header_buttons = 0);



//! Sub class of CHintBox. Shows a simplified hint as a text hint without header and footer.
/*!
CHint provides a text without header and footer,
optional disable/enable background
*/

class CHint : public CHintBox
{
	public:
		CHint(const char * const Text, bool show_background = true);
		CHint(const neutrino_locale_t Text, bool show_background = true);
};

//methods to show simplified hints
int ShowHintS(const neutrino_locale_t Text, int timeout = HINTBOX_DEFAULT_TIMEOUT, bool show_background = true);
int ShowHintS(const char * const Text, int timeout = HINTBOX_DEFAULT_TIMEOUT, bool show_background = true);


#endif
