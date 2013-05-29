/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Copyright (C) 2004 Martin Griep 'vivamiga'

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

#include <gui/epgplus.h>
#include <sectionsdclient/sectionsdclient.h>

#include <gui/widget/icons.h>
#include <gui/widget/buttons.h>
#include <gui/widget/messagebox.h>
#include <gui/widget/stringinput.h>
#include "bouquetlist.h"

#include <zapit/client/zapittools.h>
#include <driver/rcinput.h>
#include <driver/screen_max.h>
#include <driver/fade.h>
#include <zapit/satconfig.h>
#include <zapit/getservices.h>
#include <eitd/sectionsd.h>

#include <algorithm>
#include <sstream>

#define COL_MENUCONTENT_P1                 254-8*4+1
#define COL_MENUCONTENT_P2                 254-8*4+2
extern CBouquetList *bouquetList;

Font * fonts[EpgPlus::NumberOfFontSettings];
int sizes[EpgPlus::NumberOfSizeSettings];

time_t EpgPlus::duration = 0;

int EpgPlus::horGap1Height = 0;
int EpgPlus::horGap2Height = 0;
int EpgPlus::verGap1Width = 0;
int EpgPlus::verGap2Width = 0;

int EpgPlus::horGap1Color = 0;
int EpgPlus::horGap2Color = 0;
int EpgPlus::verGap1Color = 0;
int EpgPlus::verGap2Color = 0;

int EpgPlus::sliderWidth = 0;
int EpgPlus::channelsTableWidth = 0;

static EpgPlus::FontSetting fontSettingTable[] = {
	{ EpgPlus::EPGPlus_header_font,				"Regular",	27 },
	{ EpgPlus::EPGPlus_timeline_fonttime,			"Bold",		16 },
	{ EpgPlus::EPGPlus_timeline_fontdate,			"Bold",		14 },
	{ EpgPlus::EPGPlus_channelentry_font,			"Bold",		16 },
	{ EpgPlus::EPGPlus_channelevententry_font,		"Regular",	16 },
	{ EpgPlus::EPGPlus_footer_fontbouquetchannelname,	"Bold",		24 },
	{ EpgPlus::EPGPlus_footer_fonteventdescription,		"Regular",	16 },
	{ EpgPlus::EPGPlus_footer_fonteventshortdescription,	"Regular",	16 },
	{ EpgPlus::EPGPlus_footer_fontbuttons,			"Regular",	16 },
};

/* negative size means "screen width in percent" */
static EpgPlus::SizeSetting sizeSettingTable[] = {
	{EpgPlus::EPGPlus_channelentry_width, -15 }, /* 15 percent of screen width */
	{EpgPlus::EPGPlus_channelentry_separationlineheight, 2},
	{EpgPlus::EPGPlus_slider_width, 15},
	{EpgPlus::EPGPlus_horgap1_height, 4},
	{EpgPlus::EPGPlus_horgap2_height, 4},
	{EpgPlus::EPGPlus_vergap1_width, 4},
	{EpgPlus::EPGPlus_vergap2_width, 4},
};

static bool bigfont = false;
static int current_bouquet;

Font *EpgPlus::Header::font = NULL;

EpgPlus::Header::Header (CFrameBuffer * pframeBuffer, int px, int py, int pwidth)
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
	font = fonts[EPGPlus_header_font];
}

void EpgPlus::Header::paint(const char * Name)
{
	std::string head = g_Locale->getText (LOCALE_EPGPLUS_HEAD);
	if(Name) {
		head += " ";
		head += Name;
	}
	this->frameBuffer->paintBoxRel (this->x, this->y, this->width, this->font->getHeight()+4, COL_MENUHEAD_PLUS_0, RADIUS_LARGE, CORNER_TOP);
	this->font->RenderString (this->x + 10, this->y + this->font->getHeight() + 2, 
			this->width - 20, head, COL_MENUHEAD, 0, true);
			//this->width - 20, g_Locale->getText (LOCALE_EPGPLUS_HEAD) , COL_MENUHEAD, 0, true);
        int icol_w, icol_h;
        frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_HELP, &icol_w, &icol_h);
	this->frameBuffer->paintIcon (NEUTRINO_ICON_BUTTON_HELP, this->x + this->width - icol_w - RADIUS_LARGE - 10, this->y+ ((this->font->getHeight()-icol_h)/2), icol_h);

}

int EpgPlus::Header::getUsedHeight()
{
	return font->getHeight() + 8 ;
}

Font *EpgPlus::TimeLine::fontTime = NULL;
Font *EpgPlus::TimeLine::fontDate = NULL;

EpgPlus::TimeLine::TimeLine (CFrameBuffer * pframeBuffer, int px, int py, int pwidth, int pstartX, int pdurationX)
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
	fontTime = fonts[EPGPlus_timeline_fonttime];
	fontDate = fonts[EPGPlus_timeline_fontdate];
}

EpgPlus::TimeLine::~TimeLine()
{
}

void EpgPlus::TimeLine::paint (time_t startTime, int pduration)
{
	this->clearMark();

	int xPos = this->startX;

	this->currentDuration = pduration;
	int numberOfTicks = this->currentDuration / (60 * 60) * 2;
	int tickDist = (this->durationX) / numberOfTicks;
	time_t tickTime = startTime;
	bool toggleColor = false;

	// display date of begin
	this->frameBuffer->paintBoxRel (this->x, this->y, this->width, this->fontTime->getHeight()
					, toggleColor ? COL_MENUCONTENT_PLUS_2 : COL_MENUCONTENT_PLUS_1);

	this->fontDate->RenderString (this->x + 4, this->y + this->fontDate->getHeight()
				      , this->width, EpgPlus::getTimeString (startTime, "%d-%b") , COL_MENUCONTENT, 0, true);	// UTF-8

	// paint ticks
	for (int i = 0; i < numberOfTicks; ++i, xPos += tickDist, tickTime += pduration / numberOfTicks) {
		int xWidth = tickDist;
		if (xPos + xWidth > this->x + width)
			xWidth = this->x + width - xPos;

		this->frameBuffer->paintBoxRel (xPos, this->y, xWidth, this->fontTime->getHeight()
						, toggleColor ? COL_MENUCONTENT_PLUS_1 : COL_MENUCONTENT_PLUS_2);

		std::string timeStr = EpgPlus::getTimeString (tickTime, "%H");

		int textWidth = this->fontTime->getRenderWidth (timeStr, true);

		this->fontTime->RenderString (xPos - textWidth - 4, this->y + this->fontTime->getHeight()
					      , textWidth, timeStr, toggleColor ? COL_MENUCONTENT_P1 : COL_MENUCONTENT_P2, 0, true);	// UTF-8

		timeStr = EpgPlus::getTimeString (tickTime, "%M");
		textWidth = this->fontTime->getRenderWidth (timeStr, true);
		this->fontTime->RenderString (xPos + 4, this->y + this->fontTime->getHeight()
					      , textWidth, timeStr, toggleColor ? COL_MENUCONTENT_P1 : COL_MENUCONTENT_P2, 0, true);	// UTF-8

		toggleColor = !toggleColor;
	}
}

