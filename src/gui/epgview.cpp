/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Copyright (C) 2007-2014 Stefan Seyfried

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

#include <algorithm>
#include <gui/adzap.h>
#include <gui/epgview.h>
#include <gui/eventlist.h>

#include <gui/widget/buttons.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/icons.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/mountchooser.h>
#include <gui/components/cc.h>
#include <gui/timerlist.h>
#include <zapit/zapit.h>
#include <system/helpers.h>

#include <global.h>
#include <neutrino.h>

#include <driver/screen_max.h>
#include <driver/fade.h>
#include <gui/filebrowser.h>
#include <gui/followscreenings.h>
#include <gui/moviebrowser/mb.h>
#include <gui/movieplayer.h>
#include <gui/pictureviewer.h>
#include <gui/tmdb.h>
#include <driver/record.h>
#include <driver/fontrenderer.h>

#include <zapit/bouquets.h>
#include <zapit/getservices.h>
#include <eitd/sectionsd.h>
#include <timerdclient/timerdclient.h>

extern CPictureViewer * g_PicViewer;

#define ICON_LARGE_WIDTH 26

int findItem(std::string strItem, std::vector<std::string> & vecItems) {
	for (std::vector<std::string>::size_type nCnt = 0; nCnt < vecItems.size(); nCnt++) {
		std::string strThisItem = vecItems[nCnt];
		if (strItem == strThisItem) {
			return nCnt;
		}
	}
	return -1;
}

// 21.07.2005 - rainerk
// Merge multiple extended event strings into one description and localize the label
// Examples:
//   Actor1-ActorX      -> Darsteller 1, 2, 3
//   Year of production -> Produktionsjahr
//   Director           -> Regisseur
//   Guests             -> Gäste
void reformatExtendedEvents(std::string strItem, std::string strLabel, bool bUseRange, CEPGData & epgdata) {
	std::vector<std::string> & vecDescriptions = epgdata.itemDescriptions;
	std::vector<std::string> & vecItems = epgdata.items;
	// Merge multiple events into 1 (Actor1-)
	if (bUseRange) {
		bool bHasItems = false;
		char index[3];
		// Maximum of 10 items should suffice
		for (int nItem = 1; nItem < 11; nItem++) {
			sprintf(index, "%d", nItem);
			// Look for matching items
			int nPos = findItem(std::string(strItem) + std::string(index), vecDescriptions);
			if (-1 != nPos) {
				std::string item = std::string(vecItems[nPos]);
				vecDescriptions.erase(vecDescriptions.begin() + nPos);
				vecItems.erase(vecItems.begin() + nPos);
				if (false == bHasItems) {
					// First item added, so create new label item
					vecDescriptions.push_back(strLabel);
					vecItems.push_back(item + ", ");
					bHasItems = true;
				} else {
					vecItems.back().append(item).append(", ");
				}
			}
		}
		// Remove superfluous ", "
		if (bHasItems) {
			vecItems.back().resize(vecItems.back().length() - 2);
		}
	} else {	// Single string event (e.g. Director)
		// Look for matching items
		int nPos = findItem(strItem, vecDescriptions);
		if (-1 != nPos) {
			vecDescriptions[nPos] = strLabel;
		}
	}
}

CEpgData::CEpgData()
{
	bigFonts 	= false;
	frameBuffer 	= CFrameBuffer::getInstance();
	tmdb_active 	= false;
	mp_movie_info 	= NULL;
	header     	= NULL;
	Bottombox 	= NULL;
	pb		= NULL;
	font_title	= NULL;
}

CEpgData::~CEpgData()
{
	ResetModules();
}

void CEpgData::start()
{
	ox = frameBuffer->getScreenWidthRel(bigFonts ? false /* big */ : true /* small */);
	oy = frameBuffer->getScreenHeightRel(bigFonts ? false /* big */ : true /* small */);

	font_title   = g_Font[SNeutrinoSettings::FONT_TYPE_EPG_TITLE];
	topheight    = font_title->getHeight();
	topboxheight = topheight + 6;
	botboxheight = g_Font[SNeutrinoSettings::FONT_TYPE_EPG_DATE]->getHeight() + 6;
	buttonheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_FOOT]->getHeight() + 6;
	if (buttonheight < 30)
		buttonheight = 30; // the buttons and icons need space
	oy-=buttonheight/2;
	/* this is the text box height - and the height of the scroll bar */
	sb = oy - topboxheight - botboxheight - buttonheight;
	/* button box is handled separately (why?) */
	oy -= buttonheight;
	medlineheight = g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->getHeight();
	medlinecount  = sb / medlineheight;
	toph = topboxheight;

	sx = getScreenStartX(ox);
	sy = getScreenStartY(oy + buttonheight); /* button box is handled separately (why?) */
}

void CEpgData::addTextToArray(const std::string & text, int screening) // UTF-8
{
	//printf("line: >%s<\n", text.c_str() );
	if (text==" ")
	{
		emptyLineCount ++;
		if (emptyLineCount<2)
		{
			epgText.push_back(epg_pair(text,screening));
		}
	}
	else
	{
		emptyLineCount = 0;
		epgText.push_back(epg_pair(text,screening));
	}
}

void CEpgData::processTextToArray(std::string text, int screening, bool has_cover) // UTF-8
{
	std::string	aktLine = "";
	std::string	aktWord = "";
	int	aktWidth = 0;
	text += ' ';
	char* text_= (char*) text.c_str();

	while (*text_!=0)
	{
		if ( (*text_==' ') || (*text_=='\n') || (*text_=='-') || (*text_=='.') )
		{
			// Houdini: if there is a newline (especially in the Premiere Portal EPGs) do not forget to add aktWord to aktLine
			// after width check, if width check failes do newline, add aktWord to next line
			// and "reinsert" i.e. reloop for the \n
			if (*text_!='\n')
				aktWord += *text_;

			// check the wordwidth - add to this line if size ok
			int aktWordWidth = g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->getRenderWidth(aktWord);
			if ((aktWordWidth+aktWidth)<(ox - 2*OFFSET_INNER_MID - SCROLLBAR_WIDTH - (has_cover ? ((ox/4)+OFFSET_INNER_MID) : 0)))
			{//space ok, add
				aktWidth += aktWordWidth;
				aktLine += aktWord;

				if (*text_=='\n')
				{	//enter-handler
					addTextToArray( aktLine, screening );
					aktLine = "";
					aktWidth= 0;
				}
				aktWord = "";
			}
			else
			{//new line needed
				addTextToArray( aktLine, screening);
				aktLine = aktWord;
				aktWidth = aktWordWidth;
				aktWord = "";
				// Houdini: in this case where we skipped \n and space is too low, exec newline and rescan \n
				// otherwise next word comes direct after aktLine
				if (*text_=='\n')
					continue;
			}
		}
		else
		{
			aktWord += *text_;
		}
		text_++;
	}
	//add the rest
	addTextToArray( aktLine + aktWord, screening );
}

