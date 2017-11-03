/*
	neutrino bouquet editor - channel selection

	Copyright (C) 2001 Steffen Hehn 'McClean'
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

#include <unistd.h>

#include <global.h>
#include <neutrino.h>

#include <driver/fontrenderer.h>
#include <driver/screen_max.h>
#include <gui/widget/icons.h>
#include <zapit/getservices.h>
#include <zapit/zapit.h>

#include "bouqueteditor_chanselect.h"

extern CBouquetManager *g_bouquetManager;

CBEChannelSelectWidget::CBEChannelSelectWidget(const std::string & Caption, CZapitBouquet* Bouquet, CZapitClient::channelsMode Mode)
{
	caption = Caption;
	bouquet = Bouquet;
	mode =    Mode;
	selected = 0;
	liststart = 0;
	channellist_sort_mode = SORT_ALPHA;
	bouquetChannels = NULL;

	int iw, ih;
	action_icon_width = 0;
	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_DUMMY_SMALL, &action_icon_width, &ih);

	status_icon_width = 0;
	frameBuffer->getIconSize(NEUTRINO_ICON_SCRAMBLED, &iw, &ih);
	status_icon_width = std::max(status_icon_width, iw);
	frameBuffer->getIconSize(NEUTRINO_ICON_STREAMING, &iw, &ih);
	status_icon_width = std::max(status_icon_width, iw);
}

CBEChannelSelectWidget::~CBEChannelSelectWidget()
{
}

void CBEChannelSelectWidget::paintItem(int pos)
{
	int ypos = y + header_height + pos*item_height;
	unsigned int current = liststart + pos;

	bool i_selected	= current == selected;
	int i_radius = RADIUS_NONE;

	fb_pixel_t color;
	fb_pixel_t bgcolor;

	getItemColors(color, bgcolor, i_selected);

	if (i_selected)
	{
		if (current < Channels.size() || Channels.empty())
			paintDetails(pos, current);

		i_radius = RADIUS_LARGE;
	}
	else
	{
		if (current < Channels.size() && (Channels[current]->flags & CZapitChannel::NOT_PRESENT))
			color = COL_MENUCONTENTINACTIVE_TEXT;
	}

	if (i_radius)
		frameBuffer->paintBoxRel(x, ypos, width - SCROLLBAR_WIDTH, item_height, COL_MENUCONTENT_PLUS_0);
	frameBuffer->paintBoxRel(x, ypos, width - SCROLLBAR_WIDTH, item_height, bgcolor, i_radius);

	if (current < Channels.size())
	{
		if (isChannelInBouquet(current))
			frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_GREEN, x + OFFSET_INNER_MID, ypos, item_height);
		else
			frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_DUMMY_SMALL, x + OFFSET_INNER_MID, ypos, item_height);

		int text_offset = 2*OFFSET_INNER_MID + action_icon_width;
		item_font->RenderString(x + text_offset, ypos + item_height, width - text_offset - SCROLLBAR_WIDTH - 2*OFFSET_INNER_MID - status_icon_width, Channels[current]->getName(), color);

		if (Channels[current]->scrambled)
			frameBuffer->paintIcon(NEUTRINO_ICON_SCRAMBLED, x + width - SCROLLBAR_WIDTH - OFFSET_INNER_MID - status_icon_width, ypos, item_height);
		else if (!Channels[current]->getUrl().empty())
			frameBuffer->paintIcon(NEUTRINO_ICON_STREAMING, x + width - SCROLLBAR_WIDTH - OFFSET_INNER_MID - status_icon_width, ypos, item_height);
	}
}

void CBEChannelSelectWidget::paintItems()
{
	liststart = (selected/items_count)*items_count;

	for(unsigned int count = 0; count < items_count; count++)
		paintItem(count);

	int total_pages;
	int current_page;
	getScrollBarData(&total_pages, &current_page, Channels.size(), items_count, selected);
	paintScrollBar(x + width - SCROLLBAR_WIDTH, y + header_height, SCROLLBAR_WIDTH, body_height, total_pages, current_page);
}

void CBEChannelSelectWidget::paintHead()
{
	CBEGlobals::paintHead(caption + (mode == CZapitClient::MODE_TV ? " - TV" : " - Radio"),
				mode == CZapitClient::MODE_TV ? NEUTRINO_ICON_VIDEO : NEUTRINO_ICON_AUDIO);
}

struct button_label CBEChannelSelectButtons[] =
{
	{ NEUTRINO_ICON_BUTTON_RED,	LOCALE_CHANNELLIST_FOOT_SORT_ALPHA	},
	{ NEUTRINO_ICON_BUTTON_OKAY,	LOCALE_BOUQUETEDITOR_SWITCH		},
	{ NEUTRINO_ICON_BUTTON_HOME,	LOCALE_BOUQUETEDITOR_RETURN		}
};

void CBEChannelSelectWidget::paintFoot()
{
	switch (channellist_sort_mode)
	{
		case SORT_FREQ:
		{
			CBEChannelSelectButtons[0].locale = LOCALE_CHANNELLIST_FOOT_SORT_FREQ;
			break;
		}
		case SORT_SAT:
		{
			CBEChannelSelectButtons[0].locale = LOCALE_CHANNELLIST_FOOT_SORT_SAT;
			break;
		}
		case SORT_CH_NUMBER:
		{
			CBEChannelSelectButtons[0].locale = LOCALE_CHANNELLIST_FOOT_SORT_CHNUM;
			break;
		}
		case SORT_ALPHA:
		default:
		{
			CBEChannelSelectButtons[0].locale = LOCALE_CHANNELLIST_FOOT_SORT_ALPHA;
			break;
		}
	}

	const short numbuttons = sizeof(CBEChannelSelectButtons)/sizeof(CBEChannelSelectButtons[0]);

	CBEGlobals::paintFoot(numbuttons, CBEChannelSelectButtons);
}

std::string CBEChannelSelectWidget::getInfoText(int index)
{
	std::string res = "";

	if (Channels.empty())
		return res;

	std::string satname = CServiceManager::getInstance()->GetSatelliteName(Channels[index]->getSatellitePosition());
	if (IS_WEBCHAN(Channels[index]->getChannelID()))
		satname = "Web-Channel"; // TODO split into WebTV/WebRadio
	transponder t;
	CServiceManager::getInstance()->GetTransponder(Channels[index]->getTransponderId(), t);
	std::string desc = t.description();
	if (Channels[index]->pname)
	{
		if (desc.empty())
			desc = std::string(Channels[index]->pname);
		else
			desc += " (" + std::string(Channels[index]->pname) + ")";
	}
	if (!Channels[index]->getDesc().empty())
		desc += "\n" + Channels[index]->getDesc();

	res = satname + " - " + desc;

	return res;
}

void CBEChannelSelectWidget::updateSelection(unsigned int newpos)
{
	if (newpos == selected || newpos == (unsigned int)-1)
		return;

	unsigned int prev_selected = selected;
	selected = newpos;

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

int CBEChannelSelectWidget::exec(CMenuTarget* parent, const std::string & /*actionKey*/)
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	int res = menu_return::RETURN_REPAINT;
	selected = 0;

	if (parent)
		parent->hide();

	bouquetChannels = mode == CZapitClient::MODE_TV ? &(bouquet->tvChannels) : &(bouquet->radioChannels);

	Channels.clear();

	if (mode == CZapitClient::MODE_RADIO)
		CServiceManager::getInstance()->GetAllRadioChannels(Channels);
	else
		CServiceManager::getInstance()->GetAllTvChannels(Channels);

	sort(Channels.begin(), Channels.end(), CmpChannelByChName());

	paintHead();
	paintBody();
	paintFoot();
	paintItems();

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(*timeout_ptr);

	channelChanged = false;
	bool loop = true;
	while (loop)
	{
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);

		if (msg <= CRCInput::RC_MaxRC)
			timeoutEnd = CRCInput::calcTimeoutEnd(*timeout_ptr);

		if ((msg == CRCInput::RC_timeout) || (msg == (neutrino_msg_t)g_settings.key_channelList_cancel) || (msg == CRCInput::RC_home))
		{
			loop = false;
		}
		else if (msg == CRCInput::RC_up || msg == (neutrino_msg_t)g_settings.key_pageup ||
			 msg == CRCInput::RC_down || msg == (neutrino_msg_t)g_settings.key_pagedown)
		{
			int new_selected = UpDownKey(Channels, msg, items_count, selected);
			updateSelection(new_selected);
		}
		else if (msg == (neutrino_msg_t) g_settings.key_list_start || msg == (neutrino_msg_t) g_settings.key_list_end)
		{
			if (!Channels.empty())
			{
				int new_selected = msg == (neutrino_msg_t) g_settings.key_list_start ? 0 : Channels.size() - 1;
				updateSelection(new_selected);
			}
		}
		else if (msg == CRCInput::RC_ok || msg == CRCInput::RC_green)
		{
			if (selected < Channels.size())
				selectChannel();
		}
		else if (msg == CRCInput::RC_red)
		{
			if (selected < Channels.size())
				sortChannels();
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

	hide();
	return res;
}

