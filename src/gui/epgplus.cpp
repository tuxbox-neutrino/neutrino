/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Copyright (C) 2004 Martin Griep 'vivamiga'
	Copyright (C) 2009-2014 Stefan Seyfried
	Copyright (C) 2017 Sven Hoefer

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

#include <iostream>

#include <global.h>
#include <neutrino.h>

#include <gui/infoviewer.h>
#include <gui/followscreenings.h>
#include <gui/epgplus.h>
#include <gui/epgview.h>
#include <gui/moviebrowser/mb.h>
#include <sectionsdclient/sectionsdclient.h>
#include <timerdclient/timerdclient.h>

#include <gui/widget/icons.h>
#include <gui/widget/buttons.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/stringinput.h>
#include "bouquetlist.h"

#include <zapit/client/zapittools.h>
#include <driver/rcinput.h>
#include <driver/screen_max.h>
#include <driver/fade.h>
#include <driver/fontrenderer.h>
#include <zapit/satconfig.h>
#include <zapit/getservices.h>
#include <eitd/sectionsd.h>

#include <algorithm>
#include <sstream>

extern CBouquetList *bouquetList;

int sizes[EpgPlus::NumberOfSizeSettings];

time_t EpgPlus::duration = 0;

int EpgPlus::sliderWidth = 0;
int EpgPlus::channelsTableWidth = 0;

/* negative size means "screen width in percent" */
static EpgPlus::SizeSetting sizeSettingTable[] =
{
	{ EpgPlus::EPGPlus_channelentry_width, -15 }, /* 15 percent of screen width */
	{ EpgPlus::EPGPlus_separationline_thickness, 1 }
};

static bool bigfont = false;
static int current_bouquet;

Font *EpgPlus::Header::font = NULL;

EpgPlus::Header::Header(CFrameBuffer * pframeBuffer, int px, int py, int pwidth)
{
	this->frameBuffer = pframeBuffer;
	this->x = px;
	this->y = py;
	this->width = pwidth;
}

EpgPlus::Header::~Header()
{
}

void EpgPlus::Header::init()
{
	font = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE];
}

void EpgPlus::Header::paint(const char * Name)
{
	std::string head = Name ? Name : g_Locale->getText(LOCALE_EPGPLUS_HEAD);

	CComponentsHeader _header(this->x, this->y, this->width, this->font->getHeight(), head);
	_header.paint(CC_SAVE_SCREEN_NO);
}

int EpgPlus::Header::getUsedHeight()
{
	return font->getHeight();
}

Font *EpgPlus::TimeLine::font = NULL;
int EpgPlus::TimeLine::separationLineThickness = 0;

EpgPlus::TimeLine::TimeLine(CFrameBuffer * pframeBuffer, int px, int py, int pwidth, int pstartX, int pdurationX)
{
	this->frameBuffer = pframeBuffer;
	this->x = px;
	this->y = py;
	this->width = pwidth;
	this->startX = pstartX;
	this->durationX = pdurationX;
}

void EpgPlus::TimeLine::init()
{
	font = g_Font[SNeutrinoSettings::FONT_TYPE_EPG_DATE];
	separationLineThickness = sizes[EPGPlus_separationline_thickness];
}

EpgPlus::TimeLine::~TimeLine()
{
}

void EpgPlus::TimeLine::paint(time_t _startTime, int pduration)
{
	this->clearMark();

	int xPos = this->startX;

	this->currentDuration = pduration;
	int numberOfTicks = this->currentDuration / (60 * 60) * 2;
	int tickDist = (this->durationX) / numberOfTicks;
	time_t tickTime = _startTime;
	bool toggleColor = false;

	// display date of begin
	this->frameBuffer->paintBoxRel(this->x, this->y, this->width, this->font->getHeight(),
					COL_MENUCONTENT_PLUS_0);

	this->font->RenderString(this->x + OFFSET_INNER_MID, this->y + this->font->getHeight(),
					this->width, EpgPlus::getTimeString(_startTime, "%d-%b"), COL_MENUCONTENT_TEXT);

	// paint ticks
	for (int i = 0; i < numberOfTicks; ++i, xPos += tickDist, tickTime += pduration / numberOfTicks)
	{
		int xWidth = tickDist;
		if (xPos + xWidth > this->x + width)
			xWidth = this->x + width - xPos;

		this->frameBuffer->paintBoxRel(xPos, this->y, xWidth, this->font->getHeight(),
						toggleColor ? COL_MENUCONTENT_PLUS_0 : COL_MENUCONTENT_PLUS_1);

		std::string timeStr = EpgPlus::getTimeString(tickTime, "%H");

		int textWidth = this->font->getRenderWidth(timeStr);

		this->font->RenderString(xPos - textWidth - OFFSET_INNER_MIN, this->y + this->font->getHeight(),
						textWidth, timeStr, COL_MENUCONTENT_TEXT);

		timeStr = EpgPlus::getTimeString(tickTime, "%M");
		textWidth = this->font->getRenderWidth(timeStr);
		this->font->RenderString(xPos + OFFSET_INNER_MIN, this->y + this->font->getHeight(),
						textWidth, timeStr, COL_MENUCONTENT_TEXT);

		toggleColor = !toggleColor;
	}
}

void EpgPlus::TimeLine::paintGrid()
{
	int xPos = this->startX;
	int numberOfTicks = this->currentDuration / (60 * 60) * 2;
	int tickDist = (this->durationX) / numberOfTicks;
	// paint ticks
	for (int i = 0; i < numberOfTicks; ++i, xPos += tickDist)
	{
		// display a line for the tick
		this->frameBuffer->paintVLineRel(xPos, this->y, this->font->getHeight(), COL_MENUCONTENTDARK_PLUS_0);
	}
}

void EpgPlus::TimeLine::paintMark(time_t _startTime, int pduration, int px, int pwidth)
{
	// clear old mark
	this->clearMark();

	// paint new mark
	this->frameBuffer->paintBoxRel(px, this->y + this->font->getHeight(),
					pwidth, this->font->getHeight() , COL_MENUCONTENTSELECTED_PLUS_0);

	// display start time before mark
	std::string timeStr = EpgPlus::getTimeString(_startTime, "%H:%M");
	int textWidth = this->font->getRenderWidth(timeStr);

	this->font->RenderString(px - textWidth - OFFSET_INNER_MIN, this->y + this->font->getHeight() + this->font->getHeight(),
					textWidth, timeStr, COL_MENUCONTENT_TEXT);

	// display end time after mark
	timeStr = EpgPlus::getTimeString(_startTime + pduration, "%H:%M");
	textWidth = font->getRenderWidth(timeStr);

	if (px + pwidth + textWidth < this->x + this->width)
	{
		this->font->RenderString(px + pwidth + OFFSET_INNER_MIN, this->y + this->font->getHeight() + this->font->getHeight(),
						textWidth, timeStr, COL_MENUCONTENT_TEXT);
	}
	else if (textWidth < pwidth - OFFSET_INNER_MID)
	{
		this->font->RenderString(px + pwidth - textWidth - OFFSET_INNER_MIN, this->y + this->font->getHeight() + this->font->getHeight(),
						textWidth, timeStr, COL_MENUCONTENTSELECTED_TEXT);
	}

	// paint the separation line
	if (separationLineThickness > 0)
	{
		this->frameBuffer->paintBoxRel(this->x, this->y + this->font->getHeight() + this->font->getHeight(),
					this->width, this->separationLineThickness, COL_MENUCONTENTDARK_PLUS_0);
	}
}