void CEpgData::showText(int startPos, int ypos, bool has_cover, bool fullClear)
{
	// recalculate
	medlineheight = g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->getHeight();
	medlinecount = sb / medlineheight;

	std::string cover = "/tmp/tmdb.jpg"; //todo: maybe add a getCover()-function to tmdb class
	int cover_max_width = ox/4; //25%
	int cover_max_height = sb-(2*OFFSET_INNER_MID);
	int cover_width = 0;
	int cover_height = 0;
	int cover_offset = 0;

	if (has_cover)
	{
		g_PicViewer->getSize(cover.c_str(), &cover_width, &cover_height);
		if (cover_width && cover_height)
		{
			g_PicViewer->rescaleImageDimensions(&cover_width, &cover_height, cover_max_width, cover_max_height);
			cover_offset = cover_width + OFFSET_INNER_MID;
		}
	}

	int textSize = epgText.size();
	int y=ypos;
	const char tok = ' ';
	int offset = 0, count = 0;
	int max_mon_w = 0, max_wday_w = 0;
	int digi = g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->getRenderWidth("29..");
	for(int i = 0; i < 12;i++){
		max_mon_w = std::max(max_mon_w, g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->getRenderWidth(std::string(g_Locale->getText(CLocaleManager::getMonth(i))) + " "));
		if(i > 6)
			continue;
		max_wday_w = std::max(max_wday_w, g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->getRenderWidth(std::string(g_Locale->getText(CLocaleManager::getWeekday(i))) + " "));
	}
	int offs = fullClear ? 0 : cover_offset;
	frameBuffer->paintBoxRel(sx+offs, y, ox-SCROLLBAR_WIDTH-offs, sb, COL_MENUCONTENT_PLUS_0); // background of the text box

	if (has_cover) {
		if (!g_PicViewer->DisplayImage(cover ,sx+OFFSET_INNER_MID ,y+OFFSET_INNER_MID+((sb-cover_height)/2), cover_width, cover_height, CFrameBuffer::TM_NONE)) {
			cover_offset = 0;
			frameBuffer->paintBoxRel(sx, y, ox-SCROLLBAR_WIDTH, sb, COL_MENUCONTENT_PLUS_0); // background of the text box
		}
	}
	int logo_offset = 0;
	int icon_w = 0;
	int icon_h = 0;
	if (tmdb_active && startPos == 0)
	{
		frameBuffer->getIconSize(NEUTRINO_ICON_TMDB, &icon_w, &icon_h);
		frameBuffer->paintIcon(NEUTRINO_ICON_TMDB, sx+OFFSET_INNER_MID+cover_offset, y+(medlineheight-icon_h)/2);
		logo_offset = icon_w + OFFSET_INNER_MID;
	}
	if (stars > 0 && startPos == 0)
	{
		frameBuffer->getIconSize(NEUTRINO_ICON_STAR_OFF, &icon_w, &icon_h);
		for (int i = 0; i < 10; i++)
			frameBuffer->paintIcon(NEUTRINO_ICON_STAR_OFF, sx+10+cover_offset+logo_offset + i*(icon_w+3), y+(medlineheight-icon_h)/2);
		for (int i = 0; i < stars; i++)
			frameBuffer->paintIcon(NEUTRINO_ICON_STAR_ON, sx+10+cover_offset+logo_offset + i*(icon_w+3), y+(medlineheight-icon_h)/2);
	}
	for (int i = startPos; i < textSize && i < startPos + medlinecount; i++, y += medlineheight)
	{
		if(epgText[i].second){
			std::string::size_type pos1 = epgText[i].first.find_first_not_of(tok, 0);
			std::string::size_type pos2 = epgText[i].first.find_first_of(tok, pos1);
			while( pos2 != std::string::npos || pos1 != std::string::npos ){
				switch(count){
					case 1:
					offset += max_wday_w;
					break;
					case 3:
					offset += max_mon_w;
					break;
					default:
					offset += digi;
					break;
				}
				g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->RenderString(sx+OFFSET_INNER_MID+offset, y+medlineheight, ox - SCROLLBAR_WIDTH - 2*OFFSET_INNER_MID - offset, epgText[i].first.substr(pos1, pos2 - pos1), (epgText[i].second==2)? COL_MENUCONTENTINACTIVE_TEXT: COL_MENUCONTENT_TEXT);
				count++;
				pos1 = epgText[i].first.find_first_not_of(tok, pos2);
				pos2 = epgText[i].first.find_first_of(tok, pos1);
			}
			offset = 0;
			count = 0;
		}
		else{
			g_Font[( i< info1_lines ) ?SNeutrinoSettings::FONT_TYPE_EPG_INFO1:SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->RenderString(sx+OFFSET_INNER_MID+cover_offset, y+medlineheight, ox - SCROLLBAR_WIDTH - 2*OFFSET_INNER_MID - cover_offset, epgText[i].first, COL_MENUCONTENT_TEXT);
		}
	}

	int total_pages;
	int current_page;
	getScrollBarData(&total_pages, &current_page, textSize, medlinecount, startPos + 1);
	paintScrollBar(sx + ox - SCROLLBAR_WIDTH, ypos, SCROLLBAR_WIDTH, sb, total_pages, current_page);
}

#define GENRE_MOVIE_COUNT 9
const neutrino_locale_t genre_movie[GENRE_MOVIE_COUNT] =
{
	LOCALE_GENRE_MOVIE_0,
	LOCALE_GENRE_MOVIE_1,
	LOCALE_GENRE_MOVIE_2,
	LOCALE_GENRE_MOVIE_3,
	LOCALE_GENRE_MOVIE_4,
	LOCALE_GENRE_MOVIE_5,
	LOCALE_GENRE_MOVIE_6,
	LOCALE_GENRE_MOVIE_7,
	LOCALE_GENRE_MOVIE_8
};
#define GENRE_NEWS_COUNT 5
const neutrino_locale_t genre_news[GENRE_NEWS_COUNT] =
{
	LOCALE_GENRE_NEWS_0,
	LOCALE_GENRE_NEWS_1,
	LOCALE_GENRE_NEWS_2,
	LOCALE_GENRE_NEWS_3,
	LOCALE_GENRE_NEWS_4
};
#define GENRE_SHOW_COUNT 4
const neutrino_locale_t genre_show[GENRE_SHOW_COUNT] =
{
	LOCALE_GENRE_SHOW_0,
	LOCALE_GENRE_SHOW_1,
	LOCALE_GENRE_SHOW_2,
	LOCALE_GENRE_SHOW_3
};
#define GENRE_SPORTS_COUNT 12
const neutrino_locale_t genre_sports[GENRE_SPORTS_COUNT] =
{
	LOCALE_GENRE_SPORTS_0,
	LOCALE_GENRE_SPORTS_1,
	LOCALE_GENRE_SPORTS_2,
	LOCALE_GENRE_SPORTS_3,
	LOCALE_GENRE_SPORTS_4,
	LOCALE_GENRE_SPORTS_5,
	LOCALE_GENRE_SPORTS_6,
	LOCALE_GENRE_SPORTS_7,
	LOCALE_GENRE_SPORTS_8,
	LOCALE_GENRE_SPORTS_9,
	LOCALE_GENRE_SPORTS_10,
	LOCALE_GENRE_SPORTS_11
};
#define GENRE_CHILDRENS_PROGRAMMES_COUNT 6
const neutrino_locale_t genre_childrens_programmes[GENRE_CHILDRENS_PROGRAMMES_COUNT] =
{
	LOCALE_GENRE_CHILDRENS_PROGRAMMES_0,
	LOCALE_GENRE_CHILDRENS_PROGRAMMES_1,
	LOCALE_GENRE_CHILDRENS_PROGRAMMES_2,
	LOCALE_GENRE_CHILDRENS_PROGRAMMES_3,
	LOCALE_GENRE_CHILDRENS_PROGRAMMES_4,
	LOCALE_GENRE_CHILDRENS_PROGRAMMES_5
};
#define GENRE_MUSIC_DANCE_COUNT 7
const neutrino_locale_t genre_music_dance[GENRE_MUSIC_DANCE_COUNT] =
{
	LOCALE_GENRE_MUSIC_DANCE_0,
	LOCALE_GENRE_MUSIC_DANCE_1,
	LOCALE_GENRE_MUSIC_DANCE_2,
	LOCALE_GENRE_MUSIC_DANCE_3,
	LOCALE_GENRE_MUSIC_DANCE_4,
	LOCALE_GENRE_MUSIC_DANCE_5,
	LOCALE_GENRE_MUSIC_DANCE_6
};
#define GENRE_ARTS_COUNT 12
const neutrino_locale_t genre_arts_dance[GENRE_ARTS_COUNT] =
{
	LOCALE_GENRE_ARTS_0,
	LOCALE_GENRE_ARTS_1,
	LOCALE_GENRE_ARTS_2,
	LOCALE_GENRE_ARTS_3,
	LOCALE_GENRE_ARTS_4,
	LOCALE_GENRE_ARTS_5,
	LOCALE_GENRE_ARTS_6,
	LOCALE_GENRE_ARTS_7,
	LOCALE_GENRE_ARTS_8,
	LOCALE_GENRE_ARTS_9,
	LOCALE_GENRE_ARTS_10,
	LOCALE_GENRE_ARTS_11
};
#define GENRE_SOCIAL_POLITICAL_COUNT 4
const neutrino_locale_t genre_social_political[GENRE_SOCIAL_POLITICAL_COUNT] =
{
	LOCALE_GENRE_SOCIAL_POLITICAL_0,
	LOCALE_GENRE_SOCIAL_POLITICAL_1,
	LOCALE_GENRE_SOCIAL_POLITICAL_2,
	LOCALE_GENRE_SOCIAL_POLITICAL_3
};
#define GENRE_DOCUS_MAGAZINES_COUNT 8
const neutrino_locale_t genre_docus_magazines[GENRE_DOCUS_MAGAZINES_COUNT] =
{
	LOCALE_GENRE_DOCUS_MAGAZINES_0,
	LOCALE_GENRE_DOCUS_MAGAZINES_1,
	LOCALE_GENRE_DOCUS_MAGAZINES_2,
	LOCALE_GENRE_DOCUS_MAGAZINES_3,
	LOCALE_GENRE_DOCUS_MAGAZINES_4,
	LOCALE_GENRE_DOCUS_MAGAZINES_5,
	LOCALE_GENRE_DOCUS_MAGAZINES_6,
	LOCALE_GENRE_DOCUS_MAGAZINES_7
};
#define GENRE_TRAVEL_HOBBIES_COUNT 8
const neutrino_locale_t genre_travel_hobbies[GENRE_TRAVEL_HOBBIES_COUNT] =
{
	LOCALE_GENRE_TRAVEL_HOBBIES_0,
	LOCALE_GENRE_TRAVEL_HOBBIES_1,
	LOCALE_GENRE_TRAVEL_HOBBIES_2,
	LOCALE_GENRE_TRAVEL_HOBBIES_3,
	LOCALE_GENRE_TRAVEL_HOBBIES_4,
	LOCALE_GENRE_TRAVEL_HOBBIES_5,
	LOCALE_GENRE_TRAVEL_HOBBIES_6,
	LOCALE_GENRE_TRAVEL_HOBBIES_7
};
const unsigned char genre_sub_classes[10] =
{
	GENRE_MOVIE_COUNT,
	GENRE_NEWS_COUNT,
	GENRE_SHOW_COUNT,
	GENRE_SPORTS_COUNT,
	GENRE_CHILDRENS_PROGRAMMES_COUNT,
	GENRE_MUSIC_DANCE_COUNT,
	GENRE_ARTS_COUNT,
	GENRE_SOCIAL_POLITICAL_COUNT,
	GENRE_DOCUS_MAGAZINES_COUNT,
	GENRE_TRAVEL_HOBBIES_COUNT
};
const neutrino_locale_t * genre_sub_classes_list[10] =
{
	genre_movie,
	genre_news,
	genre_show,
	genre_sports,
	genre_childrens_programmes,
	genre_music_dance,
	genre_arts_dance,
	genre_social_political,
	genre_docus_magazines,
	genre_travel_hobbies
};

const char * GetGenre(const unsigned char contentClassification) // UTF-8
{
	neutrino_locale_t res;

	//unsigned char i = (contentClassification & 0x0F0);
	int i = (contentClassification & 0xF0);
//printf("GetGenre: contentClassification %X i %X bool %s\n", contentClassification, i, i < 176 ? "yes" : "no"); fflush(stdout);
	if ((i >= 0x010) && (i < 0x0B0))
	{
		i >>= 4;
		i--;
//printf("GetGenre: i %d content %d\n", i, contentClassification & 0x0F); fflush(stdout);
		res = genre_sub_classes_list[i][((contentClassification & 0x0F) < genre_sub_classes[i]) ? (contentClassification & 0x0F) : 0];
	}
	else
		res = LOCALE_GENRE_UNKNOWN;
	return g_Locale->getText(res);
}

static bool sortByDateTime (const CChannelEvent& a, const CChannelEvent& b)
{
	return a.startTime< b.startTime;
}
bool CEpgData::isCurrentEPG(const t_channel_id channel_id)
{
	t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
	if(( epg_done!= -1 ) && live_channel_id == channel_id){
			return true;
	}
	return false;
}

int CEpgData::show_mp(MI_MOVIE_INFO *mi, int mp_position, int mp_duration, bool doLoop)
{
	int res = menu_return::RETURN_REPAINT;

	mp_movie_info = mi;
	if (mp_movie_info == NULL)
		return res;

	if (mp_movie_info->epgTitle.empty()) /* no epg info found */
	{
		ShowHint(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_EPGVIEWER_NOTFOUND)); // UTF-8
		hide();
		return res;
	}
	epgText.clear();
	epgData.title = mp_movie_info->epgTitle;
	epgData.info1 = mp_movie_info->epgInfo1;
	epgData.info2 = mp_movie_info->epgInfo2;

	epgData.itemDescriptions.clear();
	epgData.items.clear();
	epgData.fsk = mp_movie_info->parentalLockAge;
	epgData.table_id = mp_movie_info->epgId;
#ifdef FULL_CONTENT_CLASSIFICATION
	epgData.contentClassification.clear();
#else
	epgData.contentClassification = 0;
#endif
	epgData.epg_times.dauer = mp_movie_info->length * 60; // we need the seconds

	extMovieInfo.clear();
	if ( !mp_movie_info->productionCountry.empty() || mp_movie_info->productionDate != 0)
	{
		extMovieInfo += mp_movie_info->productionCountry;
		extMovieInfo += " ";
		extMovieInfo += to_string(mp_movie_info->productionDate);
		extMovieInfo += "\n";
	}
	if (!mp_movie_info->serieName.empty())
	{
		extMovieInfo += "\n";
		extMovieInfo += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_SERIE);
		extMovieInfo += ": ";
		extMovieInfo += mp_movie_info->serieName;
		extMovieInfo += "\n";
	}
	if (!mp_movie_info->channelName.empty())
	{
		extMovieInfo += "\n";
		extMovieInfo += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_CHANNEL);
		extMovieInfo += ": ";
		extMovieInfo += mp_movie_info->channelName;
		extMovieInfo += "\n";
	}
	if (mp_movie_info->rating != 0)
	{
		extMovieInfo += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_RATING);
		extMovieInfo += ": ";
		extMovieInfo += to_string(mp_movie_info->rating / 10);
		extMovieInfo += ",";
		extMovieInfo += to_string(mp_movie_info->rating % 10);
		extMovieInfo += "/10";
		extMovieInfo += "\n";
	}
	if (mp_movie_info->quality != 0)
	{
		extMovieInfo += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_QUALITY);
		extMovieInfo += ": ";
		extMovieInfo += to_string(mp_movie_info->quality);
		extMovieInfo += "\n";
	}
	if (mp_movie_info->parentalLockAge != 0)
	{
		extMovieInfo += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_PARENTAL_LOCKAGE);
		extMovieInfo += ": ";
		extMovieInfo += to_string(mp_movie_info->parentalLockAge);
		extMovieInfo += " ";
		extMovieInfo += g_Locale->getText(LOCALE_UNIT_LONG_YEARS);
		extMovieInfo += "\n";
	}
	if (!mp_movie_info->audioPids.empty())
	{
		extMovieInfo += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_AUDIO);
		extMovieInfo += ": ";
		for (unsigned int i = 0; i < mp_movie_info->audioPids.size(); i++)
		{
			if (i)
				extMovieInfo += ", ";
			extMovieInfo += mp_movie_info->audioPids[i].AudioPidName;
		}
		extMovieInfo += "\n";
	}
	if (mp_movie_info->genreMajor != 0)
	{
		neutrino_locale_t locale_genre;
		unsigned char i = (mp_movie_info->genreMajor & 0x0F0);
		if (i >= 0x010 && i < 0x0B0)
		{
			i >>= 4;
			i--;
			locale_genre = genre_sub_classes_list[i][((mp_movie_info->genreMajor & 0x0F) < genre_sub_classes[i]) ? (mp_movie_info->genreMajor & 0x0F) : 0];
		}
		else
			locale_genre = LOCALE_GENRE_UNKNOWN;
		extMovieInfo += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_GENRE_MAJOR);
		extMovieInfo += ": ";
		extMovieInfo += g_Locale->getText(locale_genre);
		extMovieInfo += "\n";
	}

	extMovieInfo += "\n";

	tm *date_tm = localtime(&mp_movie_info->dateOfLastPlay);
	extMovieInfo += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_PREVPLAYDATE);
	extMovieInfo += ": ";
	extMovieInfo += strftime("%F", date_tm);
	extMovieInfo += "\n";

	date_tm = localtime(&mp_movie_info->file.Time);
	extMovieInfo += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_RECORDDATE);
	extMovieInfo += ": ";
	extMovieInfo += strftime("%F", date_tm);
	extMovieInfo += "\n";

	extMovieInfo += "\n";

	if (mp_movie_info->file.Size != 0)
	{
		extMovieInfo += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_SIZE);
		extMovieInfo += ": ";
		extMovieInfo += to_string(mp_movie_info->file.Size >> 20);
		extMovieInfo += "\n";
	}

	extMovieInfo += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_PATH);
	extMovieInfo += ": ";
	extMovieInfo += mp_movie_info->file.getPath();
	extMovieInfo += "\n";

	extMovieInfo += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_FILE);
	extMovieInfo += ": ";
	extMovieInfo += mp_movie_info->file.getFileName();
	extMovieInfo += "\n";

	// this calculation is taken from timeosd.cpp
	epg_done = (mp_duration && mp_duration > 100) ? (mp_position * 100 / mp_duration) : -1;
	if (epg_done > 100)
		epg_done = 100;
	//printf("[%s:%d] epg_done: %d\n", __func__, __LINE__, epg_done);

	res = show(mp_movie_info->epgId >> 16, 0, 0, doLoop, false, true);
	if(!epgTextSwitch.empty())
	{
		mp_movie_info->epgInfo2 = epgTextSwitch;
		CMovieInfo m_movieInfo;
		//printf("#####[%s:%d] saveMovieInfo\n", __func__, __LINE__);
		m_movieInfo.saveMovieInfo(*mp_movie_info);
	}
	return res;
}

