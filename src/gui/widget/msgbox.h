/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean

	Initial implementation as an interface of the CMsgBox class
	Copyright (C) 2005 Günther
	Günther@tuxbox.berlios.org

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

#ifndef __C_MSGBOX__
#define __C_MSGBOX__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "hintbox.h"

#define MSGBOX_MIN_WIDTH HINTBOX_MIN_WIDTH
#define MSGBOX_MIN_HEIGHT HINTBOX_MIN_HEIGHT

#define MSGBOX_DEFAULT_TIMEOUT g_settings.timing[SNeutrinoSettings::TIMING_STATIC_MESSAGES]
#define DEFAULT_MSGBOX_TEXT_MODE (CMsgBox::CENTER | CMsgBox::AUTO_WIDTH | CMsgBox::AUTO_HIGH)

//! Sub class of CHintBox. Shows a window as a messagebox
/*!
CMsgBox provides a small window  with header, text and footer
with predefined buttons.
Optional you can add an icon in the header and/or beside left of
text and context buttons on the right site of header.
CMsgBox objects return predefined result values of type msg_result_t.
Button combinations are defined with button_define_t
*/
class CMsgBox : public CHintBox
{
	public:
		/* enum definition */
		enum msg_result_t
		{
			mbrYes    	= 0x01,
			mbrNo     	= 0x02,
			mbrCancel 	= 0x04,
			mbrBack   	= 0x08,
			mbrOk     	= 0x10,
			mbrTimeout 	= 0x20,

			mbrNone 	= 0x00
		};
		enum button_define_t
		{
			mbYes           = 0x01,
			mbNo            = 0x02,
			mbCancel        = 0x04,
			mbBack          = 0x08,
			mbOk            = 0x10,
			mbOKCancel	= 0x20,
			mbYesNoCancel	= 0x40,
			mbYesNo		= 0x80,
			mbNoYes		= 0x100,
			mbAll		= 0x200,
			//unused allign stuff, only for compatibility
			mbBtnAlignCenter1 = 0x0400, /* centered, large distances */
			mbBtnAlignCenter2 = 0x0800, /* centered, small distances */
			mbBtnAlignLeft    = 0x1000,
			mbBtnAlignRight   = 0x2000
		};

		enum modes //TODO
		{
			AUTO_WIDTH		= CTextBox::AUTO_WIDTH,
			AUTO_HIGH		= CTextBox::AUTO_HIGH,
			SCROLL			= CTextBox::SCROLL,
// 			TITLE			= 0x08,
// 			FOOT			= 0x10,
// 			BORDER			= 0x20,
			CENTER			= CTextBox::CENTER,
			NO_AUTO_LINEBREAK	= CTextBox::NO_AUTO_LINEBREAK
		};

	private:
		void init(	const int& Height,
				const int& ShowButtons,
				const msg_result_t& Default_result,
				const fb_pixel_t& color_frame,
				const fb_pixel_t& color_body,
				const fb_pixel_t& color_shadow,
				const int& frame_width);
		void refreshFoot(void);
		int		mb_show_button;

		///current result value of selected button, see also getResult()
		msg_result_t	result;
		///defined default result, independently from current selected button result, see also setDefaultResult(), getDefaultResult()
		msg_result_t	default_result;
		///enable/disable default result on timeout
		bool enable_timeout_result;
		///initialize basic timeout
		void initTimeOut();

		///alternate button text
		std::string btn_text_ok, btn_text_yes, btn_text_no, btn_text_cancel, btn_text_back;
		///assigned button captions
		std::string BTN_TEXT(const int& showed_button);

		///enables/disable button background
		bool btn_enable_bg;

