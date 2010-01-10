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

#include <algorithm>
#include <gui/epgview.h>

#include <gui/widget/hintbox.h>
#include <gui/widget/icons.h>
#include <gui/widget/messagebox.h>
#include <gui/widget/mountchooser.h>
#include <gui/widget/progressbar.h>
#include <gui/timerlist.h>

#include <global.h>
#include <neutrino.h>

#include <driver/encoding.h>
#include <driver/screen_max.h>
#include <gui/filebrowser.h>
#include <gui/customcolor.h>
#include <gui/pictureviewer.h>

extern bool pb_blink;
extern CPictureViewer * g_PicViewer;
#define PIC_W 52
#define PIC_H 39

#define ICON_LARGE_WIDTH 26
#define ROUND_RADIUS 9

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
	bigFonts = false;
	frameBuffer = CFrameBuffer::getInstance();
}

#define MAX_W 540
#define MAX_H 320

void CEpgData::start()
{
	ox = w_max (MAX_W * (bigFonts ? BIG_FONT_FAKTOR : 1), 0);
	oy = h_max (MAX_H * (bigFonts ? BIG_FONT_FAKTOR : 1), 0);
	sx = getScreenStartX( ox );

	topheight     = g_Font[SNeutrinoSettings::FONT_TYPE_EPG_TITLE]->getHeight();
	topboxheight  = topheight + 6;

	if (topboxheight < PIC_H) topboxheight = PIC_H;

	botheight     = g_Font[SNeutrinoSettings::FONT_TYPE_EPG_DATE]->getHeight();
	botboxheight  = botheight + 6;
	medlineheight = g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->getHeight();
	medlinecount  = (oy- botboxheight)/medlineheight;
	sb = medlinecount* medlineheight;

	oy = botboxheight+medlinecount*medlineheight; // recalculate //FIXME
	sy = getScreenStartY(oy- topboxheight);
	toph = topboxheight;
}

void CEpgData::addTextToArray(const std::string & text) // UTF-8
{
	//printf("line: >%s<\n", text.c_str() );
	if (text==" ")
	{
		emptyLineCount ++;
		if (emptyLineCount<2)
		{
			epgText.push_back(text);
		}
	}
	else
	{
		emptyLineCount = 0;
		epgText.push_back(text);
	}
}

void CEpgData::processTextToArray(std::string text) // UTF-8
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
			int aktWordWidth = g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->getRenderWidth(aktWord, true);
			if ((aktWordWidth+aktWidth)<(ox- 20- 15))
			{//space ok, add
				aktWidth += aktWordWidth;
				aktLine += aktWord;

				if (*text_=='\n')
				{	//enter-handler
					addTextToArray( aktLine );
					aktLine = "";
					aktWidth= 0;
				}
				aktWord = "";
			}
			else
			{//new line needed
				addTextToArray( aktLine );
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
	addTextToArray( aktLine + aktWord );
}

