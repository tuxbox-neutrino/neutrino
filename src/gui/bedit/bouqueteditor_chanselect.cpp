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

#include <unistd.h>
#include <global.h>
#include <neutrino.h>
#include "bouqueteditor_chanselect.h"

#include <driver/fontrenderer.h>
#include <driver/screen_max.h>
#include <gui/widget/icons.h>


#include <zapit/zapit.h>
#include <zapit/getservices.h>

extern CBouquetManager *g_bouquetManager;

CBEChannelSelectWidget::CBEChannelSelectWidget(const std::string & Caption, CZapitBouquet* Bouquet, CZapitClient::channelsMode Mode)
	:CListBox(Caption.c_str())
{
	int icol_w, icol_h;

	bouquet = Bouquet;
	mode =    Mode;
	iconoffset = 0;
	info_height =  0;
	theight     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	fheight     = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->getHeight();
	footerHeight= footer.getHeight();

	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_GREEN, &icol_w, &icol_h);
	iheight = std::max(fheight, icol_h+2);
	iconoffset = std::max(iconoffset, icol_w);

	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_OKAY, &icol_w, &icol_h);
	ButtonHeight = std::max(footerHeight, icol_h+4);
	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_HOME, &icol_w, &icol_h);
	ButtonHeight = std::max(footerHeight, icol_h+4);

	liststart = 0;
	channellist_sort_mode = SORT_ALPHA;
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
		if ((*bouquetChannels)[i]->getChannelID() == Channels[index]->getChannelID())
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
	int i_radius = RADIUS_NONE;

	fb_pixel_t color;
	fb_pixel_t bgcolor;

	getItemColors(color, bgcolor, pselected);

	if (pselected)
	{
		if (itemNr < getItemCount())
		{
			initItem2DetailsLine (paintNr, itemNr);
			paintDetails(itemNr);
		}
		i_radius = RADIUS_LARGE;
	}
	else
	{
		if (itemNr < getItemCount() && (Channels[itemNr]->flags & CZapitChannel::NOT_PRESENT))
			color = COL_MENUCONTENTINACTIVE_TEXT;
	}

	if (i_radius)
		frameBuffer->paintBoxRel(x,ypos, width- 15, iheight, COL_MENUCONTENT_PLUS_0);
	frameBuffer->paintBoxRel(x,ypos, width- 15, iheight, bgcolor, i_radius);

	if(itemNr < getItemCount())
	{
		if( isChannelInBouquet(itemNr))
			frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_GREEN, x + OFFSET_INNER_MID, ypos, iheight);
		else
			frameBuffer->paintBoxRel(x + OFFSET_INNER_MID, ypos, iconoffset, iheight, bgcolor);

		g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x + 2*OFFSET_INNER_MID + 2*iconoffset, ypos + iheight - (iheight-fheight)/2, width - 3*OFFSET_INNER_MID - 2*iconoffset, Channels[itemNr]->getName(), color);
		if(Channels[itemNr]->scrambled)
			frameBuffer->paintIcon(NEUTRINO_ICON_SCRAMBLED, x + width - 15 - OFFSET_INNER_MID - iconoffset, ypos, fheight);
		else if (!Channels[itemNr]->getUrl().empty())
			frameBuffer->paintIcon(NEUTRINO_ICON_STREAMING, x + width - 15 - OFFSET_INNER_MID - iconoffset, ypos, fheight);
	}
}

void CBEChannelSelectWidget::onOkKeyPressed()
{
	if (selected >= Channels.size())
		return;
	setModified();
	if (isChannelInBouquet(selected))
		bouquet->removeService(Channels[selected]);
	else
		bouquet->addService(Channels[selected]);

	bouquetChannels = mode == CZapitClient::MODE_TV ? &(bouquet->tvChannels) : &(bouquet->radioChannels);

	paintItem( selected, selected - liststart, false);
	g_RCInput->postMsg( CRCInput::RC_down, 0 );
}

void CBEChannelSelectWidget::onRedKeyPressed()
{
	if (selected >= Channels.size())
		return;

	channellist_sort_mode++;
	if(channellist_sort_mode > SORT_END)
		channellist_sort_mode = 0;
	switch(channellist_sort_mode)
	{
		case SORT_ALPHA:
			sort(Channels.begin(), Channels.end(), CmpChannelByChName());
		break;
		case SORT_FREQ:
			sort(Channels.begin(), Channels.end(), CmpChannelByFreq());
		break;
		case SORT_SAT:
			sort(Channels.begin(), Channels.end(), CmpChannelBySat());
		break;
		case SORT_CH_NUMBER:
			sort(Channels.begin(), Channels.end(), CmpChannelByChNum());
		break;
		default:
			sort(Channels.begin(), Channels.end(), CmpChannelByChName());
		break;	  
	}
	paintFoot();
	paint();
}

