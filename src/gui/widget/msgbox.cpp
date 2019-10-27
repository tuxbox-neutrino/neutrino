/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean

	Initial implementation as an interface of the CMsgBox class
	Copyright (C) 2005 Günther
	Günther@tuxbox.berlios.org

	Implementation of CComponent Window class.
	Copyright (C) 2014-2017 Thilo Graf 'dbt'

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

#include "msgbox.h"
#include <system/debug.h>

#define MAX_WINDOW_WIDTH  (g_settings.screen_EndX - g_settings.screen_StartX )
#define MAX_WINDOW_HEIGHT (g_settings.screen_EndY - g_settings.screen_StartY - 40)

#define MIN_WINDOW_WIDTH  (MAX_WINDOW_WIDTH>>1)
#define MIN_WINDOW_HEIGHT 40

using namespace std; /* TODO: remove all std:: prefixes in this file */

CMsgBox::CMsgBox(	const char* Text,
			const char* Title,
			const char* Icon,
			const char* Picon,
			const int& Width,
			const int& Height,
			const int& ShowButtons,
			const msg_result_t& Default_result,
			const int& Text_mode,
			const fb_pixel_t& color_frame,
			const fb_pixel_t& color_body,
			const fb_pixel_t& color_shadow,
			const int& frame_width) : CHintBox(	Title,
								Text,
								Width,
								Icon,
								Picon,
								0,
								Text_mode)
{
	init(Height, ShowButtons, Default_result, color_frame, color_body, color_shadow, frame_width);
}

CMsgBox::CMsgBox(	const char* Text,
			const neutrino_locale_t locale_Title,
			const char* Icon,
			const char* Picon,
			const int& Width,
			const int& Height,
			const int& ShowButtons,
			const msg_result_t& Default_result,
			const int& Text_mode,
			const fb_pixel_t& color_frame,
			const fb_pixel_t& color_body,
			const fb_pixel_t& color_shadow,
			const int& frame_width) : CHintBox(	locale_Title,
								Text,
								Width,
								Icon,
								Picon,
								0,
								Text_mode)
{
	init(Height, ShowButtons, Default_result, color_frame, color_body, color_shadow, frame_width);
}

void CMsgBox::init(	const int& Height,
			const int& ShowButtons,
			const msg_result_t& Default_result,
			const fb_pixel_t& color_frame,
			const fb_pixel_t& color_body,
			const fb_pixel_t& color_shadow,
			const int& frame_width)
{
	cc_item_type.name = "msgbox";

	initTimeOut();

	col_frame	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;
	fr_thickness	= frame_width;

	//enable footer and add its height
	showFooter(true);
#if 0
	ccw_h_footer = ccw_footer->getHeight()+OFFSET_INNER_MID;
	ccw_footer->setHeight(ccw_h_footer);
#endif
	btn_enable_bg = true;
	ccw_col_footer = ccw_body->getColorBody();
	ccw_footer->doPaintBg(false);
	int h_current = height;
	h_current += ccw_footer->getHeight();
	height = max(max(MSGBOX_MIN_HEIGHT, Height), h_current);

	//ensure matching height for screen
	height = min(MAX_WINDOW_HEIGHT, height);
	width = min(MAX_WINDOW_WIDTH, width);

	shadow = CC_SHADOW_ON;

	//set result
	result = default_result = Default_result;

	//add and initialize footer buttons with required buttons and basic properties
	if (ShowButtons > -1)
		mb_show_button = ShowButtons;
	initButtons();
}

void CMsgBox::initTimeOut()
{
	timeout		= NO_TIMEOUT;
	enable_timeout_result = false;
}