void EpgPlus::TimeLine::clearMark()
{
	this->frameBuffer->paintBoxRel(this->x, this->y + this->font->getHeight(),
					this->width, this->font->getHeight() , COL_MENUCONTENT_PLUS_0);
}

int EpgPlus::TimeLine::getUsedHeight()
{
	return 2*font->getHeight() + separationLineThickness;
}

Font *EpgPlus::ChannelEventEntry::font = NULL;
int EpgPlus::ChannelEventEntry::separationLineThickness = 0;

EpgPlus::ChannelEventEntry::ChannelEventEntry(const CChannelEvent * pchannelEvent, CFrameBuffer * pframeBuffer, TimeLine * ptimeLine, Footer * pfooter, int px, int py, int pwidth)
{
	// copy neccessary?
	if (pchannelEvent != NULL)
		this->channelEvent = *pchannelEvent;

	this->frameBuffer = pframeBuffer;
	this->timeLine = ptimeLine;
	this->footer = pfooter;
	this->x = px;
	this->y = py;
	this->width = pwidth;
}

void EpgPlus::ChannelEventEntry::init()
{
	//TODO: re-implement bigfont handling
	font = g_Font[SNeutrinoSettings::FONT_TYPE_EPGPLUS_ITEM];
	separationLineThickness = sizes[EPGPlus_separationline_thickness];
}

EpgPlus::ChannelEventEntry::~ChannelEventEntry()
{
}

bool EpgPlus::ChannelEventEntry::isSelected(time_t _selectedTime) const
{
	return (_selectedTime >= this->channelEvent.startTime) && (_selectedTime < this->channelEvent.startTime + time_t (this->channelEvent.duration));
}

void EpgPlus::ChannelEventEntry::paint(bool pisSelected, bool toggleColor)
{
	this->frameBuffer->paintBoxRel(this->x, this->y, this->width, this->font->getHeight(),
					this->channelEvent.description.empty()? COL_MENUCONTENT_PLUS_0 : (pisSelected ? COL_MENUCONTENTSELECTED_PLUS_0 : (toggleColor ? COL_MENUCONTENT_PLUS_0 : COL_MENUCONTENT_PLUS_1)));

	this->font->RenderString(this->x + OFFSET_INNER_SMALL, this->y + this->font->getHeight(),
					this->width - 2*OFFSET_INNER_SMALL > 0 ? this->width - 2*OFFSET_INNER_SMALL : 0, this->channelEvent.description, pisSelected ? COL_MENUCONTENTSELECTED_TEXT : COL_MENUCONTENT_TEXT);

	// paint the separation lines
	if (separationLineThickness > 0)
	{
		// left side
		this->frameBuffer->paintBoxRel(this->x, this->y,
					this->separationLineThickness, this->font->getHeight(), COL_MENUCONTENTDARK_PLUS_0);

		// bottom
		this->frameBuffer->paintBoxRel(this->x, this->y + this->font->getHeight(),
					this->width, this->separationLineThickness, COL_MENUCONTENTDARK_PLUS_0);
	}

	if (pisSelected) {
		if (this->channelEvent.description.empty())
		{	// dummy channel event
			this->timeLine->clearMark();
		}
		else
		{
			this->timeLine->paintMark(this->channelEvent.startTime, this->channelEvent.duration, this->x, this->width);
		}

		CShortEPGData shortEpgData;

		bool ret = CEitManager::getInstance()->getEPGidShort(this->channelEvent.eventID, &shortEpgData);
		this->footer->paintEventDetails(this->channelEvent.description, ret ? shortEpgData.info1 : "");

		this->timeLine->paintGrid();
	}
}

int EpgPlus::ChannelEventEntry::getUsedHeight()
{
	return font->getHeight() + separationLineThickness;
}

Font *EpgPlus::ChannelEntry::font = NULL;
int EpgPlus::ChannelEntry::separationLineThickness = 0;

EpgPlus::ChannelEntry::ChannelEntry(const CZapitChannel * pchannel, int pindex, CFrameBuffer * pframeBuffer, Footer * pfooter, CBouquetList * pbouquetList, int px, int py, int pwidth)
{
	this->channel = pchannel;

	if (pchannel != NULL)
	{
		std::stringstream pdisplayName;
		//pdisplayName << pindex + 1 << " " << pchannel->getName();
		pdisplayName << pchannel->number << " " << pchannel->getName();

		this->displayName = pdisplayName.str();
	}

	this->index = pindex;

	this->frameBuffer = pframeBuffer;
	this->footer = pfooter;
	this->bouquetList = pbouquetList;

	this->x = px;
	this->y = py;
	this->width = pwidth;

	this->detailsLine = NULL;
}

void EpgPlus::ChannelEntry::init()
{
	//TODO: re-implement bigfont handling
	font = g_Font[SNeutrinoSettings::FONT_TYPE_EPGPLUS_ITEM];
	separationLineThickness = sizes[EPGPlus_separationline_thickness];
}

EpgPlus::ChannelEntry::~ChannelEntry()
{
	for (TCChannelEventEntries::iterator It = this->channelEventEntries.begin();
			It != this->channelEventEntries.end();
			++It)
	{
		delete *It;
	}
	this->channelEventEntries.clear();

	if (this->detailsLine)
	{
		delete this->detailsLine;
		this->detailsLine = NULL;
	}
}

void EpgPlus::ChannelEntry::paint(bool isSelected, time_t _selectedTime)
{
	this->frameBuffer->paintBoxRel(this->x, this->y, this->width, this->font->getHeight(),
					isSelected ? COL_MENUCONTENTSELECTED_PLUS_0 : COL_MENUCONTENT_PLUS_0);

	this->font->RenderString(this->x + OFFSET_INNER_MID, this->y + this->font->getHeight(),
					this->width - 2*OFFSET_INNER_MID, this->displayName, isSelected ? COL_MENUCONTENTSELECTED_TEXT : COL_MENUCONTENT_TEXT);

	if (isSelected)
	{
#if 0
		for (uint32_t i = 0; i < this->bouquetList->Bouquets.size(); ++i)
		{
			CBouquet *bouquet = this->bouquetList->Bouquets[i];
			for (int j = 0; j < bouquet->channelList->getSize(); ++j)
			{

				if ((*bouquet->channelList)[j]->number == this->channel->number)
				{
					this->footer->setBouquetChannelName(bouquet->channelList->getName(), this->channel->getName());

					bouquet = NULL;
					break;
				}
			}
			if (bouquet == NULL)
				break;
		}
#endif
		if (this->channel->pname)
		{
			this->footer->setBouquetChannelName(this->channel->pname, this->channel->getName());
		}
		else
		{
			this->footer->setBouquetChannelName(CServiceManager::getInstance()->GetSatelliteName(this->channel->getSatellitePosition()), this->channel->getName());
		}
	}
	// paint the separation line
	if (separationLineThickness > 0)
	{
		this->frameBuffer->paintBoxRel(this->x, this->y + this->font->getHeight(),
						this->width, this->separationLineThickness, COL_MENUCONTENTDARK_PLUS_0);
	}

	bool toggleColor = false;
	for (TCChannelEventEntries::iterator It = this->channelEventEntries.begin();
			It != this->channelEventEntries.end();
			++It)
	{
		(*It)->paint(isSelected && (*It)->isSelected(_selectedTime), toggleColor);

		toggleColor = !toggleColor;
	}

	// kill detailsline
	if (detailsLine)
	{
		detailsLine->kill();
		delete detailsLine;
		detailsLine = NULL;
	}

	// paint detailsline
	if (isSelected)
	{
		int xPos	= this->x - DETAILSLINE_WIDTH;
		int yPosTop	= this->y + this->font->getHeight()/2;
		int yPosBottom	= this->footer->y + this->footer->getUsedHeight()/2;

		if (detailsLine == NULL)
		{
			detailsLine = new CComponentsDetailsLine(xPos, yPosTop, yPosBottom, this->font->getHeight()/2, this->footer->getUsedHeight() - RADIUS_LARGE*2);
		}
		detailsLine->paint(false);
	}
}

