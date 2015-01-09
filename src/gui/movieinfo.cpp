/***************************************************************************
	Neutrino-GUI  -   DBoxII-Project

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

	***********************************************************

	Module Name: movieinfo.cpp .

	Description: Implementation of the CMovieInfo class
	             This class loads, saves and shows the movie Information from the any .xml File on HD

	Date:	  Nov 2005

	Author: Günther@tuxbox.berlios.org

	Copyright(C) 2009, 2012 Stefan Seyfried

****************************************************************************/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <gui/widget/msgbox.h>
#include <gui/movieinfo.h>
#include <system/helpers.h>

#include <neutrino.h>

// #define XMLTREE_LIB
#ifdef XMLTREE_LIB
#include <xmltree/xmltree.h>
#include <xmltree/xmltok.h>
#endif
#define TRACE printf

/************************************************************************

************************************************************************/
CMovieInfo::CMovieInfo()
{
	//TRACE("[mi] new\r\n");
}

CMovieInfo::~CMovieInfo()
{
	//TRACE("[mi] del\r\n");
	;
}

/************************************************************************

************************************************************************/
bool CMovieInfo::convertTs2XmlName(std::string& filename)
{
	size_t lastdot = filename.find_last_of(".");
	if (lastdot != string::npos) {
		filename.erase(lastdot + 1);
		filename.append("xml");
		return true;
	}
	return false;
}

/************************************************************************

************************************************************************/
static void XML_ADD_TAG_STRING(std::string &_xml_text_, const char *_tag_name_, std::string _tag_content_)
{
	_xml_text_ += "\t\t<";
	_xml_text_ += _tag_name_;
	_xml_text_ += ">";
	_xml_text_ += ZapitTools::UTF8_to_UTF8XML(_tag_content_.c_str());
	_xml_text_ += "</";
	_xml_text_ += _tag_name_;
	_xml_text_ += ">\n";
}

static void XML_ADD_TAG_UNSIGNED(std::string &_xml_text_, const char *_tag_name_, unsigned int _tag_content_)
{
	_xml_text_ += "\t\t<";
	_xml_text_ += _tag_name_;
	_xml_text_ += ">";
	_xml_text_ += to_string(_tag_content_);
	_xml_text_ += "</";
	_xml_text_ += _tag_name_;
	_xml_text_ += ">\n";
}

static void XML_ADD_TAG_LONG(std::string &_xml_text_, const char *_tag_name_, uint64_t _tag_content_)
{
	_xml_text_ += "\t\t<";
	_xml_text_ += _tag_name_;
	_xml_text_ += ">";\
	_xml_text_ += to_string(_tag_content_);
	_xml_text_ += "</";
	_xml_text_ += _tag_name_;
	_xml_text_ += ">\n";
}

#if 0
std::string decodeXmlSpecialChars(std::string s);

static void XML_GET_DATA_STRING(XMLTreeNode *_node_, const char *_tag_, std::string &_string_dest_)
{
	if(!strcmp(_node_->GetType(), _tag_) && _node_->GetData())
		_string_dest_ = decodeXmlSpecialChars(_node_->GetData());
}

static void XML_GET_DATA_INT(XMLTreeNode *_node_, const char *_tag_, int _int_dest_)
{
	if(!strcmp(_node_->GetType(), _tag_) && _node_->GetData())
		_int_dest_ = atoi(_node_->GetData());
}

static void XML_GET_DATA_LONG(XMLTreeNode *_node_, const char *_tag_,long int _int_dest_)
{
	if(!strcmp(_node_->GetType(), _tag_) && _node_->GetData());
		sscanf(_node_->GetData(), "%llu", &_int_dest_);
}
#endif