void CMsgBox::initButtons()
{
	button_label_cc btn;
	vector<button_label_cc> v_buttons;

	//evaluate combinations
	if (mb_show_button & mbAll)
		mb_show_button = (mbOk|mbYes|mbNo|mbCancel|mbBack); //stupid! only demo
	if (mb_show_button & mbOKCancel)
		mb_show_button = (mbOk|mbCancel);
	if (mb_show_button & mbYesNoCancel)
		mb_show_button = (mbYes|mbNo|mbCancel);
	if (mb_show_button & mbYesNo)
		mb_show_button = (mbYes|mbNo);

	//assign button text, result values, direct keys and alternate keys
	if (mb_show_button & mbOk){
		btn.button = NEUTRINO_ICON_BUTTON_OKAY;
		btn.text = BTN_TEXT(mbOk);
		btn.directKeys.clear();
		btn.directKeys.push_back(CRCInput::RC_ok);
		btn.btn_result = mbrOk;
		btn.btn_alias = mbOk;
		v_buttons.push_back(btn);
	}
	if (mb_show_button & mbNo){
		btn.button = NEUTRINO_ICON_BUTTON_RED;
		btn.text = BTN_TEXT(mbNo);
		btn.directKeys.clear();
		btn.directKeys.push_back(CRCInput::RC_red);
		btn.directKeys.push_back(CRCInput::RC_home);
		btn.btn_result = mbrNo;
		btn.btn_alias = mbNo;
		v_buttons.push_back(btn);
	}
	if (mb_show_button & mbYes){
		btn.button = NEUTRINO_ICON_BUTTON_GREEN;
		btn.text = BTN_TEXT(mbYes);
		btn.directKeys.clear();
		btn.directKeys.push_back(CRCInput::RC_green);
		btn.directKeys.push_back(CRCInput::RC_ok);
		btn.btn_result = mbrYes;
		btn.btn_alias = mbYes;
		v_buttons.push_back(btn);
	}
	if (mb_show_button & mbCancel){
		btn.button = NEUTRINO_ICON_BUTTON_HOME;
		btn.text = BTN_TEXT(mbCancel);
		btn.directKeys.clear();
		btn.directKeys.push_back(CRCInput::RC_home);
		btn.directKeys.push_back(CRCInput::RC_setup);
		btn.btn_result = mbrCancel;
		btn.btn_alias = mbCancel;
		v_buttons.push_back(btn);
	}
	if (mb_show_button & mbBack){
		btn.button = NEUTRINO_ICON_BUTTON_HOME;
		btn.text = BTN_TEXT(mbBack);
		btn.directKeys.clear();
		btn.directKeys.push_back(CRCInput::RC_home);
		btn.btn_result = mbrBack;
		btn.btn_alias = mbBack;
		v_buttons.push_back(btn);
	}
	if (mb_show_button & mbNoYes){
		btn.button = NEUTRINO_ICON_BUTTON_RED;
		btn.text = BTN_TEXT(mbYes);
		btn.directKeys.clear();
		btn.directKeys.push_back(CRCInput::RC_red);
		btn.directKeys.push_back(CRCInput::RC_ok);
		btn.btn_result = mbrYes;
		btn.btn_alias = mbYes;
		v_buttons.push_back(btn);

		btn.button = NEUTRINO_ICON_BUTTON_GREEN;
		btn.text = BTN_TEXT(mbNo);
		btn.directKeys.clear();
		btn.directKeys.push_back(CRCInput::RC_green);
		btn.directKeys.push_back(CRCInput::RC_home);
		btn.btn_result = mbrNo;
		btn.btn_alias = mbNo;
		v_buttons.push_back(btn);
	}

	ccw_footer->setButtonLabels(v_buttons, 0, 125);
	ccw_footer->getButtonChainObject()->setColorBody(col_body);

	//show buttons with background and shadow
	ccw_footer->enableButtonBg(btn_enable_bg);
	ccw_footer->enableButtonShadow(CC_SHADOW_ON, OFFSET_SHADOW/2, true);

	//set position of meassage window and refresh window properties
	setCenterPos(CC_ALONG_X);
	y = frameBuffer->getScreenY() + ((frameBuffer->getScreenHeight() - height) >> 2),
	Refresh();

	//set the 1st button as default selected button
	ccw_footer->setSelectedButton(0);

	//define default selected button from default_result
	if (v_buttons.size() > 1){
		for (size_t i = 0; i< v_buttons.size(); i++){
			if (v_buttons[i].btn_result == default_result){
				ccw_footer->setSelectedButton(i);
				break;
			}
		}
	}
}



void CMsgBox::enableDefaultResultOnTimeOut(bool enable)
{
	enable_timeout_result = enable;
}

