/*
	movieinfo - Neutrino-GUI

	Copyright (C) 2005 Günther <Günther@tuxbox.berlios.org>

	Copyright (C) 2009, 2012 Stefan Seyfried
	Copyright (C) 2015 Sven Hoefer (svenhoefer)

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

#include <global.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <driver/movieinfo.h>
#include <system/helpers.h>

#include <neutrino.h>

#define TRACE printf

CMovieInfo::CMovieInfo()
{
	//TRACE("[mi] new\n");
}

CMovieInfo::~CMovieInfo()
{
	//TRACE("[mi] del\n");
	;
}

bool CMovieInfo::convertTs2XmlName(std::string& filename)
{
	size_t lastdot = filename.find_last_of(".");
	if (lastdot != std::string::npos) {
		filename.erase(lastdot + 1);
		filename.append("xml");
		return true;
	}
	return false;
}

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

bool CMovieInfo::encodeMovieInfoXml(std::string * extMessage, MI_MOVIE_INFO * movie_info)
{
	//TRACE("[mi]->encodeMovieInfoXml\n");

	*extMessage = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n"
			"<" MI_XML_TAG_NEUTRINO " commandversion=\"1\">\n"
			"\t<" MI_XML_TAG_RECORD " command=\""
			"record"
			"\">\n";
	XML_ADD_TAG_STRING(*extMessage, MI_XML_TAG_CHANNELNAME, movie_info->channelName);
	XML_ADD_TAG_STRING(*extMessage, MI_XML_TAG_EPGTITLE, movie_info->epgTitle);
	XML_ADD_TAG_LONG(*extMessage, MI_XML_TAG_ID, movie_info->channelId);
	XML_ADD_TAG_STRING(*extMessage, MI_XML_TAG_INFO1, movie_info->epgInfo1);
	XML_ADD_TAG_STRING(*extMessage, MI_XML_TAG_INFO2, movie_info->epgInfo2);
	XML_ADD_TAG_LONG(*extMessage, MI_XML_TAG_EPGID, movie_info->epgId); // %llu
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_MODE, movie_info->mode); // %d
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_VIDEOPID, movie_info->VideoPid); // %u
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_VIDEOTYPE, movie_info->VideoType); // %u
	if ( !movie_info->audioPids.empty() ) {
		*extMessage += "\t\t<" MI_XML_TAG_AUDIOPIDS ">\n";

		for (unsigned int i = 0; i < movie_info->audioPids.size(); i++) // pids.APIDs.size()
		{
			*extMessage += "\t\t\t<" MI_XML_TAG_AUDIO " " MI_XML_TAG_PID "=\"";
			*extMessage += to_string(movie_info->audioPids[i].AudioPid);
			*extMessage += "\" " MI_XML_TAG_ATYPE "=\"";
			*extMessage += to_string(movie_info->audioPids[i].atype);
			*extMessage += "\" " MI_XML_TAG_SELECTED "=\"";
			*extMessage += to_string(movie_info->audioPids[i].selected);
			*extMessage += "\" " MI_XML_TAG_NAME "=\"";
			*extMessage += ZapitTools::UTF8_to_UTF8XML(movie_info->audioPids[i].AudioPidName.c_str());
			*extMessage += "\"/>\n";
		}
		*extMessage += "\t\t</" MI_XML_TAG_AUDIOPIDS ">\n";
	}
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_VTXTPID, movie_info->VtxtPid); // %u

	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_GENRE_MAJOR, movie_info->genreMajor);
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_GENRE_MINOR, movie_info->genreMinor);
	XML_ADD_TAG_STRING(*extMessage, MI_XML_TAG_SERIE_NAME, movie_info->serieName);
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_LENGTH, movie_info->length);
	XML_ADD_TAG_STRING(*extMessage, MI_XML_TAG_PRODUCT_COUNTRY, movie_info->productionCountry);
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_PRODUCT_DATE, movie_info->productionDate);
	XML_ADD_TAG_UNSIGNED(*extMessage, MI_XML_TAG_RATING, movie_info->rating);
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


bool CMovieInfo::saveMovieInfo(MI_MOVIE_INFO & movie_info, CFile * file)
{
	//TRACE("[mi]->saveMovieInfo\n");

	bool result = true;
	std::string text;
	CFile file_xml;

	if (file == NULL) {
		file_xml.Name = movie_info.file.Name;
		result = convertTs2XmlName(file_xml.Name);
	} else {
		file_xml.Name = file->Name;
	}
	TRACE("[mi] saveMovieInfo: %s\n", file_xml.Name.c_str());

	if (result == true) {
		result = encodeMovieInfoXml(&text, &movie_info);
		if (result == true) {
			result = saveFile(file_xml, text);	// save
			if (result == false) {
				TRACE("[mi] saveMovieInfo: save error\n");
			}
		} else {
			TRACE("[mi] saveMovieInfo: encoding error\n");
		}
	} else {
		TRACE("[mi] saveMovieInfo: error\n");
	}
	return (result);
}


bool CMovieInfo::loadMovieInfo(MI_MOVIE_INFO * movie_info, CFile * file)
{
	//TRACE("[mi]->loadMovieInfo\n");

	bool result = true;
	CFile file_xml;

	if (file == NULL) {
		// if there is no give file, we use the file name from movie info
		// but we have to convert the ts name to xml name first
		file_xml.Name = movie_info->file.Name;
		result = convertTs2XmlName(file_xml.Name);
	} else {
		file_xml.Name = file->Name;
	}

	if (result == true) {
		// load xml file in buffer
		std::string text;
		result = loadFile(file_xml, text);
		if (result == true)
			result = parseXmlTree(text, movie_info);
	}
	if (movie_info->productionDate > 50 && movie_info->productionDate < 200) // backwardcompaibility
		movie_info->productionDate += 1900;

	return (result);
}

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
		_dest_ = htmlEntityDecode(_dest_);\
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
		_dest_ = strtoull(&_text_[pos_prev], NULL, 10);\
		continue;\
	}

bool CMovieInfo::parseXmlTree(std::string &_text, MI_MOVIE_INFO *movie_info)
{
	int bookmark_nr = 0;
	movie_info->dateOfLastPlay = 0;	// UNIX Epoch time
	//bool result = false;

	const char *text = _text.c_str();
	int bytes = _text.length();

	int pos = 0;

	AUDIO_PIDS audio_pids;

	while ((pos = find_next_char('<', text, pos, bytes)) != -1) {
		pos++;
		GET_XML_DATA_STRING(text, pos, MI_XML_TAG_CHANNELNAME, movie_info->channelName)
		GET_XML_DATA_STRING(text, pos, MI_XML_TAG_EPGTITLE, movie_info->epgTitle)
		GET_XML_DATA_LONG(text, pos, MI_XML_TAG_ID, movie_info->channelId)
		GET_XML_DATA_STRING(text, pos, MI_XML_TAG_INFO1, movie_info->epgInfo1)
		GET_XML_DATA_STRING(text, pos, MI_XML_TAG_INFO2, movie_info->epgInfo2)
		GET_XML_DATA_LONG(text, pos, MI_XML_TAG_EPGID, movie_info->epgId)
		GET_XML_DATA_INT(text, pos, MI_XML_TAG_MODE, movie_info->mode)
		GET_XML_DATA_INT(text, pos, MI_XML_TAG_VIDEOPID, movie_info->VideoPid)
		GET_XML_DATA_INT(text, pos, MI_XML_TAG_VIDEOTYPE, movie_info->VideoType)
		GET_XML_DATA_STRING(text, pos, MI_XML_TAG_NAME, movie_info->channelName)
		GET_XML_DATA_INT(text, pos, MI_XML_TAG_VTXTPID, movie_info->VtxtPid)
		GET_XML_DATA_INT(text, pos, MI_XML_TAG_GENRE_MAJOR, movie_info->genreMajor)
		GET_XML_DATA_INT(text, pos, MI_XML_TAG_GENRE_MINOR, movie_info->genreMinor)
		GET_XML_DATA_STRING(text, pos, MI_XML_TAG_SERIE_NAME, movie_info->serieName)
		GET_XML_DATA_INT(text, pos, MI_XML_TAG_LENGTH, movie_info->length)
		GET_XML_DATA_STRING(text, pos, MI_XML_TAG_PRODUCT_COUNTRY, movie_info->productionCountry)
		GET_XML_DATA_INT(text, pos, MI_XML_TAG_PRODUCT_DATE, movie_info->productionDate)
		GET_XML_DATA_INT(text, pos, MI_XML_TAG_PARENTAL_LOCKAGE, movie_info->parentalLockAge)
		GET_XML_DATA_INT(text, pos, MI_XML_TAG_RATING, movie_info->rating)
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
				pos2 = (int)(ptr - &text[pos]);
			//pos2 = strcspn(&text[pos],MI_XML_TAG_PID);
			if (pos2 >= 0) {
				pos2 += sizeof(MI_XML_TAG_PID);
				while (text[pos + pos2] != '\"' && text[pos + pos2] != 0 && text[pos + pos2] != '/')
					pos2++;
				if (text[pos + pos2] == '\"')
					audio_pids.AudioPid = atoi(&text[pos + pos2 + 1]);
			} else
				audio_pids.AudioPid = 0;

			audio_pids.atype = 0;
			pos2 = -1;
			ptr = strstr(&text[pos], MI_XML_TAG_ATYPE);
			if (ptr)
				pos2 = (int)(ptr - &text[pos]);
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
				pos2 = (int)(ptr - &text[pos]);
			//pos2 = strcspn(&text[pos],MI_XML_TAG_SELECTED);
			if (pos2 >= 0) {
				pos2 += sizeof(MI_XML_TAG_SELECTED);
				while (text[pos + pos2] != '\"' && text[pos + pos2] != 0 && text[pos + pos2] != '/')
					pos2++;
				if (text[pos + pos2] == '\"')
					audio_pids.selected = atoi(&text[pos + pos2 + 1]);
			}

			audio_pids.AudioPidName = "";
			//pos2 = strcspn(&text[pos],MI_XML_TAG_NAME);
			pos2 = -1;
			ptr = strstr(&text[pos], MI_XML_TAG_NAME);
			if (ptr)
				pos2 = (int)(ptr - &text[pos]);
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
						audio_pids.AudioPidName.append(&text[pos + pos2 + 1], pos3 - pos2 - 1);
						audio_pids.AudioPidName = htmlEntityDecode(audio_pids.AudioPidName);
					}
				}
			}
			//printf("MOVIE INFO: apid %d type %d name %s selected %d\n", audio_pids.AudioPid, audio_pids.atype, audio_pids.epgAudioPidName.c_str(), audio_pids.selected);
			unsigned int j, asize = movie_info->audioPids.size();
			for (j = 0; j < asize && audio_pids.AudioPid != movie_info->audioPids[j].AudioPid; j++);
			if (j == asize)
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
											movie_info->bookmarks.user[bookmark_nr].name = htmlEntityDecode(movie_info->bookmarks.user[bookmark_nr].name);
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
}


bool CMovieInfo::addNewBookmark(MI_MOVIE_INFO * movie_info, MI_BOOKMARK & new_bookmark)
{
	TRACE("[mi] addNewBookmark\n");

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


void CMovieInfo::clearMovieInfo(MI_MOVIE_INFO * movie_info)
{
	//TRACE("[mi]->clearMovieInfo \r\n");
	movie_info->file.Name = "";
	movie_info->file.Size = 0;	// Megabytes
	movie_info->file.Time = 0;
	movie_info->dateOfLastPlay = 0;	// UNIX Epoch time
	movie_info->dirItNr = 0;	//
	movie_info->genreMajor = 0;	//genreMajor;
	movie_info->genreMinor = 0;	//genreMinor;
	movie_info->length = 0;	// (minutes)
	movie_info->quality = 0;	// (3 stars: classics, 2 stars: very good, 1 star: good, 0 stars: OK)
	movie_info->productionDate = 0;	// (Year)  years since 1900
	movie_info->parentalLockAge = 0;	// MI_PARENTAL_LOCKAGE (0,6,12,16,18)

	movie_info->channelId = 0;
	movie_info->epgId = 0;
	movie_info->mode = 0;
	movie_info->VideoPid = 0;
	movie_info->VideoType = 0;
	movie_info->VtxtPid = 0;

	movie_info->audioPids.clear();

	movie_info->productionCountry = "";
	movie_info->epgTitle = "";
	movie_info->epgInfo1 = "";	//epgInfo1
	movie_info->epgInfo2 = "";	//epgInfo2
	movie_info->channelName = "";
	movie_info->serieName = "";	// (name e.g. 'StarWars)
	movie_info->bookmarks.end = 0;
	movie_info->bookmarks.start = 0;
	movie_info->bookmarks.lastPlayStop = 0;
	for (int i = 0; i < MI_MOVIE_BOOK_USER_MAX; i++) {
		movie_info->bookmarks.user[i].pos = 0;
		movie_info->bookmarks.user[i].length = 0;
		movie_info->bookmarks.user[i].name = "";
	}
	movie_info->tfile.clear();
	movie_info->ytdate.clear();
	movie_info->ytid.clear();
}

void MI_MOVIE_INFO::clear(void)
{
	file.Name = "";
	file.Url = "";
	file.Size = 0; // Megabytes
	file.Time = 0;
	dateOfLastPlay = 0; // UNIX Epoch time
	dirItNr = 0;
	genreMajor = 0;
	genreMinor = 0;
	length = 0;
	rating = 0;
	quality = 0;
	productionDate = 0;
	parentalLockAge = 0;
	//format = 0;
	//audio = 0;

	channelId = 0;
	epgId = 0;
	mode = 0;
	VideoPid = 0;
	VideoType = 0;
	VtxtPid = 0;

	audioPids.clear();

	productionCountry = "";
	epgTitle = "";
	epgInfo1 = "";
	epgInfo2 = "";
	channelName = "";
	serieName = "";
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
	delAsk = true;
	source = UNKNOWN;
}

bool CMovieInfo::loadFile(CFile &file, std::string &buffer)
{
	bool result = true;

	int fd = open(file.Name.c_str(), O_RDONLY);
	if (fd == -1)
	{
		TRACE("[mi] loadFile: cannot open (%s)\n", file.Name.c_str());
		return false;
	}
	struct stat st;
	if (fstat(fd, &st)) {
		close(fd);
		return false;
	}

	if (!st.st_size)
		return false;

	char buf[st.st_size];
	if (st.st_size != read(fd, buf, st.st_size)) {
		TRACE("[mi] loadFile: cannot read (%s)\n", file.Name.c_str());
		result = false;
	} else
		buffer = std::string(buf, st.st_size);

	close(fd);

	return result;
}

bool CMovieInfo::saveFile(const CFile & file, std::string &text)
{
	bool result = false;
	int fd;
	if ((fd = open(file.Name.c_str(), O_SYNC | O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) >= 0) {
		write(fd, text.c_str(), text.size());
		//fdatasync(fd);
		close(fd);
		result = true;
		//TRACE("[mi] saved (%d)\n",nr);
	} else {
		TRACE("[mi] ERROR: cannot open\n");
	}
	return (result);
}