int CEpgData::show(const t_channel_id channel_id, uint64_t a_id, time_t* a_startzeit, bool doLoop, bool callFromfollowlist, bool mp_info )
{
	int res = menu_return::RETURN_REPAINT;
	static uint64_t id = 0;
	static time_t startzeit = 0;
	call_fromfollowlist = callFromfollowlist;
	if (a_startzeit)
		startzeit=*a_startzeit;
	id=a_id;

	tmdb_active = false;
	stars = 0;

	t_channel_id epg_id = channel_id;
	CZapitChannel * channel = CServiceManager::getInstance()->FindChannel(channel_id);
	if (channel)
		epg_id = channel->getEpgID();
	if (!mp_info)
		GetEPGData(epg_id, id, &startzeit);

	epgTextSwitch.clear();
	if (!mp_info)
		extMovieInfo.clear();
	if (doLoop)
	{
		if (!bigFonts && g_settings.bigFonts) {
			g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->setSize((int)(g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->getSize() * BIGFONT_FACTOR));
			g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->setSize((int)(g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->getSize() * BIGFONT_FACTOR));
		}
		bigFonts = g_settings.bigFonts;
		start();
		CEitManager::getInstance()->getEventsServiceKey(epg_id, evtlist);
		// Houdini added for Private Premiere EPG start sorted by start date/time 2005-08-15
		sort(evtlist.begin(),evtlist.end(),sortByDateTime);
	}

	if (epgData.title.empty()) /* no epg info found */
	{
		ShowHint(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_EPGVIEWER_NOTFOUND)); // UTF-8
		hide();
		return res;
	}

	const int pic_h = 39;
	toph = std::max(toph, pic_h);

	std::string lname;
	std::string channel_name;
	if (mp_info)
		channel_name = mp_movie_info->channelName;
	else
		channel_name = g_Zapit->getChannelName(channel_id);

	if (!topboxheight)
		start();

	sb = oy - toph - botboxheight;

	// 21.07.2005 - rainerk
	// Only show info1 if it's not included in info2!
	std::string strEpisode = "";	// Episode title in case info1 gets stripped
	if (!epgData.info1.empty()) {
		bool bHide = false;
		if (false == epgData.info2.empty()) {
			// Look for the first . in info1, usually this is the title of an episode.
			std::string::size_type nPosDot = epgData.info1.find('.');
			if (std::string::npos != nPosDot) {
				nPosDot += 2; // Skip dot and first blank
				/*      Houdini: changed for safty reason (crashes with some events at WDR regional)
				                        if (nPosDot < epgData.info2.length()) {   // Make sure we don't overrun the buffer
				*/
				if (nPosDot < epgData.info2.length() && nPosDot < epgData.info1.length()) {   // Make sure we don't overrun the buffer

					// Check if the stuff after the dot equals the beginning of info2
					if (0 == epgData.info2.find(epgData.info1.substr(nPosDot, epgData.info1.length() - nPosDot))) {
						strEpisode = epgData.info1.substr(0, nPosDot) + "\n";
						bHide = true;
					}
				}
			}
			// Compare strings normally if not positively found to be equal before
			if (false == bHide && 0 == epgData.info2.find(epgData.info1)) {
				bHide = true;
			}
		}
		if (false == bHide) {
			processTextToArray(epgData.info1);
		}
	}

	info1_lines = epgText.size();

	//scan epg-data - sort to list
	if ((epgData.info2.empty()) && (info1_lines == 0))
		processTextToArray(g_Locale->getText(LOCALE_EPGVIEWER_NODETAILED)); // UTF-8
	else
		processTextToArray(strEpisode + epgData.info2);

	// Add a blank line
	processTextToArray("");

	// 21.07.2005 - rainerk
	// Show extended information
	if ( !epgData.itemDescriptions.empty() && !epgData.items.empty()) {
		char line[256];
		std::vector<std::string>::iterator description;
		std::vector<std::string>::iterator item;
		for (description = epgData.itemDescriptions.begin(), item = epgData.items.begin(); description != epgData.itemDescriptions.end(), item != epgData.items.end(); ++description, ++item) {
			sprintf(line, "%s: %s", (*(description)).c_str(), (*(item)).c_str());
			processTextToArray(line);
		}
	}

	// Show FSK information
	if (epgData.fsk > 0)
	{
		char fskInfo[4];
		sprintf(fskInfo, "%d", epgData.fsk);
		processTextToArray(std::string(g_Locale->getText(LOCALE_EPGVIEWER_AGE_RATING)) + ": " + fskInfo); // UTF-8
	}

	// Show length information
	char lengthInfo[11];
	sprintf(lengthInfo, "%d", epgData.epg_times.dauer / 60);
	processTextToArray(std::string(g_Locale->getText(LOCALE_EPGVIEWER_LENGTH)) + ": " + lengthInfo); // UTF-8

	if (!mp_info)
	{
		// Show audio information
		std::string audioInfo = "";
		CSectionsdClient::ComponentTagList tags;
		bool hasComponentTags = CEitManager::getInstance()->getComponentTagsUniqueKey( epgData.eventID, tags);
		if (hasComponentTags)
		{
			for (unsigned int i = 0; i < tags.size(); i++)
				if (tags[i].streamContent == 2 && !tags[i].component.empty())
					audioInfo += tags[i].component + ", ";

			if (!audioInfo.empty())
			{
				audioInfo.erase(audioInfo.size()-2);
				processTextToArray(std::string(g_Locale->getText(LOCALE_EPGVIEWER_AUDIO)) + ": " + audioInfo); // UTF-8
			}
		}
	}

	// Show genre information