int CMsgBox::exec()
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;
	int res = menu_return::RETURN_REPAINT;

	ccw_footer->getSelectedButtonObject()->setButtonAlias(mb_show_button);
	int selected = ccw_footer->getSelectedButton();
	if (timeout == NO_TIMEOUT)
		timeout = 0;
	if (timeout == DEFAULT_TIMEOUT)
		timeout = g_settings.timing[SNeutrinoSettings::TIMING_STATIC_MESSAGES];

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(timeout);

	if (timeout > 0)
		enableTimeOutBar();

	bool loop = true;
	while (loop)
	{
		g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd );


		//***timeout result***
		if (msg == CRCInput::RC_timeout && timeout > 0)
		{
			result = enable_timeout_result ? default_result : mbrTimeout;
			loop = false;
		}
#if 0
		else if (((msg == CRCInput::RC_timeout && timeout > 0) ||
			  (msg  == (neutrino_msg_t)g_settings.key_channelList_cancel)) &&
			  (mb_show_button & (mbCancel | mbBack)))
		{
			result = (mb_show_button & mbCancel) ? mbrCancel : (mb_show_button & mbOk) ? mbrOk: mbrBack;
			loop   = false;
		}
#endif
		//***navi buttons for scroll***
		else if (msg == CRCInput::RC_up )
		{
			scroll_up();
		}
		else if (msg == CRCInput::RC_down)
		{
			scroll_down();
		}
		//***navi buttons for button selection***
		else if(msg == CRCInput::RC_right || msg == CRCInput::RC_left)
		{
			if (msg == CRCInput::RC_right)
				ccw_footer->setSelectedButton(selected+1);
			else
				ccw_footer->setSelectedButton(selected-1);
			mb_show_button = ccw_footer->getSelectedButtonObject()->getButtonAlias();
			selected = ccw_footer->getSelectedButton();

			//***refresh buttons only if we have more than one button, this avoids unnecessary repaints with possible flicker effects***
			if (ccw_footer->getButtonChainObject()->size()>1)
				refreshFoot();

			//***refresh timeout on any pressed navi key! This resets current timeout end to initial value***
			if (timeout > 0) {
				timeout_pb->setValues(0, timeout);
				timeoutEnd = CRCInput::calcTimeoutEnd(timeout);
			}
			dprintf(DEBUG_INFO, "\033[32m[CMsgBox]   [%s - %d] result = %d, mb_show_button = %d\033[0m\n", __func__, __LINE__, result, mb_show_button);
		}

		//***action buttons without preselection***
		for (size_t i = 0; i< ccw_footer->getButtonChainObject()->size(); i++){
			CComponentsButton* btn_action = static_cast<CComponentsButton*>(ccw_footer->getButtonChainObject()->getCCItem(i));
			if (btn_action->hasButtonDirectKey(msg)){
				result = (msg_result_t)btn_action->getButtonResult();
				dprintf(DEBUG_INFO, "\033[32m[CMsgBox]   [%s - %d] result = %d, mb_show_button = %d\033[0m\n", __func__, __LINE__, result, mb_show_button);
				loop = false;
			}
		}
		//***action button 'ok' handled with selected button and its predefined result***
		if ((msg == CRCInput::RC_ok) && (ccw_footer->getSelectedButtonObject()->getButtonAlias() == mb_show_button)){
			result = (msg_result_t)ccw_footer->getSelectedButtonObject()->getButtonResult();
			loop = false;
		}
		//***action button 'home' with general cancel result***
		else if (msg == CRCInput::RC_home){
			result = mbrCancel;
			loop = false;
		}
		//***ignore***
		else if (CNeutrinoApp::getInstance()->listModeKey(msg)){
			// do nothing //TODO: if passed rc messages are ignored rc messaages: has no effect here too!!
		}
		else if (CNeutrinoApp::getInstance()->handleMsg(msg, data) & messages_return::cancel_all)
		{
			dprintf(DEBUG_INFO, "\033[32m[CMsgBox]   [%s - %d]  messages_return::cancel_all\033[0m\n", __func__, __LINE__);
			res  = menu_return::RETURN_EXIT_ALL;
			loop = false;
		}
	}

	disableTimeOutBar();
	return res;
}


void CMsgBox::refreshFoot(void)
{
	ccw_footer->getButtonChainObject()->paint();
}