bool CMovieInfo::encodeMovieInfoXml(std::string * extMessage, MI_MOVIE_INFO * movie_info)
{
	//TRACE("[mi]->encodeMovieInfoXml\r\n");

	*extMessage = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n"
			"<" MI_XML_TAG_NEUTRINO " commandversion=\"1\">\n"
			"\t<" MI_XML_TAG_RECORD " command=\""
			"record"
			"\">\n";
	XML_ADD_TAG_STRING(*extMessage, MI_XML_TAG_CHANNELNAME, movie_info->epgChannel);
	XML_ADD_TAG_STRING(*extMessage, MI_XML_TAG_EPGTITLE, movie_info->epgTitle);
	XML_ADD_TAG_LONG(*extMessage, MI_XML_TAG_ID, movie_info->epgId);
	XML_ADD_TAG_STRING(*extMessage, MI_XML_TAG_INFO1, movie_info->epgInfo1);
	XML_ADD_TAG_STRING(*extMessage, MI_XML_TAG_INFO2, movie_info->epgInfo2);
	XML_ADD_TAG_LONG(*extMessage, MI_XML_TAG_EPGID, movie_info->epgEpgId);	// %llu
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_MODE, movie_info->epgMode);	//%d
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_VIDEOPID, movie_info->epgVideoPid);	//%u
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_VIDEOTYPE, movie_info->VideoType);	//%u
	if ( !movie_info->audioPids.empty() ) {
		*extMessage += "\t\t<" MI_XML_TAG_AUDIOPIDS ">\n";

		for (unsigned int i = 0; i < movie_info->audioPids.size(); i++)	// pids.APIDs.size()
		{
			*extMessage += "\t\t\t<" MI_XML_TAG_AUDIO " " MI_XML_TAG_PID "=\"";
			*extMessage += to_string(movie_info->audioPids[i].epgAudioPid);
			*extMessage += "\" " MI_XML_TAG_ATYPE "=\"";
			*extMessage += to_string(movie_info->audioPids[i].atype);
			*extMessage += "\" " MI_XML_TAG_SELECTED "=\"";
			*extMessage += to_string(movie_info->audioPids[i].selected);
			*extMessage += "\" " MI_XML_TAG_NAME "=\"";
			*extMessage += ZapitTools::UTF8_to_UTF8XML(movie_info->audioPids[i].epgAudioPidName.c_str());
			*extMessage += "\"/>\n";
		}
		*extMessage += "\t\t</" MI_XML_TAG_AUDIOPIDS ">\n";
	}
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_VTXTPID, movie_info->epgVTXPID);	//%u
	/*****************************************************
	 *	new tags				*/
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_GENRE_MAJOR, movie_info->genreMajor);
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_GENRE_MINOR, movie_info->genreMinor);
	XML_ADD_TAG_STRING(*extMessage, MI_XML_TAG_SERIE_NAME, movie_info->serieName);
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_LENGTH, movie_info->length);
	XML_ADD_TAG_STRING(*extMessage, MI_XML_TAG_PRODUCT_COUNTRY, movie_info->productionCountry);
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_PRODUCT_DATE, movie_info->productionDate);
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_QUALITY, movie_info->quality);
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_PARENTAL_LOCKAGE, movie_info->parentalLockAge);
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_DATE_OF_LAST_PLAY, movie_info->dateOfLastPlay);
	*extMessage += "\t\t<" MI_XML_TAG_BOOKMARK ">\n"
			"\t";
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_BOOKMARK_START, movie_info->bookmarks.start);
	*extMessage += "\t";
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_BOOKMARK_END, movie_info->bookmarks.end);
	*extMessage += "\t";
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_BOOKMARK_LAST, movie_info->bookmarks.lastPlayStop);
	for (int i = 0; i < MI_MOVIE_BOOK_USER_MAX; i++) {
		if (movie_info->bookmarks.user[i].pos != 0 || i == 0) {
			// encode any valid book, at least 1
			*extMessage += "\t\t\t<" MI_XML_TAG_BOOKMARK_USER " " MI_XML_TAG_BOOKMARK_USER_POS "=\"";
			*extMessage += to_string(movie_info->bookmarks.user[i].pos);
			*extMessage += "\" " MI_XML_TAG_BOOKMARK_USER_TYPE "=\"";
			*extMessage += to_string(movie_info->bookmarks.user[i].length);
			*extMessage += "\" " MI_XML_TAG_BOOKMARK_USER_NAME "=\"";
			*extMessage += ZapitTools::UTF8_to_UTF8XML(movie_info->bookmarks.user[i].name.c_str());
			*extMessage += "\"/>\n";
		}
	}

	*extMessage += "\t\t</" MI_XML_TAG_BOOKMARK ">\n"
			"\t</" MI_XML_TAG_RECORD ">\n"
			"</" MI_XML_TAG_NEUTRINO ">\n";
	return true;
}

/************************************************************************

************************************************************************/
bool CMovieInfo::saveMovieInfo(MI_MOVIE_INFO & movie_info, CFile * file)
{
	//TRACE("[mi]->saveXml \r\n");
	bool result = true;
	std::string text;
	CFile file_xml;

	if (file == NULL) {
		file_xml.Name = movie_info.file.Name;
		result = convertTs2XmlName(file_xml.Name);
	} else {
		file_xml.Name = file->Name;
	}
	TRACE("[mi] saveXml: %s\r\n", file_xml.Name.c_str());

	if (result == true) {
		result = encodeMovieInfoXml(&text, &movie_info);
		if (result == true) {
			result = saveFile(file_xml, text);	// save
			if (result == false) {
				TRACE("[mi] saveXml: save error\r\n");
			}
		} else {
			TRACE("[mi] saveXml: encoding error\r\n");
		}
	} else {
		TRACE("[mi] saveXml: error\r\n");
	}
	return (result);
}

/************************************************************************

************************************************************************/
bool CMovieInfo::loadMovieInfo(MI_MOVIE_INFO * movie_info, CFile * file)
{
	//TRACE("[mi]->loadMovieInfo \r\n");
	bool result = true;
	CFile file_xml;

	if (file == NULL) {
		// if there is no give file, we use the file name from movie info but we have to convert the ts name to xml name first
		file_xml.Name = movie_info->file.Name;
		result = convertTs2XmlName(file_xml.Name);
	} else {
		file_xml.Name = file->Name;
	}

	if (result == true) {
		// load xml file in buffer
		std::string text;
		result = loadFile(file_xml, text);
		if (result == true) {
#ifdef XMLTREE_LIB
			result = parseXmlTree(text, movie_info);
#else /* XMLTREE_LIB */
			result = parseXmlQuickFix(text, movie_info);
#endif /* XMLTREE_LIB */
		}
	}
	if (movie_info->productionDate > 50 && movie_info->productionDate < 200)	// backwardcompaibility
		movie_info->productionDate += 1900;

	return (result);
}