#ifdef FULL_CONTENT_CLASSIFICATION
	if (!epgData.contentClassification.empty())
		processTextToArray(std::string(g_Locale->getText(LOCALE_EPGVIEWER_GENRE)) + ": " + GetGenre(epgData.contentClassification[0])); // UTF-8
//	processTextToArray( epgData.userClassification.c_str() );
#else
	if (epgData.contentClassification)
		processTextToArray(std::string(g_Locale->getText(LOCALE_EPGVIEWER_GENRE)) + ": " + GetGenre(epgData.contentClassification)); // UTF-8
#endif

	// -- display more screenings on the same channel
	// -- 2002-05-03 rasc
	has_follow_screenings = false;
	if (!mp_info && hasFollowScreenings(channel_id, epgData.title)) {
		processTextToArray(""); // UTF-8
		processTextToArray(std::string(g_Locale->getText(LOCALE_EPGVIEWER_MORE_SCREENINGS)) + ':'); // UTF-8
		FollowScreenings(channel_id, epgData.title);
		has_follow_screenings = true;
	}

	// show extended movie info
	if (mp_info && !extMovieInfo.empty())
		processTextToArray(extMovieInfo);

#if 0
	/* neat for debugging duplicate event issues etc. */
	char *epgid;
	if (asprintf(&epgid, "EPG ID:%04X.%02X", (int)((epgData.eventID)&0x0FFFF), epgData.table_id) >= 0)
	{
		processTextToArray(""); // UTF-8
		processTextToArray(epgid);
		free(epgid);
	}