int EpgPlus::ChannelEntry::getUsedHeight()
{
	return font->getHeight() + separationLineThickness;
}

Font *EpgPlus::Footer::fontBouquetChannelName = NULL;
Font *EpgPlus::Footer::fontEventDescription = NULL;
Font *EpgPlus::Footer::fontEventInfo1 = NULL;

EpgPlus::Footer::Footer(CFrameBuffer * pframeBuffer, int px, int py, int pwidth, int pbuttonHeight)
{
	this->frameBuffer = pframeBuffer;
	this->x = px;
	this->y = py;
	this->width = pwidth;

	this->buttonHeight = pbuttonHeight;
	this->buttonY = this->y - OFFSET_INTER - this->buttonHeight;
}

EpgPlus::Footer::~Footer()
{
}

void EpgPlus::Footer::init()
{
	fontBouquetChannelName = g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMSMALL];
	fontEventDescription = g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMLARGE];
	fontEventInfo1 = g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_EVENT];
}

void EpgPlus::Footer::setBouquetChannelName(const std::string & newBouquetName, const std::string & newChannelName)
{
	this->currentBouquetName = newBouquetName;
	this->currentChannelName = newChannelName;
}

int EpgPlus::Footer::getUsedHeight()
{
	return fontBouquetChannelName->getHeight() + fontEventDescription->getHeight() + fontEventInfo1->getHeight() + 2*OFFSET_INNER_SMALL;
}

void EpgPlus::Footer::paintEventDetails(const std::string & description, const std::string & info1)
{
	int yPos = this->y;
	int frame_thickness = 2;

	// clear the whole footer
	this->frameBuffer->paintBoxRel(this->x, yPos, this->width, this->getUsedHeight(), COL_MENUCONTENTDARK_PLUS_0, RADIUS_LARGE);
	this->frameBuffer->paintBoxFrame(this->x, yPos, this->width, this->getUsedHeight(), frame_thickness, COL_FRAME_PLUS_0, RADIUS_LARGE);

	// display bouquet and channel name
	yPos += OFFSET_INNER_SMALL + this->fontBouquetChannelName->getHeight();
	this->fontBouquetChannelName->RenderString(this->x + OFFSET_INNER_MID, yPos, this->width - 2*OFFSET_INNER_MID, this->currentBouquetName + ": " + this->currentChannelName, COL_MENUCONTENT_TEXT);

	// display event's descrition
	yPos += this->fontEventDescription->getHeight();
	this->fontEventDescription->RenderString(this->x + OFFSET_INNER_MID, yPos, this->width - 2*OFFSET_INNER_MID, description, COL_MENUCONTENT_TEXT);

	// display event's info1
	yPos += this->fontEventInfo1->getHeight();
	this->fontEventInfo1->RenderString(this->x + OFFSET_INNER_MID, yPos, this->width - 2*OFFSET_INNER_MID, info1, COL_MENUCONTENT_TEXT);
}

struct button_label buttonLabels[] =
{
	{ NEUTRINO_ICON_BUTTON_RED, LOCALE_EPGPLUS_ACTIONS },
	{ NEUTRINO_ICON_BUTTON_GREEN, LOCALE_EPGPLUS_PREV_BOUQUET },
	{ NEUTRINO_ICON_BUTTON_YELLOW, LOCALE_EPGPLUS_NEXT_BOUQUET },
	{ NEUTRINO_ICON_BUTTON_BLUE, LOCALE_EPGPLUS_OPTIONS },
	{ NEUTRINO_ICON_BUTTON_INFO_SMALL,LOCALE_EPGMENU_EVENTINFO }
};

void EpgPlus::Footer::paintButtons(button_label * pbuttonLabels, int numberOfButtons)
{
	int buttonWidth = (this->width);
	CComponentsFooter _footer;
	_footer.paintButtons(this->x, this->buttonY, buttonWidth, buttonHeight, numberOfButtons, pbuttonLabels, buttonWidth/numberOfButtons);
}

EpgPlus::EpgPlus()
{
	selectedChannelEntry = NULL;
	this->init();
}

EpgPlus::~EpgPlus()
{
	this->free();
}