void CMsgBox::setButtonText(const int& showed_button, const std::string& text)
{
	switch (showed_button)
	{
		case mbYes:
			btn_text_yes = text;
			break;
		case mbNo:
			btn_text_no = text;
			break;
		case mbCancel:
			btn_text_cancel = text;
			break;
		case mbBack:
			btn_text_back = text;
			break;
		case mbOk:
			btn_text_ok = text;
			break;
		default:
			return;
			break;
	}
	initButtons();
}

inline std::string CMsgBox::BTN_TEXT(const int& showed_button)
{
	string ret = "";

	switch (showed_button)
	{
		case mbYes:
			ret = btn_text_yes.empty() ? g_Locale->getText(LOCALE_MESSAGEBOX_YES) : btn_text_yes;
			break;
		case mbNo:
			ret = btn_text_no.empty() ? g_Locale->getText(LOCALE_MESSAGEBOX_NO) : btn_text_no;
			break;
		case mbCancel:
			ret = btn_text_cancel.empty() ? g_Locale->getText(LOCALE_MESSAGEBOX_CANCEL) : btn_text_cancel;
			break;
		case mbBack:
			ret = btn_text_back.empty() ? g_Locale->getText(LOCALE_MESSAGEBOX_BACK) : btn_text_back;
			break;
		case mbOk:
			ret = btn_text_ok.empty() ? g_Locale->getText(LOCALE_MESSAGEBOX_OK) : btn_text_ok;
			break;
		default:
			break;
	}

	return ret;
}


int ShowMsg2UTF(	const char * const Title,
			const char * const Text,
			const CMsgBox::msg_result_t Default,
			const uint32_t ShowButtons,
			const char * const Icon,
			const int Width,
			const int Timeout,
			bool returnDefaultOnTimeout,
			const int& Text_mode,
			fb_pixel_t color_frame)
{
	CMsgBox msgBox (Text,
			Title,
			Icon,
			NULL,
			Width,
			MSGBOX_MIN_HEIGHT,
			ShowButtons,
			Default,
			Text_mode,
			color_frame,
			COL_MENUCONTENT_PLUS_0,
			COL_SHADOW_PLUS_0
		       );

	msgBox.enableDefaultResultOnTimeOut(returnDefaultOnTimeout);
	msgBox.setTimeOut(Timeout);

	msgBox.paint();
	msgBox.exec();
	int  res = msgBox.getResult();
	msgBox.hide();
	return res;
}

int ShowMsg2UTF(	const neutrino_locale_t Title,
			const char * const Text,
			const CMsgBox::msg_result_t Default,
			const uint32_t ShowButtons,
			const char * const Icon,
			const int Width,
			const int Timeout,
			bool returnDefaultOnTimeout,
			const int& Text_mode,
			fb_pixel_t color_frame)
{
	int result = ShowMsg2UTF(g_Locale->getText(Title), Text, Default, ShowButtons, Icon, Width, Timeout, returnDefaultOnTimeout, Text_mode, color_frame);

	return (result);
}

//for compatibility
int ShowMsg(	const neutrino_locale_t Title,
			const char * const Text,
			const CMsgBox::msg_result_t Default,
			const uint32_t ShowButtons,
			const char * const Icon,
			const int Width,
			const int Timeout,
			bool returnDefaultOnTimeout,
			const int& Text_mode,
			fb_pixel_t color_frame)
{
	int result = ShowMsg2UTF(Title, Text, Default, ShowButtons, Icon, Width, Timeout, returnDefaultOnTimeout, Text_mode, color_frame);

	return (result);
}

int ShowMsg(	const char * const Title,
			const char * const Text,
			const CMsgBox::msg_result_t Default,
			const uint32_t ShowButtons,
			const char * const Icon,
			const int Width,
			const int Timeout,
			bool returnDefaultOnTimeout,
			const int& Text_mode,
			fb_pixel_t color_frame)
{
	int result = ShowMsg2UTF(Title, Text, Default, ShowButtons, Icon, Width, Timeout, returnDefaultOnTimeout, Text_mode, color_frame);

	return (result);
}

