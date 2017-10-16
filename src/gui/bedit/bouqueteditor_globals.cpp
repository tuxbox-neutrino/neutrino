/*
	neutrino bouquet editor - globals

	Copyright (C) 2017 Sven Hoefer

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

#include <driver/screen_max.h>

#include "bouqueteditor_globals.h"


CBEGlobals::CBEGlobals()
{
	frameBuffer = CFrameBuffer::getInstance();
	timeout_ptr = &g_settings.timing[SNeutrinoSettings::TIMING_MENU];

	header 	= NULL;
	footer 	= NULL;
	dline 	= NULL;
	ibox 	= NULL;

	init();
}

void CBEGlobals::init()
{
	item_font = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST];
	info_font = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR];

	width  = frameBuffer->getScreenWidthRel();
	height = frameBuffer->getScreenHeightRel();

	if (!header)
		header = new CComponentsHeader();
	header_height = header->getHeight();
	item_height = item_font->getHeight();

	if (!footer)
		footer = new CComponentsFooter();
	footer_height = footer->getHeight();

	info_height = 2*info_font->getHeight() + 2*OFFSET_INNER_SMALL;

	items_count = (height - header_height - footer_height - OFFSET_INTER - info_height - 2*OFFSET_SHADOW) / item_height;
	body_height = items_count*item_height;
	height = header_height + body_height + footer_height + OFFSET_INTER + info_height + 2*OFFSET_SHADOW; // recalc height

        x = getScreenStartX(width);
        y = getScreenStartY(height);
}

CBEGlobals::~CBEGlobals()
{
	ResetModules();
}

void CBEGlobals::ResetModules()
{
	if (dline){
		delete dline; dline = NULL;
	}
	if (ibox){
		delete ibox; ibox = NULL;
	}
	if (header){
		delete header; header = NULL;
	}
	if (footer){
		delete footer; footer = NULL;
	}
}

void CBEGlobals::paintDetails(int pos, int current)
{
	int xpos  = x - DETAILSLINE_WIDTH;
	int ypos1 = y + header_height + pos*item_height;
	int ypos2 = y + height - info_height - OFFSET_SHADOW;
	int ypos1a = ypos1 + (item_height/2);
	int ypos2a = ypos2 + (info_height/2);

	if (pos >= 0)
	{
		if (dline == NULL)
			dline = new CComponentsDetailsLine();
		dline->setDimensionsAll(xpos, ypos1a, ypos2a, item_height/2, info_height - RADIUS_LARGE*2);

		dline->paint();

		if (ibox == NULL)
			ibox = new CComponentsInfoBox();
		ibox->setColorBody(COL_MENUCONTENTDARK_PLUS_0);
		ibox->setTextColor(COL_MENUCONTENTDARK_TEXT);
		ibox->setFrameThickness(FRAME_WIDTH_MIN);
		ibox->setCorner(RADIUS_LARGE);
		ibox->enableShadow();
		ibox->enableColBodyGradient(g_settings.theme.menu_Hint_gradient, COL_MENUFOOT_PLUS_0, g_settings.theme.menu_Hint_gradient_direction);// COL_MENUFOOT_PLUS_0 is default footer color
		ibox->setDimensionsAll(x, ypos2, width, info_height);
		ibox->setText(getInfoText(current), CTextBox::AUTO_WIDTH | CTextBox::NO_AUTO_LINEBREAK, info_font);

		ibox->paint();
	}
	else
	{
		if (dline)
		{
			if (dline->isPainted())
				dline->hide();
		}
		if (ibox)
		{
			if (ibox->isPainted())
				ibox->hide();
		}
	}
}

void CBEGlobals::paintBody()
{
	PaintBoxRel(x, y + header_height, width, body_height, COL_MENUCONTENT_PLUS_0, RADIUS_NONE, CORNER_NONE, CC_SHADOW_ON);
}

void CBEGlobals::hide()
{
	frameBuffer->paintBackgroundBoxRel(x, y, width + OFFSET_SHADOW, height + OFFSET_SHADOW);

	killDetails();
}

void CBEGlobals::killDetails()
{
	if (dline)
		dline->kill();
	if (ibox)
		ibox->kill();
}

void CBEGlobals::paintFoot(const size_t& label_count, const struct button_label * const content)
{
	if (!footer)
		footer = new CComponentsFooter();
	footer->setCorner(RADIUS_LARGE, CORNER_BOTTOM);
	footer->enableShadow(CC_SHADOW_ON, -1, true);
	footer->paintButtons(x, y + header_height + body_height, width, footer_height, label_count, content);
}

void CBEGlobals::paintHead(const std::string& Caption, const char* Icon)
{
	if (!header)
		header = new CComponentsHeader();
	header->setCaption(Caption);
	header->setIcon(Icon);
	header->setDimensionsAll(x, y, width, header_height);
	header->setCorner(RADIUS_LARGE, CORNER_TOP);
	header->enableShadow(CC_SHADOW_RIGHT | CC_SHADOW_CORNER_TOP_RIGHT | CC_SHADOW_CORNER_BOTTOM_RIGHT, -1, true);
	header->paint(CC_SAVE_SCREEN_NO);
}