void EpgPlus::createChannelEntries(int selectedChannelEntryIndex)
{
	for (TChannelEntries::iterator It = this->displayedChannelEntries.begin();
			It != this->displayedChannelEntries.end();
			++It)
	{
		delete *It;
	}
	this->displayedChannelEntries.clear();

	this->selectedChannelEntry = NULL;

	if (selectedChannelEntryIndex < this->channelList->getSize())
	{
		for (;;)
		{
			if (selectedChannelEntryIndex < this->channelListStartIndex)
			{
				this->channelListStartIndex -= this->maxNumberOfDisplayableEntries;
				if (this->channelListStartIndex < 0)
					this->channelListStartIndex = 0;
			}
			else if (selectedChannelEntryIndex >= this->channelListStartIndex + this->maxNumberOfDisplayableEntries)
			{
				this->channelListStartIndex += this->maxNumberOfDisplayableEntries;
			}
			else
				break;
		}

		int yPosChannelEntry = this->channelsTableY;
		int yPosEventEntry = this->eventsTableY;

		for (int i = this->channelListStartIndex;
				(i < this->channelListStartIndex + this->maxNumberOfDisplayableEntries) && (i < this->channelList->getSize());
				++i, yPosChannelEntry += this->entryHeight, yPosEventEntry += this->entryHeight)
		{

			CZapitChannel * channel = (*this->channelList)[i];

			ChannelEntry *channelEntry = new ChannelEntry(channel, i, this->frameBuffer, this->footer, this->bouquetList, this->channelsTableX + 2, yPosChannelEntry, this->channelsTableWidth);
			//printf("Going to get getEventsServiceKey for %llx\n", (channel->getChannelID() & 0xFFFFFFFFFFFFULL));
			CChannelEventList channelEventList;
			CEitManager::getInstance()->getEventsServiceKey(channel->getEpgID(), channelEventList);
			//printf("channelEventList size %d\n", channelEventList.size());

			int widthEventEntry = 0;
			time_t lastEndTime = this->startTime;

			CChannelEventList::const_iterator lastIt(channelEventList.end());
			//for (CChannelEventList::const_iterator It = channelEventList.begin(); (It != channelEventList.end()) && (It->startTime < (this->startTime + this->duration)); ++It)
			for (CChannelEventList::const_iterator It = channelEventList.begin(); It != channelEventList.end(); ++It)
			{
				//if (0x2bc000b004b7ULL == (channel->getChannelID() & 0xFFFFFFFFFFFFULL)) printf("*** Check1 event %s event start %ld this start %ld\n", It->description.c_str(), It->startTime, (this->startTime + this->duration));
				if (!(It->startTime < (this->startTime + this->duration)) )
					continue;
				if ((lastIt == channelEventList.end()) || (lastIt->startTime != It->startTime)) {
					int startTimeDiff = It->startTime - this->startTime;
					int endTimeDiff = this->startTime + time_t (this->duration) - It->startTime - time_t (It->duration);
				//if (0x2bc000b004b7ULL == (channel->getChannelID() & 0xFFFFFFFFFFFFULL)) printf("*** Check event %s\n", It->description.c_str());
					if ((startTimeDiff >= 0) && (endTimeDiff >= 0))
					{
						// channel event fits completely in the visible part of time line
						startTimeDiff = 0;
						endTimeDiff = 0;
					}
					else if ((startTimeDiff < 0) && (endTimeDiff < 0))
					{
						// channel event starts and ends outside visible part of the time line but covers complete visible part
					}
					else if ((startTimeDiff < 0) && (endTimeDiff < this->duration))
					{
						// channel event starts before visible part of the time line but ends in the visible part
						endTimeDiff = 0;
					}
					else if ((endTimeDiff < 0) && (startTimeDiff < this->duration))
					{
						// channel event ends after visible part of the time line but starts in the visible part
						startTimeDiff = 0;
					}
					else if (startTimeDiff > 0)
					{
						// channel event starts and ends after visible part of the time line => break the loop
						//if (0x2bc000b004b7ULL == (channel->getChannelID() & 0xFFFFFFFFFFFFULL)) printf("*** break 1\n");
						break;
					}
					else
					{
						// channel event starts and ends after visible part of the time line => ignore the channel event
						//if (0x2bc000b004b7ULL == (channel->getChannelID() & 0xFFFFFFFFFFFFULL)) printf("*** continue 1 startTimeDiff %ld endTimeDiff %ld\n", startTimeDiff, endTimeDiff);
						continue;
					}

					if (lastEndTime < It->startTime)
					{
						// there is a gap between last end time and new start time => fill it with a new event entry
						CChannelEvent channelEvent;
						channelEvent.startTime = lastEndTime;
						channelEvent.duration = It->startTime - channelEvent.startTime;

						ChannelEventEntry *channelEventEntry = new ChannelEventEntry (&channelEvent, this->frameBuffer, this->timeLine, this->footer, this->eventsTableX + ((channelEvent.startTime - this->startTime) * this->eventsTableWidth) / this->duration, yPosEventEntry, (channelEvent.duration * this->eventsTableWidth) / this->duration + 1);
						channelEntry->channelEventEntries.push_back (channelEventEntry);
					}
					// correct position
					int xPosEventEntry = this->eventsTableX + ((It->startTime - startTimeDiff - this->startTime) * this->eventsTableWidth) / this->duration;

					// correct width
					widthEventEntry = ((It->duration + startTimeDiff + endTimeDiff) * this->eventsTableWidth) / this->duration + 1;

					if (widthEventEntry < 0)
						widthEventEntry = 0;

					if (xPosEventEntry + widthEventEntry > this->eventsTableX + this->eventsTableWidth)
						widthEventEntry = this->eventsTableX + this->eventsTableWidth - xPosEventEntry;

					ChannelEventEntry *channelEventEntry = new ChannelEventEntry(&(*It) , this->frameBuffer, this->timeLine, this->footer, xPosEventEntry, yPosEventEntry, widthEventEntry);

					channelEntry->channelEventEntries.push_back(channelEventEntry);
					lastEndTime = It->startTime + It->duration;
				}
				lastIt = It;
			}

			if (lastEndTime < this->startTime + time_t (this->duration))
			{
				// there is a gap between last end time and end of the timeline => fill it with a new event entry
				CChannelEvent channelEvent;
				channelEvent.startTime = lastEndTime;
				channelEvent.duration = this->startTime + this->duration - channelEvent.startTime;

				ChannelEventEntry *channelEventEntry = new ChannelEventEntry(&channelEvent, this->frameBuffer, this->timeLine, this->footer, this->eventsTableX + ((channelEvent.startTime - this->startTime) * this->eventsTableWidth) / this->duration, yPosEventEntry, (channelEvent.duration * this->eventsTableWidth) / this->duration + 1);
				channelEntry->channelEventEntries.push_back(channelEventEntry);
			}
			this->displayedChannelEntries.push_back(channelEntry);
		}

		this->selectedChannelEntry = this->displayedChannelEntries[selectedChannelEntryIndex - this->channelListStartIndex];
	}
}

void EpgPlus::init()
{
	frameBuffer = CFrameBuffer::getInstance();
#if 0
	currentViewMode = ViewMode_Scroll;
	currentSwapMode = SwapMode_ByPage;
#endif
	usableScreenWidth = frameBuffer->getScreenWidthRel();
	usableScreenHeight = frameBuffer->getScreenHeightRel();

	for (size_t i = 0; i < NumberOfSizeSettings; ++i)
	{
		int size = sizeSettingTable[i].size;
		if (size < 0) /* size < 0 == width in percent x -1 */
			size = usableScreenWidth * size / -100;
		sizes[i] = size;
	}

	Header::init();
	TimeLine::init();
	ChannelEntry::init();
	ChannelEventEntry::init();
	Footer::init();

	channelsTableWidth = sizes[EPGPlus_channelentry_width];
	sliderWidth = SCROLLBAR_WIDTH;

	int headerHeight = Header::getUsedHeight();
	int timeLineHeight = TimeLine::getUsedHeight();
	this->entryHeight = ChannelEntry::getUsedHeight();

	int buttonHeight = headerHeight;

	int footerHeight = Footer::getUsedHeight();

	this->maxNumberOfDisplayableEntries = (this->usableScreenHeight - headerHeight - timeLineHeight - buttonHeight - OFFSET_INTER - footerHeight) / this->entryHeight;
	this->bodyHeight = this->maxNumberOfDisplayableEntries * entryHeight;

	this->usableScreenHeight = headerHeight + timeLineHeight + this->bodyHeight + buttonHeight + OFFSET_INTER + footerHeight; // recalc deltaY
	this->usableScreenX = getScreenStartX(this->usableScreenWidth);
	this->usableScreenY = getScreenStartY(this->usableScreenHeight);

	this->headerX = this->usableScreenX;
	this->headerY = this->usableScreenY;
	this->headerWidth = this->usableScreenWidth;

	this->timeLineX = this->usableScreenX;
	this->timeLineY = this->usableScreenY + headerHeight;
	this->timeLineWidth = this->usableScreenWidth;

	this->footerX = usableScreenX;
	this->footerY = this->usableScreenY + this->usableScreenHeight - footerHeight;
	this->footerWidth = this->usableScreenWidth;

	this->channelsTableX = this->usableScreenX;
	this->channelsTableY = this->timeLineY + timeLineHeight;
	this->channelsTableHeight = this->bodyHeight;

	this->eventsTableX = this->channelsTableX + channelsTableWidth;
	this->eventsTableY = this->channelsTableY;
	this->eventsTableWidth = this->usableScreenWidth - this->channelsTableWidth - this->sliderWidth;
	this->eventsTableHeight = this->bodyHeight;

	this->sliderX = this->usableScreenX + this->usableScreenWidth - this->sliderWidth;
	this->sliderY = this->eventsTableY;
	this->sliderHeight = this->bodyHeight;

	this->channelListStartIndex = 0;
	this->startTime = 0;
	this->duration = 2 * 60 * 60;

	this->refreshAll = false;
	this->currentViewMode = ViewMode_Scroll;
	this->currentSwapMode = SwapMode_ByBouquet; //SwapMode_ByPage;

	this->header = new Header(this->frameBuffer, this->headerX, this->headerY, this->headerWidth);

	this->timeLine = new TimeLine(this->frameBuffer, this->timeLineX, this->timeLineY, this->timeLineWidth, this->eventsTableX, this->eventsTableWidth);

	this->footer = new Footer(this->frameBuffer, this->footerX, this->footerY, this->footerWidth, buttonHeight);
}