#endif

	COSDFader fader(g_settings.theme.menu_Content_alpha);
	fader.StartFadeIn();

	// show the epg
	// header + logo
	if (!header)
	{
		header = new CComponentsHeader();
		header->setColorBody(COL_MENUHEAD_PLUS_0);
		header->enableColBodyGradient(g_settings.theme.menu_Head_gradient, COL_MENUCONTENT_PLUS_0, g_settings.theme.menu_Head_gradient_direction);
		header->enableClock(true, "%H:%M", "%H %M", true);
	}

	header->setDimensionsAll(sx, sy, ox, toph);
	header->setCaptionFont(font_title);
	header->setCaption(epgData.title);

	if (header->isPainted())
		header->hideCCItems();

	// set channel logo
	header->setChannelLogo(channel_id, channel_name);

	//paint head
	header->paint(CC_SAVE_SCREEN_NO);

	int showPos = 0;
	textCount = epgText.size();
	showText(showPos, sy + toph);

	// small bottom box with left/right navigation
	if (!Bottombox)
		Bottombox = new CNaviBar(sx, sy+oy-botboxheight, ox, botboxheight);

	if (!mp_info){
		std::string fromto = epg_start + " - " + epg_end;

		GetPrevNextEPGData(epgData.eventID, &epgData.epg_times.startzeit);

		Bottombox->enableArrows(prev_id && !call_fromfollowlist, next_id && !call_fromfollowlist);
		Bottombox->setText(fromto, epg_date);
	}

	//paint bottombox contents
	Bottombox->paint(false);
	showProgressBar();

	// show Timer Event Buttons
	showTimerEventBar(true, isCurrentEPG(channel_id), mp_info);

	if ( doLoop )
	{
		neutrino_msg_t      msg = 0;
		neutrino_msg_data_t data = 0;

		int scrollCount = 0;

		bool loop = true;
		bool epgTextSwitchClear = true;

		int timeout = g_settings.timing[SNeutrinoSettings::TIMING_EPG];
		uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(timeout);

		while (loop)
		{
			g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd );
			if ( msg <= CRCInput::RC_MaxRC )
				timeoutEnd = CRCInput::calcTimeoutEnd(timeout);

			scrollCount = medlinecount;

			switch ( msg )
			{
			case NeutrinoMessages::EVT_TIMER:
				if (data == fader.GetFadeTimer()) {
					if (fader.FadeDone())
						loop = false;
				}
				else
					CNeutrinoApp::getInstance()->handleMsg(msg, data);

				if (!mp_info)
				{
					if (data == g_InfoViewer->getUpdateTimer())
					{
						GetEPGData(channel_id, id, &startzeit, false);
						showProgressBar();
					}
				}
				else if (epg_done != -1)
				{
					CMoviePlayerGui::getInstance().UpdatePosition();
					int mp_position = CMoviePlayerGui::getInstance().GetPosition();
					int mp_duration = CMoviePlayerGui::getInstance().GetDuration();

					// this calculation is taken from timeosd.cpp
					epg_done = (mp_duration && mp_duration > 100) ? (mp_position * 100 / mp_duration) : -1;
					if (epg_done > 100)
						epg_done = 100;
					//printf("[%s:%d] epg_done: %d\n", __func__, __LINE__, epg_done);

					showProgressBar();
				}
				break;
			case NeutrinoMessages::EVT_CURRENTNEXT_EPG:
				if (/*!id && */ ((*(t_channel_id *) data) == (channel_id & 0xFFFFFFFFFFFFULL))) {
					show(channel_id,0,NULL,false);
					showPos=0;
				}
				CNeutrinoApp::getInstance()->handleMsg(msg, data);
				break;
			case CRCInput::RC_left:
				if ((prev_id != 0) && !call_fromfollowlist && !mp_info)
				{
					toph = topboxheight;
					show(channel_id, prev_id, &prev_zeit, false);
					showPos=0;
				}
				break;
			case CRCInput::RC_right:
				if ((next_id != 0) && !call_fromfollowlist && !mp_info)
				{
					toph = topboxheight;
					show(channel_id, next_id, &next_zeit, false);
					showPos=0;
				}
				break;
			case CRCInput::RC_down:
				if (showPos+scrollCount<textCount)
				{
					showPos += scrollCount;
					showText(showPos, sy + toph, tmdb_active, false);
				}
				break;
			case CRCInput::RC_up:
				if (showPos > 0) {
					showPos -= scrollCount;
					if (showPos < 0)
						showPos = 0;
					showText(showPos, sy + toph, tmdb_active, false);
				}
				break;
			case CRCInput::RC_page_up:
				if(isCurrentEPG(channel_id)){
					int zapBackPeriod = g_settings.adzap_zapBackPeriod / 60;
					if (zapBackPeriod < 120)
						zapBackPeriod++;
					if (zapBackPeriod > 120)
						zapBackPeriod = 120;
					g_settings.adzap_zapBackPeriod = zapBackPeriod * 60;
					showTimerEventBar(true, true);
				}
				break;
			case CRCInput::RC_page_down:
				if(isCurrentEPG(channel_id)){
					int zapBackPeriod = g_settings.adzap_zapBackPeriod / 60;
					if (zapBackPeriod > 1)
						zapBackPeriod--;
					if (zapBackPeriod < 1)
						zapBackPeriod = 1;
					g_settings.adzap_zapBackPeriod = zapBackPeriod * 60;
					showTimerEventBar(true, true);
				}
				break;
			case CRCInput::RC_red:
				if (mp_info)
				{
					epgTextSwitchClear = false;
					loop = false;
					break;
				}
				else if (!g_settings.minimode && (g_settings.recording_type != CNeutrinoApp::RECORDING_OFF))
				{
					std::string recDir;
					//CTimerdClient timerdclient;
					if (g_Timerd->isTimerdAvailable())
					{
						bool doRecord = true;
						recDir = g_settings.network_nfs_recordingdir;
						if (g_settings.recording_choose_direct_rec_dir == 2) {
							CFileBrowser b;
							b.Dir_Mode=true;
							hide();
							if (b.exec(g_settings.network_nfs_recordingdir.c_str())) {
								recDir = b.getSelectedFile()->Name;
							} else
								doRecord = false;
							if (!bigFonts && g_settings.bigFonts) {
								g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->setSize((int)(g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->getSize() * BIGFONT_FACTOR));
								g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->setSize((int)(g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->getSize() * BIGFONT_FACTOR));
							}
							bigFonts = g_settings.bigFonts;
							show(channel_id,epgData.eventID,&epgData.epg_times.startzeit,false);
							showPos=0;
						}
						else if (g_settings.recording_choose_direct_rec_dir == 1)
						{
							int lid = -1;
							CMountChooser recDirs(LOCALE_TIMERLIST_RECORDING_DIR,NEUTRINO_ICON_SETTINGS,&lid,NULL,g_settings.network_nfs_recordingdir.c_str());
							if (recDirs.hasItem())
							{
								hide();
								recDirs.exec(NULL,"");
								if (!bigFonts && g_settings.bigFonts) {
									g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->setSize((int)(g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->getSize() * BIGFONT_FACTOR));
									g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->setSize((int)(g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->getSize() * BIGFONT_FACTOR));
								}
								bigFonts = g_settings.bigFonts;
								show(channel_id,epgData.eventID,&epgData.epg_times.startzeit,false);
								showPos=0;
								timeoutEnd = CRCInput::calcTimeoutEnd(timeout);
							} else
							{
								printf("no network devices available\n");
							}
							if (lid != -1)
								recDir = g_settings.network_nfs[lid].local_dir;
						}
						if (doRecord && g_settings.recording_already_found_check)
						{
							CHintBox loadBox(LOCALE_RECORDING_ALREADY_FOUND_CHECK, LOCALE_MOVIEBROWSER_SCAN_FOR_MOVIES);
							loadBox.paint();
							CMovieBrowser moviebrowser;
							const char *rec_title = epgData.title.c_str();
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
						if (doRecord && !call_fromfollowlist)
						{
							CFollowScreenings m(channel_id,
								epgData.epg_times.startzeit,
								epgData.epg_times.startzeit + epgData.epg_times.dauer,
								epgData.title, epgData.eventID, TIMERD_APIDS_CONF, true, recDir, &evtlist);
							m.exec(NULL, "");
							timeoutEnd = CRCInput::calcTimeoutEnd(timeout);
						}
						else if (doRecord)
						{
							if (g_Timerd->addRecordTimerEvent(channel_id,
											  epgData.epg_times.startzeit,
											  epgData.epg_times.startzeit + epgData.epg_times.dauer,
											  epgData.eventID, epgData.epg_times.startzeit,
											  epgData.epg_times.startzeit - (ANNOUNCETIME + 120 ),
											  TIMERD_APIDS_CONF, true, epgData.epg_times.startzeit - (ANNOUNCETIME + 120) > time(NULL), recDir, false) == -1)
							{
								if (askUserOnTimerConflict(epgData.epg_times.startzeit - (ANNOUNCETIME + 120),
											   epgData.epg_times.startzeit + epgData.epg_times.dauer))
								{
									g_Timerd->addRecordTimerEvent(channel_id,
												      epgData.epg_times.startzeit,
												      epgData.epg_times.startzeit + epgData.epg_times.dauer,
												      epgData.eventID, epgData.epg_times.startzeit,
												      epgData.epg_times.startzeit - (ANNOUNCETIME + 120 ),
												      TIMERD_APIDS_CONF, true, epgData.epg_times.startzeit - (ANNOUNCETIME + 120) > time(NULL), recDir, true);
									ShowMsg(LOCALE_TIMER_EVENTRECORD_TITLE, LOCALE_TIMER_EVENTRECORD_MSG, CMsgBox::mbrBack, CMsgBox::mbBack, NEUTRINO_ICON_INFO);
									timeoutEnd = CRCInput::calcTimeoutEnd(timeout);
								}
							} else {
								ShowMsg(LOCALE_TIMER_EVENTRECORD_TITLE, LOCALE_TIMER_EVENTRECORD_MSG, CMsgBox::mbrBack, CMsgBox::mbBack, NEUTRINO_ICON_INFO);
								timeoutEnd = CRCInput::calcTimeoutEnd(timeout);
							}
						}
					}
					else
						printf("timerd not available\n");
				}
				break;
			case CRCInput::RC_help:
			{
				if (g_settings.tmdb_enabled)
				{
					showPos = 0;
					if (!tmdb_active) {
						cTmdb* tmdb = new cTmdb(epgData.title);
						if ((tmdb->getResults() > 0) && (!tmdb->getDescription().empty())) {
							epgText_saved = epgText;
							epgText.clear();
							tmdb_active = !tmdb_active;

							epgTextSwitch = tmdb->getDescription();
							if (!tmdb->getCast().empty())
								epgTextSwitch += "\n\n"+(std::string)g_Locale->getText(LOCALE_EPGEXTENDED_ACTORS)+":\n"+ tmdb->getCast()+"\n";

							processTextToArray(tmdb->CreateEPGText(), 0, tmdb->hasCover());
							textCount = epgText.size();
							stars = tmdb->getStars();
							showText(showPos, sy + toph, tmdb_active);
						} else {
							ShowMsg(LOCALE_MESSAGEBOX_INFO, LOCALE_EPGVIEWER_NODETAILED, CMsgBox::mbrOk , CMsgBox::mbrOk);
						}
						delete tmdb;
					} else {
						epgText = epgText_saved;
						textCount = epgText.size();
						tmdb_active = !tmdb_active;
						stars=0;
						showText(showPos, sy + toph);
					}
				}
				break;
			}
			case CRCInput::RC_yellow:
			{
				if (!mp_info)
				{
					if (isCurrentEPG(channel_id))
					{
						CAdZapMenu::getInstance()->exec(NULL, "enable");
						loop = false;
					}
					//CTimerdClient timerdclient;
					else if (g_Timerd->isTimerdAvailable())
					{
						g_Timerd->addZaptoTimerEvent(channel_id,
							     epgData.epg_times.startzeit - (g_settings.zapto_pre_time * 60),
							     epgData.epg_times.startzeit - ANNOUNCETIME - (g_settings.zapto_pre_time * 60), 0,
							     epgData.eventID, epgData.epg_times.startzeit, 0);
						ShowMsg(LOCALE_TIMER_EVENTTIMED_TITLE, LOCALE_TIMER_EVENTTIMED_MSG, CMsgBox::mbrBack, CMsgBox::mbBack, NEUTRINO_ICON_INFO);

						timeoutEnd = CRCInput::calcTimeoutEnd(timeout);
					}
					else
						printf("timerd not available\n");
				}
				break;
			}
			case CRCInput::RC_blue:
			{	
				if(!followlist.empty() && !call_fromfollowlist){
					hide();
					time_t tmp_sZeit  = epgData.epg_times.startzeit;
					uint64_t  tmp_eID = epgData.eventID;

					CEventList *ee = new CEventList;
					res = ee->exec(channel_id, g_Locale->getText(LOCALE_EPGVIEWER_MORE_SCREENINGS_SHORT),"","",followlist); // UTF-8
					delete ee;
					if (res == menu_return::RETURN_EXIT_ALL)
						loop = false;
					else {
						if (!bigFonts && g_settings.bigFonts) {
							g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->setSize((int)(g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->getSize() * BIGFONT_FACTOR));
							g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->setSize((int)(g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->getSize() * BIGFONT_FACTOR));
						}
						bigFonts = g_settings.bigFonts;
						show(channel_id,tmp_eID,&tmp_sZeit,false);
						showPos=0;
					}
				}
				break;
			}
			case CRCInput::RC_info:
				bigFonts = bigFonts ? false : true;
				ResetModules();
				frameBuffer->paintBackgroundBoxRel(sx, sy, ox, oy);
				showTimerEventBar (false);
				start();
//				textypos = sy;
//printf("bigFonts %d\n", bigFonts);
				if (bigFonts)
				{
					g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->setSize((int)(g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->getSize() * BIGFONT_FACTOR));
					g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->setSize((int)(g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->getSize() * BIGFONT_FACTOR));
				} else
				{
					g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->setSize((int)(g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->getSize() / BIGFONT_FACTOR));
					g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->setSize((int)(g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->getSize() / BIGFONT_FACTOR));
				}
				g_settings.bigFonts = bigFonts;
				if (mp_info)
					show(mp_movie_info->epgId >> 16, 0, 0, false, false, true);
				else
					show(channel_id, id, &startzeit, false, call_fromfollowlist);
				showPos=0;
				break;
			case CRCInput::RC_ok:
			case CRCInput::RC_timeout:
				if(fader.StartFadeOut()) {
					timeoutEnd = CRCInput::calcTimeoutEnd(1);
					msg = 0;
				} else
					loop = false;
				break;
			default:
				if (msg == (neutrino_msg_t)g_settings.key_channelList_cancel) {
					if(fader.StartFadeOut()) {
						timeoutEnd = CRCInput::calcTimeoutEnd(1);
						msg = 0;
					} else
						loop = false;
				}
				else if (CNeutrinoApp::getInstance()->listModeKey(msg)) {
					if (!call_fromfollowlist) {
						g_RCInput->postMsg (msg, 0);
						loop = false;
					}
				}
				else if (msg == NeutrinoMessages::EVT_SERVICESCHANGED || msg == NeutrinoMessages::EVT_BOUQUETSCHANGED) {
					g_RCInput->postMsg(msg, data);
					loop = false;
					res = menu_return::RETURN_EXIT_ALL;
				}
				else
				{
					if ( CNeutrinoApp::getInstance()->handleMsg( msg, data ) & messages_return::cancel_all )
					{
						loop = false;
						res = menu_return::RETURN_EXIT_ALL;
					}
				}
			}
		}
		hide();
		fader.StopFade();
		if(epgTextSwitchClear)
			epgTextSwitch.clear();
	}
	return res;
}

