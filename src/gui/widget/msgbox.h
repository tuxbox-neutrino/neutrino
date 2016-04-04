/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean

	Initial implementation as an interface of the CMsgBox class
	Copyright (C) 2005 Günther
	Günther@tuxbox.berlios.org

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

#ifndef __C_MSGBOX__
#define __C_MSGBOX__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "hintbox.h"
#include <gui/widget/drawable.h>

#define MSGBOX_MIN_WIDTH HINTBOX_MIN_WIDTH
#define MSGBOX_MIN_HEIGHT HINTBOX_MIN_HEIGHT + 75


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
			mbrYes    = 0,
			mbrNo     = 1,
			mbrCancel = 2,
			mbrBack   = 3,
			mbrOk     = 4,
			mbrTimeout = 5,

			mbrNone = -1
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
			mbAll		= 0x100,
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
		void init(const int& Height = -1, const int& ShowButtons = -1, const msg_result_t& Default_result = mbrNone);
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

		void initButtons();
	public:
		/* Constructor */
		CMsgBox(const char* Text,
			const char* Title = NULL,
			const char* Icon = NULL,
			const char* Picon = NULL,
			const int& Width = MSGBOX_MIN_WIDTH,
			const int& Height = MSGBOX_MIN_HEIGHT,
			const int& ShowButtons = mbCancel,
			const msg_result_t& Default_result = mbrCancel,
			const int& Text_mode = CMsgBox::CENTER | CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH | CTextBox::SCROLL);

		CMsgBox(const char* Text,
			const neutrino_locale_t locale_Title = NONEXISTANT_LOCALE,
			const char* Icon = NULL,
			const char* Picon = NULL,
			const int& Width = MSGBOX_MIN_WIDTH,
			const int& Height = MSGBOX_MIN_HEIGHT,
			const int& ShowButtons = mbCancel,
			const msg_result_t& Default_result = mbrCancel,
			const int& Text_mode = CMsgBox::CENTER | CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH | CTextBox::SCROLL);

// 		~CMsgBox(); //inherited
		int     exec();

		///returns current result as msg_result_t, result corresponds to the current result value of selected button
		msg_result_t	getResult(){return result;};
		///returns current default result as msg_result_t, independently from current selected button result
		msg_result_t	getDefaultResult(){return default_result;};
		///sets current default result as msg_result_t, independently from current selected button result
		void	setDefaultResult(const msg_result_t& Default_result){default_result = Default_result;}

		/*!
		Sets the displayed buttons.
		This member allows to set and overrides already defined buttons from constructor,
		parameter ''ShowButtons'' accepts given types, find under button_define_t enumeration
			mbYes
			mbNo
			mbCancel
			mbBack
			mbOk
			mbOKCancel
			mbYesNoCancel
			mbYesNo
			mbAll
		*/
		void	setShowedButtons(const int& ShowButtons){mb_show_button = ShowButtons; initButtons();}

		///define timeout, timeout is enabled if parmeter 1 > -1, otherwise it will be disabled,
		void 	setTimeOut(const int& Timeout){timeout = Timeout;};
		///enable/disable defined timeout, otherwise it will be ignored, parameter 1: bool default = true
		void 	enableDefaultResultOnTimeOut(bool enable = true);

		/*!
		Default defined button text is already predefiend with parameter 'ShowButtons' from constructor.
		This member allows to define an alternate text for an already defined button,
		1st parameter ''showed button'' accepts
			mbYes
			mbNo
			mbCancel
			mbBack
			mbOk
		others will be ignored!
		2nd parameter sets the new text for button as std::string.
		Result values are not touched!
		*/
		void setButtonText(const int& showed_button, const std::string& text);

// 		bool	setText(const std::string* newText);
};

#define DEFAULT_TEXT_MODE (CMsgBox::CENTER | CMsgBox::AUTO_WIDTH | CMsgBox::AUTO_HIGH)

int ShowMsg2UTF(	const neutrino_locale_t Title,
						const char * const Text, 
						const CMsgBox::msg_result_t Default,
						const uint32_t ShowButtons, 
						const char * const Icon = NULL, 
						const int Width = MSGBOX_MIN_WIDTH,
						const int Timeout = -1,
						bool returnDefaultOnTimeout = false,
						const int& Text_mode = DEFAULT_TEXT_MODE,
						fb_pixel_t color_frame = HINTBOX_DEFAULT_FRAME_COLOR); // UTF-8