/************************************************************************

************************************************************************/
#if 0 
//never used
bool CMovieInfo::parseXmlTree(char */*text*/, MI_MOVIE_INFO * /*movie_info*/)
{
#ifndef XMLTREE_LIB
	return (false);		// no XML lib available return false
#else /* XMLTREE_LIB */

	//int helpIDtoLoad = 80;

	//XMLTreeParser *parser=new XMLTreeParser("ISO-8859-1");
	XMLTreeParser *parser = new XMLTreeParser(NULL);

	if (!parser->Parse(text, strlen(text), 1)) {
		TRACE("parse error: %s at line %d \r\n", parser->ErrorString(parser->GetErrorCode()), parser->GetCurrentLineNumber());
		//fclose(in);
		delete parser;
		return (false);
	}

	XMLTreeNode *root = parser->RootNode();
	if (!root) {
		TRACE(" root error \r\n");
		delete parser;
		return (false);
	}

	if (strcmp(root->GetType(), MI_XML_TAG_NEUTRINO)) {
		TRACE("not neutrino file. %s", root->GetType());
		delete parser;
		return (false);
	}

	XMLTreeNode *node = parser->RootNode();

	for (node = node->GetChild(); node; node = node->GetNext()) {
		if (!strcmp(node->GetType(), MI_XML_TAG_RECORD)) {
			for (XMLTreeNode * xam1 = node->GetChild(); xam1; xam1 = xam1->GetNext()) {
				XML_GET_DATA_STRING(xam1, MI_XML_TAG_CHANNELNAME, movie_info->epgChannel);
				XML_GET_DATA_STRING(xam1, MI_XML_TAG_EPGTITLE, movie_info->epgTitle);
				XML_GET_DATA_LONG(xam1, MI_XML_TAG_ID, movie_info->epgId);
				XML_GET_DATA_STRING(xam1, MI_XML_TAG_INFO1, movie_info->epgInfo1);
				XML_GET_DATA_STRING(xam1, MI_XML_TAG_INFO2, movie_info->epgInfo2);
				XML_GET_DATA_LONG(xam1, MI_XML_TAG_EPGID, movie_info->epgEpgId);	// %llu
				XML_GET_DATA_INT(xam1, MI_XML_TAG_MODE, movie_info->epgMode);	//%d
				XML_GET_DATA_INT(xam1, MI_XML_TAG_VIDEOPID, movie_info->epgVideoPid);	//%u
				XML_GET_DATA_INT(xam1, MI_XML_TAG_VIDEOTYPE, movie_info->VideoType);	//%u
				if (!strcmp(xam1->GetType(), MI_XML_TAG_AUDIOPIDS)) {
					for (XMLTreeNode * xam2 = xam1->GetChild(); xam2; xam2 = xam2->GetNext()) {
						if (!strcmp(xam2->GetType(), MI_XML_TAG_AUDIO)) {
							EPG_AUDIO_PIDS pids;
							pids.epgAudioPid = atoi(xam2->GetAttributeValue(MI_XML_TAG_PID));
							pids.atype = atoi(xam2->GetAttributeValue(MI_XML_TAG_ATYPE));
							pids.selected = atoi(xam2->GetAttributeValue(MI_XML_TAG_SELECTED));
							pids.epgAudioPidName = decodeXmlSpecialChars(xam2->GetAttributeValue(MI_XML_TAG_NAME));
//printf("MOVIE INFO: apid %d type %d name %s selected %d\n", pids.epgAudioPid, pids.atype, pids.epgAudioPidName.c_str(), pids.selected);
							movie_info->audioPids.push_back(pids);
						}
					}
				}
				XML_GET_DATA_INT(xam1, MI_XML_TAG_VTXTPID, movie_info->epgVTXPID);	//%u
				/*****************************************************
				 *	new tags										*/
				XML_GET_DATA_INT(xam1, MI_XML_TAG_GENRE_MAJOR, movie_info->genreMajor);
				XML_GET_DATA_INT(xam1, MI_XML_TAG_GENRE_MINOR, movie_info->genreMinor);
				XML_GET_DATA_STRING(xam1, MI_XML_TAG_SERIE_NAME, movie_info->serieName);
				XML_GET_DATA_INT(xam1, MI_XML_TAG_LENGTH, movie_info->length);
				XML_GET_DATA_STRING(xam1, MI_XML_TAG_PRODUCT_COUNTRY, movie_info->productionCountry);
				//if(!strcmp(xam1->GetType(), MI_XML_TAG_PRODUCT_COUNTRY)) if(xam1->GetData() != NULL)strncpy(movie_info->productionCountry, xam1->GetData(),4);
				XML_GET_DATA_INT(xam1, MI_XML_TAG_PRODUCT_DATE, movie_info->productionDate);
				XML_GET_DATA_INT(xam1, MI_XML_TAG_QUALITIY, movie_info->quality);
				XML_GET_DATA_INT(xam1, MI_XML_TAG_QUALITY, movie_info->quality);
				XML_GET_DATA_INT(xam1, MI_XML_TAG_PARENTAL_LOCKAGE, movie_info->parentalLockAge);
				XML_GET_DATA_INT(xam1, MI_XML_TAG_DATE_OF_LAST_PLAY, movie_info->dateOfLastPlay);

				if (!strcmp(xam1->GetType(), MI_XML_TAG_BOOKMARK)) {
					for (XMLTreeNode * xam2 = xam1->GetChild(); xam2; xam2 = xam2->GetNext()) {
						XML_GET_DATA_INT(xam2, MI_XML_TAG_BOOKMARK_START, movie_info->bookmarks.start);
						XML_GET_DATA_INT(xam2, MI_XML_TAG_BOOKMARK_END, movie_info->bookmarks.end);
						XML_GET_DATA_INT(xam2, MI_XML_TAG_BOOKMARK_LAST, movie_info->bookmarks.lastPlayStop);
					}
				}
				/*****************************************************/
			}
		}
	}

	delete parser;
	if (movie_info->epgInfo2.empty()) {
		movie_info->epgInfo2 = movie_info->epgInfo1;
		//movie_info->epgInfo1 = "";
	}
#endif /* XMLTREE_LIB */
	return (true);
}
#endif
/************************************************************************

************************************************************************/
void CMovieInfo::showMovieInfo(MI_MOVIE_INFO & movie_info)
{
	std::string print_buffer = movie_info.epgInfo1;
	print_buffer += "\n";
	if (movie_info.epgInfo1 != movie_info.epgInfo2) {
		print_buffer += movie_info.epgInfo2;
		print_buffer += "\n";
	}

	if ( !movie_info.productionCountry.empty() || movie_info.productionDate != 0) {
		print_buffer += movie_info.productionCountry;
		print_buffer += to_string(movie_info.productionDate + 1900);
		print_buffer += "\n";
	}
	if (!movie_info.serieName.empty()) {
		print_buffer += "\n";
		print_buffer += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_SERIE);
		print_buffer += ": ";
		print_buffer += movie_info.serieName;
		print_buffer += "\n";
	}
	if (!movie_info.epgChannel.empty()) {
		print_buffer += "\n";
		print_buffer += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_CHANNEL);
		print_buffer += ": ";
		print_buffer += movie_info.epgChannel;
		print_buffer += "\n";
	}
	if (movie_info.quality != 0) {
		print_buffer += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_QUALITY);
		print_buffer += ": ";
		print_buffer += to_string(movie_info.quality);
		print_buffer += "\n";
	}
	if (movie_info.parentalLockAge != 0) {
		print_buffer += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_PARENTAL_LOCKAGE);
		print_buffer += ": ";
		print_buffer += to_string(movie_info.parentalLockAge);
		print_buffer += " ";
		print_buffer += g_Locale->getText(LOCALE_UNIT_LONG_YEARS);
		print_buffer += "\n";
	}
	if (movie_info.length != 0) {
		print_buffer += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_LENGTH);
		print_buffer += ": ";
		print_buffer += to_string(movie_info.length);
		print_buffer += "\n";
	}
	if ( !movie_info.audioPids.empty() ) {
		print_buffer += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_AUDIO);
		print_buffer += ": ";
		for (unsigned int i = 0; i < movie_info.audioPids.size(); i++) {
			if (i)
				print_buffer += ", ";
			print_buffer += movie_info.audioPids[i].epgAudioPidName;
		}
		print_buffer += "\n";
	}
	if (movie_info.genreMajor != 0)
	{
		neutrino_locale_t locale_genre;
		unsigned char i = (movie_info.genreMajor & 0x0F0);
		if (i >= 0x010 && i < 0x0B0)
		{
			i >>= 4;
			i--;
			locale_genre = genre_sub_classes_list[i][((movie_info.genreMajor & 0x0F) < genre_sub_classes[i]) ? (movie_info.genreMajor & 0x0F) : 0];
		}
		else
			locale_genre = LOCALE_GENRE_UNKNOWN;
		print_buffer += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_GENRE_MAJOR);
		print_buffer += ": ";
		print_buffer += g_Locale->getText(locale_genre);
		print_buffer += "\n";
	}

	tm *date_tm = localtime(&movie_info.dateOfLastPlay);
	print_buffer += "\n";
	print_buffer += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_PREVPLAYDATE);
	print_buffer += ": ";
	print_buffer += strftime("%F", date_tm);
	print_buffer += "\n";

	date_tm = localtime(&movie_info.file.Time);
	print_buffer += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_RECORDDATE);
	print_buffer += ": ";
	print_buffer += strftime("%F", date_tm);
	print_buffer += "\n";

	if (movie_info.file.Size != 0) {
		print_buffer += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_SIZE);
		print_buffer += ": ";
		print_buffer += to_string(movie_info.file.Size >> 20);
		print_buffer += "\n";
	}

	print_buffer += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_FILE);
	print_buffer += ": ";
	print_buffer += movie_info.file.Name;
	print_buffer += "\n";

	ShowMsg2UTF(movie_info.epgTitle.empty()? movie_info.file.getFileName().c_str() : movie_info.epgTitle.c_str(), print_buffer.c_str(), CMsgBox::mbrBack, CMsgBox::mbBack);	// UTF-8*/

}