void CEpgData::hide()
{
	// 2004-09-10 rasc  (bugfix, scale large font settings back to normal)
	if (bigFonts) {
		bigFonts = false;
		g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->setSize((int)(g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->getSize() / BIGFONT_FACTOR));
		g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->setSize((int)(g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->getSize() / BIGFONT_FACTOR));
	}

	ResetModules();

	frameBuffer->paintBackgroundBoxRel(sx, sy, ox, oy);
	showTimerEventBar (false);
}

void CEpgData::GetEPGData(const t_channel_id channel_id, uint64_t id, time_t* startzeit, bool clear )
{
	if(clear)
		epgText.clear();
	emptyLineCount = 0;
	epgData.title.clear();

	bool res;
	if ( id!= 0 )
		res = CEitManager::getInstance()->getEPGid(id, *startzeit, &epgData);
	else
		res = CEitManager::getInstance()->getActualEPGServiceKey(channel_id, &epgData );

	if ( res )
	{
		// If we have items, merge and localize them (e.g. actor1, actor2, ... -> Actors)
		if (false == epgData.itemDescriptions.empty()) {
			reformatExtendedEvents("Year of production", g_Locale->getText(LOCALE_EPGEXTENDED_YEAR_OF_PRODUCTION), false, epgData);
			reformatExtendedEvents("Original title", g_Locale->getText(LOCALE_EPGEXTENDED_ORIGINAL_TITLE), false, epgData);
			reformatExtendedEvents("Director", g_Locale->getText(LOCALE_EPGEXTENDED_DIRECTOR), false, epgData);
			reformatExtendedEvents("Actor", g_Locale->getText(LOCALE_EPGEXTENDED_ACTORS), true, epgData);
			reformatExtendedEvents("Guests", g_Locale->getText(LOCALE_EPGEXTENDED_GUESTS), false, epgData);
			reformatExtendedEvents("Presenter", g_Locale->getText(LOCALE_EPGEXTENDED_PRESENTER), false, epgData);
		}

		struct tm *pStartZeit = localtime(&(epgData.epg_times).startzeit);
		tmp_curent_zeit = (epgData.epg_times).startzeit;
		char temp[20]={0};
		strftime( temp, sizeof(temp),"%d.%m.%Y", pStartZeit);
		epg_date = g_Locale->getText(CLocaleManager::getWeekday(pStartZeit));
		epg_date += ".";
		epg_date += temp;
		strftime( temp, sizeof(temp), "%H:%M", pStartZeit);
		epg_start= temp;

		long int uiEndTime((epgData.epg_times).startzeit+ (epgData.epg_times).dauer);
		struct tm *pEndeZeit = localtime((time_t*)&uiEndTime);
		strftime( temp, sizeof(temp), "%H:%M", pEndeZeit);
		epg_end= temp;

		epg_done= -1;
		if (( time(NULL)- (epgData.epg_times).startzeit )>= 0 )
		{
			unsigned nProcentagePassed = ((time(NULL) - (epgData.epg_times).startzeit) * 100 / (epgData.epg_times).dauer);
			if (nProcentagePassed<= 100)
				epg_done= nProcentagePassed;
		}
	}
//printf("GetEPGData:: items %d descriptions %d\n", epgData.items.size(), epgData.itemDescriptions.size());
}

