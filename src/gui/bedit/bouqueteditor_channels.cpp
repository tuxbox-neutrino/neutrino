/*
	neutrino bouquet editor - channels editor

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Copyright (C) 2011 CoolStream International Ltd
	Copyright (C) 2009,2011,2013,2016 Stefan Seyfried
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

#include <driver/fontrenderer.h>
#include <driver/screen_max.h>
#include <gui/components/cc.h>
#include <gui/widget/icons.h>
#include <gui/widget/keyboard_input.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/stringinput.h>
#include <zapit/femanager.h>
#include <zapit/getservices.h>

#include "bouqueteditor_channels.h"
#include "bouqueteditor_chanselect.h"

extern CBouquetManager *g_bouquetManager;

CBEChannelWidget::CBEChannelWidget(const std::string & Caption, unsigned int Bouquet)
{
	selected = 0;
	origPosition = 0;
	newPosition = 0;
	channelsChanged = false;
	liststart = 0;
	state = beDefault;
	caption = Caption;
	bouquet = Bouquet;
	mode = CZapitClient::MODE_TV;

	Channels = NULL;

	int iw, ih;
	action_icon_width = 0;
	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_DUMMY_SMALL, &action_icon_width, &ih);

	status_icon_width = 0;
	frameBuffer->getIconSize(NEUTRINO_ICON_MARKER_SCRAMBLED, &iw, &ih);
	status_icon_width = std::max(status_icon_width, iw);
	frameBuffer->getIconSize(NEUTRINO_ICON_MARKER_STREAMING, &iw, &ih);
	status_icon_width = std::max(status_icon_width, iw);
	frameBuffer->getIconSize(NEUTRINO_ICON_MARKER_LOCK, &iw, &ih);
	status_icon_width = std::max(status_icon_width, iw);

}

CBEChannelWidget::~CBEChannelWidget()
{
}

void CBEChannelWidget::paintItem(int pos)
{
	int ypos = y + header_height + pos*item_height;
	unsigned int current = liststart + pos;

	bool i_selected	= current == selected;
	int i_radius	= RADIUS_NONE;

	fb_pixel_t color;
	fb_pixel_t bgcolor;

	getItemColors(color, bgcolor, i_selected);

	if (i_selected)
	{
		if (current < Channels->size() || Channels->empty())
			paintDetails(pos, current);

		i_radius = RADIUS_LARGE;
	}
	else
	{
		if (current < Channels->size() && ((*Channels)[current]->flags & CZapitChannel::NOT_PRESENT))
			color = COL_MENUCONTENTINACTIVE_TEXT;
	}

	if (i_radius)
		frameBuffer->paintBoxRel(x, ypos, width - SCROLLBAR_WIDTH, item_height, COL_MENUCONTENT_PLUS_0);
	frameBuffer->paintBoxRel(x, ypos, width - SCROLLBAR_WIDTH, item_height, bgcolor, i_radius);

	if (current < Channels->size())
	{
		if ((i_selected) && (state == beMoving))
			frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_YELLOW, x + OFFSET_INNER_MID, ypos, item_height);
		else
			frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_DUMMY_SMALL, x + OFFSET_INNER_MID, ypos, item_height);

		int text_offset = 2*OFFSET_INNER_MID + action_icon_width;
		item_font->RenderString(x + text_offset, ypos + item_height, width - text_offset - SCROLLBAR_WIDTH - 3*OFFSET_INNER_MID - 2*status_icon_width, (*Channels)[current]->getName(), color);

		if ((*Channels)[current]->scrambled)
			frameBuffer->paintIcon(NEUTRINO_ICON_MARKER_SCRAMBLED, x + width - SCROLLBAR_WIDTH - OFFSET_INNER_MID - status_icon_width, ypos, item_height);
		else if (!(*Channels)[current]->getUrl().empty())
			frameBuffer->paintIcon(NEUTRINO_ICON_MARKER_STREAMING, x + width - SCROLLBAR_WIDTH - OFFSET_INNER_MID - status_icon_width, ypos, item_height);

		if ((*Channels)[current]->bLocked)
			frameBuffer->paintIcon(NEUTRINO_ICON_MARKER_LOCK, x + width - SCROLLBAR_WIDTH - 2*OFFSET_INNER_MID - 2*status_icon_width, ypos, item_height);
	}
}

void CBEChannelWidget::paintItems()
{
	liststart = (selected/items_count)*items_count;

	for(unsigned int count = 0; count < items_count; count++)
		paintItem(count);

	int total_pages;
	int current_page;
	getScrollBarData(&total_pages, &current_page, Channels->size(), items_count, selected);
	paintScrollBar(x + width - SCROLLBAR_WIDTH, y + header_height, SCROLLBAR_WIDTH, body_height, total_pages, current_page);
}

void CBEChannelWidget::paintHead()
{
	if (!header->isPainted())
		header->addContextButton(CComponentsHeader::CC_BTN_LEFT | CComponentsHeader::CC_BTN_RIGHT);
	CBEGlobals::paintHead(caption + (mode == CZapitClient::MODE_TV ? " - TV" : " - Radio"),
					mode == CZapitClient::MODE_TV ? NEUTRINO_ICON_VIDEO : NEUTRINO_ICON_AUDIO);
}

const struct button_label CBEChannelWidgetButtons[] =
{
	{ NEUTRINO_ICON_BUTTON_RED,	LOCALE_BOUQUETEDITOR_DELETE	},
	{ NEUTRINO_ICON_BUTTON_GREEN,	LOCALE_BOUQUETEDITOR_ADD	},
	{ NEUTRINO_ICON_BUTTON_YELLOW,	LOCALE_BOUQUETEDITOR_MOVE	},
	{ NEUTRINO_ICON_BUTTON_BLUE,	LOCALE_BOUQUETEDITOR_RENAME	},
	{ NEUTRINO_ICON_BUTTON_DUMMY_SMALL, LOCALE_BOUQUETEDITOR_SWITCHMODE },
//	{ NEUTRINO_ICON_BUTTON_FORWARD,	LOCALE_BOUQUETEDITOR_MOVE_TO	}, // TODO upgrade
	{ NEUTRINO_ICON_BUTTON_STOP,	LOCALE_BOUQUETEDITOR_LOCK	}
};

void CBEChannelWidget::paintFoot()
{
	size_t numbuttons = sizeof(CBEChannelWidgetButtons)/sizeof(CBEChannelWidgetButtons[0]);

	CBEGlobals::paintFoot(numbuttons, CBEChannelWidgetButtons);
}

std::string CBEChannelWidget::getInfoText(int index)
{
	std::string res = "";

	if (Channels->empty())
		return res;

	std::string satname = CServiceManager::getInstance()->GetSatelliteName((*Channels)[index]->getSatellitePosition());
	if (IS_WEBCHAN((*Channels)[index]->getChannelID()))
		satname = "Web-Channel"; // TODO split into WebTV/WebRadio
	transponder t;
	CServiceManager::getInstance()->GetTransponder((*Channels)[index]->getTransponderId(), t);
	std::string desc = t.description();
	if ((*Channels)[index]->pname)
	{
		if (desc.empty())
			desc = std::string((*Channels)[index]->pname);
		else
			desc += " (" + std::string((*Channels)[index]->pname) + ")";
	}
	if (!(*Channels)[index]->getDesc().empty())
		desc += "\n" + (*Channels)[index]->getDesc();

	res = satname + " - " + desc;

	return res;
}

void CBEChannelWidget::updateSelection(unsigned int newpos)
{
	if (newpos == selected || newpos == (unsigned int)-1)
		return;

	unsigned int prev_selected = selected;
	selected = newpos;

	if (state == beDefault)
	{
		unsigned int oldliststart = liststart;
		liststart = (selected/items_count)*items_count;
		if (oldliststart != liststart)
		{
			paintItems();
		}
		else
		{
			paintItem(prev_selected - liststart);
			paintItem(selected - liststart);
		}
	}
	else
	{
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

	mode = CZapitClient::MODE_TV;
	if (g_bouquetManager->Bouquets[bouquet]->tvChannels.empty())
		mode = CZapitClient::MODE_RADIO;
	else if (g_bouquetManager->Bouquets[bouquet]->radioChannels.empty())
		mode = CZapitClient::MODE_TV;

	Channels = mode == CZapitClient::MODE_TV ? &(g_bouquetManager->Bouquets[bouquet]->tvChannels) : &(g_bouquetManager->Bouquets[bouquet]->radioChannels);

	paintHead();
	paintBody();
	paintFoot();
	paintItems();

	channelsChanged = false;

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(*timeout_ptr);

	bool loop = true;
	while (loop)
	{
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);

		if (msg <= CRCInput::RC_MaxRC)
			timeoutEnd = CRCInput::calcTimeoutEnd(*timeout_ptr);

		if ((msg == CRCInput::RC_timeout) || (msg == (neutrino_msg_t)g_settings.key_channelList_cancel))
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
			int new_selected = UpDownKey(*Channels, msg, items_count, selected);
			updateSelection(new_selected);
		}
		else if (msg == (neutrino_msg_t) g_settings.key_list_start || msg == (neutrino_msg_t) g_settings.key_list_end)
		{
			if (!(Channels->empty()))
			{
				int new_selected = msg == (neutrino_msg_t) g_settings.key_list_start ? 0 : Channels->size() - 1;
				updateSelection(new_selected);
			}
		}
		else if (msg == CRCInput::RC_red)
		{
			if (state == beDefault)
				deleteChannel();
		}
		else if (msg == CRCInput::RC_green)
		{
			if (state == beDefault)
				addChannel();

			timeoutEnd = CRCInput::calcTimeoutEnd(*timeout_ptr);
		}
		else if (msg == CRCInput::RC_yellow)
		{
			liststart = (selected/items_count)*items_count;
			if (state == beDefault)
				beginMoveChannel();
			else if (state == beMoving)
				finishMoveChannel();
			paintItem(selected - liststart);
		}
		else if (msg == CRCInput::RC_blue)
		{
			if (selected < Channels->size()) /* Channels->size() might be 0 */
			{
				if (state == beDefault)
					renameChannel();
			}
		}
		else if (msg == CRCInput::RC_stop)
		{
			if (selected < Channels->size()) /* Channels->size() might be 0 */
			{
				if (state == beDefault)
					switchLockChannel();
			}
		}