void CBEChannelSelectWidget::sortChannels()
{
	channellist_sort_mode++;
	if (channellist_sort_mode >= SORT_END)
		channellist_sort_mode = SORT_ALPHA;
	switch (channellist_sort_mode)
	{
		case SORT_FREQ:
		{
			sort(Channels.begin(), Channels.end(), CmpChannelByFreq());
			break;
		}
		case SORT_SAT:
		{
			sort(Channels.begin(), Channels.end(), CmpChannelBySat());
			break;
		}
		case SORT_CH_NUMBER:
		{
			sort(Channels.begin(), Channels.end(), CmpChannelByChNum());
			break;
		}
		case SORT_ALPHA:
		default:
		{
			sort(Channels.begin(), Channels.end(), CmpChannelByChName());
			break;
		}
	}
	paintFoot();
	paintItems();
}

void CBEChannelSelectWidget::selectChannel()
{
	channelChanged = true;

	if (isChannelInBouquet(selected))
		bouquet->removeService(Channels[selected]);
	else
		bouquet->addService(Channels[selected]);

	bouquetChannels = mode == CZapitClient::MODE_TV ? &(bouquet->tvChannels) : &(bouquet->radioChannels);

	paintItem(selected - liststart);
	g_RCInput->postMsg(CRCInput::RC_down, 0);
}

bool CBEChannelSelectWidget::isChannelInBouquet(int index)
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
	return channelChanged;
}