		void initButtons();
	public:
		/**CMsgBox Constructor
		* @param[in]	Text
		* 	@li 	expects type const char*, this is the message text inside the window, text is UTF-8 encoded
		* @param[in]	Title
		* 	@li 	optional: expects type const char*, default = NULL, this causes default title "Information"
		* @param[in]	Icon
		* 	@li 	optional: expects type const char*, defines the icon name on the left side of titlebar, default = DEFAULT_HEADER_ICON
		* @param[in]	Picon
		* 	@li 	optional: expects type const char*, defines the picon name on the left side of message text, default = NULL (non Icon)
		* @param[in]	Width
		* 	@li 	optional: expects type int, defines box width, default value = MSGBOX_MIN_WIDTH
		* @param[in]	Height
		* 	@li 	optional: expects type int, defines box width, default value = MSGBOX_MIN_HEIGHT
		* @param[in]	ShowButtons
		* 	@li 	optional: expects type int, defines which buttons are available on screen, default value =  mbCancel
		* 	@see	setShowedButtons()
		* @param[in]	Default_result
		* 	@li 	optional: expects type int, defines default result value, default value =  mbrCancel
		* 		possible values are:
		* 		mbrYes    	= 0,
		* 		mbrNo     	= 1,
		* 		mbrCancel 	= 2,
		* 		mbrBack   	= 3,
		* 		mbrOk     	= 4,
		* 		mbrTimeout 	= 5,
		*
		* 		mbrNone 	= -1
		* 	@see	setDefaultResult(), getResult(); getDefaultResult(), enableDefaultResultOnTimeOut()
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
		* 		NOTE: default parameter to find in macro DEFAULT_MSGBOX_TEXT_MODE
		* @param[in]	color_frame
		* 	@li 	optional: expects type fb_pixel_t, defines frame color, default = HINTBOX_DEFAULT_FRAME_COLOR
		* @param[in]	color_body
		* 	@li 	optional: expects type fb_pixel_t, defines color color, default = COL_MENUCONTENT_PLUS_0
		* @param[in]	color_shadow
		* 	@li 	optional: expects type fb_pixel_t, defines shadow color, default = COL_SHADOW_PLUS_0
		* @param[in]	frame_width
		* 	@li 	optional: expects type int, defines frame width around, default = 0
		*
		* 	@see	class CHintBox()
		*/
		CMsgBox(const char* Text,
			const char* Title = NULL,
			const char* Icon = DEFAULT_HEADER_ICON,
			const char* Picon = NULL,
			const int& Width = MSGBOX_MIN_WIDTH,
			const int& Height = MSGBOX_MIN_HEIGHT,
			const int& ShowButtons = mbCancel,
			const msg_result_t& Default_result = mbrNone,
			const int& Text_mode = DEFAULT_MSGBOX_TEXT_MODE,
			const fb_pixel_t& color_frame = HINTBOX_DEFAULT_FRAME_COLOR,
			const fb_pixel_t& color_body = COL_MENUCONTENT_PLUS_0,
			const fb_pixel_t& color_shadow = COL_SHADOW_PLUS_0,
			const int& frame_width = HINTBOX_DEFAULT_FRAME_WIDTH
			);

		/**CMsgBox Constructor
		* @param[in]	Text
		* 	@li 	expects type const char*, this is the message text inside the window, text is UTF-8 encoded
		* @param[in]	Title
		* 	@li 	optional: expects type neutrino_locale_t with locale entry from /system/locals.h default = NONEXISTANT_LOCALE, this causes default title "Information"
		* @param[in]	Icon
		* 	@li 	optional: expects type const char*, defines the icon name on the left side of titlebar, default = DEFAULT_HEADER_ICON
		* @param[in]	Picon
		* 	@li 	optional: expects type const char*, defines the picon name on the left side of message text, default = NULL (non Icon)
		* @param[in]	Width
		* 	@li 	optional: expects type int, defines box width, default value = MSGBOX_MIN_WIDTH
		* @param[in]	Height
		* 	@li 	optional: expects type int, defines box width, default value = MSGBOX_MIN_HEIGHT
		* @param[in]	ShowButtons
		* 	@li 	optional: expects type int, defines which buttons are available on screen, default value =  mbCancel
		* 	@see	setShowedButtons()
		* @param[in]	Default_result
		* 	@li 	optional: expects type msg_result_t, defines default result value, default value =  mbrNone
		* 		possible values are:
		* 		mbrYes,
		* 		mbrNo,
		* 		mbrCancel,
		* 		mbrBack,
		* 		mbrOk,
		* 		mbrTimeout,
		* 		mbrNone
		* 	@see	setDefaultResult(), getResult(); getDefaultResult(), enableDefaultResultOnTimeOut()
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
		* 		NOTE: default parameter to find in macro DEFAULT_MSGBOX_TEXT_MODE
		* @param[in]	color_frame
		* 	@li 	optional: expects type fb_pixel_t, defines frame color, default = HINTBOX_DEFAULT_FRAME_COLOR
		* @param[in]	color_body
		* 	@li 	optional: expects type fb_pixel_t, defines color color, default = COL_MENUCONTENT_PLUS_0
		* @param[in]	color_shadow
		* 	@li 	optional: expects type fb_pixel_t, defines shadow color, default = COL_SHADOW_PLUS_0
		* @param[in]	frame_width
		* 	@li 	optional: expects type int, defines frame width around, default = 0
		* 	@see	class CHintBox()
		*/
		CMsgBox(const char* Text,
			const neutrino_locale_t locale_Title = NONEXISTANT_LOCALE,
			const char* Icon = DEFAULT_HEADER_ICON,
			const char* Picon = NULL,
			const int& Width = MSGBOX_MIN_WIDTH,
			const int& Height = MSGBOX_MIN_HEIGHT,
			const int& ShowButtons = mbCancel,
			const msg_result_t& Default_result = mbrNone,
			const int& Text_mode = DEFAULT_MSGBOX_TEXT_MODE,
			const fb_pixel_t& color_frame = HINTBOX_DEFAULT_FRAME_COLOR,
			const fb_pixel_t& color_body = COL_MENUCONTENT_PLUS_0,
			const fb_pixel_t& color_shadow = COL_SHADOW_PLUS_0,
			const int& frame_width = HINTBOX_DEFAULT_FRAME_WIDTH
		);

