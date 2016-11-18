/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Implement CComponent-Windowclass.
	Copyright (C) 2015 Thilo Graf 'dbt'

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
#include <gui/widget/helpbox.h>

using namespace std;

Helpbox::Helpbox(	const string& Title,
			const string& Default_Text,
			const int& text_mode,
			const int& line_space,
			Font* default_font_text,
			const char* Icon):
			CComponentsWindowMax(	Title,
						Icon,
						NULL,
						CC_SHADOW_ON,
						COL_MENUCONTENT_PLUS_6,
						COL_MENUCONTENT_PLUS_0,
						COL_SHADOW_PLUS_0)
{
	page = 0;
	hbox_y = 1;
	setWindowHeaderButtons(CComponentsHeader::CC_BTN_MENU | CComponentsHeader::CC_BTN_EXIT);
	ccw_footer->setButtonLabel(NEUTRINO_ICON_BUTTON_HOME, LOCALE_MESSAGEBOX_BACK);

	hbox_font = default_font_text;
	if (default_font_text == NULL)
		hbox_font = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO];

	if (!Default_Text.empty())
		addLine(Default_Text.c_str(), Default_Text, text_mode, line_space, HELPBOX_DEFAULT_LINE_INDENT ,hbox_font);

	//ensure hided channellist, because shared RC_ok
	CNeutrinoApp::getInstance()->allowChannelList(false);
}


void Helpbox::addLine(const std::string& icon, const std::string& text, const int& text_mode, const int& line_space, const int& line_indent, Font* font_text)
{
	CComponentsItem *pre_item = !ccw_body->empty() ? ccw_body->back() : NULL; //get the last current item

	if (pre_item){
		if (pre_item->getPageNumber() == page)
			hbox_y = pre_item->getYPos() + pre_item->getHeight();
	}

	int h_line = line_space;
	Font* font = hbox_font;
	if (font_text){
		h_line = max(h_line, font_text->getHeight());
		font = font_text;
	}

	CComponentsFrmChain *line = new CComponentsFrmChain(line_indent, hbox_y, 0, h_line);
	if ((hbox_y + h_line)>ccw_body->getHeight()){
		addPagebreak();
		line->setYPos(hbox_y);
	}
	line->setPageNumber(page);

	int w_body = ccw_body->getWidth();
	line->setWidth(w_body - line_indent - 40);
	line->setXPos(line_indent);
	line->setColorBody(ccw_body->getColorBody());

	CComponentsPicture *picon = NULL;
	int w_picon = 0;
	if (!icon.empty()){
		picon = new CComponentsPicture (0, 0, icon);
		w_picon = picon->getHeight();
		picon->doPaintBg(false);
		picon->setYPos(line->getHeight()/2 - w_picon/2);
		picon->SetTransparent(CFrameBuffer::TM_BLACK);
		line->addCCItem(picon);
	}

	if (!text.empty()){
		int x_text = w_picon + (picon ? 10 : 0);
		CComponentsText * txt = new CComponentsText(x_text, 0, line->getWidth()-x_text, line_space, text, text_mode, font);
#if 0 //"contrast agent", if you want to see where the text items are drawn.
		txt->setColorBody(COL_RED);
		txt->doPaintBg(true);
#endif
		line->addCCItem(txt);
	}
	addWindowItem(line);
}




void Helpbox::addSeparatorLine(const int& line_space, const int& line_indent, bool enable_gradient)
{
	CComponentsItem *pre_item = !ccw_body->empty() ? ccw_body->back() : NULL; //get the last current item

	if (pre_item){
		if (pre_item->getPageNumber() == page)
			hbox_y = pre_item->getYPos() + pre_item->getHeight();
	}

	int h_line = line_space;

	CComponentsFrmChain *line = new CComponentsFrmChain(line_indent, hbox_y, 0, h_line);
	line->setPageNumber(page);

	int w_body = ccw_body->getWidth();
	line->setWidth(w_body - line_indent - 40);
	line->setXPos(line_indent);
	line->setColorBody(ccw_body->getColorBody());

	CComponentsShapeSquare *sepline = new CComponentsShapeSquare (0, 0, line->getWidth(), 2);
	sepline->setYPos(line->getHeight()/2 - sepline->getHeight()/2);
	sepline->setColorBody(COL_MENUCONTENTINACTIVE_TEXT);
	sepline->enableColBodyGradient(enable_gradient);
	sepline->setColBodyGradient(CColorGradient::gradientLight2Dark, CFrameBuffer::gradientHorizontal, CColorGradient::light);

	line->addCCItem(sepline);

	addWindowItem(line);
}

void Helpbox::addSeparator(const int& line_space)
{
	CComponentsItem *pre_item = !ccw_body->empty() ? ccw_body->back() : NULL; //get the last current item

	if (pre_item){
		if (pre_item->getPageNumber() == page)
			hbox_y = pre_item->getYPos() + pre_item->getHeight();
	}

	CComponentsFrmChain *line = new CComponentsFrmChain(0, hbox_y, 0, line_space);
	line->setPageNumber(page);

	int w_body = ccw_body->getWidth();
	line->setWidth(w_body - 40);
	line->setColorBody(ccw_body->getColorBody());

	addWindowItem(line);
}





void Helpbox::addLine(const char *icon, const char *text, const int& text_mode, const int& line_space, const int& line_indent, Font* font_text)
{
	addLine(icon, std::string(text), text_mode, line_space, line_indent, font_text);

}


void Helpbox::addLine(const char *text, const int& text_mode, const int& line_space, const int& line_indent, Font* font_text)
{
	addLine("", std::string(text), text_mode, line_space, line_indent,  font_text);
}

void Helpbox::addLine(const std::string& text, const int& text_mode, const int& line_space, const int& line_indent, Font* font_text)
{
	addLine("", text, text_mode, line_space, line_indent,  font_text);
}



void Helpbox::addPagebreak(void)
{
	page ++;
	setPageCount(page);
	hbox_y = 1;
}