void EpgPlus::free()
{
	delete this->header;
	delete this->timeLine;
	delete this->footer;
}

int EpgPlus::exec(CChannelList * pchannelList, int selectedChannelIndex, CBouquetList *pbouquetList)
{
	this->channelList = pchannelList;
	this->channelListStartIndex = int (selectedChannelIndex / maxNumberOfDisplayableEntries) * maxNumberOfDisplayableEntries;
	this->bouquetList = pbouquetList;

	int res = menu_return::RETURN_REPAINT;

	COSDFader fader(g_settings.theme.menu_Content_alpha);
	do
	{
		this->refreshFooterButtons = false;
		time_t currentTime = time(NULL);
		tm tmStartTime = *localtime(&currentTime);

		tmStartTime.tm_sec = 0;
		tmStartTime.tm_min = int (tmStartTime.tm_min / 15) * 15;

		this->startTime = mktime(&tmStartTime);
		this->selectedTime = this->startTime;
		this->firstStartTime = this->startTime;

		if (this->selectedChannelEntry != NULL)
		{
			selectedChannelIndex = this->selectedChannelEntry->index;
		}

		neutrino_msg_t msg;
		neutrino_msg_data_t data;

		this->createChannelEntries(selectedChannelIndex);

		if (!this->refreshAll)
			fader.StartFadeIn();

		this->refreshAll = false;

		this->header->paint(this->channelList->getName());

		this->footer->paintButtons(buttonLabels, sizeof(buttonLabels) / sizeof(button_label));

		this->paint();

		uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_CHANLIST]);
		bool loop = true;
		while (loop)
		{
			g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);

			if (msg <= CRCInput::RC_MaxRC)
				timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_CHANLIST]);


			if ((msg == NeutrinoMessages::EVT_TIMER) && (data == fader.GetFadeTimer()))
			{
				if (fader.FadeDone())
					loop = false;
			}
			else if ((msg == CRCInput::RC_timeout) || (msg == (neutrino_msg_t) g_settings.key_channelList_cancel))
			{
				if (fader.StartFadeOut())
				{
					timeoutEnd = CRCInput::calcTimeoutEnd( 1 );
					msg = 0;
				}
				else
					loop = false;
			}
			else if (msg == CRCInput::RC_epg)
			{
				loop = false;
			}
			else if (msg == CRCInput::RC_help)
			{
				//fprintf(stderr, "RC_help, bigfont = %d\n", bigfont);
				hide();
				bigfont = !bigfont;
				free();
				init();
				refreshAll = true;
				break;
			}
			else if (msg == CRCInput::RC_page_down)
			{
				int selected = this->selectedChannelEntry->index;
				int prev_selected = selected;
				int step = this->maxNumberOfDisplayableEntries;
				int listSize = this->channelList->getSize();

				selected += step;
				if (selected >= listSize)
				{
					if ((listSize - step -1 < prev_selected) && (prev_selected != (listSize - 1)))
						selected = listSize - 1;
					else if (((listSize / step) + 1) * step == listSize + step) // last page has full entries
						selected = 0;
					else
						selected = ((selected < (((listSize / step)+1) * step))) ? (listSize - 1) : 0;
				}

				this->createChannelEntries(selected);
				this->paint();
			}
			else if (msg == CRCInput::RC_page_up)
			{
				int selected = this->selectedChannelEntry->index;
				int prev_selected = selected;
				int step = this->maxNumberOfDisplayableEntries;

				selected -= step;
				if ((prev_selected-step) < 0)
				{
					if (prev_selected != 0 && step != 1)
						selected = 0;
					else
						selected = this->channelList->getSize() - 1;
				}

				this->createChannelEntries(selected);
				this->paint();
			}
			if (msg == CRCInput::RC_yellow)
			{
				if (!bouquetList->Bouquets.empty())
				{
					bool found = true;
					uint32_t nNext = (bouquetList->getActiveBouquetNumber()+1) % bouquetList->Bouquets.size();
					//printf("EpgPlus::exec current bouquet %d new %d\n", bouquetList->getActiveBouquetNumber(), nNext);
					if (bouquetList->Bouquets[nNext]->channelList->isEmpty())
					{
						found = false;
						nNext = nNext < bouquetList->Bouquets.size()-1 ? nNext+1 : 0;
						for (uint32_t i = nNext; i < bouquetList->Bouquets.size(); i++)
						{
							if (!bouquetList->Bouquets[i]->channelList->isEmpty())
							{
								found = true;
								nNext = i;
								break;
							}
						}
					}
					//printf("EpgPlus::exec found %d new %d\n", found, nNext);
					if (found)
					{
						pbouquetList->activateBouquet(nNext, false);
						this->channelList = bouquetList->Bouquets[nNext]->channelList;
						this->channelListStartIndex = int (channelList->getSelectedChannelIndex() / maxNumberOfDisplayableEntries) * maxNumberOfDisplayableEntries;
						this->createChannelEntries (channelList->getSelectedChannelIndex());
						this->header->paint(this->channelList->getName());
						this->paint();
					}
				}
			}
			else if (msg == CRCInput::RC_green)
			{
				if (!bouquetList->Bouquets.empty())
				{
					bool found = true;
					int nNext = (bouquetList->getActiveBouquetNumber()+bouquetList->Bouquets.size()-1) % bouquetList->Bouquets.size();
					if (bouquetList->Bouquets[nNext]->channelList->isEmpty())
					{
						found = false;
						nNext = nNext > 0 ? nNext-1 : bouquetList->Bouquets.size()-1;
						for (int i = nNext; i > 0; i--)
						{
							if (!bouquetList->Bouquets[i]->channelList->isEmpty())
							{
								found = true;
								nNext = i;
								break;
							}
						}
					}
					if (found)
					{
						pbouquetList->activateBouquet(nNext, false);
						this->channelList = bouquetList->Bouquets[nNext]->channelList;
						this->channelListStartIndex = int (channelList->getSelectedChannelIndex() / maxNumberOfDisplayableEntries) * maxNumberOfDisplayableEntries;
						this->createChannelEntries (channelList->getSelectedChannelIndex());
						this->header->paint(this->channelList->getName());
						this->paint();
					}
				}
			} 
			else if (msg == CRCInput::RC_ok)
			{
				if (selectedChannelEntry)
					CNeutrinoApp::getInstance()->channelList->zapTo_ChannelID(selectedChannelEntry->channel->getChannelID());
				current_bouquet = bouquetList->getActiveBouquetNumber();
			} 
			else if (CRCInput::isNumeric(msg))
			{
				this->hide();
				CNeutrinoApp::getInstance()->channelList->numericZap(msg);
				//printf("numericZap: prev bouquet %d new %d\n", current_bouquet, bouquetList->getActiveBouquetNumber());
				current_bouquet = bouquetList->getActiveBouquetNumber();
				this->channelList = bouquetList->Bouquets[current_bouquet]->channelList;
				this->channelListStartIndex = int (channelList->getSelectedChannelIndex() / maxNumberOfDisplayableEntries) * maxNumberOfDisplayableEntries;
				g_InfoViewer->killTitle();

				int selectedChannelEntryIndex = this->channelList->getSelectedChannelIndex();
				if (selectedChannelEntryIndex < this->channelList->getSize())
				{
					this->hide();
					this->createChannelEntries(selectedChannelEntryIndex);

					this->header->paint(this->channelList->getName());
					this->footer->paintButtons(buttonLabels, sizeof(buttonLabels) / sizeof(button_label));
					this->paint();
				}

			} 
			else if (msg == CRCInput::RC_up)
			{
				int selectedChannelEntryIndex = this->selectedChannelEntry->index;
				int prevSelectedChannelEntryIndex = selectedChannelEntryIndex;

				--selectedChannelEntryIndex;
				if (selectedChannelEntryIndex < 0)
				{
					selectedChannelEntryIndex = this->channelList->getSize() - 1;
				}


				int oldChannelListStartIndex = this->channelListStartIndex;

				this->channelListStartIndex = (selectedChannelEntryIndex / this->maxNumberOfDisplayableEntries) * this->maxNumberOfDisplayableEntries;

				if (oldChannelListStartIndex != this->channelListStartIndex)
				{
					this->createChannelEntries(selectedChannelEntryIndex);
					this->paint();
				}
				else
				{
					this->selectedChannelEntry = this->displayedChannelEntries[selectedChannelEntryIndex - this->channelListStartIndex];
					this->paintChannelEntry(prevSelectedChannelEntryIndex - this->channelListStartIndex);
					this->paintChannelEntry(selectedChannelEntryIndex - this->channelListStartIndex);
				}
			} 
			else if (msg == CRCInput::RC_down)
			{
				int selectedChannelEntryIndex = this->selectedChannelEntry->index;
				int prevSelectedChannelEntryIndex = this->selectedChannelEntry->index;

				selectedChannelEntryIndex = (selectedChannelEntryIndex + 1) % this->channelList->getSize();


				int oldChannelListStartIndex = this->channelListStartIndex;
				this->channelListStartIndex = (selectedChannelEntryIndex / this->maxNumberOfDisplayableEntries) * this->maxNumberOfDisplayableEntries;

				if (oldChannelListStartIndex != this->channelListStartIndex)
				{
					this->createChannelEntries(selectedChannelEntryIndex);
					this->paint();
				}
				else
				{
					this->selectedChannelEntry = this->displayedChannelEntries[selectedChannelEntryIndex - this->channelListStartIndex];
					this->paintChannelEntry(prevSelectedChannelEntryIndex - this->channelListStartIndex);
					this->paintChannelEntry(this->selectedChannelEntry->index - this->channelListStartIndex);
				}
			} 
			else if (msg == CRCInput::RC_red)
			{
				CMenuWidget menuWidgetActions(LOCALE_EPGPLUS_ACTIONS, NEUTRINO_ICON_FEATURES);
				menuWidgetActions.enableFade(false);
				MenuTargetAddRecordTimer record(this);
				MenuTargetRefreshEpg refresh(this);
				MenuTargetAddReminder remind(this);
				if (!g_settings.minimode)
					menuWidgetActions.addItem(new CMenuForwarder(LOCALE_EPGPLUS_RECORD, true, NULL, &record, NULL, CRCInput::RC_red), false);
				menuWidgetActions.addItem(new CMenuForwarder(LOCALE_EPGPLUS_REFRESH_EPG, true, NULL, &refresh, NULL, CRCInput::RC_green), false);
				menuWidgetActions.addItem(new CMenuForwarder(LOCALE_EPGPLUS_REMIND, true, NULL, &remind, NULL, CRCInput::RC_yellow), false);
				if (selectedChannelEntry)
					menuWidgetActions.exec(NULL, "");

				this->refreshAll = true;
			} 
			else if (msg == CRCInput::RC_blue)
			{
				CMenuWidget menuWidgetOptions(LOCALE_EPGPLUS_OPTIONS, NEUTRINO_ICON_FEATURES);
				menuWidgetOptions.enableFade(false);
				//menuWidgetOptions.addItem(new MenuOptionChooserSwitchSwapMode(this));
				menuWidgetOptions.addItem(new MenuOptionChooserSwitchViewMode(this));

				int result = menuWidgetOptions.exec(NULL, "");
				if (result == menu_return::RETURN_REPAINT)
				{
					this->refreshAll = true;
				}
				else if (result == menu_return::RETURN_EXIT_ALL)
				{
					this->refreshAll = true;
				}
			} 
			else if (msg == CRCInput::RC_left)
			{
				switch (this->currentViewMode)
				{
					case ViewMode_Stretch:
					{
						if (this->duration - 30 * 60 > 30 * 60)
						{
							this->duration -= 30 * 60;
							this->hide();
							this->refreshAll = true;
						}
						break;
					}
					case ViewMode_Scroll:
					{
						TCChannelEventEntries::const_iterator It = this->getSelectedEvent();

						if ((It != this->selectedChannelEntry->channelEventEntries.begin())
								&& (It != this->selectedChannelEntry->channelEventEntries.end()))
						{
							--It;
							this->selectedTime = (*It)->channelEvent.startTime + (*It)->channelEvent.duration / 2;
							if (this->selectedTime < this->startTime)
								this->selectedTime = this->startTime;

							this->selectedChannelEntry->paint(true, this->selectedTime);
						}
						else
						{
							if (this->startTime != this->firstStartTime)
							{
								if (this->startTime - this->duration > this->firstStartTime)
								{
									this->startTime -= this->duration;
								}
								else
								{
									this->startTime = this->firstStartTime;
								}

								this->selectedTime = this->startTime + this->duration - 1;	// select last event
								this->createChannelEntries(this->selectedChannelEntry->index);

								this->paint();
							}
						}
						break;
					}
				}
			} 
			else if (msg == CRCInput::RC_right)
			{
				switch (this->currentViewMode)
				{
					case ViewMode_Stretch:
					{
						if (this->duration + 30 * 60 < 4 * 60 * 60)
						{
							this->duration += 60 * 60;
							this->hide();
							this->refreshAll = true;
						}
						break;
					}
					case ViewMode_Scroll:
					{
						TCChannelEventEntries::const_iterator It = this->getSelectedEvent();

						if ((It != this->selectedChannelEntry->channelEventEntries.end() - 1)
								&& (It != this->selectedChannelEntry->channelEventEntries.end()))
						{
							++It;

							this->selectedTime = (*It)->channelEvent.startTime + (*It)->channelEvent.duration / 2;

							if (this->selectedTime > this->startTime + time_t (this->duration))
								this->selectedTime = this->startTime + this->duration;

							this->selectedChannelEntry->paint(true, this->selectedTime);
						}
						else
						{
							this->startTime += this->duration;
							this->selectedTime = this->startTime;
							this->createChannelEntries(this->selectedChannelEntry->index);

							this->paint();
						}
						break;
					}
				}
			} 
			else if (msg == CRCInput::RC_info)
			{
				TCChannelEventEntries::const_iterator It = this->getSelectedEvent();

				if (It != this->selectedChannelEntry->channelEventEntries.end())
				{
					if ((*It)->channelEvent.eventID != 0)
					{
						this->hide();

						time_t startTime2 = (*It)->channelEvent.startTime;
						res = g_EpgData->show(this->selectedChannelEntry->channel->getChannelID(), (*It)->channelEvent.eventID, &startTime2);

						if (res == menu_return::RETURN_EXIT_ALL)
						{
							loop = false;
						}
						else
						{
							g_RCInput->getMsg(&msg, &data, 0);

							if ((msg != CRCInput::RC_red) && (msg != CRCInput::RC_timeout))
							{
								// RC_red schlucken
								g_RCInput->postMsg(msg, data);
							}

							this->header->paint(this->channelList->getName());
							this->footer->paintButtons(buttonLabels, sizeof(buttonLabels) / sizeof(button_label));
							this->paint();
						}
					}
				}
			}
			else if (CNeutrinoApp::getInstance()->listModeKey(msg))
			{
				g_RCInput->postMsg(msg, 0);
				res = menu_return::RETURN_EXIT_ALL;
				loop = false;
			}
			else if (msg == NeutrinoMessages::EVT_SERVICESCHANGED || msg == NeutrinoMessages::EVT_BOUQUETSCHANGED)
			{
				g_RCInput->postMsg(msg, data);
				loop = false;
				res = menu_return::RETURN_EXIT_ALL;
			}
			else
			{
				if (CNeutrinoApp::getInstance()->handleMsg(msg, data) & messages_return::cancel_all)
				{
					loop = false;
					res = menu_return::RETURN_EXIT_ALL;
				}
			}

			if (this->refreshAll)
				loop = false;
			else if (this->refreshFooterButtons)
				this->footer->paintButtons(buttonLabels, sizeof(buttonLabels) / sizeof(button_label));
		}

		this->hide();

		fader.StopFade();
