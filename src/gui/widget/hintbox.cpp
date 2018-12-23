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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include "hintbox.h"
#include <gui/components/cc_timer.h>
#include <driver/fontrenderer.h>
#include <system/debug.h>

#define MSG_FONT g_Font[SNeutrinoSettings::FONT_TYPE_MESSAGE_TEXT]

/**
        x                             width                                ccw_head [relevant for CHintBox, CMsgBox. not enabled in CHint class]
    + y +---------------------------------------------------------------+/
        |[icon]|        caption                       |[context buttons]|
        +===============================================================+ timeout_pb
        | + W_FRAME (w_indentation)-----------------------------------+ |
        | |                                                           | |
 height | |  [picon]  [text]                                          | |  ccw_body > container for info box object
        | |           i n f o b o x  (added with addHintItem()        | |/
        | |                                                           | |
        | +-----------------------------------------------------------+ |  ccw_footer with buttons [relevant for CMsgBox, not enabled in CHintBox and CHint classes]
    +   +---------------------------------------------------------------+/
        |               <ok> <yes> <no> <cancel> <back> ...             |
    +   +---------------------------------------------------------------+

*/

using namespace std;


CHintBox::CHintBox(	const neutrino_locale_t Caption,
			const char * const Text,
			const int Width,
			const char * const Icon,
			const char * const Picon,
			const int& header_buttons,
			const int& text_mode,
			const int& indent): CComponentsWindow(	0, 0, Width,
									HINTBOX_MIN_HEIGHT,
									Caption,
									string(Icon == NULL ? "" : Icon),
									NULL,
									CC_SHADOW_ON)
{
	init(Text, Width, string(Picon == NULL ? "" : Picon), header_buttons, text_mode, indent);
}

CHintBox::CHintBox(	const char * const Caption,
			const char * const Text,
			const int Width,
			const char * const Icon,
			const char * const Picon,
			const int& header_buttons,
			const int& text_mode,
			const int& indent):CComponentsWindow(	0, 0, Width,
									HINTBOX_MIN_HEIGHT,
									Caption,
									string(Icon == NULL ? "" : Icon),
									NULL,
									CC_SHADOW_ON)
{
	init(string(Text), Width, string(Picon == NULL ? "" : Picon), header_buttons, text_mode, indent);
}

CHintBox::CHintBox(	const neutrino_locale_t  Caption,
			const neutrino_locale_t  Text,
			const int Width,
			const char * const Icon,
			const char * const Picon,
			const int& header_buttons,
			const int& text_mode,
			const int& indent):CComponentsWindow(	0, 0, Width,
									HINTBOX_MIN_HEIGHT,
									Caption,
									string(Icon == NULL ? "" : Icon),
									NULL,
									CC_SHADOW_ON)
{
	init(g_Locale->getText(Text), Width, string(Picon == NULL ? "" : Picon), header_buttons, text_mode, indent);
}

CHintBox::CHintBox(	const char * const Caption,
			const neutrino_locale_t  Text,
			const int Width,
			const char * const Icon,
			const char * const Picon,
			const int& header_buttons,
			const int& text_mode,
			const int& indent):CComponentsWindow(	0, 0, Width,
									HINTBOX_MIN_HEIGHT,
									Caption,
									string(Icon == NULL ? "" : Icon),
									NULL,
									CC_SHADOW_ON)
{
	init(g_Locale->getText(Text), Width, string(Picon == NULL ? "" : Picon), header_buttons, text_mode, indent);
}