/* TODO upgrade
		else if (msg == CRCInput::RC_forward)
		{
			if (selected < Channels->size())
			{
				if (state == beDefault)
					moveChannelToBouquet();
			}
		}
*/
		else if (msg == (neutrino_msg_t) g_settings.key_tvradio_mode || msg == CRCInput::RC_tv || msg == CRCInput::RC_radio)
		{
			if (msg == CRCInput::RC_radio)
				mode = CZapitClient::MODE_RADIO;
			else if (msg == CRCInput::RC_tv)
				mode = CZapitClient::MODE_TV;
			else // g_settings.key_tvradio_mode
			{
				if (mode == CZapitClient::MODE_TV)
					mode = CZapitClient::MODE_RADIO;
				else
					mode = CZapitClient::MODE_TV;
			}

			Channels = mode == CZapitClient::MODE_TV ? &(g_bouquetManager->Bouquets[bouquet]->tvChannels) : &(g_bouquetManager->Bouquets[bouquet]->radioChannels);

			selected = 0;
			paintHead();
			paintBody();
			paintFoot();
			paintItems();
		}
		else if (msg == CRCInput::RC_left || msg == CRCInput::RC_right)
		{
			unsigned int bouquet_size = g_bouquetManager->Bouquets.size();

			if (msg == CRCInput::RC_left)
			{
				if (bouquet == 0)
					bouquet = bouquet_size - 1;
				else
					bouquet--;
			}
			else
			{
				if (bouquet < bouquet_size - 1)
					bouquet++;
				else
					bouquet = 0;
			}

			Channels = mode == CZapitClient::MODE_TV ? &(g_bouquetManager->Bouquets[bouquet]->tvChannels) : &(g_bouquetManager->Bouquets[bouquet]->radioChannels);
			caption = g_bouquetManager->Bouquets[bouquet]->bName;

			selected = 0;
			paintHead();
			paintBody();
			paintFoot();
			paintItems();
		}
		else if (msg == CRCInput::RC_ok)
		{
			if (state == beDefault)
			{
				if (selected < Channels->size()) /* Channels.size() might be 0 */
					g_Zapit->zapTo_serviceID((*Channels)[selected]->getChannelID());

			}
			else if (state == beMoving)
			{
				finishMoveChannel();
			}
		}
		else if (CRCInput::isNumeric(msg))
		{
			if (state == beDefault)
			{
				//kein pushback - wenn man versehentlich wo draufkommt is die edit-arbeit umsonst
				//selected = oldselected;
				//g_RCInput->postMsg(msg, data);
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
			CNeutrinoApp::getInstance()->handleMsg(msg, data);
		}
	}
	CBEGlobals::hide();
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
	paintItems();
}

