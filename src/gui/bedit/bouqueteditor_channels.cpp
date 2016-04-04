/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2011 CoolStream International Ltd
	Copyright (C) 2009,2011,2013,2016 Stefan Seyfried

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

#include <gui/widget/msgbox.h>
#include "bouqueteditor_channels.h"

#include <global.h>
#include <neutrino.h>

#include <driver/fontrenderer.h>
#include <driver/screen_max.h>
#include "bouqueteditor_chanselect.h"
#include <gui/components/cc.h>
#include <gui/widget/icons.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/keyboard_input.h>

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
	footerHeight= footer.getHeight();

	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_YELLOW, &icol_w, &icol_h);
	iheight = std::max(fheight, icol_h+2);
	iconoffset = std::max(iconoffset, icol_w);

	frameBuffer->getIconSize(NEUTRINO_ICON_LOCK, &icol_w, &icol_h);
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
	int ypos = y+ theight+0 + pos*iheight;
	unsigned int current = liststart + pos;

	bool i_selected	= current == selected;
	int i_radius	= RADIUS_NONE;

	fb_pixel_t color;
	fb_pixel_t bgcolor;

	getItemColors(color, bgcolor, i_selected);

	if (i_selected)
	{
		if (current < Channels->size())
		{
			initItem2DetailsLine(pos, current);
			paintDetails(current);
		}
		i_radius = RADIUS_LARGE;
	}
	else
	{
		if (current < Channels->size() && ((*Channels)[current]->flags & CZapitChannel::NOT_PRESENT))
			color = COL_MENUCONTENTINACTIVE_TEXT;
	}

	if (i_radius)
		frameBuffer->paintBoxRel(x, ypos, width- 15, iheight, COL_MENUCONTENT_PLUS_0);
	frameBuffer->paintBoxRel(x, ypos, width- 15, iheight, bgcolor, i_radius);

	if ((current == selected) && (state == beMoving)) {
		frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_YELLOW, x + 5, ypos, iheight);
	}
	if (current < Channels->size())	{
		if ((*Channels)[current]->bLocked) {
			frameBuffer->paintIcon(NEUTRINO_ICON_LOCK, x + 22, ypos, iheight);
		}
		//g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x+ 5+ numwidth+ 10, ypos+ fheight, width- numwidth- 20- 15, (*Channels)[current]->getName(), color);
		//FIXME numwidth ? we not show chan numbers
		g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x+ 22 + iconoffset, ypos + iheight - (iheight-fheight)/2, width- iconoffset- 20, (*Channels)[current]->getName(), color);
		if((*Channels)[current]->scrambled)
			frameBuffer->paintIcon(NEUTRINO_ICON_SCRAMBLED, x+width- 15 - 28, ypos, fheight);
		else if (!(*Channels)[current]->getUrl().empty())
			frameBuffer->paintIcon(NEUTRINO_ICON_STREAMING, x+width- 15 - 28, ypos, fheight);
	}
}

void CBEChannelWidget::paint()
{
	liststart = (selected/listmaxshow)*listmaxshow;
	int lastnum =  liststart + listmaxshow;

	numwidth = 0;
	int maxDigitWidth = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->getMaxDigitWidth();
	int _lastnum = lastnum;
        while (_lastnum) {
                numwidth += maxDigitWidth;
                _lastnum /= 10;
        }

	for(unsigned int count=0;count<listmaxshow;count++)
	{
		paintItem(count);
	}

	int ypos = y+ theight;
	int sb = iheight* listmaxshow;
	frameBuffer->paintBoxRel(x+ width- 15,ypos, 15, sb,  COL_SCROLLBAR_PASSIVE_PLUS_0);

	int sbc= ((Channels->size()- 1)/ listmaxshow)+ 1;
	if (sbc < 1)
		sbc = 1;

	int sbh= (sb- 4)/ sbc;
	int sbs= (selected/listmaxshow);

	if (sbh)
		frameBuffer->paintBoxRel(x+ width- 13, ypos+ 2+ int(sbs* sbh) , 11, int(sbh),  COL_SCROLLBAR_ACTIVE_PLUS_0);
}

void CBEChannelWidget::paintHead()
{
	CComponentsHeader header(x, y, width, theight, caption, "" /*no header icon*/, CComponentsHeader::CC_BTN_EXIT);
	header.paint(CC_SAVE_SCREEN_NO);
}