/************************************************************************

************************************************************************/
#if 0 
//never used
void CMovieInfo::printDebugMovieInfo(MI_MOVIE_INFO & movie_info)
{
	TRACE(" FileName: %s", movie_info.file.Name.c_str());
	//TRACE(" FilePath: %s", movie_info.file.GetFilePath );
	//TRACE(" FileLength: %d", movie_info.file.GetLength );
	//TRACE(" FileStatus: %d", movie_info.file.GetStatus );

	TRACE(" ********** Movie Data ***********\r\n");	// (date, month, year)
	TRACE(" dateOfLastPlay: \t%d\r\n", (int)movie_info.dateOfLastPlay);	// (date, month, year)
	TRACE(" dirItNr: \t\t%d\r\n", movie_info.dirItNr);	//
	TRACE(" genreMajor: \t\t%d\r\n", movie_info.genreMajor);	//genreMajor;
	TRACE(" genreMinor: \t\t%d\r\n", movie_info.genreMinor);	//genreMinor;
	TRACE(" length: \t\t%d\r\n", movie_info.length);	// (minutes)
	TRACE(" quality: \t\t%d\r\n", movie_info.quality);	// (3 stars: classics, 2 stars: very good, 1 star: good, 0 stars: OK)
	TRACE(" productionCount:\t>%s<\r\n", movie_info.productionCountry.c_str());
	TRACE(" productionDate: \t%d\r\n", movie_info.productionDate);	// (Year)  years since 1900
	TRACE(" parentalLockAge: \t\t\t%d\r\n", movie_info.parentalLockAge);	// MI_PARENTAL_LOCKAGE (0,6,12,16,18)
	TRACE(" format: \t\t%d\r\n", movie_info.format);	// MI_VIDEO_FORMAT(16:9, 4:3)
	TRACE(" audio: \t\t%d\r\n", movie_info.audio);	// MI_AUDIO (AC3, Deutsch, Englisch)
	TRACE(" epgId: \t\t%d\r\n", (int)movie_info.epgId);
	TRACE(" epgEpgId: \t\t%llu\r\n", movie_info.epgEpgId);
	TRACE(" epgMode: \t\t%d\r\n", movie_info.epgMode);
	TRACE(" epgVideoPid: \t\t%d\r\n", movie_info.epgVideoPid);
	TRACE(" epgVTXPID: \t\t%d\r\n", movie_info.epgVTXPID);
	TRACE(" Size: \t\t%d\r\n", (int)movie_info.file.Size >> 20);
	TRACE(" Date: \t\t%d\r\n", (int)movie_info.file.Time);

	for (unsigned int i = 0; i < movie_info.audioPids.size(); i++) {
		TRACE(" audioPid (%d): \t\t%d\r\n", i, movie_info.audioPids[i].epgAudioPid);
		TRACE(" audioName(%d): \t\t>%s<\r\n", i, movie_info.audioPids[i].epgAudioPidName.c_str());
	}

	TRACE(" epgTitle: \t\t>%s<\r\n", movie_info.epgTitle.c_str());
	TRACE(" epgInfo1:\t\t>%s<\r\n", movie_info.epgInfo1.c_str());	//epgInfo1
	TRACE(" epgInfo2:\t\t\t>%s<\r\n", movie_info.epgInfo2.c_str());	//epgInfo2
	TRACE(" epgChannel:\t\t>%s<\r\n", movie_info.epgChannel.c_str());
	TRACE(" serieName:\t\t>%s<\r\n", movie_info.serieName.c_str());	// (name e.g. 'StarWars)

	TRACE(" bookmarks start: \t%d\r\n", movie_info.bookmarks.start);
	TRACE(" bookmarks end: \t%d\r\n", movie_info.bookmarks.end);
	TRACE(" bookmarks lastPlayStop: %d\r\n", movie_info.bookmarks.lastPlayStop);

	for (int i = 0; i < MI_MOVIE_BOOK_USER_MAX; i++) {
		if (movie_info.bookmarks.user[i].pos != 0 || i == 0) {
			TRACE(" bookmarks user, pos:%d, type:%d, name: >%s<\r\n", movie_info.bookmarks.user[i].pos, movie_info.bookmarks.user[i].length, movie_info.bookmarks.user[i].name.c_str());
		}
	}
}
#endif
/************************************************************************

************************************************************************/
static int find_next_char(char to_find, const char *text, int start_pos, int end_pos)
{
	while (start_pos < end_pos) {
		if (text[start_pos] == to_find) {
			return (start_pos);
		}
		start_pos++;
	}
	return (-1);
}