void CEpgData::showText( int startPos, int ypos )
{
	// recalculate
	medlineheight = g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->getHeight();
	medlinecount=(oy- botboxheight)/medlineheight;

	int ltextCount = epgText.size();
	int y=ypos;

	frameBuffer->paintBoxRel(sx, y, ox- 15, sb, COL_MENUCONTENT_PLUS_0);

	for (int i=startPos; i<ltextCount && i<startPos+medlinecount; i++,y+=medlineheight)
	{
		if ( i< info1_lines )
			g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->RenderString(sx+10, y+medlineheight, ox- 15- 15, epgText[i], COL_MENUCONTENT, 0, true); // UTF-8
		else
			g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->RenderString(sx+10, y+medlineheight, ox- 15- 15, epgText[i], COL_MENUCONTENT, 0, true); // UTF-8
	}

	frameBuffer->paintBoxRel(sx+ ox- 15, ypos, 15, sb,  COL_MENUCONTENT_PLUS_1);

	int sbc= ((ltextCount- 1)/ medlinecount)+ 1;
	float sbh= (sb- 4)/ sbc;
	int sbs= (startPos+ 1)/ medlinecount;

	frameBuffer->paintBoxRel(sx+ ox- 13, ypos+ 2+ int(sbs* sbh) , 11, int(sbh),  COL_MENUCONTENT_PLUS_3);
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

bool CEpgData::hasFollowScreenings(const t_channel_id /*channel_id*/, const std::string & title) {
	time_t curtime = time(NULL);

	for (CChannelEventList::iterator e = evtlist.begin(); e != evtlist.end(); ++e )
	{
		if (e->startTime > curtime && e->eventID && e->description == title)
			return true;
	}
	return false;
}

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

extern char recDir[255];
void sectionsd_getEventsServiceKey(t_channel_id serviceUniqueKey, CChannelEventList &eList, char search = 0, std::string search_text = "");
bool sectionsd_getComponentTagsUniqueKey(const event_id_t uniqueKey, CSectionsdClient::ComponentTagList& tags);

void CEpgData::showHead(const t_channel_id channel_id)
{
	std::string text1 = epgData.title;
	std::string text2 = "";
	if (g_Font[SNeutrinoSettings::FONT_TYPE_EPG_TITLE]->getRenderWidth(text1, true) > ox - PIC_W - 5)
	{
		int pos;
		do
		{
			pos = text1.find_last_of("[ .]+");
			if ( pos!=-1 )
				text1 = text1.substr( 0, pos );
		} while ( ( pos != -1 ) && (g_Font[SNeutrinoSettings::FONT_TYPE_EPG_TITLE]->getRenderWidth(text1, true) > 520 - PIC_W - 5));
		text2 = epgData.title.substr(text1.length()+ 1, uint(-1) );
	}
	int oldtoph= toph;

	toph = text2 != "" ? 2* topboxheight : topboxheight;

	if (oldtoph > toph)
		frameBuffer->paintBackgroundBox (sx, sy- oldtoph- 1, sx+ ox, sy /*- toph*/);

	//show the epg title
	frameBuffer->paintBoxRel(sx, sy- toph, ox, toph, COL_MENUHEAD_PLUS_0, ROUND_RADIUS, CORNER_TOP);//round
	bool logo_ok = false;

	logo_ok = g_PicViewer->DisplayLogo(channel_id, sx+10, sy- toph+ (toph-PIC_H)/2/*5*/, PIC_W, PIC_H);
	g_Font[SNeutrinoSettings::FONT_TYPE_EPG_TITLE]->RenderString(sx+15 +(logo_ok? PIC_W+10: 0), sy- toph+ topheight+ 3, ox-15- (logo_ok ? PIC_W+5: 0), text1, COL_MENUHEAD, 0, true);
	if (!(text2.empty()))
		g_Font[SNeutrinoSettings::FONT_TYPE_EPG_TITLE]->RenderString(sx+15+(logo_ok? PIC_W+10: 0), sy- toph+ 2* topheight+ 3, ox-15 - (logo_ok ? PIC_W+5: 0), text2, COL_MENUHEAD, 0, true);

}

int CEpgData::show(const t_channel_id channel_id, unsigned long long a_id, time_t* a_startzeit, bool doLoop )
{
	int res = menu_return::RETURN_REPAINT;
	static unsigned long long id;
	static time_t startzeit;

	if (a_startzeit)
		startzeit=*a_startzeit;
	id=a_id;

	int height;
	height = g_Font[SNeutrinoSettings::FONT_TYPE_EPG_DATE]->getHeight();
	if (doLoop)
	{
		//start();
		frameBuffer->paintBoxRel(g_settings.screen_StartX, g_settings.screen_StartY, 50, height+5, COL_INFOBAR_PLUS_0);
		g_Font[SNeutrinoSettings::FONT_TYPE_EPG_DATE]->RenderString(g_settings.screen_StartX+10, g_settings.screen_StartY+height, 40, "-@-", COL_INFOBAR);
		if (!bigFonts && g_settings.bigFonts) {
			g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->setSize((int)(g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->getSize() * BIG_FONT_FAKTOR));
			g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->setSize((int)(g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->getSize() * BIG_FONT_FAKTOR));
		}
		bigFonts = g_settings.bigFonts;
		start();
	}

	GetEPGData(channel_id, id, &startzeit );
	if (doLoop)
	{
		//evtlist = g_Sectionsd->getEventsServiceKey(channel_id&0xFFFFFFFFFFFFULL);
		evtlist.clear();
		sectionsd_getEventsServiceKey(channel_id&0xFFFFFFFFFFFFULL, evtlist);
		// Houdini added for Private Premiere EPG start sorted by start date/time 2005-08-15
		sort(evtlist.begin(),evtlist.end(),sortByDateTime);
		frameBuffer->paintBackgroundBoxRel(g_settings.screen_StartX, g_settings.screen_StartY, 50, height+5);
	}

	if (epgData.title.empty()) /* no epg info found */
	{
		ShowHintUTF(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_EPGVIEWER_NOTFOUND)); // UTF-8
		hide();
		return res;
	}

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
			if (false == bHide && false == (std::string::npos == epgData.info2.find(epgData.info1))) {
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

	// 21.07.2005 - rainerk
	// Show extended information
	if (0 != epgData.itemDescriptions.size() && 0 != epgData.items.size()) {
		char line[256];
		std::vector<std::string>::iterator description;
		std::vector<std::string>::iterator item;
		processTextToArray(""); // Add a blank line
//printf("show:: items %d descriptions %d\n", epgData.items.size(), epgData.itemDescriptions.size());
		for (description = epgData.itemDescriptions.begin(), item = epgData.items.begin(); description != epgData.itemDescriptions.end(), item != epgData.items.end(); ++description, ++item) {
//printf("item:: %s: %s\n", (*(description)).c_str(), (*(item)).c_str());fflush(stdout);
			sprintf(line, "%s: %s", (*(description)).c_str(), (*(item)).c_str());
			processTextToArray(line);
//printf("after processTextToArray\n");
		}
	}
//printf("after epgData.itemDescriptions.size\n");

	if (epgData.fsk > 0)
	{
		char _tfsk[11];
		sprintf (_tfsk, "FSK: ab %d", epgData.fsk );
		processTextToArray( _tfsk ); // UTF-8
	}

	if (epgData.contentClassification.length()> 0)
		processTextToArray(GetGenre(epgData.contentClassification[0])); // UTF-8
//	processTextToArray( epgData.userClassification.c_str() );


	// -- display more screenings on the same channel
	// -- 2002-05-03 rasc
	if (hasFollowScreenings(channel_id, epgData.title)) {
		processTextToArray(""); // UTF-8
		processTextToArray(std::string(g_Locale->getText(LOCALE_EPGVIEWER_MORE_SCREENINGS)) + ':'); // UTF-8
		FollowScreenings(channel_id, epgData.title);
	}

	showHead(channel_id);

	//show date-time....
	frameBuffer->paintBoxRel(sx, sy+oy-botboxheight, ox, botboxheight, COL_MENUHEAD_PLUS_0);
	std::string fromto;
	int widthl,widthr;
	fromto = epg_start;
	fromto += " - ";
	fromto += epg_end;

	widthl = g_Font[SNeutrinoSettings::FONT_TYPE_EPG_DATE]->getRenderWidth(fromto);
	g_Font[SNeutrinoSettings::FONT_TYPE_EPG_DATE]->RenderString(sx+40,  sy+oy-3, widthl, fromto, COL_MENUHEAD);
	widthr = g_Font[SNeutrinoSettings::FONT_TYPE_EPG_DATE]->getRenderWidth(epg_date);
	g_Font[SNeutrinoSettings::FONT_TYPE_EPG_DATE]->RenderString(sx+ox-40-widthr,  sy+oy-3, widthr, epg_date, COL_MENUHEAD);

	int showPos = 0;
	textCount = epgText.size();
	int textypos = sy;
	showText(showPos, textypos);

	// show Timer Event Buttons
	showTimerEventBar (true);

	//show Content&Component for Dolby & 16:9
	CSectionsdClient::ComponentTagList tags;
	//if ( g_Sectionsd->getComponentTagsUniqueKey( epgData.eventID, tags ) )
	if ( sectionsd_getComponentTagsUniqueKey( epgData.eventID, tags ) )
	{
		for (unsigned int i=0; i< tags.size(); i++)
		{
			if ( tags[i].streamContent == 1 && (tags[i].componentType == 2 || tags[i].componentType == 3) )
			{
				frameBuffer->paintIcon("16_9.raw" ,ox+sx-(ICON_LARGE_WIDTH+2)-(ICON_LARGE_WIDTH+2),sy + oy+5 );
			}
			else if ( tags[i].streamContent == 2 && tags[i].componentType == 5 )
			{
				frameBuffer->paintIcon("dd.raw", ox+sx-(ICON_LARGE_WIDTH+2), sy + oy+5);
			}
		}
	}

	//show progressbar
	if ( epg_done!= -1 )
	{
		int pbx = sx + 10 + widthl + 10 + ((ox-104-widthr-widthl-10-10-20)>>1);
		CProgressBar pb(pb_blink, -1, -1, 30, 100, 70, true);
		pb.paintProgressBarDefault(pbx, sy+oy-height, 104, height-6, epg_done, 104);
	}

	GetPrevNextEPGData( epgData.eventID, &epgData.epg_times.startzeit );
	if (prev_id != 0)
	{
		frameBuffer->paintBoxRel(sx+ 5, sy+ oy- botboxheight+ 4, botboxheight- 8, botboxheight- 8,  COL_MENUCONTENT_PLUS_3);
		g_Font[SNeutrinoSettings::FONT_TYPE_EPG_DATE]->RenderString(sx+ 10, sy+ oy- 3, widthr, "<", COL_MENUCONTENT + 3);
	}

	if (next_id != 0)
	{
		frameBuffer->paintBoxRel(sx+ ox- botboxheight+ 8- 5, sy+ oy- botboxheight+ 4, botboxheight- 8, botboxheight- 8,  COL_MENUCONTENT_PLUS_3);
		g_Font[SNeutrinoSettings::FONT_TYPE_EPG_DATE]->RenderString(sx+ ox- botboxheight+ 8, sy+ oy- 3, widthr, ">", COL_MENUCONTENT + 3);
	}

	if ( doLoop )
	{
		neutrino_msg_t      msg;
		neutrino_msg_data_t data;

		int scrollCount;

		bool loop = true;

		unsigned long long timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_EPG]);

		while (loop)
		{
			g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd );

			scrollCount = medlinecount;

			switch ( msg )
			{
			case NeutrinoMessages::EVT_TIMER:
				if (data == g_InfoViewer->lcdUpdateTimer) {
					GetEPGData(channel_id, id, &startzeit, false);
					if ( epg_done!= -1 ) {
						CProgressBar pb(pb_blink, -1, -1, 30, 100, 70, true);
						int pbx = sx + 10 + widthl + 10 + ((ox-104-widthr-widthl-10-10-20)>>1);
						pb.paintProgressBarDefault(pbx, sy+oy-height, 104, height-6, epg_done, 104);
					}
				}
				CNeutrinoApp::getInstance()->handleMsg(msg, data);
				break;
			case NeutrinoMessages::EVT_CURRENTNEXT_EPG:
				if (/*!id && */ ((*(t_channel_id *) data) == (channel_id & 0xFFFFFFFFFFFFULL))) {
					show(channel_id,0,NULL,false);
					showPos=0;
				}
				CNeutrinoApp::getInstance()->handleMsg(msg, data);
				break;
			case CRCInput::RC_left:
				if (prev_id != 0)
				{
					frameBuffer->paintBoxRel(sx+ 5, sy+ oy- botboxheight+ 4, botboxheight- 8, botboxheight- 8,  COL_MENUCONTENT_PLUS_1);
					g_Font[SNeutrinoSettings::FONT_TYPE_EPG_DATE]->RenderString(sx+ 10, sy+ oy- 3, widthr, "<", COL_MENUCONTENT + 1);

					show(channel_id, prev_id, &prev_zeit, false);
					showPos=0;
				}
				break;

			case CRCInput::RC_right:
				if (next_id != 0)
				{
					frameBuffer->paintBoxRel(sx+ ox- botboxheight+ 8- 5, sy+ oy- botboxheight+ 4, botboxheight- 8, botboxheight- 8,  COL_MENUCONTENT_PLUS_1);
					g_Font[SNeutrinoSettings::FONT_TYPE_EPG_DATE]->RenderString(sx+ ox- botboxheight+ 8, sy+ oy- 3, widthr, ">", COL_MENUCONTENT + 1);

					show(channel_id, next_id, &next_zeit, false);
					showPos=0;
				}
				break;

			case CRCInput::RC_down:
				if (showPos+scrollCount<textCount)
				{
					showPos += scrollCount;
					showText(showPos,textypos);
				}
				break;

			case CRCInput::RC_up:
				showPos -= scrollCount;
				if (showPos<0)
					showPos = 0;
				else
					showText(showPos,textypos);
				break;

				// 31.05.2002 dirch		record timer
			case CRCInput::RC_red:
				if (!g_settings.minimode && (g_settings.recording_type != CNeutrinoApp::RECORDING_OFF))
				{
					//CTimerdClient timerdclient;
					if (g_Timerd->isTimerdAvailable())
					{
						bool doRecord = true;
						//char *recDir = g_settings.network_nfs_recordingdir;
						strcpy(recDir, g_settings.network_nfs_recordingdir);
						if (g_settings.recording_choose_direct_rec_dir == 2) {
							CFileBrowser b;
							b.Dir_Mode=true;
							hide();
							if (b.exec(g_settings.network_nfs_recordingdir)) {
								strcpy(recDir, b.getSelectedFile()->Name.c_str());
							} else
								doRecord = false;
							show(channel_id,epgData.eventID,&epgData.epg_times.startzeit,false);
						}
						else if (g_settings.recording_choose_direct_rec_dir == 1)
						{
							int lid = -1;
							CMountChooser recDirs(LOCALE_TIMERLIST_RECORDING_DIR,NEUTRINO_ICON_SETTINGS,&lid,NULL,g_settings.network_nfs_recordingdir);
							if (recDirs.hasItem())
							{
								hide();
								recDirs.exec(NULL,"");
								show(channel_id,epgData.eventID,&epgData.epg_times.startzeit,false);
							} else
							{
								printf("no network devices available\n");
							}
							if (lid != -1)
								strcpy(recDir, g_settings.network_nfs_local_dir[lid]);
							//recDir = g_settings.network_nfs_local_dir[id];
							//else
							//recDir = NULL;
						}
						//if (recDir != NULL)
						if (doRecord)
						{
							if (g_Timerd->addRecordTimerEvent(channel_id,
											  epgData.epg_times.startzeit,
											  epgData.epg_times.startzeit + epgData.epg_times.dauer,
											  epgData.eventID, epgData.epg_times.startzeit,
											  epgData.epg_times.startzeit - (ANNOUNCETIME + 120 ),
											  TIMERD_APIDS_CONF, true, recDir,false) == -1)
							{
								if (askUserOnTimerConflict(epgData.epg_times.startzeit - (ANNOUNCETIME + 120),
											   epgData.epg_times.startzeit + epgData.epg_times.dauer))
								{
									g_Timerd->addRecordTimerEvent(channel_id,
												      epgData.epg_times.startzeit,
												      epgData.epg_times.startzeit + epgData.epg_times.dauer,
												      epgData.eventID, epgData.epg_times.startzeit,
												      epgData.epg_times.startzeit - (ANNOUNCETIME + 120 ),
												      TIMERD_APIDS_CONF, true, recDir,true);
									ShowLocalizedMessage(LOCALE_TIMER_EVENTRECORD_TITLE, LOCALE_TIMER_EVENTRECORD_MSG, CMessageBox::mbrBack, CMessageBox::mbBack, "info.raw");
								}
							} else {
								ShowLocalizedMessage(LOCALE_TIMER_EVENTRECORD_TITLE, LOCALE_TIMER_EVENTRECORD_MSG, CMessageBox::mbrBack, CMessageBox::mbBack, "info.raw");
							}
						}
					}
					else
						printf("timerd not available\n");
				}
				break;

				// 31.05.2002 dirch		zapto timer
			case CRCInput::RC_yellow:
			{
				//CTimerdClient timerdclient;
				if (g_Timerd->isTimerdAvailable())
				{
					g_Timerd->addZaptoTimerEvent(channel_id,
								     epgData.epg_times.startzeit,
								     epgData.epg_times.startzeit - ANNOUNCETIME, 0,
								     epgData.eventID, epgData.epg_times.startzeit, 0);
					ShowLocalizedMessage(LOCALE_TIMER_EVENTTIMED_TITLE, LOCALE_TIMER_EVENTTIMED_MSG, CMessageBox::mbrBack, CMessageBox::mbBack, "info.raw");
				}
				else
					printf("timerd not available\n");
				break;
			}

			case CRCInput::RC_info:
			case CRCInput::RC_help:
				bigFonts = bigFonts ? false : true;
				frameBuffer->paintBackgroundBox (sx, sy- toph, sx+ ox, sy+ oy);
				showTimerEventBar (false);
				start();
				textypos = sy;
//printf("bigFonts %d\n", bigFonts);
				if (bigFonts)
				{
					g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->setSize((int)(g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->getSize() * BIG_FONT_FAKTOR));
					g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->setSize((int)(g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->getSize() * BIG_FONT_FAKTOR));
				} else
				{
					g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->setSize((int)(g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->getSize() / BIG_FONT_FAKTOR));
					g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->setSize((int)(g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->getSize() / BIG_FONT_FAKTOR));
				}
				g_settings.bigFonts = bigFonts;
				show(channel_id, id, &startzeit, false);
				showPos=0;
				break;

			case CRCInput::RC_ok:
			case CRCInput::RC_timeout:
				loop = false;
				break;
			case CRCInput::RC_favorites:
			case CRCInput::RC_sat:
				g_RCInput->postMsg (msg, 0);
				loop = false;
				break;

			default:
				// konfigurierbare Keys handlen...
				if (msg == (neutrino_msg_t)g_settings.key_channelList_cancel)
					loop = false;
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
	}
	return res;
}

