/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2011 CoolStream International Ltd

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

#include <gui/widget/messagebox.h>
#include "bouqueteditor_channels.h"

#include <global.h>
#include <neutrino.h>

#include <driver/fontrenderer.h>
#include <driver/screen_max.h>
#include "bouqueteditor_chanselect.h"
#include <gui/widget/buttons.h>
#include <gui/widget/icons.h>

#include <zapit/getservices.h>
#include <zapit/femanager.h>

extern CBouquetManager *g_bouquetManager;

CBEChannelWidget::CBEChannelWidget(const std::string & Caption, unsigned int Bouquet)
{
	int icol_w, icol_h;
	frameBuffer = CFrameBuffer::getInstance();
	selected = 0;
	iconoffset = 0;
	origPosition = 0;
	newPosition = 0;
	listmaxshow = 0;
	numwidth = 0;
	info_height = 0;
	channelsChanged = false;
	width = 0;
	height = 0;
	x = 0;
	y = 0;

	theight     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	fheight     = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->getHeight();
	footerHeight= g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight()+6;

	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_YELLOW, &icol_w, &icol_h);
	iheight = std::max(fheight, icol_h+2);

	iconoffset = std::max(iconoffset, icol_w);

	liststart = 0;
	state = beDefault;
	caption = Caption;
	bouquet = Bouquet;
	mode = CZapitClient::MODE_TV;
	dline = NULL;
	ibox = NULL;
	Channels = NULL;
}

CBEChannelWidget::~CBEChannelWidget()
{
	delete dline;
	delete ibox;
}

void CBEChannelWidget::paintItem(int pos)
{
	uint8_t    color;
	fb_pixel_t bgcolor;
	int ypos = y+ theight+0 + pos*iheight;
	unsigned int current = liststart + pos;

	if(current == selected) {
		color   = COL_MENUCONTENTSELECTED;
		bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;

		if(current < Channels->size()) {
			initItem2DetailsLine (pos, current);
			paintDetails(current);
		}
	
		frameBuffer->paintBoxRel(x,ypos, width- 15, iheight, COL_MENUCONTENT_PLUS_0);
		frameBuffer->paintBoxRel(x,ypos, width- 15, iheight, bgcolor, RADIUS_LARGE);
	} else {
		color   = COL_MENUCONTENT;
		bgcolor = COL_MENUCONTENT_PLUS_0;
		frameBuffer->paintBoxRel(x,ypos, width- 15, iheight, bgcolor);
	}

	if ((current == selected) && (state == beMoving))
	{
		frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_YELLOW, x + 10, ypos, iheight);
	}
	if(current < Channels->size())
	{
		//g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x+ 5+ numwidth+ 10, ypos+ fheight, width- numwidth- 20- 15, (*Channels)[current]->getName(), color, 0, true);
		//FIXME numwidth ? we not show chan numbers
		g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x+ 20 + iconoffset, ypos + iheight - (iheight-fheight)/2, width- iconoffset- 20, (*Channels)[current]->getName(), color, 0, true);
		if((*Channels)[current]->scrambled)
			frameBuffer->paintIcon(NEUTRINO_ICON_SCRAMBLED, x+width- 15 - 28, ypos, fheight);

	}
}

void CBEChannelWidget::paint()
{
	liststart = (selected/listmaxshow)*listmaxshow;
	int lastnum =  liststart + listmaxshow;

	if(lastnum<10)
		numwidth = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->getRenderWidth("0");
	else if(lastnum<100)
		numwidth = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->getRenderWidth("00");
	else if(lastnum<1000)
		numwidth = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->getRenderWidth("000");
	else if(lastnum<10000)
		numwidth = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->getRenderWidth("0000");
	else // if(lastnum<100000)
		numwidth = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->getRenderWidth("00000");

	for(unsigned int count=0;count<listmaxshow;count++)
	{
		paintItem(count);
	}

	int ypos = y+ theight;
	int sb = iheight* listmaxshow;
	frameBuffer->paintBoxRel(x+ width- 15,ypos, 15, sb,  COL_MENUCONTENT_PLUS_1);

	int sbc= ((Channels->size()- 1)/ listmaxshow)+ 1;
	int sbs= (selected/listmaxshow);
	if (sbc < 1)
		sbc = 1;

	frameBuffer->paintBoxRel(x+ width- 13, ypos+ 2+ sbs * (sb-4)/sbc, 11, (sb-4)/sbc,  COL_MENUCONTENT_PLUS_3);
}

