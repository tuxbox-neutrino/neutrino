/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gui/widget/listbox.h>
#include <gui/widget/buttons.h>

#include <global.h>
#include <neutrino.h>

#include <gui/widget/icons.h>
#include <driver/fontrenderer.h>

CListBox::CListBox(const char * const Caption)
{
	frameBuffer = CFrameBuffer::getInstance();
	caption = Caption;
	liststart = 0;
	selected =  0;
	width =  400;
	height = 420;
	ButtonHeight = 25;
	modified = false;
	theight     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	fheight     = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->getHeight();
	listmaxshow = (height-theight-0)/fheight;
	height = theight+0+listmaxshow*fheight; // recalc height

	x=frameBuffer->getScreenX() + ((frameBuffer->getScreenWidth() - width) / 2);
	y=frameBuffer->getScreenY() + ((frameBuffer->getScreenHeight() - height) / 2);
}

void CListBox::setModified(void)
{
	modified = true;
}

void CListBox::paint()
{
	liststart = (selected/listmaxshow)*listmaxshow;

	for(unsigned int count=0;count<listmaxshow;count++)
	{
		paintItem(count);
	}

	int total_pages;
	int current_page;
	getScrollBarData(&total_pages, &current_page, getItemCount(), listmaxshow, selected);
	paintScrollBar(x + width - SCROLLBAR_WIDTH, y + theight, SCROLLBAR_WIDTH, fheight*listmaxshow, total_pages, current_page);
}

void CListBox::paintHead()
{
	CComponentsHeader header(x, y, width, theight, caption);
	header.paint(CC_SAVE_SCREEN_NO);
}

void CListBox::paintFoot()
{
const struct button_label ListButtons[] =
{
	{ NEUTRINO_ICON_BUTTON_OKAY, LOCALE_CHANNELLIST_EDIT },
	{ NEUTRINO_ICON_BUTTON_RED, LOCALE_BOUQUETEDITOR_ADD},
	{ NEUTRINO_ICON_BUTTON_GREEN, LOCALE_BOUQUETEDITOR_DELETE},
	{ NEUTRINO_ICON_BUTTON_HOME, LOCALE_BOUQUETEDITOR_RETURN }
};
	const short numbuttons = sizeof(ListButtons)/sizeof(ListButtons[0]);
	::paintButtons(x, y + height-ButtonHeight, width, numbuttons, ListButtons, width, ButtonHeight);

}

void CListBox::paintItem(int pos)
{
	paintItem(liststart+pos, pos, (liststart+pos==selected) );
}

void CListBox::hide()
{
	frameBuffer->paintBackgroundBoxRel(x,y, width,height+ButtonHeight);
}

unsigned int CListBox::getItemCount()
{
	// WTF? Why a fixed value?
	return 10;
}

int CListBox::getItemHeight()
{
	return fheight;
}

void CListBox::paintItem(unsigned int /*itemNr*/, int paintNr, bool pselected)
{
	int ypos = y+ theight + paintNr*getItemHeight();

	fb_pixel_t color;
	fb_pixel_t bgcolor;

	getItemColors(color, bgcolor, pselected);

	frameBuffer->paintBoxRel(x,ypos, width - SCROLLBAR_WIDTH, getItemHeight(), bgcolor);
	g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x + OFFSET_INNER_MID, ypos + fheight, width - SCROLLBAR_WIDTH - 2*OFFSET_INNER_MID, "demo", color);
}

void CListBox::updateSelection(unsigned int newpos)
{
	if(newpos == selected)
		return;

	unsigned int prev_selected = selected;
	selected = newpos;

	unsigned int oldliststart = liststart;
	liststart = (selected/listmaxshow)*listmaxshow;
	if(oldliststart!=liststart) {
		paint();
	} else {
		paintItem(prev_selected - liststart);
		paintItem(selected - liststart);
	}
}

int CListBox::exec(CMenuTarget* parent, const std::string & /*actionKey*/)
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	int res = menu_return::RETURN_REPAINT;
	selected=0;

	if (parent)
	{
		parent->hide();
	}

	paintHead();
	paint();
	paintFoot();

	bool loop=true;
	modified = false;
	while (loop)
	{
		g_RCInput->getMsg(&msg, &data, g_settings.timing[SNeutrinoSettings::TIMING_EPG]);

		if (( msg == (neutrino_msg_t)g_settings.key_channelList_cancel) ||
				( msg == CRCInput::RC_home))
		{
			loop = false;
		}
		else if (msg == CRCInput::RC_up || (int) msg == g_settings.key_pageup ||
			 msg == CRCInput::RC_down || (int) msg == g_settings.key_pagedown)
		{
			if (getItemCount() != 0) {
				int new_selected = UpDownKey((int)getItemCount(), msg, listmaxshow, selected);
				if (new_selected >= 0)
					updateSelection(new_selected);
			}
		}
		else if (msg == (neutrino_msg_t) g_settings.key_list_start || msg == (neutrino_msg_t) g_settings.key_list_end) {
			if (getItemCount()) {
				int new_selected = msg == (neutrino_msg_t) g_settings.key_list_start ? 0 : getItemCount() - 1;
				updateSelection(new_selected);
			}
		}
		else if( msg ==CRCInput::RC_ok)
		{
			onOkKeyPressed();
		}
		else if ( msg ==CRCInput::RC_red)
		{
			onRedKeyPressed();
		}
		else if ( msg ==CRCInput::RC_green)
		{
			onGreenKeyPressed();
		}
		else if ( msg ==CRCInput::RC_yellow)
		{
			onYellowKeyPressed();
		}
		else if ( msg ==CRCInput::RC_blue)
		{
			onBlueKeyPressed();
		}
		else if (CNeutrinoApp::getInstance()->listModeKey(msg))
		{
			// do nothing
		}
		else
		{
			CNeutrinoApp::getInstance()->handleMsg( msg, data );
		}
	}

	hide();
	return res;
}
