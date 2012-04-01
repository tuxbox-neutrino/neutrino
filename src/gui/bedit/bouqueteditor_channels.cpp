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

#include <gui/widget/buttons.h>
#include <gui/widget/icons.h>
#include <gui/widget/messagebox.h>

#include <gui/bedit/bouqueteditor_channels.h>

#include <global.h>
#include <neutrino.h>

#include <driver/fontrenderer.h>
#include <driver/screen_max.h>
#include <gui/bedit/bouqueteditor_chanselect.h>
#include <gui/widget/buttons.h>
#include <gui/widget/icons.h>

#include <zapit/getservices.h>
#include <zapit/frontend_c.h>

#include <zapit/client/zapitclient.h>
extern CBouquetManager *g_bouquetManager;

CBEChannelWidget::CBEChannelWidget(const std::string & Caption, unsigned int Bouquet)
{
	int icol_w, icol_h;
	frameBuffer = CFrameBuffer::getInstance();
	selected = 0;
	iconoffset = 0;

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

		// clear details
		frameBuffer->paintBoxRel(x+2, y + height + 2, width-4, info_height - 4, COL_MENUCONTENTDARK_PLUS_0, RADIUS_LARGE);

		if(current < Channels->size()) {
			paintItem2DetailsLine (pos, current);
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

void CBEChannelWidget::paintDetails(int index)
{
	char buf[128] = {0};
	int len = 0;

	transponder_id_t ct = (*Channels)[index]->getTransponderId();
	transponder_list_t::iterator tpI = transponders.find(ct);
	sat_iterator_t sit = satellitePositions.find((*Channels)[index]->getSatellitePosition());

	len = snprintf(buf, sizeof(buf), "%d ", (*Channels)[index]->getFreqId());

	if(tpI != transponders.end()) {
		char * f, *s, *m;
		switch(CFrontend::getInstance()->getInfo()->type)
		{
			case FE_QPSK:
				CFrontend::getInstance()->getDelSys(tpI->second.feparams.u.qpsk.fec_inner, dvbs_get_modulation(tpI->second.feparams.u.qpsk.fec_inner),  f, s, m);
				len += snprintf(&buf[len], sizeof(buf) - len, "%c %d %s %s %s ", tpI->second.polarization ? 'V' : 'H', tpI->second.feparams.u.qpsk.symbol_rate/1000, f, s, m);
				break;
			case FE_QAM:
				CFrontend::getInstance()->getDelSys(tpI->second.feparams.u.qam.fec_inner, tpI->second.feparams.u.qam.modulation, f, s, m);
				len += snprintf(&buf[len], sizeof(buf) - len, "%d %s %s %s ", tpI->second.feparams.u.qam.symbol_rate/1000, f, s, m);
				break;
			case FE_OFDM:
			case FE_ATSC:
				break;
		}
	}

	if((*Channels)[index]->pname) {
		snprintf(&buf[len], sizeof(buf) - len, "(%s)", (*Channels)[index]->pname);
	}
	else {
		if(sit != satellitePositions.end()) {
			snprintf(&buf[len], sizeof(buf) - len, "(%s)", sit->second.name.c_str());
		}
	}

	if(sit != satellitePositions.end()) {
		g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x+ 10, y+ height+ 5+ fheight, width - 30,  sit->second.name.c_str(), COL_MENUCONTENTDARK, 0, true);
	}
	g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x+ 10, y+ height+ 5+ 2*fheight, width - 30, buf, COL_MENUCONTENTDARK, 0, true);
}

void CBEChannelWidget::paintItem2DetailsLine (int pos, int /*ch_index*/)
{
#define ConnectLineBox_Width	16

	int xpos  = x - ConnectLineBox_Width;
	int ypos1 = y + theight+0 + pos*fheight;
	int ypos2 = y + height;
	int ypos1a = ypos1 + (fheight/2)-2;
	int ypos2a = ypos2 + (info_height/2)-2;
	fb_pixel_t col1 = COL_MENUCONTENT_PLUS_6;
	fb_pixel_t col2 = COL_MENUCONTENT_PLUS_1;

	// Clear
	frameBuffer->paintBackgroundBoxRel(xpos,y, ConnectLineBox_Width, height+info_height);

	// paint Line if detail info (and not valid list pos)
	if (pos >= 0)
	{
		int fh = fheight > 10 ? fheight - 10: 5;
		frameBuffer->paintBoxRel(xpos+ConnectLineBox_Width-4, ypos1+5, 4, fh,     col1);
		frameBuffer->paintBoxRel(xpos+ConnectLineBox_Width-4, ypos1+5, 1, fh,     col2);

		frameBuffer->paintBoxRel(xpos+ConnectLineBox_Width-4, ypos2+7, 4,info_height-14, col1);
		frameBuffer->paintBoxRel(xpos+ConnectLineBox_Width-4, ypos2+7, 1,info_height-14, col2);

		frameBuffer->paintBoxRel(xpos+ConnectLineBox_Width-16, ypos1a, 4,ypos2a-ypos1a, col1);
		frameBuffer->paintBoxRel(xpos+ConnectLineBox_Width-16, ypos1a, 1,ypos2a-ypos1a+4, col2);

		frameBuffer->paintBoxRel(xpos+ConnectLineBox_Width-16, ypos1a, 12,4, col1);
		frameBuffer->paintBoxRel(xpos+ConnectLineBox_Width-16, ypos1a, 12,1, col2);

		frameBuffer->paintBoxRel(xpos+ConnectLineBox_Width-16, ypos2a, 12,4, col1);
		frameBuffer->paintBoxRel(xpos+ConnectLineBox_Width-12, ypos2a, 8,1, col2);

		frameBuffer->paintBoxFrame(x, ypos2, width, info_height, 2, col1, RADIUS_LARGE);
	}
}