void CHintBox::init(const std::string& Text, const int& Width, const std::string& Picon, const int& header_buttons, const int& text_mode, const int& indent)
{
	cc_item_type.name = "wg.hintbox";
	int _Width	= frameBuffer->scale2Res(Width);
	timeout		= HINTBOX_DEFAULT_TIMEOUT;
	w_indentation	= indent;

	hb_font		= MSG_FONT;

	enable_txt_scroll = false;

	//enable shadow
	shadow = CC_SHADOW_ON;

	//disable disable header if no title has been defined
	if (ccw_caption.empty())
		showHeader(false);

	//set required window width and basic height, consider existent header instance and its caption width
	width 		= getMaxWidth(Text, ccw_caption, hb_font, _Width);
	height 		= max(HINTBOX_MIN_HEIGHT, min(HINTBOX_MAX_HEIGHT, height));

	ccw_buttons = header_buttons;

	//disable footer for default
	showFooter(false);

	y_hint_obj 	= CC_CENTERED;

	//initialize timeout bar and its timer
	timeout_pb 	= NULL;
	timeout_pb_timer= NULL;

	if (!Text.empty())
		addHintItem(Text, text_mode, Picon, COL_MENUCONTENT_TEXT, hb_font);
	else
		ReSize();
}

CHintBox::~CHintBox()
{
	disableTimeOutBar();
}

void CHintBox::enableTimeOutBar(bool enable)
{
	if(!enable){
		if(timeout_pb_timer){
			delete timeout_pb_timer; timeout_pb_timer = NULL;
		}
		if(timeout_pb){
			delete timeout_pb; timeout_pb = NULL;
		}
		return;
	}

	if(timeout_pb){
		timeout_pb->paint0();
		timeout_pb->setValues(timeout_pb->getValue()+1, 100*timeout);
	}else{
		timeout_pb = new CProgressBar();
		timeout_pb->setType(CProgressBar::PB_TIMESCALE);
		timeout_pb->setDimensionsAll(ccw_body->getRealXPos(), ccw_body->getRealYPos(), ccw_body->getWidth(), TIMEOUT_BAR_HEIGHT);
		timeout_pb->setValues(0, 100*timeout);
		if (!timeout_pb_timer) {
			timeout_pb_timer = new CComponentsTimer(1, true);
			const string tn = cc_item_type.name + ":timeout_bar:";
			timeout_pb_timer->setThreadName(tn);
		}
		sl_tbar_on_timer.disconnect();
		sl_tbar_on_timer = sigc::mem_fun0(this, &CHintBox::showTimeOutBar);
		timeout_pb_timer->OnTimer.connect(sl_tbar_on_timer);
		timeout_pb_timer->startTimer();
	}
}

int CHintBox::exec()
{
	neutrino_msg_t msg;
	neutrino_msg_data_t data;
	int res = messages_return::none;
	if (timeout == NO_TIMEOUT || timeout == DEFAULT_TIMEOUT)
		timeout = HINTBOX_DEFAULT_TIMEOUT;

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(timeout);

	if (timeout > 0)
		enableTimeOutBar();

	while ( ! ( res & ( messages_return::cancel_info | messages_return::cancel_all ) ) )
	{
		g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd );

		if ((msg == CRCInput::RC_timeout) || (msg == CRCInput::RC_ok))
		{
			res = messages_return::cancel_info;
		}
		else if(msg == CRCInput::RC_home)
		{
			res = messages_return::cancel_all;
		}
		else if (enable_txt_scroll && (msg == CRCInput::RC_up || msg == CRCInput::RC_down))
		{
			/* if ! enable_txt_scroll, fall through to last else branch instead */
			if (msg == CRCInput::RC_up)
				this->scroll_up();
			else
				this->scroll_down();
		}
		else if (CNeutrinoApp::getInstance()->listModeKey(msg)){
			// do nothing //TODO: if passed rc messages are ignored rc messaages: has no effect here too!!
                }
		else if(msg == CRCInput::RC_mode) {
			res = messages_return::handled;
			break;
		}
		else if((msg == CRCInput::RC_next) || (msg == CRCInput::RC_prev)) {
			res = messages_return::cancel_all;
			g_RCInput->postMsg(msg, data);
		}
		else
		{
			res = CNeutrinoApp::getInstance()->handleMsg(msg, data);
			if (res & messages_return::unhandled)
			{
				// leave here and handle above...
				g_RCInput->postMsg(msg, data);
				res = messages_return::cancel_all;
			}
		}
	}

	disableTimeOutBar();
	return res;
}