#define GET_XML_DATA_STRING(_text_,_pos_,_tag_,_dest_)\
	if(strncmp(&_text_[_pos_],_tag_,sizeof(_tag_)-1) == 0)\
	{\
		_pos_ += sizeof(_tag_) ;\
		int pos_prev = _pos_;\
		while(_pos_ < bytes && _text_[_pos_] != '<' ) _pos_++;\
		_dest_ = "";\
		_dest_.append(&_text_[pos_prev],_pos_ - pos_prev );\
		_dest_ = decodeXmlSpecialChars(_dest_);\
		_pos_ += sizeof(_tag_);\
		continue;\
	}

#define GET_XML_DATA_INT(_text_,_pos_,_tag_,_dest_)\
	if(strncmp(&_text_[pos],_tag_,sizeof(_tag_)-1) == 0)\
	{\
		_pos_ += sizeof(_tag_) ;\
		int pos_prev = _pos_;\
		while(_pos_ < bytes && _text_[_pos_] != '<' ) pos++;\
		_dest_ = atoi(&_text_[pos_prev]);\
		continue;\
	}
#define GET_XML_DATA_LONG(_text_,_pos_,_tag_,_dest_)\
	if(strncmp(&_text_[pos],_tag_,sizeof(_tag_)-1) == 0)\
	{\
		_pos_ += sizeof(_tag_) ;\
		int pos_prev = _pos_;\
		while(_pos_ < bytes && _text_[_pos_] != '<' ) pos++;\
		_dest_ = strtoull(&_text_[pos_prev], NULL, 10); /*atoll(&_text_[pos_prev]);*/\
		continue;\
	}