const struct button_label CBEChannelWidgetButtons[6] =
{
	{ NEUTRINO_ICON_BUTTON_RED   , LOCALE_BOUQUETEDITOR_DELETE     },
	{ NEUTRINO_ICON_BUTTON_GREEN , LOCALE_BOUQUETEDITOR_ADD        },
	{ NEUTRINO_ICON_BUTTON_YELLOW, LOCALE_BOUQUETEDITOR_MOVE       },
	{ NEUTRINO_ICON_BUTTON_BLUE  , LOCALE_BOUQUETEDITOR_RENAME },
	{ NEUTRINO_ICON_BUTTON_DUMMY_SMALL, LOCALE_BOUQUETEDITOR_SWITCHMODE },
        //{ NEUTRINO_ICON_BUTTON_FORWARD  , LOCALE_BOUQUETEDITOR_MOVE_TO }, // TODO upgrade
        { NEUTRINO_ICON_BUTTON_STOP  , LOCALE_BOUQUETEDITOR_LOCK     }
};

void CBEChannelWidget::paintFoot()
{
	size_t numbuttons = sizeof(CBEChannelWidgetButtons)/sizeof(CBEChannelWidgetButtons[0]);
	footer.paintButtons(x, y + (height-footerHeight), width, footerHeight, numbuttons, CBEChannelWidgetButtons, width/numbuttons-20);
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
	
	res = satname + " " + desc;
	
	return res;
}

void CBEChannelWidget::paintDetails(int index)
{
	//details line
	dline->paint(CC_SAVE_SCREEN_NO);
	
	std::string str = getInfoText(index);
	
	//info box
	ibox->setText(str, CTextBox::AUTO_WIDTH | CTextBox::NO_AUTO_LINEBREAK, g_Font[SNeutrinoSettings::FONT_TYPE_MENU_HINT]);
	ibox->setColorBody(COL_MENUCONTENTDARK_PLUS_0);
	ibox->setTextColor(COL_MENUCONTENTDARK_TEXT);
	ibox->paint(CC_SAVE_SCREEN_NO);
}

void CBEChannelWidget::initItem2DetailsLine (int pos, int /*ch_index*/)
{
	int xpos  = x - ConnectLineBox_Width;
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
			dline = new CComponentsDetailLine(xpos, ypos1a, ypos2a, fheight/2, info_height-RADIUS_LARGE*2);
		dline->setYPos(ypos1a);
		
		//infobox
		if (ibox == NULL){
			ibox = new CComponentsInfoBox();
		}

		if (ibox->isPainted())
			ibox->hide();

		ibox->setDimensionsAll(x, ypos2, width, info_height);
		ibox->setFrameThickness(2);
#if 0
		ibox->paint(false,true);
#endif
		ibox->setCorner(RADIUS_LARGE);
		ibox->disableShadow();
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
}

