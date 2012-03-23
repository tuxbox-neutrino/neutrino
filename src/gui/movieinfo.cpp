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

	Revision History:
	Date			Author		Change Description
	Nov 2005		Günther	initial start

****************************************************************************/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <gui/widget/msgbox.h>
#include <gui/movieinfo.h>

//#define XMLTREE_LIB
#ifdef XMLTREE_LIB
#include <xmltree/xmltree.h>
#include <xmltree/xmltok.h>
#endif
#define TRACE printf
#define VLC_URI "vlc://"

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
bool CMovieInfo::convertTs2XmlName(char *char_filename, int size)
{
	bool result = false;
	std::string filename = char_filename;
	if (convertTs2XmlName(&filename) == true) {
		strncpy(char_filename, filename.c_str(), size);
		char_filename[size - 1] = 0;
		result = true;
	}
	return (result);
}

/************************************************************************

************************************************************************/
bool CMovieInfo::convertTs2XmlName(std::string * filename)
{
	//TRACE("[mi]->convertTs2XmlName\r\n");
	int bytes = filename->find(".ts");
	bool result = false;

	if (bytes != -1) {
		if (bytes > 3) {
			if ((*filename)[bytes - 4] == '.') {
				bytes = bytes - 4;
			}
		}
		*filename = filename->substr(0, bytes) + ".xml";
		result = true;
	} else			// not a TS file, return!!!!!
	{
		//TRACE("    not a TS file ");
	}

	return (result);
}

/************************************************************************

************************************************************************/
#define XML_ADD_TAG_STRING(_xml_text_,_tag_name_,_tag_content_){ \
	_xml_text_ += "\t\t<"_tag_name_">"; \
	_xml_text_ += ZapitTools::UTF8_to_UTF8XML(_tag_content_.c_str()); \
	_xml_text_ += "</"_tag_name_">\n";}

#define XML_ADD_TAG_UNSIGNED(_xml_text_,_tag_name_,_tag_content_){\
	_xml_text_ +=	"\t\t<"_tag_name_">";\
	char _tmp_[50];\
	sprintf(_tmp_, "%u", (unsigned int) _tag_content_);\
	_xml_text_ +=	_tmp_;\
	_xml_text_ +=	"</"_tag_name_">\n";}

#define XML_ADD_TAG_LONG(_xml_text_,_tag_name_,_tag_content_){\
	_xml_text_ +=	"\t\t<"_tag_name_">";\
	char _tmp_[50];\
	sprintf(_tmp_, "%llu", _tag_content_);\
	_xml_text_ +=	_tmp_;\
	_xml_text_ +=	"</"_tag_name_">\n";}

#define	XML_GET_DATA_STRING(_node_,_tag_,_string_dest_){\
	if(!strcmp(_node_->GetType(), _tag_))\
	{\
		if(_node_->GetData() != NULL)\
		{\
			_string_dest_ = decodeXmlSpecialChars(_node_->GetData());\
		}\
	}}
#define	XML_GET_DATA_INT(_node_,_tag_,_int_dest_){\
	if(!strcmp(_node_->GetType(), _tag_))\
	{\
		if(_node_->GetData() != NULL)\
		{\
			_int_dest_ = atoi(_node_->GetData());\
		}\
	}}

#define	XML_GET_DATA_LONG(_node_,_tag_,_int_dest_){\
	if(!strcmp(_node_->GetType(), _tag_))\
	{\
		if(_node_->GetData() != NULL)\
		{\
			sscanf(_node_->GetData(), "%llu", &_int_dest_); \
		}\
	}}
//sscanf(_node_->GetData(), "%lld", &_int_dest_);

