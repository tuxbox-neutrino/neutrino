/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean

	Hintbox based up initial code by
	Copyright (C) 2003 Ralf Gandy 'thegoodguy'
	Copyright (C) 2004 Sven Traenkle 'zwen'
	Copyright (C) 2008-2009, 2011, 2013 Stefan Seyfried

	Implementation of CComponent Window class.
	Copyright (C) 2014-2016 Thilo Graf 'dbt'

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

#define HINTBOX_MIN_WIDTH	320 // scaled in init
#define HINTBOX_MIN_HEIGHT	CFrameBuffer::getInstance()->scale2Res(125)
#define HINTBOX_MAX_HEIGHT	CFrameBuffer::getInstance()->scale2Res(520)

#define HINTBOX_DEFAULT_TIMEOUT g_settings.timing[SNeutrinoSettings::TIMING_POPUP_MESSAGES]
#define NO_TIMEOUT 0
#define DEFAULT_TIMEOUT -1
//frame around hint container as indent
#define W_FRAME OFFSET_INNER_MID
//frame color around hint/message box
#define HINTBOX_DEFAULT_FRAME_COLOR COL_FRAME
#define TIMEOUT_BAR_HEIGHT  OFFSET_SHADOW/2

#define DEFAULT_HINTBOX_TEXT_MODE (CTextBox::CENTER)
#define DEFAULT_HEADER_ICON NEUTRINO_ICON_INFO

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
		bool enable_txt_scroll;

		Font* hb_font;

		///timeout value, see also setTimeOut()
		int timeout;

		///timeout bar
		CProgressBar *timeout_pb;
		CComponentsTimer *timeout_pb_timer;

		///scroll handler, default down and for the 1st hint item (=0), NOTE: exec() must be called! see also scroll_down()/scroll_up()
		void Scroll(bool down, const uint& hint_id = 0);

		///main init handler
		void init(	const std::string& Text,
				const int& Width,
				const std::string& Picon,
				const int& header_buttons,
				const int& text_mode,
				const int& indent);

		virtual void ReSize();
		void showTimeOutBar(){enableTimeOutBar();}
		int getMaxWidth(const std::string& Text, const std::string& Title, Font *font, const int& minWidth);

	public:
		/**CHintBox Constructor
		* @param[in]	Caption
		* 	@li 	expects type neutrino_locale_t with locale entry from /system/locals.h
		* @param[in]	Text
		* 	@li 	expects type const char*, this is the message text inside the window, text is UTF-8 encoded
		* @param[in]	Width
		* 	@li 	optional: expects type int, defines box width, default value = HINTBOX_MIN_WIDTH
		* @param[in]	Icon
		* 	@li 	optional: expects type const char*, defines the icon name on the left side of titlebar, default = DEFAULT_HEADER_ICON
		* @param[in]	Picon
		* 	@li 	optional: expects type const char*, defines the picon name on the left side of message text, default = NULL (non Icon)
		* @param[in]	header_buttons
		* 	@li 	optional: expects type int, defines the icon name on the right side of titlebar, default = 0 (non Icon)
		* 	@see	class CComponentsWindow()
		* @param[in]	text_mode
		* 	@li 	optional: expects type int, defines the text modes for embedded text lines
		* 		Possible Modes defined in /gui/widget/textbox.h
		* 		AUTO_WIDTH
		* 		AUTO_HIGH
		* 		SCROLL
		* 		CENTER
		* 		RIGHT
		* 		TOP
		* 		BOTTOM
		* 		NO_AUTO_LINEBREAK
		* 		AUTO_LINEBREAK_NO_BREAKCHARS
		* @param[in]	indent
		* 	@li	optional: expects type int, defines indent of text
		*
		* 	@see	classes CComponentsText(), CTextBox()
		*/
		CHintBox(	const neutrino_locale_t Caption,
				const char * const Text,
				const int Width = HINTBOX_MIN_WIDTH,
				const char * const Icon = DEFAULT_HEADER_ICON,
				const char * const Picon = NULL,
				const int& header_buttons = 0,
				const int& text_mode = DEFAULT_HINTBOX_TEXT_MODE,
				const int& indent = W_FRAME);

		/**CHintBox Constructor
		* @param[in]	Caption
		* 	@li 	expects type const char*
		* 	@see	for other parameters take a look to basic class CHintBox()
		*/
		CHintBox(	const char * const Caption,
				const char * const Text,
				const int Width = HINTBOX_MIN_WIDTH,
				const char * const Icon = DEFAULT_HEADER_ICON,
				const char * const Picon = NULL,
				const int& header_buttons = 0,
				const int& text_mode = DEFAULT_HINTBOX_TEXT_MODE,
				const int& indent = W_FRAME);

		/**CHintBox Constructor
		* @param[in]	Caption
		* 	@li 	expects type neutrino_locale_t with locale entry from /system/locals.h
		* @param[in]	Text
		* 	@li 	expects type neutrino_locale_t with locale entry from /system/locals.h
		* 	@see	for other parameters take a look to basic class CHintBox()
		*/
		CHintBox(	const neutrino_locale_t Caption,
				const neutrino_locale_t Text,
				const int Width = HINTBOX_MIN_WIDTH,
				const char * const Icon = DEFAULT_HEADER_ICON,
				const char * const Picon = NULL,
				const int& header_buttons = 0,
				const int& text_mode = DEFAULT_HINTBOX_TEXT_MODE,
				const int& indent = W_FRAME);

		/**CHintBox Constructor
		* @param[in]	Caption
		* 	@li 	expects type const char*
		* @param[in]	Text
		* 	@li 	expects type neutrino_locale_t with locale entry from /system/locals.h
		* 	@see	for other parameters take a look to basic class CHintBox()
		*/
		CHintBox(	const char * const Caption,
				const neutrino_locale_t Text,
				const int Width = HINTBOX_MIN_WIDTH,
				const char * const Icon = DEFAULT_HEADER_ICON,
				const char * const Picon = NULL,
				const int& header_buttons = 0,
				const int& text_mode = DEFAULT_HINTBOX_TEXT_MODE,
				const int& indent = W_FRAME);

		virtual ~CHintBox();
		/**
		* exec caller
		* @return	int
		*/
		int exec();

		/**
		* Defines timeout for message window.
		* Timeout is enabled with parameter1 = DEFAULT_TIMEOUT (-1) or any other value > 0
		* To disable timeout use NO_TIMEOUT (0)
		* @param[in]	Timeout as int as seconds
		*/
		virtual void setTimeOut(const int& Timeout){timeout = Timeout;}

		/**
		* enable/disable visualized timeout as progressbar under titlebar
		* @param[in]	enable 
		* 	@li	optional: expects type bool, default = true
		*/
		void enableTimeOutBar(bool enable = true);

		/**
		* disable visualized timeout as progressbar
		* 	@see enableTimeOutBar
		*/
		void disableTimeOutBar(){enableTimeOutBar(false);}

		/**
		* scroll handler for text objects: NOTE: exec() must be called !
		* @param[in]	hint_id
		* 	@li 	optional: expects type unsigned int, default = 0
		* 		default for the 1st hint item (=0), item id arises from the order of added items with addHintItem(), default we have minimal one item with id=0
		* @see		Scroll()
		*/
		void scroll_up(const uint& hint_id = 0);

		/**
		* scroll down handler for text objects: NOTE: exec() must be called !
		* @param[in]	hint_id
		* 	@li 	expects type unsigned int, default = 0
		* 		default for the 1st hint item (=0), item id arises from the order of added items with addHintItem(), default we h
		* @see		Scroll()
		*/
		void scroll_down(const uint& hint_id = 0);

		/**
		* Member to add a hint item
		* @param[in]	Text
		* 	@li 	expects type std::string, this is the message text inside the window, text is UTF-8 encoded
		* @param[in]	text_mode
		* 	@li 	optional: expects type int, defines the text modes for embedded text lines
		* 		Possible Modes defined in /gui/widget/textbox.h
		* 		AUTO_WIDTH
		* 		AUTO_HIGH
		* 		SCROLL
		* 		CENTER
		* 		RIGHT
		* 		TOP
		* 		BOTTOM
		* 		NO_AUTO_LINEBREAK
		* 		AUTO_LINEBREAK_NO_BREAKCHARS
		* @param[in]	Picon
		* 	@li 	optional: expects type std::string, defines the picon name on the left side of message text, default = NULL (non Icon)
		* @param[in]	color_text
		* 	@li 	optional: expects type fb_pixel_t, defines the text color, default = COL_MENUCONTENT_TEXT
		* * @param[in]	font_text
		* 	@li 	optional: expects type Font*, defines the text font type, default = NULL for system preset for message contents
		*/
		void addHintItem(	const std::string& Text,
					const int& text_mode = DEFAULT_HINTBOX_TEXT_MODE,
					const std::string& Picon = std::string(),
					const fb_pixel_t& color_text = COL_MENUCONTENT_TEXT,
					Font* font_text = NULL);

		/**
		* Member to add a hint item from specified cc-item type
		* @param[in]	cc_Item
		* 	@li 	expects type CComponentsItem*, allows to add any possible cc-item type
		* 
		* 	@see	/gui/components/cc_types.h
		*/
		void addHintItem(CComponentsItem* cc_Item){ccw_body->addCCItem(cc_Item);}

		/**
		* Sets a text to a hint item.
		* @param[in]	Text
		* 	@li 	expects type std::string, this is the message text inside the hint item, text is UTF-8 encoded
		* @param[in]	hint_id
		* 	@li 	optional: expects type unsigned int, default = 0 for the first or one and only item
		* @param[in]	text_mode
		* 	@li 	optional: expects type int, defines the text modes for embedded text lines
		* 		Possible Modes defined in /gui/widget/textbox.h
		* 		AUTO_WIDTH
		* 		AUTO_HIGH
		* 		SCROLL
		* 		CENTER
		* 		RIGHT
		* 		TOP
		* 		BOTTOM
		* 		NO_AUTO_LINEBREAK
		* 		AUTO_LINEBREAK_NO_BREAKCHARS
		* 		default: CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH | CTextBox::CENTER
		* @param[in]	color_text
		* 	@li 	optional: expects type fb_pixel_t, defines the text color, default = COL_MENUCONTENT_TEXT
		* * @param[in]	style
		* 	@li 	optional: expects type int, defines the text style NOTE: only for dynamic font
		* 		possible styles are:
		* 		FONT_STYLE_REGULAR (default)
		*		FONT_STYLE_BOLD
		*		FONT_STYLE_ITALIC
		*/
		void setMsgText(const std::string& Text,
				const uint& hint_id = 0,
				const int& mode = CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH | CTextBox::CENTER,
				Font* font_text = NULL,
				const fb_pixel_t& color_text = COL_MENUCONTENT_TEXT,
				const int& style = CComponentsText::FONT_STYLE_REGULAR);
		void setMsgText(const neutrino_locale_t& locale,
				const uint& hint_id = 0,
				const int& mode = CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH | CTextBox::CENTER,
				Font* font_text = NULL,
				const fb_pixel_t& color_text = COL_MENUCONTENT_TEXT,
				const int& style = CComponentsText::FONT_STYLE_REGULAR);
};

