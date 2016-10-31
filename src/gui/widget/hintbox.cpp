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
#include <system/debug.h>



/*
        x                             w
       y+-ccw_head----------------------------------------------+
        |icon |       caption                         | buttons |header (ccw_head)
        +-ccw_body----------------------------------------------+
        |+-obj_content-----------------------------------------+|
        ||+-obj_hint 0----------------------------------------+||body (ccw_body)
       h||| obj_picon |             obj_text                  |||           |
        ||+---------------------------------------------------+||           +-contents (obj_content)
        |+-----------------------------------------------------+|                           |
        +-------------------------------------------------------+                           +-hint 0 (obj_hint) default added with new instance of CHintBox
                                                                                            |
                                                                                            |
                                                                                            +-hint n optional added with addHintItem()
*/

using namespace std;


CHintBox::CHintBox(	const neutrino_locale_t Caption,
			const char * const Text,
			const int Width,
			const char * const Icon,
			const char * const Picon,
			const int& header_buttons,
			const int& text_mode,
			const int& indent): CComponentsWindow(	1, 1, width,
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
			const int& indent):CComponentsWindow(	1, 1, width,
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
			const int& indent):CComponentsWindow(	1, 1, width,
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
			const int& indent):CComponentsWindow(	1, 1, width,
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
	lines = 0;
	timeout		= HINTBOX_DEFAULT_TIMEOUT;
	w_indentation	= indent;
	hb_text_mode	= text_mode;

	hb_font		= g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO];

	//set required window width and basic height
	width 		= max(HINTBOX_MIN_WIDTH, max(Width, min(hb_font->getRenderWidth(Text), (int)frameBuffer->getScreenWidth())));
	height 		= max(HINTBOX_MIN_HEIGHT, height);

	ccw_buttons = header_buttons;

	//enable shadow
	shadow = true;

	//disable disable header if no title has been defined
	if (ccw_caption.empty())
		showHeader(false);

	//disable footer for default
	showFooter(false);

	//add the content container, contains the hint objects
	obj_content = new CComponentsFrmChain(CC_CENTERED, CC_CENTERED,  ccw_body->getWidth(), ccw_body->getHeight(), NULL, CC_DIR_X, ccw_body);
	obj_content->doPaintBg(false);

	y_hint_obj 	= 0;
	h_hint_obj	= obj_content->getHeight();

	//initialize timeout bar and its timer
	timeout_pb 	= NULL;
	timeout_pb_timer= NULL;

	if (!Text.empty())
		addHintItem(Text, hb_text_mode, Picon);
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
		timeout_pb->setDimensionsAll(ccw_body->getRealXPos(), ccw_body->getRealYPos(), ccw_body->getWidth(), TIMEOUT_BAR_HEIGHT);
		timeout_pb->setValues(0, 100*timeout);
		if (!timeout_pb_timer)
			timeout_pb_timer = new CComponentsTimer(1, true);
		timeout_pb_timer->OnTimer.connect(sigc::mem_fun0(this, &CHintBox::showTimeOutBar));
		timeout_pb_timer->startTimer();
	}
}