#if 0
		for (TChannelEntries::iterator It = this->displayedChannelEntries.begin();
				It != this->displayedChannelEntries.end();
				It++)
		{
			delete *It;
		}
		this->displayedChannelEntries.clear();
#endif
	}
	while (this->refreshAll);

	for (TChannelEntries::iterator It = this->displayedChannelEntries.begin();
			It != this->displayedChannelEntries.end();
			++It)
	{
		delete *It;
	}
	this->displayedChannelEntries.clear();

	return res;
}

EpgPlus::TCChannelEventEntries::const_iterator EpgPlus::getSelectedEvent() const
{
	for (TCChannelEventEntries::const_iterator It = this->selectedChannelEntry->channelEventEntries.begin();
			It != this->selectedChannelEntry->channelEventEntries.end();
			++It)
	{
		if ((*It)->isSelected(this->selectedTime))
		{
			return It;
		}
	}
	return this->selectedChannelEntry->channelEventEntries.end();
}

void EpgPlus::hide()
{
	this->frameBuffer->paintBackgroundBoxRel(this->usableScreenX, this->usableScreenY, this->usableScreenWidth, this->usableScreenHeight);
}

void EpgPlus::paintChannelEntry(int position)
{
	ChannelEntry *channelEntry = this->displayedChannelEntries[position];

	bool currentChannelIsSelected = false;
	if (this->channelListStartIndex + position == this->selectedChannelEntry->index)
	{
		currentChannelIsSelected = true;
	}
	channelEntry->paint(currentChannelIsSelected, this->selectedTime);
}