void CEpgData::hide()
{
	// 2004-09-10 rasc  (bugfix, scale large font settings back to normal)
	if (bigFonts) {
		bigFonts = false;
		g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->setSize((int)(g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->getSize() / BIG_FONT_FAKTOR));
		g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->setSize((int)(g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->getSize() / BIG_FONT_FAKTOR));
	}

	frameBuffer->paintBackgroundBox (sx, sy- toph, sx+ ox, sy+ oy);
	showTimerEventBar (false);
}

bool sectionsd_getEPGid(const event_id_t epgID, const time_t startzeit, CEPGData * epgdata);
bool sectionsd_getActualEPGServiceKey(const t_channel_id uniqueServiceKey, CEPGData * epgdata);

void CEpgData::GetEPGData(const t_channel_id channel_id, unsigned long long id, time_t* startzeit, bool clear )
{
	if(clear)
		epgText.clear();
	emptyLineCount = 0;
	epgData.title.clear();

	bool res;
	if ( id!= 0 )
		//res = g_Sectionsd->getEPGid( id, *startzeit, &epgData );
		res = sectionsd_getEPGid(id, *startzeit, &epgData);
	else
		//res = g_Sectionsd->getActualEPGServiceKey(channel_id&0xFFFFFFFFFFFFULL, &epgData );
		res = sectionsd_getActualEPGServiceKey(channel_id&0xFFFFFFFFFFFFULL, &epgData );

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
			unsigned nProcentagePassed=(unsigned)((float)(time(NULL)-(epgData.epg_times).startzeit)/(float)(epgData.epg_times).dauer*100.);
			if (nProcentagePassed<= 100)
				epg_done= nProcentagePassed;
		}
	}