		virtual ~CMsgBox(){};
		/**
		* exec caller
		* @return	int
		*/
		int exec();

		/**
		* returns current result as msg_result_t, result corresponds to the current result value of selected button
		* @return	result as int
		*/

		msg_result_t	getResult(){return result;}
		/**
		* returns current default result as msg_result_t, independently from current selected button result
		* @return	result as int
		*/

		msg_result_t	getDefaultResult(){return default_result;}
		/**
		* sets current default result as msg_result_t, independently from current selected button result
		* @param[in]	Default_result
		* 	@li 	expects type msg_result_t
		*/
		void	setDefaultResult(const msg_result_t& Default_result){default_result = Default_result;}

		/**
		* Sets the displayed buttons.
		* This member allows to set and overrides already defined buttons from constructor,
		* parameter ''ShowButtons'' accepts given types, find under button_define_t enumeration
		* @param[in]	ShowButtons
		* 	@li 	optional: expects type int, defines which buttons are available on screen, default value =  mbCancel
		* 		possible values are:
		* 		mbYes           = 0x01,
		* 		mbNo            = 0x02,
		* 		mbCancel        = 0x04,
		* 		mbBack          = 0x08,
		* 		mbOk            = 0x10,
		* 		mbOKCancel	= 0x20,
		* 		mbYesNoCancel	= 0x40,
		* 		mbYesNo		= 0x80,
		* 		mbNoYes		= 0x100,
		* 		mbAll		= 0x200,
		* 		NOTE: allign parameters are currently not supported, these values are existing for compatibility only!
		*/
		void	setShowedButtons(const int& ShowButtons){mb_show_button = ShowButtons; initButtons();}

		/**
		* define timeout, timeout is enabled if parmeter 1 > -1, otherwise it will be disabled,
		* @param[in]	Timeout
		* 	@li 	expects type int
		*/
		void 	setTimeOut(const int& Timeout){timeout = Timeout;};

		/**
		* enable/disable defined timeout, otherwise it will be ignored
		* @param[in]	enable
		* 	@li 	expects type bool, default = true
		*/
		void 	enableDefaultResultOnTimeOut(bool enable = true);

		/**
		* Default defined button text is already predefiend with parameter 'ShowButtons' from constructor.
		* This member allows to define an alternate text for an already defined button,
		* Result values are not touched!
		* @param[in]	showed_button
		* 	@li 	expects type int
		* 	@see	setShowedButtons()
		* @param[in]	text
		* 	@li 	expects type std::string, sets the new text for button
		*/
		void setButtonText(const int& showed_button, const std::string& text);

		/**
		* enables background of buttons
		* @param[in]	enable
		* 	@li 	expects type bool, default = true
		*/
		void enableButtonBg(bool enable = true);

		/**
		* disables background of buttons
		*/
		void disableButtonBg(){enableButtonBg(false);}

// 		bool	setText(const std::string* newText);
};

int ShowMsg2UTF(	const neutrino_locale_t Title,
						const char * const Text, 
						const CMsgBox::msg_result_t Default,
						const uint32_t ShowButtons, 
						const char * const Icon = NULL, 
						const int Width = MSGBOX_MIN_WIDTH,
						const int Timeout = NO_TIMEOUT,
						bool returnDefaultOnTimeout = false,
						const int& Text_mode = DEFAULT_MSGBOX_TEXT_MODE,
						fb_pixel_t color_frame = HINTBOX_DEFAULT_FRAME_COLOR); // UTF-8

int ShowMsg2UTF(	const char * const Title,
						const char * const Text, 
						const CMsgBox::msg_result_t Default,
						const uint32_t ShowButtons, 
						const char * const Icon = NULL, 
						const int Width = MSGBOX_MIN_WIDTH,
						const int Timeout = NO_TIMEOUT,
						bool returnDefaultOnTimeout = false,
						const int& Text_mode = DEFAULT_MSGBOX_TEXT_MODE,
						fb_pixel_t color_frame = HINTBOX_DEFAULT_FRAME_COLOR); // UTF-8