void CBEChannelWidget::renameChannel()
{
	std::string newName= inputName((*Channels)[selected]->getName().c_str(), LOCALE_BOUQUETEDITOR_NEWBOUQUETNAME);

	if (newName != (*Channels)[selected]->getName())
	{
		if (newName.empty())
			(*Channels)[selected]->setUserName("");
		else
			(*Channels)[selected]->setUserName(newName);

		channelsChanged = true;
	}
	paintHead();
	paintBody();
	paintFoot();
	paintItems();
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
	paintBody();
	paintFoot();
	paintItems();
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
	paintItems();
}

void CBEChannelWidget::cancelMoveChannel()
{
	state = beDefault;
	internalMoveChannel(newPosition, origPosition);
	channelsChanged = channelsChanged | false;
}
/* TODO upgrade (taken from channellist.cpp)
void CBEChannelWidget::moveChannelToBouquet()
{
	if (addChannelToBouquet())
		deleteChannel(false);
	else
		paintItems();

	paintHead();
}
*/
void CBEChannelWidget::internalMoveChannel(unsigned int fromPosition, unsigned int toPosition)
{
	if ((int) toPosition == -1)
		return;
	if (toPosition == Channels->size())
		return;

	g_bouquetManager->Bouquets[bouquet]->moveService(fromPosition, toPosition, mode == CZapitClient::MODE_TV ? 1 : 2);

	//channelsChanged = true;
	Channels = mode == CZapitClient::MODE_TV ? &(g_bouquetManager->Bouquets[bouquet]->tvChannels) : &(g_bouquetManager->Bouquets[bouquet]->radioChannels);

	selected = toPosition;
	newPosition = toPosition;
	paintItems();
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
	return channelsChanged;
}
