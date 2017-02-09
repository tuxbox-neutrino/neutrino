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
			const int& line_height,
			Font* default_font_text,
			const char* Icon):
			CComponentsWindowMax(	Title,
						Icon,
						NULL,
						CC_SHADOW_ON)
{
	page = 0;
	hbox_y = 0;
	setWindowHeaderButtons(CComponentsHeader::CC_BTN_MENU | CComponentsHeader::CC_BTN_EXIT);
	ccw_footer->setButtonLabel(NEUTRINO_ICON_BUTTON_HOME, LOCALE_MESSAGEBOX_BACK);

	hbox_font = default_font_text;
	if (hbox_font == NULL)
		hbox_font = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO];

	if (!Default_Text.empty())
		addLine(Default_Text.c_str(), Default_Text, text_mode, line_height, HELPBOX_DEFAULT_LINE_INDENT, hbox_font);

	//ensure hided channellist, because shared RC_ok
	CNeutrinoApp::getInstance()->allowChannelList(false);
}

void Helpbox::addLine(const std::string& icon, const std::string& text, const int& text_mode, const int& line_height, const int& line_indent, Font* font_text)
{
	CComponentsItem *pre_item = !ccw_body->empty() ? ccw_body->back() : NULL; //get the last current item

	if (pre_item){
		if (pre_item->getPageNumber() == page)
			hbox_y = pre_item->getYPos() + pre_item->getHeight();
	}

	Font* font = hbox_font;
	if (font_text)
		font = font_text;

	int h_line = getLineHeight(line_height, font);

	CComponentsFrmChain *line = new CComponentsFrmChain(line_indent, hbox_y, 0, h_line);

	int w_body = ccw_body->getWidth();
	line->setWidth(w_body - 2*line_indent);
	line->setXPos(line_indent);
	line->setColorBody(ccw_body->getColorBody());

	CComponentsPicture *picon = NULL;
	int w_picon = 0;
	int h_picon = 0;
	if (!icon.empty()){
		picon = new CComponentsPicture (0, 0, icon);
		w_picon = picon->getWidth();
		h_picon = picon->getHeight();
		picon->doPaintBg(false);
		picon->setYPos(line->getHeight()/2 - h_picon/2);
		picon->SetTransparent(CFrameBuffer::TM_BLACK);
		line->addCCItem(picon);
	}

	int txt_height = 0;
	if (!text.empty()){
		int x_text = w_picon + (picon ? OFFSET_INNER_MID : 0);
		CComponentsText * txt = new CComponentsText(x_text, 0, line->getWidth()-x_text, 0, text, text_mode, font);
		txt->doPaintBg(false);
		txt->forceTextPaint();
#if 0 //"contrast agent", if you want to see where the text items are drawn.
		txt->setColorBody(COL_RED);
#endif
		int lines = txt->getCTextBoxObject()->getLines();
		txt_height = std::max(lines*font->getHeight(), h_line);
		txt->setHeight(txt_height);

		line->addCCItem(txt);
	}

	if (txt_height > line->getHeight())
		line->setHeight(txt_height);

	if ((hbox_y + line->getHeight()) > ccw_body->getHeight())
	{
		addPagebreak();
		line->setYPos(hbox_y);
	}
	line->setPageNumber(page);

	addWindowItem(line);
}

void Helpbox::addSeparatorLine(const int& line_height, const int& line_indent)
{
	CComponentsItem *pre_item = !ccw_body->empty() ? ccw_body->back() : NULL; //get the last current item

	if (pre_item){
		if (pre_item->getPageNumber() == page)
			hbox_y = pre_item->getYPos() + pre_item->getHeight();
	}

	CComponentsFrmChain *line = new CComponentsFrmChain(line_indent, hbox_y, 0, getLineHeight(line_height, hbox_font, true));
	line->setPageNumber(page);

	int w_body = ccw_body->getWidth();
	line->setWidth(w_body - 2*line_indent);
	line->setXPos(line_indent);
	line->setColorBody(ccw_body->getColorBody());

	CComponentsShapeSquare *sepline = new CComponentsShapeSquare (0, 0, line->getWidth(), 2);
	sepline->setYPos(line->getHeight()/2 - sepline->getHeight()/2);
	sepline->setColorBody(COL_MENUCONTENTINACTIVE_TEXT);
	if (g_settings.theme.menu_Separator_gradient_enable)
	{
		sepline->enableColBodyGradient(true);
		sepline->setColBodyGradient(CColorGradient::gradientLight2Dark, CFrameBuffer::gradientHorizontal, CColorGradient::light);
	}

	line->addCCItem(sepline);

	addWindowItem(line);
}

void Helpbox::addSeparator(const int& line_height)
{
	CComponentsItem *pre_item = !ccw_body->empty() ? ccw_body->back() : NULL; //get the last current item

	if (pre_item){
		if (pre_item->getPageNumber() == page)
			hbox_y = pre_item->getYPos() + pre_item->getHeight();
	}

	CComponentsFrmChain *line = new CComponentsFrmChain(0, hbox_y, 0, getLineHeight(line_height, hbox_font, true));
	line->setPageNumber(page);

	int w_body = ccw_body->getWidth();
	line->setWidth(w_body - 40);
	line->setColorBody(ccw_body->getColorBody());

	addWindowItem(line);
}

void Helpbox::addLine(const char *icon, const char *text, const int& text_mode, const int& line_height, const int& line_indent, Font* font_text)
{
	addLine(icon, std::string(text), text_mode, line_height, line_indent, font_text);
}

void Helpbox::addLine(const char *text, const int& text_mode, const int& line_height, const int& line_indent, Font* font_text)
{
	addLine("", std::string(text), text_mode, line_height, line_indent, font_text);
}

void Helpbox::addLine(const std::string& text, const int& text_mode, const int& line_height, const int& line_indent, Font* font_text)
{
	addLine("", text, text_mode, line_height, line_indent, font_text);
}

void Helpbox::addPagebreak(void)
{
	page ++;
	setPageCount(page);
	hbox_y = 1;
}

int Helpbox::getLineHeight(int line_height, Font* font_text, bool separator)
{
	if (font_text == NULL)
		return 0; // should not happen

	int h = max(line_height, font_text->getHeight());

	if (separator)
		return h/2; // separators uses half height

	return h;
}