void CBEChannelWidget::updateSelection(unsigned int newpos)
{
        if (newpos == selected || newpos == (unsigned int)-1)
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


	mode = CZapitClient::MODE_TV;
	if (g_bouquetManager->Bouquets[bouquet]->tvChannels.empty())
		mode = CZapitClient::MODE_RADIO;
	else if (g_bouquetManager->Bouquets[bouquet]->radioChannels.empty())
		mode = CZapitClient::MODE_TV;
	Channels = mode == CZapitClient::MODE_TV ? &(g_bouquetManager->Bouquets[bouquet]->tvChannels) : &(g_bouquetManager->Bouquets[bouquet]->radioChannels);

	paintHead();
	paint();
	paintFoot();

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
		else if (msg == CRCInput::RC_up || msg == (neutrino_msg_t)g_settings.key_pageup ||
			 msg == CRCInput::RC_down || msg == (neutrino_msg_t)g_settings.key_pagedown)
		{
			int new_selected = UpDownKey(*Channels, msg, listmaxshow, selected);
			updateSelection(new_selected);
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
		else if(msg==CRCInput::RC_yellow)
		{
			liststart = (selected/listmaxshow)*listmaxshow;
			if (state == beDefault)
				beginMoveChannel();
			paintItem(selected - liststart);
		}
		else if(msg==CRCInput::RC_blue)
		{
			if (selected < Channels->size()) /* Channels->size() might be 0 */
			{
				if (state == beDefault)
					renameChannel();
			}
		}
		else if(msg==CRCInput::RC_stop)
		{
			if (selected < Channels->size()) /* Channels->size() might be 0 */
			{
				if (state == beDefault)
					switchLockChannel();
			}
		}
/* TODO upgrade
		else if (msg == CRCInput::RC_forward )
		{
			if (selected < Channels->size())
			{
				if (state == beDefault)
					moveChannelToBouquet();
			}
		}
*/
		else if( msg == (neutrino_msg_t) g_settings.key_tvradio_mode || msg==CRCInput::RC_tv  ) {
			if (mode == CZapitClient::MODE_TV)
				mode = CZapitClient::MODE_RADIO;
			else
				mode = CZapitClient::MODE_TV;

			Channels = mode == CZapitClient::MODE_TV ? &(g_bouquetManager->Bouquets[bouquet]->tvChannels) : &(g_bouquetManager->Bouquets[bouquet]->radioChannels);

			selected = 0;
			paintHead();
			paint();
			paintFoot();
		}

		else if(msg==CRCInput::RC_ok)
		{
			if (state == beDefault)
			{
				if (selected < Channels->size()) /* Channels.size() might be 0 */
					g_Zapit->zapTo_serviceID((*Channels)[selected]->getChannelID());

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

void CBEChannelWidget::deleteChannel()
{
	if (selected >= Channels->size()) /* Channels.size() might be 0 */
		return;

	if (ShowMsg(LOCALE_FILEBROWSER_DELETE, (*Channels)[selected]->getName(), CMsgBox::mbrNo, CMsgBox::mbYes|CMsgBox::mbNo)!=CMsgBox::mbrYes)
		return;

	g_bouquetManager->Bouquets[bouquet]->removeService((*Channels)[selected]->getChannelID());

	Channels = mode == CZapitClient::MODE_TV ? &(g_bouquetManager->Bouquets[bouquet]->tvChannels) : &(g_bouquetManager->Bouquets[bouquet]->radioChannels);

	if (selected >= Channels->size())
		selected = Channels->empty() ? 0 : (Channels->size() - 1);
	channelsChanged = true;
	paint();
}

void CBEChannelWidget::renameChannel()
{
	std::string newName= inputName((*Channels)[selected]->getName().c_str(), LOCALE_BOUQUETEDITOR_NEWBOUQUETNAME);

	if (newName != (*Channels)[selected]->getName())
	{
		if(newName.empty())
			(*Channels)[selected]->setUserName("");
		else
			(*Channels)[selected]->setUserName(newName);

		channelsChanged = true;
	}
	paintHead();
	paint();
	paintFoot();
}

void CBEChannelWidget::switchLockChannel()
{
	(*Channels)[selected]->bLocked = !(*Channels)[selected]->bLocked;
	channelsChanged = true;
	paintItem(selected - liststart);

	if (selected + 1 < (*Channels).size())
		g_RCInput->postMsg((neutrino_msg_t) CRCInput::RC_down, 0);
}

void CBEChannelWidget::addChannel()
{
	CBEChannelSelectWidget* channelSelectWidget = new CBEChannelSelectWidget(caption, g_bouquetManager->Bouquets[bouquet], mode);

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
	channelsChanged = channelsChanged | true;
	paint();
}

void CBEChannelWidget::cancelMoveChannel()
{
	state = beDefault;
	internalMoveChannel( newPosition, origPosition);
	channelsChanged = channelsChanged | false;
}
/* TODO upgrade (taken from channellist.cpp)
void CBEChannelWidget::moveChannelToBouquet()
{
	if (addChannelToBouquet())
		deleteChannel(false);
	else
		paint();

	paintHead();
}
*/
void CBEChannelWidget::internalMoveChannel( unsigned int fromPosition, unsigned int toPosition)
{
	if ( (int) toPosition == -1 ) return;
	if ( toPosition == Channels->size()) return;

	g_bouquetManager->Bouquets[bouquet]->moveService(fromPosition, toPosition,
		mode == CZapitClient::MODE_TV ? 1 : 2);

	//channelsChanged = true;
	Channels = mode == CZapitClient::MODE_TV ? &(g_bouquetManager->Bouquets[bouquet]->tvChannels) : &(g_bouquetManager->Bouquets[bouquet]->radioChannels);

	selected = toPosition;
	newPosition = toPosition;
	paint();
}

std::string CBEChannelWidget::inputName(const char * const defaultName, const neutrino_locale_t _caption)
{
	std::string Name = defaultName;

	CKeyboardInput * nameInput = new CKeyboardInput(_caption, &Name);
	nameInput->exec(this, "");

	delete nameInput;

	return std::string(Name);
}

bool CBEChannelWidget::hasChanged()
{
	return (channelsChanged);
}