bool CMovieInfo::encodeMovieInfoXml(std::string * extMessage, MI_MOVIE_INFO * movie_info)
{
	//TRACE("[mi]->encodeMovieInfoXml\r\n");
	char tmp[40];

	*extMessage = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n";
	*extMessage += "<" MI_XML_TAG_NEUTRINO " commandversion=\"1\">\n";
	*extMessage += "\t<" MI_XML_TAG_RECORD " command=\"";
	*extMessage += "record";
	*extMessage += "\">\n";
	XML_ADD_TAG_STRING(*extMessage, MI_XML_TAG_CHANNELNAME, movie_info->epgChannel);
	XML_ADD_TAG_STRING(*extMessage, MI_XML_TAG_EPGTITLE, movie_info->epgTitle);
	XML_ADD_TAG_LONG(*extMessage, MI_XML_TAG_ID, movie_info->epgId);
	XML_ADD_TAG_STRING(*extMessage, MI_XML_TAG_INFO1, movie_info->epgInfo1);
	XML_ADD_TAG_STRING(*extMessage, MI_XML_TAG_INFO2, movie_info->epgInfo2);
	XML_ADD_TAG_LONG(*extMessage, MI_XML_TAG_EPGID, movie_info->epgEpgId);	// %llu
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_MODE, movie_info->epgMode);	//%d
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_VIDEOPID, movie_info->epgVideoPid);	//%u
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_VIDEOTYPE, movie_info->VideoType);	//%u
	if (movie_info->audioPids.size() > 0) {
		//*extMessage +=        "\t\t<"MI_XML_TAG_AUDIOPIDS" selected=\"";
		//sprintf(tmp, "%u", movie_info->audioPids[0].epgAudioPid); //pids.APIDs[i].pid);
		//*extMessage += tmp;
		//*extMessage   +=      "\">\n";
		*extMessage += "\t\t<" MI_XML_TAG_AUDIOPIDS ">\n";

		for (unsigned int i = 0; i < movie_info->audioPids.size(); i++)	// pids.APIDs.size()
		{
			*extMessage += "\t\t\t<" MI_XML_TAG_AUDIO " " MI_XML_TAG_PID "=\"";
			sprintf(tmp, "%u", movie_info->audioPids[i].epgAudioPid);	//pids.APIDs[i].pid);
			*extMessage += tmp;
			*extMessage += "\" " MI_XML_TAG_ATYPE "=\"";
			sprintf(tmp, "%u", movie_info->audioPids[i].atype);	//pids.APIDs[i].pid);
			*extMessage += tmp;
			*extMessage += "\" " MI_XML_TAG_SELECTED "=\"";
			sprintf(tmp, "%u", movie_info->audioPids[i].selected);	//pids.APIDs[i].pid);
			*extMessage += tmp;
			*extMessage += "\" " MI_XML_TAG_NAME "=\"";
			*extMessage += ZapitTools::UTF8_to_UTF8XML(movie_info->audioPids[i].epgAudioPidName.c_str());
			*extMessage += "\"/>\n";
		}
		*extMessage += "\t\t</" MI_XML_TAG_AUDIOPIDS ">\n";
	}
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_VTXTPID, movie_info->epgVTXPID);	//%u
	/*****************************************************
	 *	new tags										*/
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_GENRE_MAJOR, movie_info->genreMajor);
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_GENRE_MINOR, movie_info->genreMinor);
	XML_ADD_TAG_STRING(*extMessage, MI_XML_TAG_SERIE_NAME, movie_info->serieName);
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_LENGTH, movie_info->length);
	XML_ADD_TAG_STRING(*extMessage, MI_XML_TAG_PRODUCT_COUNTRY, movie_info->productionCountry);
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_PRODUCT_DATE, movie_info->productionDate);
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_QUALITY, movie_info->quality);
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_PARENTAL_LOCKAGE, movie_info->parentalLockAge);
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_DATE_OF_LAST_PLAY, movie_info->dateOfLastPlay);
	*extMessage += "\t\t<" MI_XML_TAG_BOOKMARK ">\n";
	*extMessage += "\t";
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_BOOKMARK_START, movie_info->bookmarks.start);
	*extMessage += "\t";
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_BOOKMARK_END, movie_info->bookmarks.end);
	*extMessage += "\t";
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_BOOKMARK_LAST, movie_info->bookmarks.lastPlayStop);
	for (int i = 0; i < MI_MOVIE_BOOK_USER_MAX; i++) {
		if (movie_info->bookmarks.user[i].pos != 0 || i == 0) {
			// encode any valid book, at least 1
			*extMessage += "\t\t\t<" MI_XML_TAG_BOOKMARK_USER " " MI_XML_TAG_BOOKMARK_USER_POS "=\"";
			sprintf(tmp, "%d", movie_info->bookmarks.user[i].pos);	//pids.APIDs[i].pid);
			*extMessage += tmp;
			*extMessage += "\" " MI_XML_TAG_BOOKMARK_USER_TYPE "=\"";
			sprintf(tmp, "%d", movie_info->bookmarks.user[i].length);	//pids.APIDs[i].pid);
			*extMessage += tmp;
			*extMessage += "\" " MI_XML_TAG_BOOKMARK_USER_NAME "=\"";
			*extMessage += ZapitTools::UTF8_to_UTF8XML(movie_info->bookmarks.user[i].name.c_str());
			*extMessage += "\"/>\n";
		}
	}

	*extMessage += "\t\t</" MI_XML_TAG_BOOKMARK ">\n";
	 /*****************************************************/

	*extMessage += "\t</" MI_XML_TAG_RECORD ">\n";
	*extMessage += "</" MI_XML_TAG_NEUTRINO ">\n";
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
		result = convertTs2XmlName(&file_xml.Name);
	} else {
		file_xml.Name = file->Name;
	}
	TRACE("[mi] saveXml: %s\r\n", file_xml.Name.c_str());

	if (result == true) {
		result = encodeMovieInfoXml(&text, &movie_info);
		if (result == true) {
			result = saveFile(file_xml, text.c_str(), text.size());	// save
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
		result = convertTs2XmlName(&file_xml.Name);
	} else {
		file_xml.Name = file->Name;
	}

	if (result == true) {
		// load xml file in buffer
		char text[6000];
		result = loadFile(file_xml, text, 6000);
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
	if (movie_info->epgInfo2 == "") {
		movie_info->epgInfo2 = movie_info->epgInfo1;
		//movie_info->epgInfo1 = "";
	}
#endif /* XMLTREE_LIB */
	return (true);
}

/************************************************************************

************************************************************************/
void CMovieInfo::showMovieInfo(MI_MOVIE_INFO & movie_info)
{
	std::string print_buffer;
	tm *date_tm;
	char date_char[100];
	// prepare print buffer
	print_buffer = movie_info.epgInfo1;
	print_buffer += "\n";
	print_buffer += movie_info.epgInfo2;

	if (movie_info.productionCountry.size() != 0 || movie_info.productionDate != 0) {
		print_buffer += "\n";
		print_buffer += movie_info.productionCountry;
		print_buffer += " ";
		snprintf(date_char, 12, "%4d", movie_info.productionDate + 1900);
		print_buffer += date_char;
	}

	if (!movie_info.serieName.empty()) {
		print_buffer += "\n\n";
		print_buffer += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_SERIE);
		print_buffer += ": ";
		print_buffer += movie_info.serieName;
	}
	if (!movie_info.epgChannel.empty()) {
		print_buffer += "\n\n";
		print_buffer += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_CHANNEL);
		print_buffer += ": ";
		print_buffer += movie_info.epgChannel;
	}
	if (movie_info.quality != 0) {
		print_buffer += "\n";
		print_buffer += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_QUALITY);
		print_buffer += ": ";
		snprintf(date_char, 12, "%2d", movie_info.quality);
		print_buffer += date_char;
	}
	if (movie_info.parentalLockAge != 0) {
		print_buffer += "\n";
		print_buffer += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_PARENTAL_LOCKAGE);
		print_buffer += ": ";
		snprintf(date_char, 12, "%2d", movie_info.parentalLockAge);
		print_buffer += date_char;
		print_buffer += " Jahre";
	}
	if (movie_info.length != 0) {
		print_buffer += "\n";
		print_buffer += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_LENGTH);
		print_buffer += ": ";
		snprintf(date_char, 12, "%3d", movie_info.length);
		print_buffer += date_char;
	}
	if (movie_info.audioPids.size() != 0) {
		print_buffer += "\n";
		print_buffer += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_AUDIO);
		print_buffer += ": ";
		for (unsigned int i = 0; i < movie_info.audioPids.size(); i++) {
			print_buffer += movie_info.audioPids[i].epgAudioPidName;
			print_buffer += ", ";
		}
	}

	print_buffer += "\n\n";
	print_buffer += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_PREVPLAYDATE);
	print_buffer += ": ";
	date_tm = localtime(&movie_info.dateOfLastPlay);
	snprintf(date_char, 12, "%02d.%02d.%04d", date_tm->tm_mday, date_tm->tm_mon + 1, date_tm->tm_year + 1900);
	print_buffer += date_char;
	print_buffer += "\n";
	print_buffer += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_RECORDDATE);
	print_buffer += ": ";
	date_tm = localtime(&movie_info.file.Time);
	snprintf(date_char, 12, "%02d.%02d.%04d", date_tm->tm_mday, date_tm->tm_mon + 1, date_tm->tm_year + 1900);
	print_buffer += date_char;
	if (movie_info.file.Size != 0) {
		print_buffer += "\n";
		print_buffer += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_SIZE);
		print_buffer += ": ";
		//snprintf(date_char, 12,"%4llu",movie_info.file.Size>>20);
		sprintf(date_char, "%llu", movie_info.file.Size >> 20);
		print_buffer += date_char;
		//print_buffer += "\n";
	}
	print_buffer += "\n";
	print_buffer += g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_PATH);
	print_buffer += ": ";
	print_buffer += movie_info.file.Name;
	print_buffer += "\n";

	ShowMsg2UTF(movie_info.epgTitle.empty()? movie_info.file.getFileName().c_str() : movie_info.epgTitle.c_str(), print_buffer.c_str(), CMsgBox::mbrBack, CMsgBox::mbBack);	// UTF-8*/

}