void CBEChannelWidget::paintHead()
{
	frameBuffer->paintBoxRel(x,y, width,theight+0, COL_MENUHEAD_PLUS_0, RADIUS_LARGE, CORNER_TOP);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->RenderString(x+10,y+theight+0, width, caption.c_str() , COL_MENUHEAD, 0, true);
}

const struct button_label CBEChannelWidgetButtons[4] =
{
	{ NEUTRINO_ICON_BUTTON_RED   , LOCALE_BOUQUETEDITOR_DELETE     },
	{ NEUTRINO_ICON_BUTTON_GREEN , LOCALE_BOUQUETEDITOR_ADD        },
	{ NEUTRINO_ICON_BUTTON_YELLOW, LOCALE_BOUQUETEDITOR_MOVE       },
	{ NEUTRINO_ICON_BUTTON_BLUE  , LOCALE_BOUQUETEDITOR_SWITCHMODE }
};

void CBEChannelWidget::paintFoot()
{
	::paintButtons(x, y + (height-footerHeight), width, 4, CBEChannelWidgetButtons, width, footerHeight);
}

std::string CBEChannelWidget::getInfoText(int index)
{
	std::string res = "";
	
	std::string satname = CServiceManager::getInstance()->GetSatelliteName((*Channels)[index]->getSatellitePosition());
	transponder t;
	CServiceManager::getInstance()->GetTransponder((*Channels)[index]->getTransponderId(), t);
	std::string desc = t.description();
	if((*Channels)[index]->pname)
		desc = desc + " (" + std::string((*Channels)[index]->pname) + ")";
	else
		desc = desc + " (" + satname + ")";
	
	res = satname + "\n" + desc;
	
	return res;
}

void CBEChannelWidget::paintDetails(int index)
{
	//details line
	dline->paint();
	
	std::string str = getInfoText(index);
	
	//info box
	ibox->setText(str, CTextBox::AUTO_WIDTH | CTextBox::NO_AUTO_LINEBREAK, g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]);
	ibox->paint(CC_SAVE_SCREEN_YES);
}

void CBEChannelWidget::initItem2DetailsLine (int pos, int /*ch_index*/)
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
		if (ibox == NULL)
			ibox = new CComponentsInfoBox();

		if (ibox->isPainted())
			ibox->hide(CC_SAVE_SCREEN_NO);
		
		ibox->setDimensionsAll(x, ypos2, width, info_height);
		ibox->setFrameThickness(2);
#if 0			
		ibox->paint(false,true);
#endif
		ibox->setCornerRadius(RADIUS_LARGE);
		ibox->setShadowOnOff(CC_SHADOW_OFF);
	}
}

void CBEChannelWidget::clearItem2DetailsLine()
{
	if (dline)
		dline->kill(); //kill details line
	if (ibox)
		ibox->kill(); //kill info box
}

void CBEChannelWidget::hide()
{
	frameBuffer->paintBackgroundBoxRel(x,y, width,height+footerHeight);
	clearItem2DetailsLine ();
	frameBuffer->blit();
}

void CBEChannelWidget::updateSelection(unsigned int newpos)
{
        if(newpos == selected)
                return;

        unsigned int prev_selected = selected;
        selected = newpos;

        if (state == beDefault) {
                unsigned int oldliststart = liststart;
                liststart = (selected/listmaxshow)*listmaxshow;
                if(oldliststart!=liststart) {
                        paint();
                } else {
                        paintItem(prev_selected - liststart);
                        paintItem(selected - liststart);
                }
        } else {
                internalMoveChannel(prev_selected, selected);
        }
}

