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

#include <global.h>
#include <neutrino.h>
#include "bouqueteditor_chanselect.h"

#include <driver/fontrenderer.h>
#include <driver/screen_max.h>
#include <gui/widget/icons.h>
#include <gui/widget/buttons.h>

#include <zapit/zapit.h>
#include <zapit/getservices.h>

extern CBouquetManager *g_bouquetManager;

CBEChannelSelectWidget::CBEChannelSelectWidget(const std::string & Caption, unsigned int Bouquet, CZapitClient::channelsMode Mode)
	:CListBox(Caption.c_str())
{
	int icol_w, icol_h;

	bouquet = Bouquet;
	mode =    Mode;
	iconoffset = 0;
	info_height =  0;
	theight     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	fheight     = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->getHeight();
	footerHeight= g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight()+6;

	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_GREEN, &icol_w, &icol_h);
	iheight = std::max(fheight, icol_h+2);
	iconoffset = std::max(iconoffset, icol_w);

	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_OKAY, &icol_w, &icol_h);
	ButtonHeight = std::max(footerHeight, icol_h+4);
	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_HOME, &icol_w, &icol_h);
	ButtonHeight = std::max(footerHeight, icol_h+4);

	liststart = 0;
	bouquetChannels = NULL;
	dline = NULL;
	ibox = new CComponentsInfoBox();
}

CBEChannelSelectWidget::~CBEChannelSelectWidget()
{
	delete dline;
	delete ibox;
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

		if(itemNr < getItemCount()) {
			initItem2DetailsLine (paintNr, itemNr);
			paintDetails(itemNr);
		}

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
		if(Channels[itemNr]->scrambled)
			frameBuffer->paintIcon(NEUTRINO_ICON_SCRAMBLED, x+width- 15 - 28, ypos, fheight);
	}
	frameBuffer->blit();
}

void CBEChannelSelectWidget::onOkKeyPressed()
{
	if (selected >= Channels.size())
		return;
	setModified();
	if (isChannelInBouquet(selected))
		g_bouquetManager->Bouquets[bouquet]->removeService(Channels[selected]->channel_id);
	else
		CZapit::getInstance()->addChannelToBouquet( bouquet, Channels[selected]->channel_id);

	bouquetChannels = mode == CZapitClient::MODE_TV ? &(g_bouquetManager->Bouquets[bouquet]->tvChannels) : &(g_bouquetManager->Bouquets[bouquet]->radioChannels);

	paintItem( selected, selected - liststart, false);
	g_RCInput->postMsg( CRCInput::RC_down, 0 );
}

int CBEChannelSelectWidget::exec(CMenuTarget* parent, const std::string & actionKey)
{
	width  = frameBuffer->getScreenWidthRel();
	info_height = 2*iheight + 4;
	height = frameBuffer->getScreenHeightRel() - info_height;
	listmaxshow = (height-theight-footerHeight-0)/iheight;
	height = theight+footerHeight+listmaxshow*iheight; // recalc height

	x = getScreenStartX(width);
	if (x < ConnectLineBox_Width)
		x = ConnectLineBox_Width;
	y = getScreenStartY(height + info_height);

	bouquetChannels = mode == CZapitClient::MODE_TV ? &(g_bouquetManager->Bouquets[bouquet]->tvChannels) : &(g_bouquetManager->Bouquets[bouquet]->radioChannels);

	Channels.clear();
	if (mode == CZapitClient::MODE_RADIO) {
		CServiceManager::getInstance()->GetAllRadioChannels(Channels);
	} else {
		CServiceManager::getInstance()->GetAllTvChannels(Channels);
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
	::paintButtons(x, y + (height-footerHeight), width, 2, CBEChannelSelectButtons, width, footerHeight);
	frameBuffer->blit();

#if 0
	frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_OKAY, x+width- 3* ButtonWidth+ 8, y+height+1);
	g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(x+width- 3* ButtonWidth+ 38, y+height+24 - 2, width, g_Locale->getText(LOCALE_BOUQUETEDITOR_SWITCH), COL_INFOBAR, 0, true); // UTF-8

	frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_HOME, x+width - ButtonWidth+ 8, y+height+1);
	g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(x+width - ButtonWidth+ 38, y+height+24 - 2, width, g_Locale->getText(LOCALE_BOUQUETEDITOR_RETURN), COL_INFOBAR, 0, true); // UTF-8
#endif
}

std::string CBEChannelSelectWidget::getInfoText(int index)
{
	std::string res = "";
	
	std::string satname = CServiceManager::getInstance()->GetSatelliteName(Channels[index]->getSatellitePosition());
	transponder t;
	CServiceManager::getInstance()->GetTransponder(Channels[index]->getTransponderId(), t);
	std::string desc = t.description();
	if(Channels[index]->pname)
		desc = desc + " (" + std::string(Channels[index]->pname) + ")";
	else
		desc = desc + " (" + satname + ")";
	
	res = satname + " " + desc;
	
	return res;
}

void CBEChannelSelectWidget::paintDetails(int index)
{
	//details line
	dline->paint();
	
	std::string str = getInfoText(index);
	
	//info box
	ibox->setText(str, CTextBox::AUTO_WIDTH | CTextBox::NO_AUTO_LINEBREAK, g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]);
	ibox->setColorBody(COL_MENUCONTENTDARK_PLUS_0);
	ibox->paint(false);
}

void CBEChannelSelectWidget::initItem2DetailsLine (int pos, int /*ch_index*/)
{
	int xpos  = x - ConnectLineBox_Width;
	int ypos1 = y + theight+0 + pos*fheight;
	int ypos2 = y + height + INFO_BOX_Y_OFFSET;
	int ypos1a = ypos1 + (fheight/2)-2;
	int ypos2a = ypos2 + (info_height/2)-2;

	if (dline)
		dline->kill(); //kill details line

	// init Line if detail info (and not valid list pos)
	if (pos >= 0)
	{
		if (dline == NULL)
			dline = new CComponentsDetailLine(xpos, ypos1a, ypos2a, fheight/2+1, info_height-RADIUS_LARGE*2);
		dline->setYPos(ypos1a);

		//infobox
		if (ibox){
			ibox->setDimensionsAll(x, ypos2, width, info_height);
			ibox->setFrameThickness(2);
			ibox->setCornerRadius(RADIUS_LARGE);
			ibox->setShadowOnOff(CC_SHADOW_OFF);
		}
	}
}