//void CMovieInfo::strReplace(std::string& orig, const char* fstr, const std::string rstr)
void strReplace(std::string & orig, const char *fstr, const std::string &rstr)
{
//      replace all occurrence of fstr with rstr and, and returns a reference to itself
	size_t index = 0;
	size_t fstrlen = strlen(fstr);
	size_t rstrlen = rstr.size();

	while ((index = orig.find(fstr, index)) != std::string::npos) {
		orig.replace(index, fstrlen, rstr);
		index += rstrlen;
	}
}

std::string decodeXmlSpecialChars(std::string s)
{
	strReplace(s,"&lt;","<");
	strReplace(s,"&gt;",">");
	strReplace(s,"&amp;","&");
	strReplace(s,"&quot;","\"");
	strReplace(s,"&apos;","\'");
	return s;
}

 /************************************************************************
************************************************************************/
bool CMovieInfo::parseXmlQuickFix(std::string &_text, MI_MOVIE_INFO * movie_info)
{
#ifndef XMLTREE_LIB
	int bookmark_nr = 0;
	movie_info->dateOfLastPlay = 0;	//100*366*24*60*60;              // (date, month, year)
	//bool result = false;

	const char *text = _text.c_str();
	int bytes = _text.length();
	/** search ****/
	int pos = 0;

	EPG_AUDIO_PIDS audio_pids;

	while ((pos = find_next_char('<', text, pos, bytes)) != -1) {
		pos++;
		GET_XML_DATA_STRING(text, pos, MI_XML_TAG_CHANNELNAME, movie_info->epgChannel)
		GET_XML_DATA_STRING(text, pos, MI_XML_TAG_EPGTITLE, movie_info->epgTitle)
		GET_XML_DATA_LONG(text, pos, MI_XML_TAG_ID, movie_info->epgId)
		GET_XML_DATA_STRING(text, pos, MI_XML_TAG_INFO1, movie_info->epgInfo1)
		GET_XML_DATA_STRING(text, pos, MI_XML_TAG_INFO2, movie_info->epgInfo2)
		GET_XML_DATA_LONG(text, pos, MI_XML_TAG_EPGID, movie_info->epgEpgId)
		GET_XML_DATA_INT(text, pos, MI_XML_TAG_MODE, movie_info->epgMode)
		GET_XML_DATA_INT(text, pos, MI_XML_TAG_VIDEOPID, movie_info->epgVideoPid)
		GET_XML_DATA_INT(text, pos, MI_XML_TAG_VIDEOTYPE, movie_info->VideoType)
		GET_XML_DATA_STRING(text, pos, MI_XML_TAG_NAME, movie_info->epgChannel)
		GET_XML_DATA_INT(text, pos, MI_XML_TAG_VTXTPID, movie_info->epgVTXPID)
		GET_XML_DATA_INT(text, pos, MI_XML_TAG_GENRE_MAJOR, movie_info->genreMajor)
		GET_XML_DATA_INT(text, pos, MI_XML_TAG_GENRE_MINOR, movie_info->genreMinor)
		GET_XML_DATA_STRING(text, pos, MI_XML_TAG_SERIE_NAME, movie_info->serieName)
		GET_XML_DATA_INT(text, pos, MI_XML_TAG_LENGTH, movie_info->length)
		GET_XML_DATA_STRING(text, pos, MI_XML_TAG_PRODUCT_COUNTRY, movie_info->productionCountry)
		GET_XML_DATA_INT(text, pos, MI_XML_TAG_PRODUCT_DATE, movie_info->productionDate)
		GET_XML_DATA_INT(text, pos, MI_XML_TAG_PARENTAL_LOCKAGE, movie_info->parentalLockAge)
		GET_XML_DATA_INT(text, pos, MI_XML_TAG_QUALITIY, movie_info->quality)
		GET_XML_DATA_INT(text, pos, MI_XML_TAG_QUALITY, movie_info->quality)
		GET_XML_DATA_INT(text, pos, MI_XML_TAG_DATE_OF_LAST_PLAY, movie_info->dateOfLastPlay)

		if (strncmp(&text[pos], MI_XML_TAG_AUDIOPIDS, sizeof(MI_XML_TAG_AUDIOPIDS) - 1) == 0)
			pos += sizeof(MI_XML_TAG_AUDIOPIDS);

		/* parse audio pids */
		if (strncmp(&text[pos], MI_XML_TAG_AUDIO, sizeof(MI_XML_TAG_AUDIO) - 1) == 0) {
			pos += sizeof(MI_XML_TAG_AUDIO);

			int pos2;
			const char *ptr;

			pos2 = -1;
			ptr = strstr(&text[pos], MI_XML_TAG_PID);
			if (ptr)
				pos2 = (int)ptr - (int)&text[pos];
			//pos2 = strcspn(&text[pos],MI_XML_TAG_PID);
			if (pos2 >= 0) {
				pos2 += sizeof(MI_XML_TAG_PID);
				while (text[pos + pos2] != '\"' && text[pos + pos2] != 0 && text[pos + pos2] != '/')
					pos2++;
				if (text[pos + pos2] == '\"')
					audio_pids.epgAudioPid = atoi(&text[pos + pos2 + 1]);
			} else
				audio_pids.epgAudioPid = 0;

			audio_pids.atype = 0;
			pos2 = -1;
			ptr = strstr(&text[pos], MI_XML_TAG_ATYPE);
			if (ptr)
				pos2 = (int)ptr - (int)&text[pos];
			//pos2 = strcspn(&text[pos],MI_XML_TAG_ATYPE);
			if (pos2 >= 0) {
				pos2 += sizeof(MI_XML_TAG_ATYPE);
				while (text[pos + pos2] != '\"' && text[pos + pos2] != 0 && text[pos + pos2] != '/')
					pos2++;
				if (text[pos + pos2] == '\"')
					audio_pids.atype = atoi(&text[pos + pos2 + 1]);
			}

			audio_pids.selected = 0;
			pos2 = -1;
			ptr = strstr(&text[pos], MI_XML_TAG_SELECTED);
			if (ptr)
				pos2 = (int)ptr - (int)&text[pos];
			//pos2 = strcspn(&text[pos],MI_XML_TAG_SELECTED);
			if (pos2 >= 0) {
				pos2 += sizeof(MI_XML_TAG_SELECTED);
				while (text[pos + pos2] != '\"' && text[pos + pos2] != 0 && text[pos + pos2] != '/')
					pos2++;
				if (text[pos + pos2] == '\"')
					audio_pids.selected = atoi(&text[pos + pos2 + 1]);
			}

			audio_pids.epgAudioPidName = "";
			//pos2 = strcspn(&text[pos],MI_XML_TAG_NAME);
			pos2 = -1;
			ptr = strstr(&text[pos], MI_XML_TAG_NAME);
			if (ptr)
				pos2 = (int)ptr - (int)&text[pos];
			if (pos2 >= 0) {
				pos2 += sizeof(MI_XML_TAG_PID);
				while (text[pos + pos2] != '\"' && text[pos + pos2] != 0 && text[pos + pos2] != '/')
					pos2++;
				if (text[pos + pos2] == '\"') {
					int pos3 = pos2 + 1;
					while (text[pos + pos3] != '\"' && text[pos + pos3] != 0 && text[pos + pos3] != '/')
						pos3++;
					if (text[pos + pos3] == '\"')
					{
						audio_pids.epgAudioPidName.append(&text[pos + pos2 + 1], pos3 - pos2 - 1);
						audio_pids.epgAudioPidName = decodeXmlSpecialChars(audio_pids.epgAudioPidName);
					}
				}
			}
			//printf("MOVIE INFO: apid %d type %d name %s selected %d\n", audio_pids.epgAudioPid, audio_pids.atype, audio_pids.epgAudioPidName.c_str(), audio_pids.selected);
			movie_info->audioPids.push_back(audio_pids);
		}
		/* parse bookmarks */
		GET_XML_DATA_INT(text, pos, MI_XML_TAG_BOOKMARK_START, movie_info->bookmarks.start)
		    GET_XML_DATA_INT(text, pos, MI_XML_TAG_BOOKMARK_END, movie_info->bookmarks.end)
		    GET_XML_DATA_INT(text, pos, MI_XML_TAG_BOOKMARK_LAST, movie_info->bookmarks.lastPlayStop)

		    if (bookmark_nr < MI_MOVIE_BOOK_USER_MAX) {
			if (strncmp(&text[pos], MI_XML_TAG_BOOKMARK_USER, sizeof(MI_XML_TAG_BOOKMARK_USER) - 1) == 0) {
				pos += sizeof(MI_XML_TAG_BOOKMARK_USER);
				//int pos2 = strcspn(&text[pos],MI_XML_TAG_BOOKMARK_USER_POS);
				if (strcspn(&text[pos], MI_XML_TAG_BOOKMARK_USER_POS) == 0) {
					int pos2 = 0;
					pos2 += sizeof(MI_XML_TAG_BOOKMARK_USER_POS);
					while (text[pos + pos2] != '\"' && text[pos + pos2] != 0 && text[pos + pos2] != '/')
						pos2++;
					if (text[pos + pos2] == '\"') {
						movie_info->bookmarks.user[bookmark_nr].pos = atoi(&text[pos + pos2 + 1]);

						//pos2 = strcspn(&text[pos],MI_XML_TAG_BOOKMARK_USER_TYPE);
						pos++;
						while (text[pos + pos2] == ' ')
							pos++;
						if (strcspn(&text[pos], MI_XML_TAG_BOOKMARK_USER_TYPE) == 0) {
							pos2 += sizeof(MI_XML_TAG_BOOKMARK_USER_TYPE);
							while (text[pos + pos2] != '\"' && text[pos + pos2] != 0 && text[pos + pos2] != '/')
								pos2++;
							if (text[pos + pos2] == '\"') {
								movie_info->bookmarks.user[bookmark_nr].length = atoi(&text[pos + pos2 + 1]);

								movie_info->bookmarks.user[bookmark_nr].name = "";
								//pos2 = ;
								if (strcspn(&text[pos], MI_XML_TAG_BOOKMARK_USER_NAME) == 0) {
									pos2 += sizeof(MI_XML_TAG_BOOKMARK_USER_NAME);
									while (text[pos + pos2] != '\"' && text[pos + pos2] != 0 && text[pos + pos2] != '/')
										pos2++;
									if (text[pos + pos2] == '\"') {
										int pos3 = pos2 + 1;
										while (text[pos + pos3] != '\"' && text[pos + pos3] != 0 && text[pos + pos3] != '/')
											pos3++;
										if (text[pos + pos3] == '\"')
										{
											movie_info->bookmarks.user[bookmark_nr].name.append(&text[pos + pos2 + 1], pos3 - pos2 - 1);
											movie_info->bookmarks.user[bookmark_nr].name = decodeXmlSpecialChars(movie_info->bookmarks.user[bookmark_nr].name);
										}
									}
								}
							}
						} else
							movie_info->bookmarks.user[bookmark_nr].length = 0;
					}
					bookmark_nr++;
				} else
					movie_info->bookmarks.user[bookmark_nr].pos = 0;
			}
		}
	}

	if (movie_info->epgInfo2.empty()) {
		movie_info->epgInfo2 = movie_info->epgInfo1;
		//movie_info->epgInfo1 = "";
	}

	return (true);
#endif
	return (false);
}