/************************************************************************

************************************************************************/
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

/************************************************************************

************************************************************************/
int find_next_char(char to_find, char *text, int start_pos, int end_pos)
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
		_dest_ = atoll(&_text_[pos_prev]);\
		continue;\
	}

//void CMovieInfo::strReplace(std::string& orig, const char* fstr, const std::string rstr)
void strReplace(std::string & orig, const char *fstr, const std::string rstr)
{
//      replace all occurrence of fstr with rstr and, and returns a reference to itself
	unsigned int index = 0;
	unsigned int fstrlen = strlen(fstr);
	int rstrlen = rstr.size();

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
bool CMovieInfo::parseXmlQuickFix(char *text, MI_MOVIE_INFO * movie_info)
{
#ifndef XMLTREE_LIB
	int bookmark_nr = 0;
	movie_info->dateOfLastPlay = 0;	//100*366*24*60*60;              // (date, month, year)
	//bool result = false;

	int bytes = strlen(text);
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
		    GET_XML_DATA_INT(text, pos, MI_XML_TAG_QUALITY, movie_info->quality)
		    GET_XML_DATA_INT(text, pos, MI_XML_TAG_DATE_OF_LAST_PLAY, movie_info->dateOfLastPlay)
		    if (strncmp(&text[pos], MI_XML_TAG_AUDIOPIDS, sizeof(MI_XML_TAG_AUDIOPIDS) - 1) == 0)
			pos += sizeof(MI_XML_TAG_AUDIOPIDS);

		/* parse audio pids */
		if (strncmp(&text[pos], MI_XML_TAG_AUDIO, sizeof(MI_XML_TAG_AUDIO) - 1) == 0) {
			pos += sizeof(MI_XML_TAG_AUDIO);

			int pos2;
			char *ptr;

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
			printf("MOVIE INFO: apid %d type %d name %s selected %d\n", audio_pids.epgAudioPid, audio_pids.atype, audio_pids.epgAudioPidName.c_str(), audio_pids.selected);
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

	if (movie_info->epgInfo2 == "") {
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
				if (movie_info->bookmarks.user[i].name.size() == 0) {
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
void CMovieInfo::clearMovieInfo(MI_MOVIE_INFO * movie_info)
{
	//TRACE("[mi]->clearMovieInfo \r\n");
	tm timePlay;
	timePlay.tm_hour = 0;
	timePlay.tm_min = 0;
	timePlay.tm_sec = 0;
	timePlay.tm_year = 100;
	timePlay.tm_mday = 0;
	timePlay.tm_mon = 1;

	movie_info->file.Name = "";
	movie_info->file.Size = 0;	// Megabytes
	movie_info->file.Time = mktime(&timePlay);
	movie_info->dateOfLastPlay = mktime(&timePlay);	// (date, month, year)
	movie_info->dirItNr = 0;	//
	movie_info->genreMajor = 0;	//genreMajor;
	movie_info->genreMinor = 0;	//genreMinor;
	movie_info->length = 0;	// (minutes)
	movie_info->quality = 0;	// (3 stars: classics, 2 stars: very good, 1 star: good, 0 stars: OK)
	movie_info->productionDate = 0;	// (Year)  years since 1900
	movie_info->parentalLockAge = 0;	// MI_PARENTAL_LOCKAGE (0,6,12,16,18)
	movie_info->format = 0;	// MI_VIDEO_FORMAT(16:9, 4:3)
	movie_info->audio = 0;	// MI_AUDIO (AC3, Deutsch, Englisch)

	movie_info->epgId = 0;
	movie_info->epgEpgId = 0;
	movie_info->epgMode = 0;
	movie_info->epgVideoPid = 0;
	movie_info->VideoType = 0;
	movie_info->epgVTXPID = 0;

	movie_info->audioPids.clear();

	movie_info->productionCountry = "";
	movie_info->epgTitle = "";
	movie_info->epgInfo1 = "";	//epgInfo1
	movie_info->epgInfo2 = "";	//epgInfo2
	movie_info->epgChannel = "";
	movie_info->serieName = "";	// (name e.g. 'StarWars)
	movie_info->bookmarks.end = 0;
	movie_info->bookmarks.start = 0;
	movie_info->bookmarks.lastPlayStop = 0;
	for (int i = 0; i < MI_MOVIE_BOOK_USER_MAX; i++) {
		movie_info->bookmarks.user[i].pos = 0;
		movie_info->bookmarks.user[i].length = 0;
		movie_info->bookmarks.user[i].name = "";
	}
}

/************************************************************************

************************************************************************/
bool CMovieInfo::loadFile(CFile & file, char *buffer, int buffer_size)
{
	bool result = false;
	if (strncmp(file.getFileName().c_str(), VLC_URI, strlen(VLC_URI)) == 0) {
		result = loadFile_vlc(file, buffer, buffer_size);
	} else {
		result = loadFile_std(file, buffer, buffer_size);
	}
	return (result);
}

bool CMovieInfo::loadFile_std(CFile & file, char *buffer, int buffer_size)
{
	bool result = true;

	int fd = open(file.Name.c_str(), O_RDONLY);
	if (fd == -1)		// cannot open file, return!!!!!
	{
		TRACE("[mi] loadXml: cannot open (%s)\r\n", file.getFileName().c_str());
		return false;
	}
	//TRACE( "show_ts_info: File found (%s)\r\n" ,filename->c_str());
	// read file content to buffer
	int bytes = read(fd, buffer, buffer_size - 1);
	if (bytes <= 0)		// cannot read file into buffer, return!!!!
	{
		TRACE("[mi] loadXml: cannot read (%s)\r\n", file.getFileName().c_str());
		close(fd);
		return false;
	}
	close(fd);
	buffer[bytes] = 0;	// terminate string
	return (result);
}

bool CMovieInfo::loadFile_vlc(CFile & /*file*/, char */*buffer*/, int /*buffer_size*/)
{
	bool result = false;
	return (result);
}

/************************************************************************

************************************************************************/
bool CMovieInfo::saveFile(const CFile & file, const char *text, const int text_size)
{
	bool result = false;
	if (strncmp(file.getFileName().c_str(), VLC_URI, strlen(VLC_URI)) == 0) {
		result = saveFile_vlc(file, text, text_size);
	} else {
		result = saveFile_std(file, text, text_size);
	}
	return (result);
}

bool CMovieInfo::saveFile_std(const CFile & file, const char *text, const int text_size)
{
	bool result = false;
	int fd;
	if ((fd = open(file.Name.c_str(), O_SYNC | O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) >= 0) {
		/*int nr=*/ 
		write(fd, text, text_size);
		//fdatasync(fd);
		close(fd);
		result = true;
		//TRACE("[mi] saved (%d)\r\n",nr);
	} else {
		TRACE("[mi] ERROR: cannot open\r\n");
	}
	return (result);
}

bool CMovieInfo::saveFile_vlc(const CFile & /*file*/, const char */*text*/, const int /*text_size*/)
{
	bool result = false;
	return (result);
}

/* 	char buf[2048];

	int done;
	do
	{
		unsigned int len=fread(buf, 1, sizeof(buf), in);
		done=len<sizeof(buf);
		if (!parser->Parse(buf, len, 1))
		{
			TRACE("parse error: %s at line %d \r\n", parser->ErrorString(parser->GetErrorCode()), parser->GetCurrentLineNumber());
			fclose(in);
			delete parser;
			return (false);
		}
	} while (!done);
	fclose(in);
 *
 * */

void CMovieInfo::copy(MI_MOVIE_INFO * src, MI_MOVIE_INFO * dst)
{
	//TRACE("[mi]->clearMovieInfo \r\n");

	dst->file.Name = src->file.Name;
	dst->file.Size = src->file.Size;
	dst->file.Time = src->file.Time;
	dst->dateOfLastPlay = src->dateOfLastPlay;
	dst->dirItNr = src->dirItNr;
	dst->genreMajor = src->genreMajor;
	dst->genreMinor = src->genreMinor;
	dst->length = src->length;
	dst->quality = src->quality;
	dst->productionDate = src->productionDate;
	dst->parentalLockAge = src->parentalLockAge;
	dst->format = src->format;
	dst->audio = src->audio;

	dst->epgId = src->epgId;
	dst->epgEpgId = src->epgEpgId;
	dst->epgMode = src->epgMode;
	dst->epgVideoPid = src->epgVideoPid;
	dst->VideoType = src->VideoType;
	dst->epgVTXPID = src->epgVTXPID;

	dst->productionCountry = src->productionCountry;
	dst->epgTitle = src->epgTitle;
	dst->epgInfo1 = src->epgInfo1;
	dst->epgInfo2 = src->epgInfo2;
	dst->epgChannel = src->epgChannel;
	dst->serieName = src->serieName;
	dst->bookmarks.end = src->bookmarks.end;
	dst->bookmarks.start = src->bookmarks.start;
	dst->bookmarks.lastPlayStop = src->bookmarks.lastPlayStop;

	for (int i = 0; i < MI_MOVIE_BOOK_USER_MAX; i++) {
		dst->bookmarks.user[i].pos = src->bookmarks.user[i].pos;
		dst->bookmarks.user[i].length = src->bookmarks.user[i].length;
		dst->bookmarks.user[i].name = src->bookmarks.user[i].name;
	}

	for (unsigned int i = 0; i < src->audioPids.size(); i++) {
		EPG_AUDIO_PIDS audio_pids;
		audio_pids.epgAudioPid = src->audioPids[i].epgAudioPid;
		audio_pids.epgAudioPidName = src->audioPids[i].epgAudioPidName;
		audio_pids.atype = src->audioPids[i].atype;
		dst->audioPids.push_back(audio_pids);
	}
}