int CHintBox::exec()
{
	neutrino_msg_t msg;
	neutrino_msg_data_t data;
	int res = messages_return::none;
	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd( timeout );

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
		else if ((msg == CRCInput::RC_up) || (msg == CRCInput::RC_down))
		{
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

void CHintBox::addHintItem(const std::string& Text, const int& text_mode, const std::string& Picon, const u_int8_t& at_page_number, const fb_pixel_t& color_text, Font* font_text)
{
	dprintf(DEBUG_INFO, "[CHintBox]  [%s - %d] add new hint '%s' %s\n", __func__, __LINE__, Text.c_str(), Picon.c_str());

	//set required font and line size
	hb_font = !font_text ? hb_font : font_text;
	width = getMaxWidth(Text, width);
	int h_line = hb_font->getHeight();

	//init side picon object
	CComponentsPicture *obj_picon = new CComponentsPicture(0, timeout > 0 ? TIMEOUT_BAR_HEIGHT : 0, Picon);
	obj_picon->doPaintBg(false);
	obj_picon->SetTransparent(CFrameBuffer::TM_BLACK);
	int w_picon = obj_picon->getWidth();

	//init text item object
	int x_text_obj = (w_picon > 0) ? (w_picon + w_indentation) : 0;
	int w_text_obj = obj_content->getWidth() - w_picon - w_indentation;
	int h_text_obj = max(h_line, obj_picon->getHeight());
	CComponentsText *obj_text = new CComponentsText(x_text_obj,
							timeout > 0 ? TIMEOUT_BAR_HEIGHT : 0,
							w_text_obj,
							h_text_obj,
							Text,
							text_mode,
							hb_font);

	//provide the internal textbox object
	CTextBox *textbox = obj_text->getCTextBoxObject();
	int lines_count = textbox->getLines();

	//get required height of text object related to height of current text object, header and footer
	int fh_h = (ccw_head ? ccw_head->getHeight() : 0) + (ccw_footer ? ccw_footer->getHeight() : 0) + obj_text->getHeight();
	int h_required = min( max(height,fh_h), (int)frameBuffer->getScreenHeight()) ;//lines_count * h_line + (ccw_head ? ccw_head->getHeight() : 0) + (ccw_footer ? ccw_footer->getHeight() : 0);

	//set minimal required height
	height = max(height, min(HINTBOX_MAX_HEIGHT, max(HINTBOX_MIN_HEIGHT, h_required)));

	//if have no pre defined text mode:
	//more than 1 line or a picon is defined, then do not center text and allow scroll if > 1 lines
	if (text_mode == 0){
		if (lines_count == 1)
			obj_text->setTextMode(CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH | CTextBox::CENTER);
		if (w_picon > 1)
			obj_text->setTextMode(CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH);
		if (lines_count > 1)
			obj_text->setTextMode(CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH | (h_required > HINTBOX_MAX_HEIGHT ? CTextBox::SCROLL : CTextBox::AUTO_HIGH));
		if (lines_count > 1 && w_picon == 0)
			obj_text->setTextMode(CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH | CTextBox::CENTER | (h_required > HINTBOX_MAX_HEIGHT ? CTextBox::SCROLL : CTextBox::AUTO_HIGH));
	}
	else
		obj_text->setTextMode(text_mode);

	//text item object: don't paint background
	obj_text->doPaintBg(false);
	obj_text->setCorner(corner_rad, corner_type);

	//set text color
	obj_text->setTextColor(color_text);

	//calculate height of hint object
	if (obj_content->size() == 0){
		if (lines_count == 1){
			h_hint_obj = max(h_hint_obj, h_text_obj);
			obj_text->setYPos(CC_CENTERED);
		}
		else
			h_hint_obj = max(h_hint_obj, h_line*lines_count);
	}
	else{
		if (lines_count == 1){
			h_hint_obj = h_text_obj;
			obj_text->setYPos(CC_CENTERED);
		}
		else
			h_hint_obj = h_line*lines_count;
	}

	//init hint container object
	if (isPageChanged())
		y_hint_obj = 0;
	CComponentsFrmChain *obj_hint = new CComponentsFrmChain(	0+w_indentation,
									y_hint_obj,
									obj_content->getWidth()-2*w_indentation,
									h_hint_obj,
									NULL,
									CC_DIR_X,
									obj_content);

	//don't paint background for hint container
	obj_hint->doPaintBg(false);
	obj_hint->setCorner(corner_rad, corner_type);
	obj_hint->setPageNumber(at_page_number);

	//add the created items to obj_hint
	obj_hint->addCCItem(obj_picon);
	obj_hint->addCCItem(obj_text);

	//text object obtains the full height of its parent object
	obj_text->setHeightP(100);

	//if we have only one line and a defined picon, then do centering picon to text on the left site
	if (lines_count == 1)
		obj_picon->setYPos(CC_CENTERED);

	//set next y pos for the next hint object
	y_hint_obj += h_hint_obj;


	//recalculate new hintbox height
	ReSize();

	//set hint box position general to center and refresh window
	setCenterPos();
	Refresh();

	lines += lines_count;
}

void CHintBox::setMsgText(const std::string& Text, const uint& hint_id, const int& mode, Font* font_text, const fb_pixel_t& color_text, const int& style)
{
	uint id = hint_id;
	if (hint_id+1 > obj_content->size()){
		id = 0;
		dprintf(DEBUG_NORMAL, "[CHintBox]   [%s - %d] mismatching hint_id [%u]...\n", __func__, __LINE__, id);
	}

	CComponentsFrmChain 	*obj_hint = static_cast<CComponentsFrmChain*>(obj_content->getCCItem(id));
	CComponentsText 	*obj_text = static_cast<CComponentsText*>(obj_hint->getCCItem(1));

	//set required font and line size
	Font* font = font_text == NULL ? g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO] : font_text;
	if (obj_text)
		obj_text->setText(Text, mode, font, color_text, style);
}

void CHintBox::ReSize()
{
	int h_content_old = obj_content->getHeight();
	int h_content_new = 0;
	for (size_t i= 0; i< obj_content->size(); i++){
		CComponentsItem *item = obj_content->getCCItem(i);
		h_content_new += item->getHeight();
	}
	int h_content_diff = h_content_new - h_content_old;

	obj_content->setHeight(h_content_new);
	setHeight(height+h_content_diff);
}


void CHintBox::Scroll(bool down, const uint& hint_id)
{
	uint id = hint_id;
	if (hint_id+1 > obj_content->size()){
		id = 0;
		dprintf(DEBUG_NORMAL, "[CHintBox]   [%s - %d] mismatching hint_id [%u]...\n", __func__, __LINE__, id);
	}

	CComponentsFrmChain 	*obj_hint = static_cast<CComponentsFrmChain*>(obj_content->getCCItem(id));
	CComponentsText 	*obj_text = static_cast<CComponentsText*>(obj_hint->getCCItem(1));

	if (obj_text) {
		dprintf(DEBUG_INFO, "[CHintBox]   [%s - %d] try to scroll %s hint_id [%u]...Text= %s\n", __func__, __LINE__, down ? "down" : "up", id, obj_text->getText().c_str());
		CTextBox* textbox = obj_text->getCTextBoxObject();
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

int CHintBox::getMaxWidth(const string& Text, const int& minWidth)
{
	return max(HINTBOX_MIN_WIDTH, max(minWidth, min(hb_font->getRenderWidth(Text), (int)frameBuffer->getScreenWidth())));
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
	paint_bg = show_background;
	ccw_show_header = false;
	ccw_show_footer = false;
}

CHint::CHint(const neutrino_locale_t Text, bool show_background) : CHintBox("" , g_Locale->getText(Text))
{
	paint_bg = show_background;
	ccw_show_header = false;
	ccw_show_footer = false;
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