/************************************************************************

************************************************************************/
bool CMovieInfo::addNewBookmark(MI_MOVIE_INFO * movie_info, MI_BOOKMARK & new_bookmark)
{
	TRACE("[mi] addNewBookmark\r\n");
	bool result = false;
	if (movie_info != NULL) {
		// search for free entry
		bool loop = true;
		for (int i = 0; i < MI_MOVIE_BOOK_USER_MAX && loop == true; i++) {
			if (movie_info->bookmarks.user[i].pos == 0) {
				// empty entry found
				result = true;
				loop = false;
				movie_info->bookmarks.user[i].pos = new_bookmark.pos;
				movie_info->bookmarks.user[i].length = new_bookmark.length;
				//if(movie_info->bookmarks.user[i].name.empty())
				if (movie_info->bookmarks.user[i].name.empty() ) {
					if (new_bookmark.length == 0)
						movie_info->bookmarks.user[i].name = g_Locale->getText(LOCALE_MOVIEBROWSER_BOOK_NEW);
					if (new_bookmark.length < 0)
						movie_info->bookmarks.user[i].name = g_Locale->getText(LOCALE_MOVIEBROWSER_BOOK_TYPE_BACKWARD);
					if (new_bookmark.length > 0)
						movie_info->bookmarks.user[i].name = g_Locale->getText(LOCALE_MOVIEBROWSER_BOOK_TYPE_FORWARD);
				} else {
					movie_info->bookmarks.user[i].name = new_bookmark.name;
				}
			}
		}
	}
	return (result);
}