void CHintBox::addHintItem(const std::string& Text, const int& text_mode, const std::string& Picon, const fb_pixel_t& color_text, Font* font_text)
{
	/* set required font and line height */
	Font* item_font = !font_text ? hb_font : font_text;

	/* set picon */
	string picon = Picon;

	/* pre define required info height depends of lines and minimal needed height*/
	int line_breaks = CTextBox::getLines(Text);
	int h_font = item_font->getHeight();
	int h_lines = h_font;
	h_lines += h_font * line_breaks;

	/* get required height depends of possible lines and max height */
	h_hint_obj = min(HINTBOX_MAX_HEIGHT - (ccw_head ? ccw_head->getHeight() : 0), h_lines + 2*w_indentation);

	int txt_mode = text_mode;
	/* remove CENTER mode if picon defined */
	if (!picon.empty() && (txt_mode & CTextBox::CENTER)){
		txt_mode &= ~CTextBox::CENTER;
	}

	/* add scroll mode if needed */
	if (h_lines > h_hint_obj){
		txt_mode = text_mode | CTextBox::SCROLL;
		ccw_buttons = ccw_buttons | CComponentsHeader::CC_BTN_UP | CComponentsHeader::CC_BTN_DOWN;
		enable_txt_scroll = true;
	}

	/* define y start position of infobox inside body */
	if(!ccw_body->empty()){
		ccw_body->front()->setYPos(w_indentation);
		y_hint_obj += ccw_body->back()->getYPos()+ ccw_body->back()->getHeight();
	}

	/* calcoulation of maximal hintbox height include possible header*/
	height = min(HINTBOX_MAX_HEIGHT, (ccw_head ? ccw_head->getHeight() : 0)+ h_hint_obj);
	height = max(height, HINTBOX_MIN_HEIGHT);

	/* get current maximal width and refresh window items */
	width = getMaxWidth(Text, ccw_caption, item_font, width);

	/* initialize infobox as container for text and possible picon*/
	CComponentsInfoBox *info_box =  new CComponentsInfoBox(	0,
								y_hint_obj,
								width, // FIXME: not critical here but ccw_body->getWidth() != width, this should be the same value!
								h_hint_obj,
								Text,
								txt_mode,
								item_font,
								ccw_body,
								CC_SHADOW_OFF,
								color_text);

	/* define picon and disable bg */
	info_box->setPicture(picon);
	info_box->doPaintBg(false);

	/* recalculate new hintbox dimensions and position*/
	ReSize();
}

void CHintBox::setMsgText(const std::string& Text, const uint& hint_id, const int& mode, Font* font_text, const fb_pixel_t& color_text, const int& style)
{
	uint id = hint_id;
	if (hint_id+1 > ccw_body->size()){
		id = 0;
		dprintf(DEBUG_NORMAL, "[CHintBox]   [%s - %d] mismatching hint_id [%u]...\n", __func__, __LINE__, id);
	}

	CComponentsInfoBox	*obj_text = static_cast<CComponentsInfoBox*>(ccw_body->getCCItem(id));

	//set required font and line size
	Font* font = font_text == NULL ? MSG_FONT : font_text;
	if (obj_text)
		obj_text->setText(Text, mode, font, color_text, style);
}

void CHintBox::setMsgText(const neutrino_locale_t& locale, const uint& hint_id, const int& mode, Font* font_text, const fb_pixel_t& color_text, const int& style)
{
	setMsgText(g_Locale->getText(locale), hint_id, mode, font_text, color_text, style);
}

void CHintBox::ReSize()
{
	int h = (ccw_head ? ccw_head->getHeight() : 0);
	for (size_t i= 0; i< ccw_body->size(); i++){
		CComponentsItem *item = ccw_body->getCCItem(i);
		h += item->getHeight();
	}
	height = min(HINTBOX_MAX_HEIGHT, max(HINTBOX_MIN_HEIGHT, max(height,h)));

	/* set hint box position general to center and refresh window */
	setCenterPos(CC_ALONG_X);
	y = frameBuffer->getScreenY() + ((frameBuffer->getScreenHeight() - height) >> 2),
	Refresh();
}