void CEpgData::GetPrevNextEPGData( uint64_t id, time_t* startzeit )
{
	prev_id= 0;
	next_id= 0;
	unsigned int i;

	for ( i= 0; i< evtlist.size(); i++ )
	{
		//printf("%d %llx/%llx - %x %x\n", i, evtlist[i].eventID, id, evtlist[i].startTime, *startzeit);
		if ( ( evtlist[i].eventID == id ) && ( evtlist[i].startTime == *startzeit ) )
		{
			if ( i > 0 )
			{
				prev_id= evtlist[i- 1].eventID;
				prev_zeit= evtlist[i- 1].startTime;
			}
			if ( i < ( evtlist.size()- 1 ) )
			{
				next_id= evtlist[i+ 1].eventID;
				next_zeit= evtlist[i+ 1].startTime;
			}
			break;
		}
	}
}

//
// -- get following screenings of this program title
// -- yek! a better class design would be more helpfull
// -- BAD THING: Cross channel screenings will not be shown
// --            $$$TODO
// -- 2002-05-03 rasc
//

bool CEpgData::hasFollowScreenings(const t_channel_id /*channel_id*/, const std::string &title)
{
	CChannelEventList::iterator e;
	if(!followlist.empty())
		followlist.clear();
	for (e = evtlist.begin(); e != evtlist.end(); ++e)
	{
		if (e->startTime == tmp_curent_zeit)
			continue;
		if (! e->eventID)
			continue;
		if (e->description != title)
			continue;
		followlist.push_back(*e);
	}
	return !followlist.empty();
}