int ShowMsg(	const neutrino_locale_t Title,
						const char * const Text,
						const CMsgBox::msg_result_t Default,
						const uint32_t ShowButtons,
						const char * const Icon = NULL,
						const int Width = MSGBOX_MIN_WIDTH,
						const int Timeout = NO_TIMEOUT,
						bool returnDefaultOnTimeout = false,
						const int& Text_mode = DEFAULT_MSGBOX_TEXT_MODE,
						fb_pixel_t color_frame = HINTBOX_DEFAULT_FRAME_COLOR); // UTF-8

int ShowMsg(	const char * const Title,
						const char * const Text,
						const CMsgBox::msg_result_t Default,
						const uint32_t ShowButtons,
						const char * const Icon = NULL,
						const int Width = MSGBOX_MIN_WIDTH,
						const int Timeout = NO_TIMEOUT,
						bool returnDefaultOnTimeout = false,
						const int& Text_mode = DEFAULT_MSGBOX_TEXT_MODE,
						fb_pixel_t color_frame = HINTBOX_DEFAULT_FRAME_COLOR); // UTF-8

int ShowMsg(	const neutrino_locale_t Title,
						const std::string & Text,
						const CMsgBox::msg_result_t Default,
						const uint32_t ShowButtons,
						const char * const Icon = NULL,
						const int Width = MSGBOX_MIN_WIDTH,
						const int Timeout = NO_TIMEOUT,
						bool returnDefaultOnTimeout = false,
						const int& Text_mode = DEFAULT_MSGBOX_TEXT_MODE,
						fb_pixel_t color_frame = HINTBOX_DEFAULT_FRAME_COLOR); // UTF-8

int ShowMsg(	const neutrino_locale_t Title,
						const neutrino_locale_t Text,
						const CMsgBox::msg_result_t Default,
						const uint32_t ShowButtons,
						const char * const Icon = NULL,
						const int Width = MSGBOX_MIN_WIDTH,
						const int Timeout = NO_TIMEOUT,
						bool returnDefaultOnTimeout = false,
						const int& Text_mode = DEFAULT_MSGBOX_TEXT_MODE,
						fb_pixel_t color_frame = HINTBOX_DEFAULT_FRAME_COLOR); // UTF-8

int ShowMsg(	const std::string & Title,
						const std::string & Text,
						const CMsgBox::msg_result_t Default,
						const uint32_t ShowButtons,
						const char * const Icon = NULL,
						const int Width = MSGBOX_MIN_WIDTH,
						const int Timeout = NO_TIMEOUT,
						bool returnDefaultOnTimeout = false,
						const int& Text_mode = DEFAULT_MSGBOX_TEXT_MODE,
						fb_pixel_t color_frame = HINTBOX_DEFAULT_FRAME_COLOR); // UTF-8

int ShowMsg(	const std::string & Title,
						const neutrino_locale_t Text,
						const CMsgBox::msg_result_t Default,
						const uint32_t ShowButtons,
						const char * const Icon = NULL,
						const int Width = MSGBOX_MIN_WIDTH,
						const int Timeout = NO_TIMEOUT,
						bool returnDefaultOnTimeout = false,
						const int& Text_mode = DEFAULT_MSGBOX_TEXT_MODE,
						fb_pixel_t color_frame = HINTBOX_DEFAULT_FRAME_COLOR); // UTF-8

void DisplayErrorMessage(const char * const ErrorMsg, const int& Timeout = NO_TIMEOUT, const int& Text_mode = DEFAULT_MSGBOX_TEXT_MODE); // UTF-8
void DisplayErrorMessage(const char * const ErrorMsg, const neutrino_locale_t& caption, const int& Timeout = NO_TIMEOUT, const int& Text_mode = DEFAULT_MSGBOX_TEXT_MODE); // UTF-8
void DisplayErrorMessage(const char * const ErrorMsg, const std::string& caption, const int& Timeout = NO_TIMEOUT, const int& Text_mode = DEFAULT_MSGBOX_TEXT_MODE); // UTF-8
void DisplayInfoMessage(const char * const InfoMsg, const int& Timeout = DEFAULT_TIMEOUT, const int& Text_mode = DEFAULT_MSGBOX_TEXT_MODE, fb_pixel_t color_frame = COL_DARK_GRAY); // UTF-8
void DisplayInfoMessage(const char * const InfoMsg, const neutrino_locale_t& caption, const int& Timeout = DEFAULT_TIMEOUT, const int& Text_mode = DEFAULT_MSGBOX_TEXT_MODE, fb_pixel_t color_frame = COL_DARK_GRAY); // UTF-8
void DisplayInfoMessage(const char * const InfoMsg, const std::string& caption, const int& Timeout = DEFAULT_TIMEOUT, const int& Text_mode = DEFAULT_MSGBOX_TEXT_MODE, fb_pixel_t color_frame = COL_DARK_GRAY); // UTF-8
#endif