int ShowMsg(	const neutrino_locale_t Title,
			const std::string & Text,
			const CMsgBox::msg_result_t Default,
			const uint32_t ShowButtons,
			const char * const Icon,
			const int Width,
			const int Timeout,
			bool returnDefaultOnTimeout,
			const int& Text_mode,
			fb_pixel_t color_frame)
{
	int result = ShowMsg2UTF(Title, Text.c_str(), Default, ShowButtons, Icon, Width, Timeout, returnDefaultOnTimeout, Text_mode, color_frame);

	return (result);
}

int ShowMsg(	const neutrino_locale_t Title,
			const neutrino_locale_t Text,
			const CMsgBox::msg_result_t Default,
			const uint32_t ShowButtons,
			const char * const Icon,
			const int Width,
			const int Timeout,
			bool returnDefaultOnTimeout,
			const int& Text_mode,
			fb_pixel_t color_frame)
{
	int result = ShowMsg2UTF(g_Locale->getText(Title), g_Locale->getText(Text), Default, ShowButtons, Icon, Width, Timeout, returnDefaultOnTimeout, Text_mode, color_frame);

	return (result);
}

int ShowMsg(	const std::string & Title,
			const std::string & Text,
			const CMsgBox::msg_result_t Default,
			const uint32_t ShowButtons,
			const char * const Icon,
			const int Width,
			const int Timeout,
			bool returnDefaultOnTimeout,
			const int& Text_mode,
			fb_pixel_t color_frame)
{
	int result = ShowMsg2UTF(Title.c_str(), Text.c_str(), Default, ShowButtons, Icon, Width, Timeout, returnDefaultOnTimeout, Text_mode, color_frame);

	return (result);
}

int ShowMsg(	const std::string & Title,
			const neutrino_locale_t Text,
			const CMsgBox::msg_result_t Default,
			const uint32_t ShowButtons,
			const char * const Icon,
			const int Width,
			const int Timeout,
			bool returnDefaultOnTimeout,
			const int& Text_mode,
			fb_pixel_t color_frame)
{
	int result = ShowMsg2UTF(Title.c_str(), g_Locale->getText(Text), Default, ShowButtons, Icon, Width, Timeout, returnDefaultOnTimeout, Text_mode, color_frame);

	return (result);
}

void DisplayErrorMessage(const char * const ErrorMsg, const neutrino_locale_t& caption, const int &Timeout , const int& Text_mode)
{
	ShowMsg(caption, ErrorMsg, CMsgBox::mbrCancel, CMsgBox::mbBack, NEUTRINO_ICON_ERROR, 500, Timeout, false, Text_mode, COL_RED);
}

void DisplayErrorMessage(const char * const ErrorMsg, const std::string& caption, const int &Timeout , const int& Text_mode)
{
	ShowMsg(caption, ErrorMsg, CMsgBox::mbrCancel, CMsgBox::mbBack, NEUTRINO_ICON_ERROR, 500, Timeout, false, Text_mode, COL_RED);
}

void DisplayErrorMessage(const char * const ErrorMsg, const int &Timeout, const int& Text_mode)
{
	DisplayErrorMessage(ErrorMsg, LOCALE_MESSAGEBOX_ERROR, Timeout, Text_mode);
}

void DisplayInfoMessage(const char * const InfoMsg, const neutrino_locale_t& caption, const int& Timeout, const int& Text_mode, fb_pixel_t color_frame)
{
	ShowMsg(caption, InfoMsg, CMsgBox::mbrBack, CMsgBox::mbOk, NEUTRINO_ICON_INFO, 500, Timeout, false, Text_mode, color_frame);
}

void DisplayInfoMessage(const char * const InfoMsg, const std::string& caption, const int& Timeout, const int& Text_mode, fb_pixel_t color_frame)
{
	ShowMsg(caption, InfoMsg, CMsgBox::mbrBack, CMsgBox::mbOk, NEUTRINO_ICON_INFO, 500, Timeout, false, Text_mode, color_frame);
}

void DisplayInfoMessage(const char * const InfoMsg, const int& Timeout, const int& Text_mode, fb_pixel_t color_frame)
{
	DisplayInfoMessage(InfoMsg, LOCALE_MESSAGEBOX_INFO, Timeout, Text_mode, color_frame);
}