int CBEChannelSelectWidget::exec(CMenuTarget* parent, const std::string & actionKey)
{
	width  = frameBuffer->getScreenWidthRel();
	info_height = 2*iheight + 4;
	height = frameBuffer->getScreenHeightRel() - info_height;
	listmaxshow = (height-theight-footerHeight-0)/iheight;
	height = theight+footerHeight+listmaxshow*iheight; // recalc height

	x = getScreenStartX(width);
	if (x < DETAILSLINE_WIDTH)
		x = DETAILSLINE_WIDTH;
	y = getScreenStartY(height + info_height);

	bouquetChannels = mode == CZapitClient::MODE_TV ? &(bouquet->tvChannels) : &(bouquet->radioChannels);

	Channels.clear();
	if (mode == CZapitClient::MODE_RADIO)
		CServiceManager::getInstance()->GetAllRadioChannels(Channels);
	else
		CServiceManager::getInstance()->GetAllTvChannels(Channels);

	sort(Channels.begin(), Channels.end(), CmpChannelByChName());

	return CListBox::exec(parent, actionKey);
}

const struct button_label CBEChannelSelectButtons[] =
{
	{ NEUTRINO_ICON_BUTTON_RED,  LOCALE_CHANNELLIST_FOOT_SORT_ALPHA},
	{ NEUTRINO_ICON_BUTTON_OKAY, LOCALE_BOUQUETEDITOR_SWITCH },
	{ NEUTRINO_ICON_BUTTON_HOME, LOCALE_BOUQUETEDITOR_RETURN }
};

void CBEChannelSelectWidget::paintFoot()
{
	const short numbuttons = sizeof(CBEChannelSelectButtons)/sizeof(CBEChannelSelectButtons[0]);
	struct button_label Button[numbuttons];
	for (int i = 0; i < numbuttons; i++)
		Button[i] = CBEChannelSelectButtons[i];

	switch (channellist_sort_mode) {
		case SORT_ALPHA:
			Button[0].locale = LOCALE_CHANNELLIST_FOOT_SORT_ALPHA;
		break;
		case SORT_FREQ:
			Button[0].locale = LOCALE_CHANNELLIST_FOOT_SORT_FREQ;
		break;
		case SORT_SAT:
			Button[0].locale = LOCALE_CHANNELLIST_FOOT_SORT_SAT;
		break;
		case SORT_CH_NUMBER:
			Button[0].locale = LOCALE_CHANNELLIST_FOOT_SORT_CHNUM;
		break;
		default:
			Button[0].locale = LOCALE_CHANNELLIST_FOOT_SORT_ALPHA;
		break;
	}

	footer.paintButtons(x, y + (height-footerHeight), width, footerHeight, numbuttons, Button, width/numbuttons-20);
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
	dline->paint(CC_SAVE_SCREEN_NO);
	
	std::string str = getInfoText(index);
	
	//info box
	ibox->setText(str, CTextBox::AUTO_WIDTH | CTextBox::NO_AUTO_LINEBREAK, g_Font[SNeutrinoSettings::FONT_TYPE_MENU_HINT]);
	ibox->setColorBody(COL_MENUCONTENTDARK_PLUS_0);
	ibox->setTextColor(COL_MENUCONTENTDARK_TEXT);
	ibox->paint(false);
}

void CBEChannelSelectWidget::initItem2DetailsLine (int pos, int /*ch_index*/)
{
	int xpos  = x - DETAILSLINE_WIDTH;
	int ypos1 = y + theight+0 + pos*iheight;
	int ypos2 = y + height + OFFSET_INTER;
	int ypos1a = ypos1 + (fheight/2);
	int ypos2a = ypos2 + (info_height/2);

	if (dline)
		dline->kill(); //kill details line

	// init Line if detail info (and not valid list pos)
	if (pos >= 0)
	{
		if (dline == NULL)
			dline = new CComponentsDetailsLine(xpos, ypos1a, ypos2a, fheight/2, info_height-RADIUS_LARGE*2);
		dline->setYPos(ypos1a);

		//infobox
		if (ibox){
			ibox->setDimensionsAll(x, ypos2, width, info_height);
			ibox->setFrameThickness(2);
			ibox->setCorner(RADIUS_LARGE);
			ibox->disableShadow();
		}
	}
}

void CBEChannelSelectWidget::hide()
{
	dline->kill(); //kill details line
	ibox->kill();
	CListBox::hide();
}