int CBEChannelWidget::exec(CMenuTarget* parent, const std::string & /*actionKey*/)
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	int res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	width  = frameBuffer->getScreenWidthRel();
	info_height = 2*iheight + 4;
	height = frameBuffer->getScreenHeightRel() - info_height;
	listmaxshow = (height-theight-footerHeight-0)/iheight;
	height = theight+footerHeight+listmaxshow*iheight; // recalc height

	x = getScreenStartX(width);
	if (x < ConnectLineBox_Width)
		x = ConnectLineBox_Width;
	y = getScreenStartY(height + info_height);

	Channels = mode == CZapitClient::MODE_TV ? &(g_bouquetManager->Bouquets[bouquet]->tvChannels) : &(g_bouquetManager->Bouquets[bouquet]->radioChannels);
	paintHead();
	paint();
	paintFoot();
	frameBuffer->blit();

	channelsChanged = false;

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_EPG]);

	bool loop=true;
	while (loop)
	{
		g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd );

		if ( msg <= CRCInput::RC_MaxRC )
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_EPG]);

		if ((msg == CRCInput::RC_timeout) ||
		    (msg == (neutrino_msg_t)g_settings.key_channelList_cancel))
		{
			if (state == beDefault)
			{
				loop = false;
			}
			else if (state == beMoving)
			{
				cancelMoveChannel();
			}
		}
		else if (msg==CRCInput::RC_up || msg==(neutrino_msg_t)g_settings.key_channelList_pageup)
		{
			if (!(Channels->empty())) {
                                int step = (msg == (neutrino_msg_t)g_settings.key_channelList_pageup) ? listmaxshow : 1;  // browse or step 1
                                int new_selected = selected - step;

                                if (new_selected < 0) {
                                        if (selected != 0 && step != 1)
                                                new_selected = 0;
                                        else
                                                new_selected = Channels->size() - 1;
                                }
                                updateSelection(new_selected);
			}
		}
		else if (msg==CRCInput::RC_down || msg==(neutrino_msg_t)g_settings.key_channelList_pagedown)
		{
                        if (!(Channels->empty())) {
                                int step =  ((int) msg == g_settings.key_channelList_pagedown) ? listmaxshow : 1;  // browse or step 1
                                int new_selected = selected + step;
                                if (new_selected >= (int) Channels->size()) {
                                        if ((Channels->size() - listmaxshow -1 < selected) && (selected != (Channels->size() - 1)) && (step != 1))
                                                new_selected = Channels->size() - 1;
                                        else if (((Channels->size() / listmaxshow) + 1) * listmaxshow == Channels->size() + listmaxshow) // last page has full entries
                                                new_selected = 0;
                                        else
                                                new_selected = ((step == (int) listmaxshow) && (new_selected < (int) (((Channels->size() / listmaxshow)+1) * listmaxshow))) ? (Channels->size() - 1) : 0;
                                }
                                updateSelection(new_selected);
                        }
		}
                else if (msg == (neutrino_msg_t) g_settings.key_list_start || msg == (neutrino_msg_t) g_settings.key_list_end) {
                        if (!(Channels->empty())) {
                                int new_selected = msg == (neutrino_msg_t) g_settings.key_list_start ? 0 : Channels->size() - 1;
                                updateSelection(new_selected);
                        }
                }
		else if(msg==CRCInput::RC_red)
		{
			if (state == beDefault)
				deleteChannel();
		}
		else if(msg==CRCInput::RC_green)
		{
			if (state == beDefault)
				addChannel();
		}
		else if(msg==CRCInput::RC_blue)
		{
			if (state == beDefault)
			{
				if (mode == CZapitClient::MODE_TV)
					mode = CZapitClient::MODE_RADIO;
				else
					mode = CZapitClient::MODE_TV;

				Channels = mode == CZapitClient::MODE_TV ? &(g_bouquetManager->Bouquets[bouquet]->tvChannels) : &(g_bouquetManager->Bouquets[bouquet]->radioChannels);

				selected = 0;
				paint();
			}
		}
		else if(msg==CRCInput::RC_yellow)
		{
			liststart = (selected/listmaxshow)*listmaxshow;
			if (state == beDefault)
				beginMoveChannel();
			paintItem(selected - liststart);
		}
		else if(msg==CRCInput::RC_ok)
		{
			if (state == beDefault)
			{
				if (selected < Channels->size()) /* Channels.size() might be 0 */
					g_Zapit->zapTo_serviceID((*Channels)[selected]->channel_id);

			} else if (state == beMoving) {
				finishMoveChannel();
			}
		}
		else if( CRCInput::isNumeric(msg) )
		{
			if (state == beDefault)
			{
				//kein pushback - wenn man versehentlich wo draufkommt is die edit-arbeit umsonst
				//selected = oldselected;
				//g_RCInput->postMsg( msg, data );
				//loop=false;
			}
			else if (state == beMoving)
			{
				cancelMoveChannel();
			}
		}
		else if((msg == CRCInput::RC_sat) || (msg == CRCInput::RC_favorites)) {
		}
		else
		{
			CNeutrinoApp::getInstance()->handleMsg( msg, data );
		}
		frameBuffer->blit();
	}
	hide();
	return res;
}