std::string EpgPlus::getTimeString(const time_t & time, const std::string & format)
{
	char tmpstr[256];
	struct tm *tmStartTime = localtime(&time);

	strftime(tmpstr, sizeof(tmpstr), format.c_str(), tmStartTime);
	return tmpstr;
}

void EpgPlus::paint()
{
	// clear
	//this->frameBuffer->paintBackgroundBoxRel(this->channelsTableX, this->channelsTableY, this->usableScreenWidth, this->channelsTableHeight);
	this->frameBuffer->paintBoxRel(this->channelsTableX, this->channelsTableY, this->usableScreenWidth, this->channelsTableHeight, COL_MENUCONTENT_PLUS_0);

	// paint the time line
	timeLine->paint(this->startTime, this->duration);

	// paint the channel entries
	for (int i = 0; i < (int) this->displayedChannelEntries.size(); ++i)
	{
		this->paintChannelEntry(i);
	}

	// paint the time line grid
	this->timeLine->paintGrid();

	// paint slider
	int total_pages = ((this->channelList->getSize() - 1) / this->maxNumberOfDisplayableEntries) + 1;
	int current_page = this->selectedChannelEntry == NULL ? 0 : (this->selectedChannelEntry->index / this->maxNumberOfDisplayableEntries);

	paintScrollBar(this->sliderX, this->sliderY, this->sliderWidth, this->sliderHeight, total_pages, current_page);
}

// -- EPG+ Menue Handler Class
// -- to be used for calls from Menue
// -- (2004-03-05 rasc)

int CEPGplusHandler::exec(CMenuTarget * parent, const std::string & /*actionKey*/)
{
	int res = menu_return::RETURN_EXIT_ALL;
	EpgPlus *e;
	CChannelList *channelList;

	if (parent)
		parent->hide();

	e = new EpgPlus;

	//channelList = CNeutrinoApp::getInstance()->channelList;
	int bnum = bouquetList->getActiveBouquetNumber();
	current_bouquet = bnum;
	if (!bouquetList->Bouquets.empty() && !bouquetList->Bouquets[bnum]->channelList->isEmpty())
		channelList = bouquetList->Bouquets[bnum]->channelList;
	else
		channelList = CNeutrinoApp::getInstance()->channelList;

	e->exec(channelList, channelList->getSelectedChannelIndex(), bouquetList);
	delete e;
	// FIXME
	//printf("CEPGplusHandler::exec old bouquet %d new %d current %d\n", bnum, bouquetList->getActiveBouquetNumber(), current_bouquet);
	bouquetList->activateBouquet(current_bouquet, false);

	return res;
}

EpgPlus::MenuTargetAddReminder::MenuTargetAddReminder(EpgPlus * pepgPlus)
{
	this->epgPlus = pepgPlus;
}