int ShowMsg2UTF(	const char * const Title,
						const char * const Text, 
						const CMsgBox::msg_result_t Default,
						const uint32_t ShowButtons, 
						const char * const Icon = NULL, 
						const int Width = MSGBOX_MIN_WIDTH,
						const int Timeout = -1,
						bool returnDefaultOnTimeout = false,
						const int& Text_mode = DEFAULT_TEXT_MODE,
						fb_pixel_t color_frame = HINTBOX_DEFAULT_FRAME_COLOR); // UTF-8

int ShowMsg(	const neutrino_locale_t Title,
						const char * const Text,
						const CMsgBox::msg_result_t Default,
						const uint32_t ShowButtons,
						const char * const Icon = NULL,
						const int Width = MSGBOX_MIN_WIDTH,
						const int Timeout = -1,
						bool returnDefaultOnTimeout = false,
						const int& Text_mode = DEFAULT_TEXT_MODE,
						fb_pixel_t color_frame = HINTBOX_DEFAULT_FRAME_COLOR); // UTF-8

int ShowMsg(	const char * const Title,
						const char * const Text,
						const CMsgBox::msg_result_t Default,
						const uint32_t ShowButtons,
						const char * const Icon = NULL,
						const int Width = MSGBOX_MIN_WIDTH,
						const int Timeout = -1,
						bool returnDefaultOnTimeout = false,
						const int& Text_mode = DEFAULT_TEXT_MODE,
						fb_pixel_t color_frame = HINTBOX_DEFAULT_FRAME_COLOR); // UTF-8

int ShowMsg(	const neutrino_locale_t Title,
						const std::string & Text,
						const CMsgBox::msg_result_t Default,
						const uint32_t ShowButtons,
						const char * const Icon = NULL,
						const int Width = MSGBOX_MIN_WIDTH,
						const int Timeout = -1,
						bool returnDefaultOnTimeout = false,
						const int& Text_mode = DEFAULT_TEXT_MODE,
						fb_pixel_t color_frame = HINTBOX_DEFAULT_FRAME_COLOR); // UTF-8

int ShowMsg(	const neutrino_locale_t Title,
						const neutrino_locale_t Text,
						const CMsgBox::msg_result_t Default,
						const uint32_t ShowButtons,
						const char * const Icon = NULL,
						const int Width = MSGBOX_MIN_WIDTH,
						const int Timeout = -1,
						bool returnDefaultOnTimeout = false,
						const int& Text_mode = DEFAULT_TEXT_MODE,
						fb_pixel_t color_frame = HINTBOX_DEFAULT_FRAME_COLOR); // UTF-8

int ShowMsg(	const std::string & Title,
						const std::string & Text,
						const CMsgBox::msg_result_t Default,
						const uint32_t ShowButtons,
						const char * const Icon = NULL,
						const int Width = MSGBOX_MIN_WIDTH,
						const int Timeout = -1,
						bool returnDefaultOnTimeout = false,
						const int& Text_mode = DEFAULT_TEXT_MODE,
						fb_pixel_t color_frame = HINTBOX_DEFAULT_FRAME_COLOR); // UTF-8

void DisplayErrorMessage(const char * const ErrorMsg, const int& Text_mode = DEFAULT_TEXT_MODE); // UTF-8
void DisplayErrorMessage(const char * const ErrorMsg, const neutrino_locale_t& caption, const int& Text_mode = DEFAULT_TEXT_MODE); // UTF-8
void DisplayErrorMessage(const char * const ErrorMsg, const std::string& caption, const int& Text_mode = DEFAULT_TEXT_MODE); // UTF-8
void DisplayInfoMessage(const char * const InfoMsg, const int& Text_mode = DEFAULT_TEXT_MODE, fb_pixel_t color_frame = COL_DARK_GRAY); // UTF-8
void DisplayInfoMessage(const char * const InfoMsg, const neutrino_locale_t& caption, const int& Text_mode = DEFAULT_TEXT_MODE, fb_pixel_t color_frame = COL_DARK_GRAY); // UTF-8
void DisplayInfoMessage(const char * const InfoMsg, const std::string& caption, const int& Text_mode = DEFAULT_TEXT_MODE, fb_pixel_t color_frame = COL_DARK_GRAY); // UTF-8
#endif