void EpgPlus::TimeLine::paintGrid()
{
	int xPos = this->startX;
	int numberOfTicks = this->currentDuration / (60 * 60) * 2;
	int tickDist = (this->durationX) / numberOfTicks;
	// paint ticks
	for (int i = 0; i < numberOfTicks; ++i, xPos += tickDist) {
		// display a line for the tick
		this->frameBuffer->paintVLineRel (xPos, this->y, this->fontTime->getHeight(), COL_MENUCONTENT_PLUS_5);
	}
}

void EpgPlus::TimeLine::paintMark (time_t startTime, int pduration, int px, int pwidth)
{
	// clear old mark
	this->clearMark();

	// paint new mark
	this->frameBuffer->paintBoxRel (px, this->y + this->fontTime->getHeight()
					, pwidth, this->fontTime->getHeight() , COL_MENUCONTENTSELECTED_PLUS_0);

	// display start time before mark
	std::string timeStr = EpgPlus::getTimeString (startTime, "%H:%M");
	int textWidth = this->fontTime->getRenderWidth (timeStr, true);

	this->fontTime->RenderString (px - textWidth, this->y + this->fontTime->getHeight() + this->fontTime->getHeight()
				      , textWidth, timeStr, COL_MENUCONTENT, 0, true);	// UTF-8

	// display end time after mark
	timeStr = EpgPlus::getTimeString (startTime + pduration, "%H:%M");
	textWidth = fontTime->getRenderWidth (timeStr, true);

	if (px + pwidth + textWidth < this->x + this->width) {
		this->fontTime->RenderString (px + pwidth, this->y + this->fontTime->getHeight() + this->fontTime->getHeight()
					      , textWidth, timeStr, COL_MENUCONTENT, 0, true);	// UTF-8
	} else if (textWidth < pwidth - 10) {
		this->fontTime->RenderString (px + pwidth - textWidth, this->y + this->fontTime->getHeight() + this->fontTime->getHeight()
					      , textWidth, timeStr, COL_MENUCONTENTSELECTED, 0, true);	// UTF-8
	}
}

void EpgPlus::TimeLine::clearMark()
{
	this->frameBuffer->paintBoxRel (this->x, this->y + this->fontTime->getHeight()
					, this->width, this->fontTime->getHeight() , COL_MENUCONTENT_PLUS_0);
}

int EpgPlus::TimeLine::getUsedHeight()
{
	return std::max (fontDate->getHeight(), fontTime->getHeight())
	       + fontTime->getHeight();
}

Font *EpgPlus::ChannelEventEntry::font = NULL;
int EpgPlus::ChannelEventEntry::separationLineHeight = 0;

EpgPlus::ChannelEventEntry::ChannelEventEntry (const CChannelEvent * pchannelEvent, CFrameBuffer * pframeBuffer, TimeLine * ptimeLine, Footer * pfooter, int px, int py, int pwidth)
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
	font = fonts[EPGPlus_channelevententry_font];
	separationLineHeight = sizes[EPGPlus_channelentry_separationlineheight];
}

EpgPlus::ChannelEventEntry::~ChannelEventEntry()
{
}

bool EpgPlus::ChannelEventEntry::isSelected (time_t selectedTime) const
{
	return (selectedTime >= this->channelEvent.startTime) && (selectedTime < this->channelEvent.startTime + time_t (this->channelEvent.duration));
}

void EpgPlus::ChannelEventEntry::paint (bool pisSelected, bool toggleColor)
{
	this->frameBuffer->paintBoxRel (this->x, this->y, this->width, this->font->getHeight()
			, this->channelEvent.description.empty()? COL_MENUCONTENT_PLUS_0 : (pisSelected ? COL_MENUCONTENTSELECTED_PLUS_0 : (toggleColor ? COL_MENUCONTENT_PLUS_1 : COL_MENUCONTENT_PLUS_2)));

	this->font->RenderString (this->x + 2, this->y + this->font->getHeight()
				  , this->width - 4 > 0 ? this->width - 4 : 0, this->channelEvent.description, pisSelected ? COL_MENUCONTENTSELECTED : (toggleColor ? COL_MENUCONTENT_P1 : COL_MENUCONTENT_P2)
				  , 0, true);

	// paint the separation line
	if (separationLineHeight > 0) {
		this->frameBuffer->paintBoxRel (this->x, this->y + this->font->getHeight()
						, this->width, this->separationLineHeight, COL_MENUCONTENT_PLUS_5);
	}

	if (pisSelected) {
		if (this->channelEvent.description.empty()) {	// dummy channel event
			this->timeLine->clearMark();
		} else {
			this->timeLine->paintMark (this->channelEvent.startTime, this->channelEvent.duration, this->x, this->width);
		}

		CShortEPGData shortEpgData;

		bool ret = CEitManager::getInstance()->getEPGidShort(this->channelEvent.eventID, &shortEpgData);
		this->footer->paintEventDetails (this->channelEvent.description, ret ? shortEpgData.info1 : "");

		this->timeLine->paintGrid();
	}
}

int EpgPlus::ChannelEventEntry::getUsedHeight()
{
	return font->getHeight() + separationLineHeight;
}

Font *EpgPlus::ChannelEntry::font = NULL;
int EpgPlus::ChannelEntry::separationLineHeight = 0;