int EpgPlus::MenuTargetAddReminder::exec(CMenuTarget * /*parent*/, const std::string & /*actionKey*/)
{
	TCChannelEventEntries::const_iterator It = this->epgPlus->getSelectedEvent();

	if ((It != this->epgPlus->selectedChannelEntry->channelEventEntries.end())
			&& (!(*It)->channelEvent.description.empty()))
	{
		if (g_Timerd->isTimerdAvailable())
		{
			g_Timerd->addZaptoTimerEvent(this->epgPlus->selectedChannelEntry->channel->getChannelID(), (*It)->channelEvent.startTime - (g_settings.zapto_pre_time * 60), (*It)->channelEvent.startTime - ANNOUNCETIME - (g_settings.zapto_pre_time * 60), 0, (*It)->channelEvent.eventID, (*It)->channelEvent.startTime, 0);

			ShowMsg(LOCALE_TIMER_EVENTTIMED_TITLE, g_Locale->getText(LOCALE_TIMER_EVENTTIMED_MSG),
				CMsgBox::mbrBack, CMsgBox::mbBack, NEUTRINO_ICON_INFO);
		}
		else
			printf("timerd not available\n");
	}
	return menu_return::RETURN_EXIT_ALL;
}

EpgPlus::MenuTargetAddRecordTimer::MenuTargetAddRecordTimer(EpgPlus * pepgPlus)
{
	this->epgPlus = pepgPlus;
}

static bool sortByDateTime(const CChannelEvent& a, const CChannelEvent& b)
{
	return a.startTime < b.startTime;
}

int EpgPlus::MenuTargetAddRecordTimer::exec(CMenuTarget * /*parent*/, const std::string & /*actionKey*/)
{
	TCChannelEventEntries::const_iterator It = this->epgPlus->getSelectedEvent();

	if ((It != this->epgPlus->selectedChannelEntry->channelEventEntries.end())
			&& (!(*It)->channelEvent.description.empty()))
	{
		bool doRecord = true;
		if (g_settings.recording_already_found_check)
		{
			CHintBox loadBox(LOCALE_RECORDING_ALREADY_FOUND_CHECK, LOCALE_MOVIEBROWSER_SCAN_FOR_MOVIES);
			loadBox.paint();
			CMovieBrowser moviebrowser;
			const char *rec_title = (*It)->channelEvent.description.c_str();
			bool already_found = moviebrowser.gotMovie(rec_title);
			loadBox.hide();
			if (already_found)
			{
				printf("already found in moviebrowser: %s\n", rec_title);
				char message[1024];
				snprintf(message, sizeof(message)-1, g_Locale->getText(LOCALE_RECORDING_ALREADY_FOUND), rec_title);
				doRecord = (ShowMsg(LOCALE_RECORDING_ALREADY_FOUND_CHECK, message, CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbNo) == CMsgBox::mbrYes);
			}
		}
		if (g_Timerd->isTimerdAvailable() && doRecord)
		{
			CChannelEventList evtlist;
			CEitManager::getInstance()->getEventsServiceKey(this->epgPlus->selectedChannelEntry->channel->channel_id, evtlist);
			sort(evtlist.begin(),evtlist.end(),sortByDateTime);
			CFollowScreenings m(this->epgPlus->selectedChannelEntry->channel->channel_id,
				(*It)->channelEvent.startTime,
				(*It)->channelEvent.startTime + (*It)->channelEvent.duration,
				(*It)->channelEvent.description, (*It)->channelEvent.eventID, TIMERD_APIDS_CONF, true, "", &evtlist);
			m.exec(NULL, "");
		}
		else
			printf("timerd not available\n");
	}
	return menu_return::RETURN_EXIT_ALL;
}

EpgPlus::MenuTargetRefreshEpg::MenuTargetRefreshEpg(EpgPlus * pepgPlus)
{
	this->epgPlus = pepgPlus;
}

int EpgPlus::MenuTargetRefreshEpg::exec(CMenuTarget * /*parent*/, const std::string & /*actionKey*/)
{
	this->epgPlus->refreshAll = true;
	return menu_return::RETURN_EXIT_ALL;
}

struct CMenuOptionChooser::keyval menuOptionChooserSwitchSwapModes[] =
{
	{ EpgPlus::SwapMode_ByPage, LOCALE_EPGPLUS_BYPAGE_MODE },
	{ EpgPlus::SwapMode_ByBouquet, LOCALE_EPGPLUS_BYBOUQUET_MODE }
};

EpgPlus::MenuOptionChooserSwitchSwapMode::MenuOptionChooserSwitchSwapMode(EpgPlus * pepgPlus)
		: CMenuOptionChooser(LOCALE_EPGPLUS_SWAP_MODE, (int *) &pepgPlus->currentSwapMode, menuOptionChooserSwitchSwapModes, sizeof(menuOptionChooserSwitchSwapModes) / sizeof(CMenuOptionChooser::keyval),
				true, NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW, 0)
{
	this->epgPlus = pepgPlus;
	this->oldSwapMode = epgPlus->currentSwapMode;
	this->oldTimingMenuSettings = g_settings.timing[SNeutrinoSettings::TIMING_MENU];
}

EpgPlus::MenuOptionChooserSwitchSwapMode::~MenuOptionChooserSwitchSwapMode()
{
	g_settings.timing[SNeutrinoSettings::TIMING_MENU] = this->oldTimingMenuSettings;

	if (this->epgPlus->currentSwapMode != this->oldSwapMode)
	{
		switch (this->epgPlus->currentSwapMode)
		{
			case SwapMode_ByPage:
			{
				buttonLabels[1].locale = LOCALE_EPGPLUS_PAGE_DOWN;
				buttonLabels[2].locale = LOCALE_EPGPLUS_PAGE_UP;
				break;
			}
			case SwapMode_ByBouquet:
			{
				buttonLabels[1].locale = LOCALE_EPGPLUS_PREV_BOUQUET;
				buttonLabels[2].locale = LOCALE_EPGPLUS_NEXT_BOUQUET;
				break;
			}
		}
		this->epgPlus->refreshAll = true;
	}
}

int EpgPlus::MenuOptionChooserSwitchSwapMode::exec(CMenuTarget * parent)
{
	// change time out settings temporary
	g_settings.timing[SNeutrinoSettings::TIMING_MENU] = 1;

	CMenuOptionChooser::exec(parent);

	return menu_return::RETURN_REPAINT;
}

struct CMenuOptionChooser::keyval menuOptionChooserSwitchViewModes[] =
{
	{ EpgPlus::ViewMode_Scroll, LOCALE_EPGPLUS_STRETCH_MODE },
	{ EpgPlus::ViewMode_Stretch, LOCALE_EPGPLUS_SCROLL_MODE }
};

EpgPlus::MenuOptionChooserSwitchViewMode::MenuOptionChooserSwitchViewMode(EpgPlus * epgPlus)
		: CMenuOptionChooser(LOCALE_EPGPLUS_VIEW_MODE, (int *) &epgPlus->currentViewMode, menuOptionChooserSwitchViewModes, sizeof(menuOptionChooserSwitchViewModes) / sizeof(CMenuOptionChooser::keyval),
				true, NULL, CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE)
{
	this->oldTimingMenuSettings = g_settings.timing[SNeutrinoSettings::TIMING_MENU];
}

EpgPlus::MenuOptionChooserSwitchViewMode::~MenuOptionChooserSwitchViewMode()
{
	g_settings.timing[SNeutrinoSettings::TIMING_MENU] = this->oldTimingMenuSettings;
}

int EpgPlus::MenuOptionChooserSwitchViewMode::exec(CMenuTarget * parent)
{
	// change time out settings temporary
	g_settings.timing[SNeutrinoSettings::TIMING_MENU] = 1;

	CMenuOptionChooser::exec(parent);

	return menu_return::RETURN_REPAINT;
}