void CBEChannelWidget::clearItem2DetailsLine ()
{
	paintItem2DetailsLine (-1, 0);
}

void CBEChannelWidget::hide()
{
	frameBuffer->paintBackgroundBoxRel(x,y, width,height+footerHeight+info_height);
	clearItem2DetailsLine ();
	frameBuffer->blit();
}

int CBEChannelWidget::exec(CMenuTarget* parent, const std::string & /*actionKey*/)
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	int res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	int fw = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getWidth();
	width  = w_max ((frameBuffer->getScreenWidth() / 20 * (fw+6)), 100);
	height = h_max ((frameBuffer->getScreenHeight() / 20 * 17), (frameBuffer->getScreenHeight() / 20 * 2));
	listmaxshow = (height-theight-footerHeight-0)/iheight;
	height = theight+footerHeight+listmaxshow*iheight; // recalc height
	info_height = 2*iheight + 10;

	x = frameBuffer->getScreenX() + (frameBuffer->getScreenWidth() - width) / 2;
	y = frameBuffer->getScreenY() + (frameBuffer->getScreenHeight() - (height + info_height)) / 2;

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
			if (!(Channels->empty()))
			{
				int step = 0;
				int prev_selected = selected;

				step = (msg==(neutrino_msg_t)g_settings.key_channelList_pageup) ? listmaxshow : 1;  // browse or step 1
				selected -= step;
#if 0
				if((prev_selected-step) < 0)		// because of uint
				{
					selected = Channels->size() - 1;
				}
#endif
				if((prev_selected-step) < 0) {
					if(prev_selected != 0 && step != 1)
						selected = 0;
					else
						selected = Channels->size() - 1;
				}
				if (state == beDefault)
				{
					paintItem(prev_selected - liststart);
					unsigned int oldliststart = liststart;
					liststart = (selected/listmaxshow)*listmaxshow;
					if(oldliststart!=liststart)
					{
						paint();
					}
					else
					{
						paintItem(selected - liststart);
					}
				}
				else if (state == beMoving)
				{
					internalMoveChannel(prev_selected, selected);
				}
			}
		}
		else if (msg==CRCInput::RC_down || msg==(neutrino_msg_t)g_settings.key_channelList_pagedown)
		{
			unsigned int step = 0;
			unsigned int prev_selected = selected;

			step = (msg==(neutrino_msg_t)g_settings.key_channelList_pagedown) ? listmaxshow : 1;  // browse or step 1
			selected += step;
#if 0
			if(selected >= Channels->size())
			{
				if (((Channels->size() / listmaxshow) + 1) * listmaxshow == Channels->size() + listmaxshow) // last page has full entries
					selected = 0;
				else
					selected = ((step == listmaxshow) && (selected < (((Channels->size() / listmaxshow) + 1) * listmaxshow))) ? (Channels->size() - 1) : 0;
			}
#endif
			if(selected >= Channels->size()) {
				if((Channels->size() - listmaxshow -1 < prev_selected) && (prev_selected != (Channels->size() - 1)) && (step != 1))
					selected = Channels->size() - 1;
				else if (((Channels->size() / listmaxshow) + 1) * listmaxshow == Channels->size() + listmaxshow) // last page has full entries
					selected = 0;
				else
					selected = ((step == listmaxshow) && (selected < (((Channels->size() / listmaxshow)+1) * listmaxshow))) ? (Channels->size() - 1) : 0;
			}
			if (state == beDefault)
			{
				paintItem(prev_selected - liststart);
				unsigned int oldliststart = liststart;
				liststart = (selected/listmaxshow)*listmaxshow;
				if(oldliststart!=liststart)
				{
					paint();
				}
				else
				{
					paintItem(selected - liststart);
				}
			}
			else if (state == beMoving)
			{
				internalMoveChannel(prev_selected, selected);
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