EpgPlus::ChannelEntry::ChannelEntry (const CZapitChannel * pchannel, int pindex, CFrameBuffer * pframeBuffer, Footer * pfooter, CBouquetList * pbouquetList, int px, int py, int pwidth)
{
	this->channel = pchannel;

	if (pchannel != NULL) {
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
}

void EpgPlus::ChannelEntry::init()
{
	font = fonts[EPGPlus_channelentry_font];
	separationLineHeight = sizes[EPGPlus_channelentry_separationlineheight];
}

EpgPlus::ChannelEntry::~ChannelEntry()
{
	for (TCChannelEventEntries::iterator It = this->channelEventEntries.begin();
			It != this->channelEventEntries.end(); ++It) {
		delete *It;
	}
	this->channelEventEntries.clear();
}

void EpgPlus::ChannelEntry::paint (bool isSelected, time_t selectedTime)
{
	this->frameBuffer->paintBoxRel (this->x, this->y, this->width, this->font->getHeight(),
			isSelected ? COL_MENUCONTENTSELECTED_PLUS_0 : COL_MENUCONTENT_PLUS_0);

	this->font->RenderString (this->x + 2, this->y + this->font->getHeight(),
			this->width - 4, this->displayName, isSelected ? COL_MENUCONTENTSELECTED : COL_MENUCONTENT, 0, true);

	if (isSelected) {
#if 0
		for (uint32_t i = 0; i < this->bouquetList->Bouquets.size(); ++i) {
			CBouquet *bouquet = this->bouquetList->Bouquets[i];
			for (int j = 0; j < bouquet->channelList->getSize(); ++j) {

				if ((*bouquet->channelList)[j]->number == this->channel->number) {
					this->footer->setBouquetChannelName (bouquet->channelList->getName()
									     , this->channel->getName()
									    );

					bouquet = NULL;
					break;
				}
			}
			if (bouquet == NULL)
				break;
		}
#endif
		if(this->channel->pname) {
			this->footer->setBouquetChannelName(this->channel->pname, this->channel->getName());
		} else {
			this->footer->setBouquetChannelName(
					CServiceManager::getInstance()->GetSatelliteName(this->channel->getSatellitePosition()),
					this->channel->getName());
		}
	}
	// paint the separation line
	if (separationLineHeight > 0) {
		this->frameBuffer->paintBoxRel (this->x, this->y + this->font->getHeight(),
				this->width, this->separationLineHeight, COL_MENUCONTENT_PLUS_5);
	}

	bool toggleColor = false;
	for (TCChannelEventEntries::iterator It = this->channelEventEntries.begin();
			It != this->channelEventEntries.end();
			++It) {
		(*It)->paint (isSelected && (*It)->isSelected (selectedTime), toggleColor);

		toggleColor = !toggleColor;
	}
}

int EpgPlus::ChannelEntry::getUsedHeight()
{
	return font->getHeight() + separationLineHeight;
}

Font *EpgPlus::Footer::fontBouquetChannelName = NULL;
Font *EpgPlus::Footer::fontEventDescription = NULL;
Font *EpgPlus::Footer::fontEventShortDescription = NULL;
Font *EpgPlus::Footer::fontButtons = NULL;
int EpgPlus::Footer::color = 0;

EpgPlus::Footer::Footer (CFrameBuffer * pframeBuffer, int px, int py, int pwidth, int /*height*/)
{
	this->frameBuffer = pframeBuffer;
	this->x = px;
	this->y = py;
	this->width = pwidth;
 	this->buttonHeight = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight()+8; //TODO get height from buttons
}

EpgPlus::Footer::~Footer()
{
}

void EpgPlus::Footer::init()
{
	fontBouquetChannelName = fonts[EPGPlus_footer_fontbouquetchannelname];
	fontEventDescription = fonts[EPGPlus_footer_fonteventdescription];
	fontEventShortDescription = fonts[EPGPlus_footer_fonteventshortdescription];
	fontButtons = fonts[EPGPlus_footer_fontbuttons];
}

void EpgPlus::Footer::setBouquetChannelName (const std::string & newBouquetName, const std::string & newChannelName)
{
	this->currentBouquetName = newBouquetName;
	this->currentChannelName = newChannelName;
}

int EpgPlus::Footer::getUsedHeight()
{
	return fontBouquetChannelName->getHeight() + fontEventDescription->getHeight()
	       + fontEventShortDescription->getHeight() /* + fontButtons->getHeight()*/;
}

void EpgPlus::Footer::paintEventDetails (const std::string & description, const std::string & shortDescription)
{
	int yPos = this->y;

	int height = this->fontBouquetChannelName->getHeight();

	// clear the region
	this->frameBuffer->paintBoxRel (this->x, yPos, this->width, height, COL_MENUHEAD_PLUS_0);

	yPos += height;

	// display new text
	this->fontBouquetChannelName->RenderString (this->x + 10, yPos, this->width - 20, this->currentBouquetName + " : " + this->currentChannelName, COL_MENUHEAD, 0, true);

	height = this->fontEventDescription->getHeight();

	// clear the region
	this->frameBuffer->paintBoxRel (this->x, yPos, this->width, height, COL_MENUHEAD_PLUS_0);

	yPos += height;

	// display new text
	this->fontEventDescription->RenderString (this->x + 10, yPos, this->width - 20, description, COL_MENUHEAD, 0, true);

	height = this->fontEventShortDescription->getHeight();

	// clear the region
	this->frameBuffer->paintBoxRel (this->x, yPos, this->width, height, COL_MENUHEAD_PLUS_0);

	yPos += height;

	// display new text
	this->fontEventShortDescription->RenderString (this->x + 10, yPos, this->width - 20, shortDescription, COL_MENUHEAD, 0, true);
}

struct button_label buttonLabels[] = {
	{NEUTRINO_ICON_BUTTON_RED, LOCALE_EPGPLUS_ACTIONS},
	{NEUTRINO_ICON_BUTTON_GREEN, LOCALE_EPGPLUS_PREV_BOUQUET /*LOCALE_EPGPLUS_PAGE_UP*/},
	{NEUTRINO_ICON_BUTTON_YELLOW, LOCALE_EPGPLUS_NEXT_BOUQUET /*LOCALE_EPGPLUS_PAGE_DOWN*/},
	{NEUTRINO_ICON_BUTTON_BLUE, LOCALE_EPGPLUS_OPTIONS}
};

void EpgPlus::Footer::paintButtons (button_label * pbuttonLabels, int numberOfButtons)
{
	int buttonWidth = (this->width);
	int yPos = this->y + this->getUsedHeight();
	::paintButtons (this->x, yPos, buttonWidth, numberOfButtons, pbuttonLabels, buttonWidth, buttonHeight);
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

void EpgPlus::createChannelEntries (int selectedChannelEntryIndex)
{
	for (TChannelEntries::iterator It = this->displayedChannelEntries.begin();
			It != this->displayedChannelEntries.end(); ++It) {
		delete *It;
	}
	this->displayedChannelEntries.clear();

	this->selectedChannelEntry = NULL;

	if (selectedChannelEntryIndex < this->channelList->getSize()) {
		for (;;) {
			if (selectedChannelEntryIndex < this->channelListStartIndex) {
				this->channelListStartIndex -= this->maxNumberOfDisplayableEntries;
				if (this->channelListStartIndex < 0)
					this->channelListStartIndex = 0;
			} else if (selectedChannelEntryIndex >= this->channelListStartIndex + this->maxNumberOfDisplayableEntries) {
				this->channelListStartIndex += this->maxNumberOfDisplayableEntries;
			} else
				break;
		}

		int yPosChannelEntry = this->channelsTableY;
		int yPosEventEntry = this->eventsTableY;

		for (int i = this->channelListStartIndex; (i < this->channelListStartIndex + this->maxNumberOfDisplayableEntries)
				&& (i < this->channelList->getSize());
				++i, yPosChannelEntry += this->entryHeight, yPosEventEntry += this->entryHeight) {

			CZapitChannel * channel = (*this->channelList)[i];

			ChannelEntry *channelEntry = new ChannelEntry (channel, i, this->frameBuffer, this->footer, this->bouquetList, this->channelsTableX + 2, yPosChannelEntry, this->channelsTableWidth);
//printf("Going to get getEventsServiceKey for %llx\n", (channel->channel_id & 0xFFFFFFFFFFFFULL));
			CChannelEventList channelEventList;
			CEitManager::getInstance()->getEventsServiceKey(channel->channel_id, channelEventList);
//printf("channelEventList size %d\n", channelEventList.size());

			int xPosEventEntry = this->eventsTableX;
			int widthEventEntry = 0;
			time_t lastEndTime = this->startTime;

			CChannelEventList::const_iterator lastIt (channelEventList.end());
			//for (CChannelEventList::const_iterator It = channelEventList.begin(); (It != channelEventList.end()) && (It->startTime < (this->startTime + this->duration)); ++It)
			for (CChannelEventList::const_iterator It = channelEventList.begin(); It != channelEventList.end(); ++It)
			{
//if(0x2bc000b004b7ULL == (channel->channel_id & 0xFFFFFFFFFFFFULL)) printf("*** Check1 event %s event start %ld this start %ld\n", It->description.c_str(), It->startTime, (this->startTime + this->duration));
				if (!(It->startTime < (this->startTime + this->duration)) )
					continue;
				if ((lastIt == channelEventList.end()) || (lastIt->startTime != It->startTime)) {
					int startTimeDiff = It->startTime - this->startTime;
					int endTimeDiff = this->startTime + time_t (this->duration) - It->startTime - time_t (It->duration);
//if(0x2bc000b004b7ULL == (channel->channel_id & 0xFFFFFFFFFFFFULL)) printf("*** Check event %s\n", It->description.c_str());
					if ((startTimeDiff >= 0) && (endTimeDiff >= 0)) {
						// channel event fits completely in the visible part of time line
						startTimeDiff = 0;
						endTimeDiff = 0;
					} else if ((startTimeDiff < 0) && (endTimeDiff < 0)) {
						// channel event starts and ends outside visible part of the time line but covers complete visible part
					} else if ((startTimeDiff < 0) && (endTimeDiff < this->duration)) {
						// channel event starts before visible part of the time line but ends in the visible part
						endTimeDiff = 0;
					} else if ((endTimeDiff < 0) && (startTimeDiff < this->duration)) {
						// channel event ends after visible part of the time line but starts in the visible part
						startTimeDiff = 0;
					} else if (startTimeDiff > 0) {	// channel event starts and ends after visible part of the time line => break the loop
//if(0x2bc000b004b7ULL == (channel->channel_id & 0xFFFFFFFFFFFFULL)) printf("*** break 1\n");
						break;
					} else {				// channel event starts and ends after visible part of the time line => ignore the channel event
//if(0x2bc000b004b7ULL == (channel->channel_id & 0xFFFFFFFFFFFFULL)) printf("*** continue 1 startTimeDiff %ld endTimeDiff %ld\n", startTimeDiff, endTimeDiff);
						continue;
					}

					if (lastEndTime < It->startTime) {	// there is a gap between last end time and new start time => fill it with a new event entry

						CChannelEvent channelEvent;
						channelEvent.startTime = lastEndTime;
						channelEvent.duration = It->startTime - channelEvent.startTime;

						ChannelEventEntry *channelEventEntry = new ChannelEventEntry (&channelEvent, this->frameBuffer, this->timeLine, this->footer, this->eventsTableX + ((channelEvent.startTime - this->startTime) * this->eventsTableWidth) / this->duration, yPosEventEntry, (channelEvent.duration * this->eventsTableWidth) / this->duration + 1);
						channelEntry->channelEventEntries.push_back (channelEventEntry);
					}
					// correct position
					xPosEventEntry = this->eventsTableX + ((It->startTime - startTimeDiff - this->startTime) * this->eventsTableWidth) / this->duration;

					// correct width
					widthEventEntry = ((It->duration + startTimeDiff + endTimeDiff) * this->eventsTableWidth) / this->duration + 1;

					if (widthEventEntry < 0)
						widthEventEntry = 0;

					if (xPosEventEntry + widthEventEntry > this->eventsTableX + this->eventsTableWidth)
						widthEventEntry = this->eventsTableX + this->eventsTableWidth - xPosEventEntry;

					ChannelEventEntry *channelEventEntry = new ChannelEventEntry (&(*It) , this->frameBuffer, this->timeLine, this->footer, xPosEventEntry, yPosEventEntry, widthEventEntry);

					channelEntry->channelEventEntries.push_back (channelEventEntry);
					lastEndTime = It->startTime + It->duration;
				}
				lastIt = It;
			}

			if (lastEndTime < this->startTime + time_t (this->duration)) {	// there is a gap between last end time and end of the timeline => fill it with a new event entry

				CChannelEvent channelEvent;
				channelEvent.startTime = lastEndTime;
				channelEvent.duration = this->startTime + this->duration - channelEvent.startTime;

				ChannelEventEntry *channelEventEntry = new ChannelEventEntry (&channelEvent, this->frameBuffer, this->timeLine, this->footer, this->eventsTableX + ((channelEvent.startTime - this->startTime) * this->eventsTableWidth) / this->duration, yPosEventEntry, (channelEvent.duration * this->eventsTableWidth) / this->duration + 1);
				channelEntry->channelEventEntries.push_back (channelEventEntry);
			}
			this->displayedChannelEntries.push_back (channelEntry);
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
	std::string FileName = std::string (g_settings.font_file);
	for (size_t i = 0; i < NumberOfFontSettings; ++i) {
		int size = fontSettingTable[i].size;
		if (bigfont && (fontSettingTable[i].settingID == EpgPlus::EPGPlus_channelentry_font ||
				fontSettingTable[i].settingID == EpgPlus::EPGPlus_channelevententry_font)) {
			size = size * 3 / 2; /* increase font size for channel name and event title */
		}
		std::string family = g_fontRenderer->getFamily (FileName.c_str());
		Font *font = g_fontRenderer->getFont(family.c_str(), fontSettingTable[i].style, size);

		if (font == NULL)
			font = g_fontRenderer->getFont(family.c_str(), "Regular", size);

		fonts[i] = font;
	}

	for (size_t i = 0; i < NumberOfSizeSettings; ++i) {
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
	sliderWidth = sizes[EPGPlus_slider_width];

	horGap1Height = sizes[EPGPlus_horgap1_height];
	horGap2Height = sizes[EPGPlus_horgap2_height];
	verGap1Width = sizes[EPGPlus_vergap1_width];
	verGap2Width = sizes[EPGPlus_vergap2_width];

	int headerHeight = Header::getUsedHeight();
	int timeLineHeight = TimeLine::getUsedHeight();
	this->entryHeight = ChannelEntry::getUsedHeight();

        int icol_w, icol_h, h2;

        CFrameBuffer::getInstance()->getIconSize(NEUTRINO_ICON_BUTTON_RED, &icol_w, &icol_h);
	CFrameBuffer::getInstance()->getIconSize(NEUTRINO_ICON_BUTTON_HELP, &icol_w, &h2);
// 	if(icol_h < h2)
// 		icol_h = h2;

	int buttonHeight = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight()+8; //TODO get height from buttons/*std::max (icol_h+8, fonts[EPGPlus_footer_fontbuttons]->getHeight());*/
	int footerHeight = Footer::getUsedHeight() + buttonHeight;

	this->maxNumberOfDisplayableEntries = (this->usableScreenHeight - headerHeight - timeLineHeight - horGap1Height - horGap2Height - footerHeight) / this->entryHeight;

	this->usableScreenHeight = headerHeight + timeLineHeight + horGap1Height + this->maxNumberOfDisplayableEntries * this->entryHeight + horGap2Height + footerHeight;	// recalc deltaY
	this->usableScreenX = getScreenStartX(this->usableScreenWidth);
	this->usableScreenY = getScreenStartY(this->usableScreenHeight);

	this->headerX = this->usableScreenX;
	this->headerY = this->usableScreenY;
	this->headerWidth = this->usableScreenWidth;

	this->timeLineX = this->usableScreenX;
	this->timeLineY = this->usableScreenY + headerHeight;
	this->timeLineWidth = this->usableScreenWidth;

	this->horGap1X = this->usableScreenX;
	this->horGap1Y = this->timeLineY + timeLineHeight;
	this->horGap1Width = this->usableScreenWidth;

	this->footerX = usableScreenX;
	this->footerY = this->usableScreenY + this->usableScreenHeight - footerHeight;
	this->footerWidth = this->usableScreenWidth;

	this->horGap2X = this->usableScreenX;
	this->horGap2Y = this->footerY - horGap2Height;
	this->horGap2Width = this->usableScreenWidth;

	this->channelsTableX = this->usableScreenX;
	this->channelsTableY = this->timeLineY + timeLineHeight + horGap1Height;
	this->channelsTableHeight = this->maxNumberOfDisplayableEntries * entryHeight;

	this->verGap1X = this->channelsTableX + channelsTableWidth;
	this->verGap1Y = this->channelsTableY;
	this->verGap1Height = this->channelsTableHeight;

	this->eventsTableX = this->channelsTableX + channelsTableWidth + verGap1Width;
	this->eventsTableY = this->channelsTableY;
	this->eventsTableWidth = this->usableScreenWidth - this->channelsTableWidth - this->sliderWidth - verGap1Width - verGap2Width;
	this->eventsTableHeight = this->channelsTableHeight;

	this->sliderX = this->usableScreenX + this->usableScreenWidth - this->sliderWidth;
	this->sliderY = this->eventsTableY;
	this->sliderHeight = this->channelsTableHeight;

	this->verGap2X = this->sliderX - verGap2Width;
	this->verGap2Y = this->channelsTableY;
	this->verGap2Height = this->channelsTableHeight;

	this->channelListStartIndex = 0;
	this->startTime = 0;
	this->duration = 2 * 60 * 60;

	this->refreshAll = false;
	this->currentViewMode = ViewMode_Scroll;
	this->currentSwapMode = SwapMode_ByBouquet; //SwapMode_ByPage;

	this->header = new Header (this->frameBuffer, this->headerX, this->headerY, this->headerWidth);

	this->timeLine = new TimeLine (this->frameBuffer, this->timeLineX, this->timeLineY, this->timeLineWidth, this->eventsTableX, this->eventsTableWidth);

	this->footer = new Footer (this->frameBuffer, this->footerX, this->footerY, this->footerWidth, buttonHeight);
}

void EpgPlus::free()
{
	delete this->header;
	delete this->timeLine;
	delete this->footer;
	int i;
	for (i = 0; i < NumberOfFontSettings; ++i) {
		delete fonts[i];
	}
}

int EpgPlus::exec (CChannelList * pchannelList, int selectedChannelIndex, CBouquetList *pbouquetList)
{
	this->channelList = pchannelList;
	this->channelListStartIndex = int (selectedChannelIndex / maxNumberOfDisplayableEntries) * maxNumberOfDisplayableEntries;
	this->bouquetList = pbouquetList;

	int res = menu_return::RETURN_REPAINT;

	COSDFader fader(g_settings.menu_Content_alpha);
	do {
		this->refreshFooterButtons = false;
		time_t currentTime = time (NULL);
		tm tmStartTime = *localtime (&currentTime);

		tmStartTime.tm_sec = 0;
		tmStartTime.tm_min = int (tmStartTime.tm_min / 15) * 15;

		this->startTime = mktime (&tmStartTime);
		this->selectedTime = this->startTime;
		this->firstStartTime = this->startTime;

		if (this->selectedChannelEntry != NULL) {
			selectedChannelIndex = this->selectedChannelEntry->index;
		}

		neutrino_msg_t msg;
		neutrino_msg_data_t data;

		this->createChannelEntries (selectedChannelIndex);

		if(!this->refreshAll)
			fader.StartFadeIn();

		this->refreshAll = false;

		this->header->paint(this->channelList->getName());

		this->footer->paintButtons (buttonLabels, sizeof (buttonLabels) / sizeof (button_label));

		this->paint();
		frameBuffer->blit();

		uint64_t timeoutEnd = CRCInput::calcTimeoutEnd (g_settings.timing[SNeutrinoSettings::TIMING_CHANLIST]);
		bool loop = true;
		while (loop) {
			g_RCInput->getMsgAbsoluteTimeout (&msg, &data, &timeoutEnd);

			if (msg <= CRCInput::RC_MaxRC)
				timeoutEnd = CRCInput::calcTimeoutEnd (g_settings.timing[SNeutrinoSettings::TIMING_CHANLIST]);


			if((msg == NeutrinoMessages::EVT_TIMER) && (data == fader.GetTimer())) {
				if(fader.Fade())
					loop = false;
			}
			else if ((msg == CRCInput::RC_timeout) || (msg == (neutrino_msg_t) g_settings.key_channelList_cancel)) {
				if(fader.StartFadeOut()) {
					timeoutEnd = CRCInput::calcTimeoutEnd( 1 );
					msg = 0;
				} else
					loop = false;
			}
			else if (msg == CRCInput::RC_epg) {
				//fprintf(stderr, "RC_Epg, bigfont = %d\n", bigfont);
				hide();
				bigfont = !bigfont;
				free();
				init();
				refreshAll = true;
				break;
			}
			else if ((msg == CRCInput::RC_page_down)) {
				int selected = this->selectedChannelEntry->index;
				int prev_selected = selected;
				int step = this->maxNumberOfDisplayableEntries;
				int listSize = this->channelList->getSize();

				selected += step;
				if(selected >= listSize) {
					if((listSize - step -1 < prev_selected) && (prev_selected != (listSize - 1)))
						selected = listSize - 1;
					else if (((listSize / step) + 1) * step == listSize + step) // last page has full entries
						selected = 0;
					else
						selected = ((selected < (((listSize / step)+1) * step))) ? (listSize - 1) : 0;
				}

				this->createChannelEntries (selected);
				this->paint();
			}
			else if (msg == CRCInput::RC_page_up) {
				int selected = this->selectedChannelEntry->index;
				int prev_selected = selected;
				int step = this->maxNumberOfDisplayableEntries;

				selected -= step;
				if((prev_selected-step) < 0) {
					if(prev_selected != 0 && step != 1)
						selected = 0;
					else
						selected = this->channelList->getSize() - 1;
				}

				this->createChannelEntries (selected);
				this->paint();
			}
			if (msg == CRCInput::RC_yellow) {
				if (!bouquetList->Bouquets.empty()) {
					bool found = true;
					uint32_t nNext = (bouquetList->getActiveBouquetNumber()+1) % bouquetList->Bouquets.size();
//printf("**************************** EpgPlus::exec current bouquet %d new %d\n", bouquetList->getActiveBouquetNumber(), nNext);
					if(bouquetList->Bouquets[nNext]->channelList->isEmpty() ) {
						found = false;
						nNext = nNext < bouquetList->Bouquets.size()-1 ? nNext+1 : 0;
						for(uint32_t i = nNext; i < bouquetList->Bouquets.size(); i++) {
							if(!bouquetList->Bouquets[i]->channelList->isEmpty() ) {
								found = true;
								nNext = i;
								break;
							}
						}
					}
//printf("**************************** EpgPlus::exec found %d new %d\n", found, nNext);
					if(found) {
						pbouquetList->activateBouquet (nNext, false);
						this->channelList = bouquetList->Bouquets[nNext]->channelList;
						this->channelListStartIndex = int (channelList->getSelectedChannelIndex() / maxNumberOfDisplayableEntries) * maxNumberOfDisplayableEntries;
						this->createChannelEntries (channelList->getSelectedChannelIndex());
						this->header->paint(this->channelList->getName());
						this->paint();
					}
				}
			}
			else if (msg == CRCInput::RC_green) {
				if (!bouquetList->Bouquets.empty()) {
					bool found = true;
					int nNext = (bouquetList->getActiveBouquetNumber()+bouquetList->Bouquets.size()-1) % bouquetList->Bouquets.size();
					if(bouquetList->Bouquets[nNext]->channelList->isEmpty()) {
						found = false;
						nNext = nNext > 0 ? nNext-1 : bouquetList->Bouquets.size()-1;
						for(int i = nNext; i > 0; i--) {
							if(!bouquetList->Bouquets[i]->channelList->isEmpty()) {
								found = true;
								nNext = i;
								break;
							}
						}
					}
					if(found) {
						pbouquetList->activateBouquet (nNext, false);
						this->channelList = bouquetList->Bouquets[nNext]->channelList;
						this->channelListStartIndex = int (channelList->getSelectedChannelIndex() / maxNumberOfDisplayableEntries) * maxNumberOfDisplayableEntries;
						this->createChannelEntries (channelList->getSelectedChannelIndex());
						this->header->paint(this->channelList->getName());
						this->paint();
					}
				}
			} 
			else if (msg == CRCInput::RC_ok) {
				if (selectedChannelEntry)
					CNeutrinoApp::getInstance()->channelList->zapTo_ChannelID(selectedChannelEntry->channel->channel_id);
				current_bouquet = bouquetList->getActiveBouquetNumber();
			} 
			else if (CRCInput::isNumeric (msg)) {
				this->hide();
				CNeutrinoApp::getInstance()->channelList->numericZap (msg);
//printf("numericZap: prev bouquet %d new %d\n", current_bouquet, bouquetList->getActiveBouquetNumber());
				current_bouquet = bouquetList->getActiveBouquetNumber();
				this->channelList = bouquetList->Bouquets[current_bouquet]->channelList;
				this->channelListStartIndex = int (channelList->getSelectedChannelIndex() / maxNumberOfDisplayableEntries) * maxNumberOfDisplayableEntries;
				g_InfoViewer->killTitle();

				int selectedChannelEntryIndex = this->channelList->getSelectedChannelIndex();
				if (selectedChannelEntryIndex < this->channelList->getSize()) {
					this->hide();
					this->createChannelEntries (selectedChannelEntryIndex);

					this->header->paint(this->channelList->getName());
					this->footer->paintButtons (buttonLabels, sizeof (buttonLabels) / sizeof (button_label));
					this->paint();
				}

			} 
			else if (msg == CRCInput::RC_up) {
				int selectedChannelEntryIndex = this->selectedChannelEntry->index;
				int prevSelectedChannelEntryIndex = selectedChannelEntryIndex;

				--selectedChannelEntryIndex;
				if (selectedChannelEntryIndex < 0) {
					selectedChannelEntryIndex = this->channelList->getSize() - 1;
				}


				int oldChannelListStartIndex = this->channelListStartIndex;

				this->channelListStartIndex = (selectedChannelEntryIndex / this->maxNumberOfDisplayableEntries) * this->maxNumberOfDisplayableEntries;

				if (oldChannelListStartIndex != this->channelListStartIndex) {
					this->createChannelEntries (selectedChannelEntryIndex);
					this->paint();
				} else {
					this->selectedChannelEntry = this->displayedChannelEntries[selectedChannelEntryIndex - this->channelListStartIndex];
					this->paintChannelEntry (prevSelectedChannelEntryIndex - this->channelListStartIndex);
					this->paintChannelEntry (selectedChannelEntryIndex - this->channelListStartIndex);
				}
			} 
			else if (msg == CRCInput::RC_down) {
				int selectedChannelEntryIndex = this->selectedChannelEntry->index;
				int prevSelectedChannelEntryIndex = this->selectedChannelEntry->index;

				selectedChannelEntryIndex = (selectedChannelEntryIndex + 1) % this->channelList->getSize();


				int oldChannelListStartIndex = this->channelListStartIndex;
				this->channelListStartIndex = (selectedChannelEntryIndex / this->maxNumberOfDisplayableEntries) * this->maxNumberOfDisplayableEntries;

				if (oldChannelListStartIndex != this->channelListStartIndex) {
					this->createChannelEntries (selectedChannelEntryIndex);
					this->paint();
				} else {
					this->selectedChannelEntry = this->displayedChannelEntries[selectedChannelEntryIndex - this->channelListStartIndex];
					this->paintChannelEntry (prevSelectedChannelEntryIndex - this->channelListStartIndex);
					this->paintChannelEntry (this->selectedChannelEntry->index - this->channelListStartIndex);
				}
			} 
			else if (msg == (neutrino_msg_t) CRCInput::RC_red) {
				CMenuWidget menuWidgetActions(LOCALE_EPGPLUS_ACTIONS, NEUTRINO_ICON_FEATURES);
				menuWidgetActions.enableFade(false);
				MenuTargetAddRecordTimer record(this);
				MenuTargetRefreshEpg refresh(this);
				MenuTargetAddReminder remind(this);
				if (!g_settings.minimode)
					menuWidgetActions.addItem (new CMenuForwarder (LOCALE_EPGPLUS_RECORD, true, NULL, &record, NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED), false);
				menuWidgetActions.addItem (new CMenuForwarder (LOCALE_EPGPLUS_REFRESH_EPG, true, NULL, &refresh, NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN), false);
				menuWidgetActions.addItem (new CMenuForwarder (LOCALE_EPGPLUS_REMIND, true, NULL, &remind, NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW), false);
				if (selectedChannelEntry)
					menuWidgetActions.exec (NULL, "");

				this->refreshAll = true;
			} 
			else if (msg == (neutrino_msg_t) CRCInput::RC_blue) {

				CMenuWidget menuWidgetOptions(LOCALE_EPGPLUS_OPTIONS, NEUTRINO_ICON_FEATURES);
				menuWidgetOptions.enableFade(false);
				//menuWidgetOptions.addItem (new MenuOptionChooserSwitchSwapMode (this));
				menuWidgetOptions.addItem (new MenuOptionChooserSwitchViewMode (this));

				int result = menuWidgetOptions.exec (NULL, "");
				if (result == menu_return::RETURN_REPAINT) {
					this->refreshAll = true;
				} else if (result == menu_return::RETURN_EXIT_ALL) {
					this->refreshAll = true;
				}
			} 
			else if (msg == CRCInput::RC_left) {
				switch (this->currentViewMode) {
					case ViewMode_Stretch:
						{
							if (this->duration - 30 * 60 > 30 * 60) {
								this->duration -= 30 * 60;
								this->hide();
								this->refreshAll = true;
							}
						}
						break;
					case ViewMode_Scroll:
						{
							TCChannelEventEntries::const_iterator It = this->getSelectedEvent();

							if ((It != this->selectedChannelEntry->channelEventEntries.begin())
									&& (It != this->selectedChannelEntry->channelEventEntries.end())
							   ) {
								--It;
								this->selectedTime = (*It)->channelEvent.startTime + (*It)->channelEvent.duration / 2;
								if (this->selectedTime < this->startTime)
									this->selectedTime = this->startTime;

								this->selectedChannelEntry->paint (true, this->selectedTime);
							} else {
								if (this->startTime != this->firstStartTime) {

									if (this->startTime - this->duration > this->firstStartTime) {
										this->startTime -= this->duration;
									} else {
										this->startTime = this->firstStartTime;
									}

									this->selectedTime = this->startTime + this->duration - 1;	// select last event
									this->createChannelEntries (this->selectedChannelEntry->index);

									this->paint();
								}
							}
						}
						break;
				}
			} 
			else if (msg == CRCInput::RC_right) {
				switch (this->currentViewMode) {
					case ViewMode_Stretch:
						{
							if (this->duration + 30 * 60 < 4 * 60 * 60) {
								this->duration += 60 * 60;
								this->hide();
								this->refreshAll = true;
							}
						}
						break;

					case ViewMode_Scroll:
						{
							TCChannelEventEntries::const_iterator It = this->getSelectedEvent();

							if ((It != this->selectedChannelEntry->channelEventEntries.end() - 1)
									&& (It != this->selectedChannelEntry->channelEventEntries.end())) {
								++It;

								this->selectedTime = (*It)->channelEvent.startTime + (*It)->channelEvent.duration / 2;

								if (this->selectedTime > this->startTime + time_t (this->duration))
									this->selectedTime = this->startTime + this->duration;

								this->selectedChannelEntry->paint (true, this->selectedTime);
							} else {
								this->startTime += this->duration;
								this->selectedTime = this->startTime;
								this->createChannelEntries (this->selectedChannelEntry->index);

								this->paint();
							}
						}
						break;
				}
			} 
			else if (msg == CRCInput::RC_help || msg == CRCInput::RC_info) {
				TCChannelEventEntries::const_iterator It = this->getSelectedEvent();

				if (It != this->selectedChannelEntry->channelEventEntries.end()) {

					if ((*It)->channelEvent.eventID != 0) {
						this->hide();

						time_t startTime2 = (*It)->channelEvent.startTime;
						res = g_EpgData->show (this->selectedChannelEntry->channel->channel_id, (*It)->channelEvent.eventID, &startTime2);

						if (res == menu_return::RETURN_EXIT_ALL) {
							loop = false;
						} else {
							g_RCInput->getMsg (&msg, &data, 0);

							if ((msg != CRCInput::RC_red) && (msg != CRCInput::RC_timeout)) {
								// RC_red schlucken
								g_RCInput->postMsg (msg, data);
							}

							this->header->paint(this->channelList->getName());
							this->footer->paintButtons (buttonLabels, sizeof (buttonLabels) / sizeof (button_label));
							this->paint();
						}
					}
				}
			}
			else if (msg == CRCInput::RC_sat || msg == CRCInput::RC_favorites) {
				g_RCInput->postMsg (msg, 0);
				res = menu_return::RETURN_EXIT_ALL;
				loop = false;
			}
			else {
				if (CNeutrinoApp::getInstance()->handleMsg (msg, data) & messages_return::cancel_all) {
					loop = false;
					res = menu_return::RETURN_EXIT_ALL;
				}
			}

			if (this->refreshAll)
				loop = false;
			else if (this->refreshFooterButtons)
				this->footer->paintButtons (buttonLabels, sizeof (buttonLabels) / sizeof (button_label));
			frameBuffer->blit();
		}

		this->hide();

		fader.Stop();
#if 0
		for (TChannelEntries::iterator It = this->displayedChannelEntries.begin();
				It != this->displayedChannelEntries.end(); It++) {
			delete *It;
		}
		this->displayedChannelEntries.clear();
#endif
	}
	while (this->refreshAll);
	for (TChannelEntries::iterator It = this->displayedChannelEntries.begin();
			It != this->displayedChannelEntries.end(); ++It) {
		delete *It;
	}
	this->displayedChannelEntries.clear();

	return res;
}

EpgPlus::TCChannelEventEntries::const_iterator EpgPlus::getSelectedEvent() const
{
	for (TCChannelEventEntries::const_iterator It = this->selectedChannelEntry->channelEventEntries.begin();
			It != this->selectedChannelEntry->channelEventEntries.end();
			++It) {
		if ((*It)->isSelected (this->selectedTime)) {
			return It;
		}
	}
	return this->selectedChannelEntry->channelEventEntries.end();
}

void EpgPlus::hide()
{
	this->frameBuffer->paintBackgroundBoxRel (this->usableScreenX, this->usableScreenY, this->usableScreenWidth, this->usableScreenHeight);
	frameBuffer->blit();
}

void EpgPlus::paintChannelEntry (int position)
{
	ChannelEntry *channelEntry = this->displayedChannelEntries[position];

	bool currentChannelIsSelected = false;
	if (this->channelListStartIndex + position == this->selectedChannelEntry->index) {
		currentChannelIsSelected = true;
	}
	channelEntry->paint (currentChannelIsSelected, this->selectedTime);
}

std::string EpgPlus::getTimeString (const time_t & time, const std::string & format)
{
	char tmpstr[256];
	struct tm *tmStartTime = localtime (&time);

	strftime (tmpstr, sizeof (tmpstr), format.c_str(), tmStartTime);
	return tmpstr;
}

void EpgPlus::paint()
{
	// clear
	//this->frameBuffer->paintBackgroundBoxRel (this->channelsTableX, this->channelsTableY, this->usableScreenWidth, this->channelsTableHeight);
	this->frameBuffer->paintBoxRel (this->channelsTableX, this->channelsTableY, this->usableScreenWidth, this->channelsTableHeight, COL_MENUCONTENT_PLUS_0);

	// paint the gaps
	this->frameBuffer->paintBoxRel (this->horGap1X, this->horGap1Y, this->horGap1Width, horGap1Height, horGap1Color);
	this->frameBuffer->paintBoxRel (this->horGap2X, this->horGap2Y, this->horGap2Width, horGap2Height, horGap2Color);
	this->frameBuffer->paintBoxRel (this->verGap1X, this->verGap1Y, verGap1Width, this->verGap1Height, verGap1Color);
	this->frameBuffer->paintBoxRel (this->verGap2X, this->verGap2Y, verGap2Width, this->verGap2Height, verGap2Color);

	// paint the time line
	timeLine->paint (this->startTime, this->duration);

	// paint the channel entries
	for (int i = 0; i < (int) this->displayedChannelEntries.size(); ++i) {
		this->paintChannelEntry (i);
	}

	// paint the time line grid
	this->timeLine->paintGrid();

	// paint slider
	this->frameBuffer->paintBoxRel (this->sliderX, this->sliderY, this->sliderWidth, this->sliderHeight, COL_MENUCONTENT_PLUS_0);

	int tmp = ((this->channelList->getSize() - 1) / this->maxNumberOfDisplayableEntries) + 1;
	int sliderKnobPosition = this->selectedChannelEntry == NULL ? 0 : (this->selectedChannelEntry->index / this->maxNumberOfDisplayableEntries);

	this->frameBuffer->paintBoxRel(this->sliderX + 2, this->sliderY + sliderKnobPosition * (sliderHeight-4)/tmp
					, this->sliderWidth - 4, (sliderHeight-4)/tmp, COL_MENUCONTENT_PLUS_3);
}

//  -- EPG+ Menue Handler Class
//  -- to be used for calls from Menue
//  -- (2004-03-05 rasc)

int CEPGplusHandler::exec (CMenuTarget * parent, const std::string & /*actionKey*/)
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
	if(!bouquetList->Bouquets.empty() && !bouquetList->Bouquets[bnum]->channelList->isEmpty() )
		channelList = bouquetList->Bouquets[bnum]->channelList;
	else
		channelList = CNeutrinoApp::getInstance()->channelList;

	e->exec (channelList, channelList->getSelectedChannelIndex(), bouquetList);
	delete e;
//FIXME
//printf("CEPGplusHandler::exec old bouquet %d new %d current %d\n", bnum, bouquetList->getActiveBouquetNumber(), current_bouquet);
	bouquetList->activateBouquet(current_bouquet, false);

	return res;
}

EpgPlus::MenuTargetAddReminder::MenuTargetAddReminder (EpgPlus * pepgPlus) {
	this->epgPlus = pepgPlus;
}

int EpgPlus::MenuTargetAddReminder::exec (CMenuTarget * /*parent*/, const std::string & /*actionKey*/)
{
	TCChannelEventEntries::const_iterator It = this->epgPlus->getSelectedEvent();

	if ((It != this->epgPlus->selectedChannelEntry->channelEventEntries.end())
			&& (!(*It)->channelEvent.description.empty())
	   ) {
		if (g_Timerd->isTimerdAvailable()) {
			g_Timerd->addZaptoTimerEvent (this->epgPlus->selectedChannelEntry->channel->channel_id, (*It)->channelEvent.startTime - (g_settings.zapto_pre_time * 60), (*It)->channelEvent.startTime - ANNOUNCETIME - (g_settings.zapto_pre_time * 60), 0, (*It)->channelEvent.eventID, (*It)->channelEvent.startTime, 0);

			ShowMsgUTF (LOCALE_TIMER_EVENTTIMED_TITLE, g_Locale->getText (LOCALE_TIMER_EVENTTIMED_MSG)
				    , CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO);	// UTF-8
		} else
			printf ("timerd not available\n");
	}
	return menu_return::RETURN_EXIT_ALL;
}

EpgPlus::MenuTargetAddRecordTimer::MenuTargetAddRecordTimer (EpgPlus * pepgPlus) {
	this->epgPlus = pepgPlus;
}

int EpgPlus::MenuTargetAddRecordTimer::exec (CMenuTarget * /*parent*/, const std::string & /*actionKey*/)
{
	TCChannelEventEntries::const_iterator It = this->epgPlus->getSelectedEvent();

	if ((It != this->epgPlus->selectedChannelEntry->channelEventEntries.end())
			&& (!(*It)->channelEvent.description.empty())
	   ) {
		if (g_Timerd->isTimerdAvailable()) {

			g_Timerd->addRecordTimerEvent (this->epgPlus->selectedChannelEntry->channel->channel_id, (*It)->channelEvent.startTime, (*It)->channelEvent.startTime + (*It)->channelEvent.duration, (*It)->channelEvent.eventID, (*It)->channelEvent.startTime, (*It)->channelEvent.startTime - (ANNOUNCETIME + 120)
						       , TIMERD_APIDS_CONF, true);
			ShowMsgUTF (LOCALE_TIMER_EVENTRECORD_TITLE, g_Locale->getText (LOCALE_TIMER_EVENTRECORD_MSG)
				    , CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO);	// UTF-8
		} else
			printf ("timerd not available\n");
	}
	return menu_return::RETURN_EXIT_ALL;
}

EpgPlus::MenuTargetRefreshEpg::MenuTargetRefreshEpg (EpgPlus * pepgPlus) {
	this->epgPlus = pepgPlus;
}

int EpgPlus::MenuTargetRefreshEpg::exec (CMenuTarget * /*parent*/, const std::string & /*actionKey*/)
{
	this->epgPlus->refreshAll = true;
	return menu_return::RETURN_EXIT_ALL;
}

struct CMenuOptionChooser::keyval menuOptionChooserSwitchSwapModes[] = {
	{ EpgPlus::SwapMode_ByPage, LOCALE_EPGPLUS_BYPAGE_MODE },
	{ EpgPlus::SwapMode_ByBouquet, LOCALE_EPGPLUS_BYBOUQUET_MODE }
};

EpgPlus::MenuOptionChooserSwitchSwapMode::MenuOptionChooserSwitchSwapMode (EpgPlus * pepgPlus)
		:CMenuOptionChooser (LOCALE_EPGPLUS_SWAP_MODE, (int *) &pepgPlus->currentSwapMode, menuOptionChooserSwitchSwapModes, sizeof (menuOptionChooserSwitchSwapModes) / sizeof (CMenuOptionChooser::keyval)
				     , true, NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW,0)
{
	this->epgPlus = pepgPlus;
	this->oldSwapMode = epgPlus->currentSwapMode;
	this->oldTimingMenuSettings = g_settings.timing[SNeutrinoSettings::TIMING_MENU];
}

EpgPlus::MenuOptionChooserSwitchSwapMode::~MenuOptionChooserSwitchSwapMode()
{
	g_settings.timing[SNeutrinoSettings::TIMING_MENU] = this->oldTimingMenuSettings;

	if (this->epgPlus->currentSwapMode != this->oldSwapMode) {
		switch (this->epgPlus->currentSwapMode) {
		case SwapMode_ByPage:
			buttonLabels[1].locale = LOCALE_EPGPLUS_PAGE_DOWN;
			buttonLabels[2].locale = LOCALE_EPGPLUS_PAGE_UP;
			break;
		case SwapMode_ByBouquet:
			buttonLabels[1].locale = LOCALE_EPGPLUS_PREV_BOUQUET;
			buttonLabels[2].locale = LOCALE_EPGPLUS_NEXT_BOUQUET;
			break;
		}
		this->epgPlus->refreshAll = true;
	}
}

int EpgPlus::MenuOptionChooserSwitchSwapMode::exec (CMenuTarget * parent)
{
	// change time out settings temporary
	g_settings.timing[SNeutrinoSettings::TIMING_MENU] = 1;

	CMenuOptionChooser::exec (parent);

	return menu_return::RETURN_REPAINT;
}

struct CMenuOptionChooser::keyval menuOptionChooserSwitchViewModes[] = {
	{ EpgPlus::ViewMode_Scroll, LOCALE_EPGPLUS_STRETCH_MODE },
	{ EpgPlus::ViewMode_Stretch, LOCALE_EPGPLUS_SCROLL_MODE }
};

EpgPlus::MenuOptionChooserSwitchViewMode::MenuOptionChooserSwitchViewMode (EpgPlus * epgPlus)
		:CMenuOptionChooser (LOCALE_EPGPLUS_VIEW_MODE, (int *) &epgPlus->currentViewMode, menuOptionChooserSwitchViewModes, sizeof (menuOptionChooserSwitchViewModes) / sizeof (CMenuOptionChooser::keyval)
				     , true, NULL, CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE) {
	this->oldTimingMenuSettings = g_settings.timing[SNeutrinoSettings::TIMING_MENU];
}

EpgPlus::MenuOptionChooserSwitchViewMode::~MenuOptionChooserSwitchViewMode()
{
	g_settings.timing[SNeutrinoSettings::TIMING_MENU] = this->oldTimingMenuSettings;
}

int EpgPlus::MenuOptionChooserSwitchViewMode::exec (CMenuTarget * parent)
{
	// change time out settings temporary
	g_settings.timing[SNeutrinoSettings::TIMING_MENU] = 1;

	CMenuOptionChooser::exec (parent);

	return menu_return::RETURN_REPAINT;
}