//printf("GetEPGData:: items %d descriptions %d\n", epgData.items.size(), epgData.itemDescriptions.size());
}

void CEpgData::GetPrevNextEPGData( unsigned long long id, time_t* startzeit )
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
	/* Houdini: dirty RTL double event workaround, if prev/next event has same starttime as actual event skip it */
	if ((prev_zeit == *startzeit) && ((i-1) > 0))
	{
		prev_id   = evtlist[i- 2].eventID;
		prev_zeit = evtlist[i- 2].startTime;
	}
	if ((next_zeit == *startzeit) && ((i+1) < (evtlist.size()- 1)))
	{
		next_id   = evtlist[i+ 2].eventID;
		next_zeit = evtlist[i+ 2].startTime;
	}

}

//
// -- get following screenings of this program title
// -- yek! a better class design would be more helpfull
// -- BAD THING: Cross channel screenings will not be shown
// --            $$$TODO
// -- 2002-05-03 rasc
//

int CEpgData::FollowScreenings (const t_channel_id /*channel_id*/, const std::string & title)

{
	CChannelEventList::iterator e;
	struct  tm		*tmStartZeit;
	std::string		screening_dates,screening_nodual;
	int				count;
	char			tmpstr[256]={0};


	count = 0;
	screening_dates = screening_nodual = "";
	// alredy read: evtlist = g_Sectionsd->getEventsServiceKey( channel_id&0xFFFFFFFFFFFFULL );

	for ( e= evtlist.begin(); e != evtlist.end(); ++e )
	{
		if (e->startTime <= tmp_curent_zeit) continue;
		if (! e->eventID) continue;
		if (e->description == title) {
			count++;
			tmStartZeit = localtime(&(e->startTime));

			screening_dates = "    ";

			screening_dates += g_Locale->getText(CLocaleManager::getWeekday(tmStartZeit));
			screening_dates += '.';

			strftime(tmpstr, sizeof(tmpstr), "  %d.", tmStartZeit );
			screening_dates += tmpstr;

			screening_dates += g_Locale->getText(CLocaleManager::getMonth(tmStartZeit));

			strftime(tmpstr, sizeof(tmpstr), ".  %H:%M ", tmStartZeit );
			screening_dates += tmpstr;
			if (screening_dates != screening_nodual) {
				screening_nodual=screening_dates;
				processTextToArray(screening_dates ); // UTF-8
			}
		}
	}
	if (count == 0)
		processTextToArray("---\n"); // UTF-8
	return count;
}