/************************************************************************

************************************************************************/

void MI_MOVIE_INFO::clear(void)
{
	tm timePlay;
	timePlay.tm_hour = 0;
	timePlay.tm_min = 0;
	timePlay.tm_sec = 0;
	timePlay.tm_year = 100;
	timePlay.tm_mday = 0;
	timePlay.tm_mon = 1;

	file.Name = "";
	file.Url = "";
	file.Size = 0;	// Megabytes
	file.Time = mktime(&timePlay);
	dateOfLastPlay = mktime(&timePlay);	// (date, month, year)
	dirItNr = 0;	//
	genreMajor = 0;	//genreMajor;
	genreMinor = 0;	//genreMinor;
	length = 0;	// (minutes)
	quality = 0;	// (3 stars: classics, 2 stars: very good, 1 star: good, 0 stars: OK)
	productionDate = 0;	// (Year)  years since 1900
	parentalLockAge = 0;	// MI_PARENTAL_LOCKAGE (0,6,12,16,18)
//	format = 0;	// MI_VIDEO_FORMAT(16:9, 4:3)
//	audio = 0;	// MI_AUDIO (AC3, Deutsch, Englisch)

	epgId = 0;
	epgEpgId = 0;
	epgMode = 0;
	epgVideoPid = 0;
	VideoType = 0;
	epgVTXPID = 0;

	audioPids.clear();

	productionCountry = "";
	epgTitle = "";
	epgInfo1 = "";	//epgInfo1
	epgInfo2 = "";	//epgInfo2
	epgChannel = "";
	serieName = "";	// (name e.g. 'StarWars)
	bookmarks.end = 0;
	bookmarks.start = 0;
	bookmarks.lastPlayStop = 0;
	for (int i = 0; i < MI_MOVIE_BOOK_USER_MAX; i++) {
		bookmarks.user[i].pos = 0;
		bookmarks.user[i].length = 0;
		bookmarks.user[i].name = "";
	}
	tfile = "";
	ytdate = "";
	ytid = "";
	ytitag = 0;
	marked = false;
}

/************************************************************************

************************************************************************/
bool CMovieInfo::loadFile(CFile & file, std::string &buffer)
{
	bool result = true;

	int fd = open(file.Name.c_str(), O_RDONLY);
	if (fd == -1)		// cannot open file, return!!!!!
	{
		TRACE("[mi] loadXml: cannot open (%s)\r\n", file.Name.c_str());
		return false;
	}
	struct stat st;
	if (fstat(fd, &st)) {
		close(fd);
		return false;
	}
	char buf[st.st_size];
	if (st.st_size != read(fd, buf, st.st_size)) {
		TRACE("[mi] loadXml: cannot read (%s)\r\n", file.Name.c_str());
		result = false;
	} else
		buffer = std::string(buf, st.st_size);

	close(fd);

	return result;
}

/************************************************************************

************************************************************************/
bool CMovieInfo::saveFile(const CFile & file, std::string &text)
{
	bool result = false;
	int fd;
	if ((fd = open(file.Name.c_str(), O_SYNC | O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) >= 0) {
		/*int nr=*/ 
		write(fd, text.c_str(), text.size());
		//fdatasync(fd);
		close(fd);
		result = true;
		//TRACE("[mi] saved (%d)\r\n",nr);
	} else {
		TRACE("[mi] ERROR: cannot open\r\n");
	}
	return (result);
}