void CBEChannelWidget::deleteChannel()
{
	if (selected >= Channels->size()) /* Channels.size() might be 0 */
		return;

	if (ShowMsgUTF(LOCALE_FILEBROWSER_DELETE, (*Channels)[selected]->getName(), CMessageBox::mbrNo, CMessageBox::mbYes|CMessageBox::mbNo)!=CMessageBox::mbrYes)
		return;

	g_bouquetManager->Bouquets[bouquet]->removeService((*Channels)[selected]->channel_id);

	Channels = mode == CZapitClient::MODE_TV ? &(g_bouquetManager->Bouquets[bouquet]->tvChannels) : &(g_bouquetManager->Bouquets[bouquet]->radioChannels);

	if (selected >= Channels->size())
		selected = Channels->empty() ? 0 : (Channels->size() - 1);
	channelsChanged = true;
	paint();
}

void CBEChannelWidget::addChannel()
{
	CBEChannelSelectWidget* channelSelectWidget = new CBEChannelSelectWidget(caption, bouquet, mode);

	channelSelectWidget->exec(this, "");
	if (channelSelectWidget->hasChanged())
	{
		channelsChanged = true;
		Channels = mode == CZapitClient::MODE_TV ? &(g_bouquetManager->Bouquets[bouquet]->tvChannels) : &(g_bouquetManager->Bouquets[bouquet]->radioChannels);
	}
	delete channelSelectWidget;
	paintHead();
	paint();
	paintFoot();
}

void CBEChannelWidget::beginMoveChannel()
{
	state = beMoving;
	origPosition = selected;
	newPosition = selected;
}

void CBEChannelWidget::finishMoveChannel()
{
	state = beDefault;
	paint();
}

void CBEChannelWidget::cancelMoveChannel()
{
	state = beDefault;
	internalMoveChannel( newPosition, origPosition);
	channelsChanged = false;
}

void CBEChannelWidget::internalMoveChannel( unsigned int fromPosition, unsigned int toPosition)
{
	if ( (int) toPosition == -1 ) return;
	if ( toPosition == Channels->size()) return;

	g_bouquetManager->Bouquets[bouquet]->moveService(fromPosition, toPosition,
		mode == CZapitClient::MODE_TV ? 1 : 2);

	channelsChanged = true;
	Channels = mode == CZapitClient::MODE_TV ? &(g_bouquetManager->Bouquets[bouquet]->tvChannels) : &(g_bouquetManager->Bouquets[bouquet]->radioChannels);

	selected = toPosition;
	newPosition = toPosition;
	paint();
}

bool CBEChannelWidget::hasChanged()
{
	return (channelsChanged);
}