//
// -- Just display or hide TimerEventbar
// -- 2002-05-13 rasc
//

void CEpgData::showTimerEventBar (bool pshow)

{
	int  x,y,w,h;
	int  cellwidth;		// 4 cells
	int  h_offset, pos;

	w = ox;
	h = 30;
	x = sx;
	y = sy + oy;
	h_offset = 5;
	cellwidth = w / 4;


	frameBuffer->paintBackgroundBoxRel(x,y,w,h);
	// hide only?
	if (! pshow) return;

	frameBuffer->paintBoxRel(x,y,w,h, COL_MENUHEAD_PLUS_0, ROUND_RADIUS, CORNER_BOTTOM);//round

	// Button: Timer Record & Channelswitch
	if (g_settings.recording_type != CNeutrinoApp::RECORDING_OFF)
	{
		pos = 0;
		frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_RED, x+8+cellwidth*pos, y+h_offset );
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(x+29+cellwidth*pos, y+h-h_offset, w-30, g_Locale->getText(LOCALE_TIMERBAR_RECORDEVENT), COL_INFOBAR, 0, true); // UTF-8
	}
	// Button: Timer Channelswitch
	pos = 2;
	frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_YELLOW, x+8+cellwidth*pos, y+h_offset );
	g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(x+29+cellwidth*pos, y+h-h_offset, w-30, g_Locale->getText(LOCALE_TIMERBAR_CHANNELSWITCH), COL_INFOBAR, 0, true); // UTF-8
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