void CHintBox::Scroll(bool down, const uint& hint_id)
{
	uint id = hint_id;
	if (hint_id+1 > ccw_body->size()){
		id = 0;
		dprintf(DEBUG_NORMAL, "[CHintBox]   [%s - %d] mismatching hint_id [%u]...\n", __func__, __LINE__, id);
	}

	CComponentsInfoBox	*obj_text = static_cast<CComponentsInfoBox*>(ccw_body->getCCItem(id));

	if (obj_text) {
		dprintf(DEBUG_DEBUG, "[CHintBox]   [%s - %d] try to scroll %s hint_id [%u]...Text= %s\n", __func__, __LINE__, down ? "down" : "up", id, obj_text->getText().c_str());
		CTextBox* textbox = obj_text->cctext->getCTextBoxObject();
		if (textbox) {
			textbox->enableBackgroundPaint(true);
			if (down)
				textbox->scrollPageDown(1);
			else
				textbox->scrollPageUp(1);
			textbox->enableBackgroundPaint(false);
		}
	}
}

void CHintBox::scroll_up(const uint& hint_id)
{
	Scroll(false, hint_id);
}

void CHintBox::scroll_down(const uint& hint_id)
{
	Scroll(true, hint_id);
}

int CHintBox::getMaxWidth(const string& Text, const string& Title, Font *font, const int& minWidth)
{
	int res = max(HINTBOX_MIN_WIDTH, max(minWidth+2*w_indentation, min(CTextBox::getMaxLineWidth(Text, font)+2*w_indentation, (int)frameBuffer->getScreenWidth())));
	if (ccw_show_header){
		initHeader();
		return max(res, ccw_head->getCaptionFont()->getRenderWidth(Title) + 2*w_indentation);
	}

	return res;
}

int ShowHint(const char * const Caption, const char * const Text, const int Width, int timeout, const char * const Icon, const char * const Picon, const int& header_buttons)
{
	int res = messages_return::none;

	CHintBox hintBox(Caption, Text, Width, Icon, Picon, header_buttons);
	hintBox.setTimeOut(timeout);
	hintBox.paint();
	res = hintBox.exec();
	hintBox.hide();

	return res;
}

int ShowHint(const neutrino_locale_t Caption, const char * const Text, const int Width, int timeout, const char * const Icon, const char * const Picon, const int& header_buttons)
{
	const char * caption = g_Locale->getText(Caption);
	return ShowHint(caption, Text, Width, timeout, Icon, Picon, header_buttons);
}

int ShowHint(const neutrino_locale_t Caption, const neutrino_locale_t Text, const int Width, int timeout, const char * const Icon, const char * const Picon, const int& header_buttons)
{
	return ShowHint(Caption, g_Locale->getText(Text),Width,timeout,Icon, Picon, header_buttons);
}

int ShowHint(const char * const Caption, const neutrino_locale_t Text, const int Width, int timeout, const char * const Icon, const char * const Picon, const int& header_buttons)
{
	return ShowHint(Caption, g_Locale->getText(Text),Width,timeout, Icon, Picon, header_buttons);
}


CHint::CHint(const char * const Text, bool show_background) : CHintBox("" , Text)
{
	initHint(show_background);
}

CHint::CHint(const neutrino_locale_t Text, bool show_background) : CHintBox("" , g_Locale->getText(Text))
{
	initHint(show_background);
}

int ShowHintS(const char * const Text, int timeout, bool show_background)
{
	int res = messages_return::none;

	CHint hint(Text, show_background);
	hint.setTimeOut(timeout);
	hint.paint();
	res = hint.exec();
	hint.hide();

	return res;
}

int ShowHintS(const neutrino_locale_t Text, int timeout, bool show_background)
{
	return ShowHintS(g_Locale->getText(Text), timeout, show_background);
}