/**
* Simplified methodes to show hintboxes on screen
* Text is UTF-8 encoded
* 	@see	for possible parameters take a look to CHintBox()
*/
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
		/**CHint Constructor
		* @param[in]	Text
		* 	@li 	expects type const char*, this is the message text inside the window, text is UTF-8 encoded
		* @param[in]	show_background
		* 	@li 	optional: expects type bool, enable/disable backround paint, default = true
		*/
		CHint(const char * const Text, bool show_background = true);
		/**CHint Constructor
		* @param[in]	Text
		* 	@li 	expects type neutrino_locale_t, this is the message text inside the window, text is UTF-8 encoded
		* @param[in]	show_background
		* 	@li 	optional: expects type bool, enable/disable backround paint, default = true
		*/
		CHint(const neutrino_locale_t Text, bool show_background = true);
};

/**
* Simplified methodes to show hintboxes without titlebar and footer
* Text is UTF-8 encoded
* @param[in]	timeout
* 	@li	optional: expects type int as seconds, default = HINTBOX_DEFAULT_TIMEOUT (get from settings)
* @param[in]	show_background
* 	@li 	optional: expects type bool, enable/disable backround paint, default = true
* 	@see	for possible text parameters take a look to CHintBox()
*/
int ShowHintS(const neutrino_locale_t Text, int timeout = HINTBOX_DEFAULT_TIMEOUT, bool show_background = true);
int ShowHintS(const char * const Text, int timeout = HINTBOX_DEFAULT_TIMEOUT, bool show_background = true);


#endif