int CEpgData::FollowScreenings (const t_channel_id /*channel_id*/, const std::string & /*title*/)
{
	CChannelEventList::iterator e;
	struct  tm		*tmStartZeit;
	std::string		screening_dates,screening_nodual;
	int			count = 0;
	int 			flag = 1;

	screening_dates = screening_nodual = "";

	for (e = followlist.begin(); e != followlist.end(); ++e)
	{
		count++;
		tmStartZeit = localtime(&(e->startTime));

		screening_dates = g_Locale->getText(CLocaleManager::getWeekday(tmStartZeit));
		screening_dates += strftime(", %d", tmStartZeit);
		screening_dates += g_Locale->getText(CLocaleManager::getMonth(tmStartZeit));
		screening_dates += strftime(", %R", tmStartZeit);
		if (e->startTime <= tmp_curent_zeit)
			flag = 2;
		else
			flag = 1;

		if (screening_dates != screening_nodual) {
			screening_nodual=screening_dates;

			processTextToArray(screening_dates, flag ); // UTF-8
		}
	}
	return count;
}

void CEpgData::showProgressBar()
{
	int w = ox/10;
	int x = sx + (ox - w)/2;
	int h = botboxheight - 2*OFFSET_INNER_SMALL;
	int y = sy + oy - botboxheight + (botboxheight - h)/2;
	if (!pb){
		pb = new CProgressBar(x, y, w, h);
		pb->setType(CProgressBar::PB_TIMESCALE);
	}
	//show progressbar
	if (epg_done != -1)
	{
		pb->setValues(epg_done, 100);
		pb->paint(true);
	}else{
		pb->hide();
	}
}

//
// -- Just display or hide TimerEventbar
// -- 2002-05-13 rasc
//

#define EpgButtonsMax 4
const struct button_label EpgButtons[][EpgButtonsMax] =
{
	{ // full view
		{ NEUTRINO_ICON_BUTTON_RED, LOCALE_TIMERBAR_RECORDEVENT },
		{ NEUTRINO_ICON_BUTTON_YELLOW, LOCALE_TIMERBAR_CHANNELSWITCH },
		{ NEUTRINO_ICON_BUTTON_BLUE, LOCALE_EPGVIEWER_MORE_SCREENINGS_SHORT },
		{ NEUTRINO_ICON_BUTTON_INFO_SMALL, LOCALE_CHANNELLIST_ADDITIONAL }
	},
	{ // w/o followscreenings
		{ NEUTRINO_ICON_BUTTON_RED, LOCALE_TIMERBAR_RECORDEVENT },
		{ NEUTRINO_ICON_BUTTON_YELLOW, LOCALE_TIMERBAR_CHANNELSWITCH },
		{ NEUTRINO_ICON_BUTTON_INFO_SMALL, LOCALE_CHANNELLIST_ADDITIONAL }
	},
	{ // movieplayer mode
		{ NEUTRINO_ICON_BUTTON_RED, LOCALE_EPG_SAVING },
		{ NEUTRINO_ICON_BUTTON_INFO_SMALL, LOCALE_CHANNELLIST_ADDITIONAL }
	}
};

void CEpgData::showTimerEventBar (bool pshow, bool adzap, bool mp_info)
{
	int  x, y, w, h, fh;
        int icol_w, icol_h;

	x = sx;
	y = sy + oy;
	w = ox;

	// why we don't use buttonheight member?
	fh = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_FOOT]->getHeight();

        frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_RED, &icol_w, &icol_h);
	h = std::max(fh, icol_h+4);

	// hide only?
	if (! pshow) {
		frameBuffer->paintBackgroundBoxRel(sx,y,ox,h);
		return;
	}

	std::string adzap_button;
	if (adzap)
	{
		adzap_button = g_Locale->getText(LOCALE_ADZAP);
		adzap_button += " " + to_string(g_settings.adzap_zapBackPeriod / 60) + " ";
		adzap_button += g_Locale->getText(LOCALE_UNIT_SHORT_MINUTE);
	}
	bool tmdb = g_settings.tmdb_enabled;
	bool fscr = (has_follow_screenings && !call_fromfollowlist);
	if (mp_info)
		::paintButtons(x, y, w, tmdb ? 2 : 1, EpgButtons[2], w, h);
	else
	{
		int c = EpgButtonsMax;
		if (!tmdb)
			c--; // reduce tmdb button
		if (!fscr)
			c--; // reduce blue button
		if (g_settings.recording_type != CNeutrinoApp::RECORDING_OFF)
			::paintButtons(x, y, w, c, EpgButtons[fscr ? 0 : 1], w, h, "", false, COL_MENUFOOT_TEXT, adzap ? adzap_button.c_str() : NULL, 1);
		else
			::paintButtons(x, y, w, c, &EpgButtons[fscr ? 0 : 1][1], w, h, "", false, COL_MENUFOOT_TEXT, adzap ? adzap_button.c_str() : NULL, 0);
	}
}

void CEpgData::ResetModules()
{
	if (header){
		delete header; header = NULL;
	}
	if (Bottombox){
		delete Bottombox; Bottombox = NULL;
	}
	if (pb){
		delete pb; pb = NULL;
	}
}

//  -- EPG Data Viewer Menu Handler Class
//  -- to be used for calls from Menue
//  -- (2004-03-06 rasc)

int CEPGDataHandler::exec(CMenuTarget* parent, const std::string &/*actionkey*/)
{
	int           res = menu_return::RETURN_EXIT_ALL;
	CChannelList  *channelList;
	CEpgData      *e;


	if (parent) {
		parent->hide();
	}

	e = new CEpgData;

	channelList = CNeutrinoApp::getInstance()->channelList;
	e->show( channelList->getActiveChannel_ChannelID() );
	delete e;

	return res;
}
