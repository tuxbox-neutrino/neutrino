/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Kommentar:

	Diese GUI wurde von Grund auf neu programmiert und sollte nun vom
	Aufbau und auch den Ausbaumoeglichkeiten gut aussehen. Neutrino basiert
	auf der Client-Server Idee, diese GUI ist also von der direkten DBox-
	Steuerung getrennt. Diese wird dann von Daemons uebernommen.


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
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gui/bedit/bouqueteditor_chanselect.h>

#include <global.h>
#include <neutrino.h>

#include <driver/fontrenderer.h>
#include <driver/screen_max.h>
#include <gui/widget/icons.h>
#include <gui/widget/buttons.h>

#include <zapit/client/zapitclient.h>
#include <zapit/channel.h>
#include <zapit/bouquets.h>

extern tallchans allchans;
extern CBouquetManager *g_bouquetManager;
void addChannelToBouquet(const unsigned int bouquet, const t_channel_id channel_id);

CBEChannelSelectWidget::CBEChannelSelectWidget(const std::string & Caption, unsigned int Bouquet, CZapitClient::channelsMode Mode)
	:CListBox(Caption.c_str())
{
	int icol_w, icol_h, fh;

	bouquet = Bouquet;
	mode =    Mode;
	ButtonHeight = 25;
	iconoffset = 0;

	theight     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	fheight     = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->getHeight();

	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_GREEN, &icol_w, &icol_h);
	iheight = std::max(fheight, icol_h+2);
	iconoffset = std::max(iconoffset, icol_w);

	fh = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight();
	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_OKAY, &icol_w, &icol_h);
	ButtonHeight = std::max(fh, icol_h+4);
	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_HOME, &icol_w, &icol_h);
	ButtonHeight = std::max(ButtonHeight, icol_h+4);

	liststart = 0;
}

uint	CBEChannelSelectWidget::getItemCount()
{
	return Channels.size();
}

bool CBEChannelSelectWidget::isChannelInBouquet( int index)
{
	for (unsigned int i=0; i< bouquetChannels->size(); i++)
	{
		if ((*bouquetChannels)[i]->channel_id == Channels[index]->channel_id)
			return true;
	}
	return false;
}

bool CBEChannelSelectWidget::hasChanged()
{
	return modified;
}

void CBEChannelSelectWidget::paintItem(uint32_t itemNr, int paintNr, bool pselected)
{
	int ypos = y+ theight + paintNr*iheight;

	uint8_t    color;
	fb_pixel_t bgcolor;
	if (pselected)
	{
		color   = COL_MENUCONTENTSELECTED;
		bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
		frameBuffer->paintBoxRel(x,ypos, width- 15, iheight, COL_MENUCONTENT_PLUS_0);
		frameBuffer->paintBoxRel(x,ypos, width- 15, iheight, bgcolor, RADIUS_LARGE);
	}
	else
	{
		color   = COL_MENUCONTENT;
		bgcolor = COL_MENUCONTENT_PLUS_0;
		frameBuffer->paintBoxRel(x,ypos, width- 15, iheight, bgcolor);
	}

	if(itemNr < getItemCount())
	{
		if( isChannelInBouquet(itemNr))
		{
			frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_GREEN, x+10, ypos, iheight);
		}
		else
		{
			//frameBuffer->paintBoxRel(x+8, ypos+4, NEUTRINO_ICON_BUTTON_GREEN_WIDTH, fheight-4, bgcolor);
			frameBuffer->paintBoxRel(x+10, ypos, iconoffset, iheight, bgcolor);
		}
		//g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x + 8 + NEUTRINO_ICON_BUTTON_GREEN_WIDTH + 8, ypos+ fheight, width - (8 + NEUTRINO_ICON_BUTTON_GREEN_WIDTH + 8 + 8), Channels[itemNr]->getName(), color, 0, true);
		g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x + 20 + iconoffset, ypos + iheight - (iheight-fheight)/2, width - 20 - iconoffset, Channels[itemNr]->getName(), color, 0, true);
	}
}

void CBEChannelSelectWidget::onOkKeyPressed()
{
	setModified();
	if (isChannelInBouquet(selected))
		g_bouquetManager->Bouquets[bouquet]->removeService(Channels[selected]->channel_id);
	else
		addChannelToBouquet( bouquet, Channels[selected]->channel_id);

	bouquetChannels = mode == CZapitClient::MODE_TV ? &(g_bouquetManager->Bouquets[bouquet]->tvChannels) : &(g_bouquetManager->Bouquets[bouquet]->radioChannels);

	paintItem( selected, selected - liststart, false);
	g_RCInput->postMsg( CRCInput::RC_down, 0 );
}

int CBEChannelSelectWidget::exec(CMenuTarget* parent, const std::string & actionKey)
{
	int fw = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getWidth();
	int fh = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight();
	width  = w_max (52 * fw, 20);
	height = h_max (20 * fh, 50);

	listmaxshow = (height-theight-0)/iheight;
	height = theight+0+listmaxshow*iheight; // recalc height
	x = frameBuffer->getScreenX() + (frameBuffer->getScreenWidth() - width) / 2;
	y = frameBuffer->getScreenY() + (frameBuffer->getScreenHeight() - height) / 2;

	bouquetChannels = mode == CZapitClient::MODE_TV ? &(g_bouquetManager->Bouquets[bouquet]->tvChannels) : &(g_bouquetManager->Bouquets[bouquet]->radioChannels);

	Channels.clear();
	if (mode == CZapitClient::MODE_RADIO) {
		for (tallchans_iterator it = allchans.begin(); it != allchans.end(); it++)
			if (it->second.getServiceType() == ST_DIGITAL_RADIO_SOUND_SERVICE)
				Channels.push_back(&(it->second));
	} else {
		for (tallchans_iterator it = allchans.begin(); it != allchans.end(); it++)
			if (it->second.getServiceType() == ST_DIGITAL_TELEVISION_SERVICE)
				Channels.push_back(&(it->second));
	}
	sort(Channels.begin(), Channels.end(), CmpChannelByChName());

	return CListBox::exec(parent, actionKey);
}

const struct button_label CBEChannelSelectButtons[] =
{
	{ NEUTRINO_ICON_BUTTON_OKAY, LOCALE_BOUQUETEDITOR_SWITCH },
	{ NEUTRINO_ICON_BUTTON_HOME, LOCALE_BOUQUETEDITOR_RETURN }
};

void CBEChannelSelectWidget::paintFoot()
{
	int _ButtonHeight = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight()+8;
	::paintButtons(x, y+height, width, 2, CBEChannelSelectButtons, _ButtonHeight);

#if 0
	frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_OKAY, x+width- 3* ButtonWidth+ 8, y+height+1);
	g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(x+width- 3* ButtonWidth+ 38, y+height+24 - 2, width, g_Locale->getText(LOCALE_BOUQUETEDITOR_SWITCH), COL_INFOBAR, 0, true); // UTF-8

	frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_HOME, x+width - ButtonWidth+ 8, y+height+1);
	g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(x+width - ButtonWidth+ 38, y+height+24 - 2, width, g_Locale->getText(LOCALE_BOUQUETEDITOR_RETURN), COL_INFOBAR, 0, true); // UTF-8
#endif
}
