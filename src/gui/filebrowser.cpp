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

/* include <config.h> before <gui/filebrowser.h> to enable 64 bit file offsets */
#include <gui/filebrowser.h>

#include <gui/widget/buttons.h>
#include <gui/widget/icons.h>
#include <gui/widget/messagebox.h>

#include <driver/encoding.h>

#include <algorithm>
#include <iostream>
#include <cctype>

#include <global.h>
#include <neutrino.h>

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sstream>

#include <sys/stat.h>

#include <curl/curl.h>
#include <curl/easy.h>

#ifndef NEW_LIBCURL
#include <curl/types.h>
#endif

#include <driver/encoding.h>

#include <xmltree/xmlinterface.h>

#ifdef __USE_FILE_OFFSET64
typedef struct dirent64 dirent_struct;
#define my_alphasort alphasort64
#define my_scandir scandir64
typedef struct stat64 stat_struct;
#define my_stat stat64
#define my_lstat lstat64
#else
typedef struct dirent dirent_struct;
#define my_alphasort alphasort
#define my_scandir scandir
typedef struct stat stat_struct;
#define my_stat stat
#define my_lstat lstat
#error not using 64 bit file offsets
#endif

#define SMSKEY_TIMEOUT 2000
//------------------------------------------------------------------------
size_t CurlWriteToString(void *ptr, size_t size, size_t nmemb, void *data)
{
	std::string* pStr = (std::string*) data;
	pStr->append((char*) ptr, nmemb); // do only add the correct size, do not go until end of data (chunked transfer)
	return size*nmemb;
}

//------------------------------------------------------------------------
//------------------------------------------------------------------------

SMSKeyInput::SMSKeyInput()
{
	resetOldKey();
	m_timeout = SMSKEY_TIMEOUT;
}
//------------------------------------------------------------------------

unsigned char SMSKeyInput::handleMsg(const neutrino_msg_t msg)
{
	timeval keyTime;
	gettimeofday(&keyTime,NULL);
	bool timeoutNotReached = (keyTime.tv_sec*1000+keyTime.tv_usec/1000
				  <= m_oldKeyTime.tv_sec*1000+m_oldKeyTime.tv_usec/1000 + m_timeout);

// 	printf("act: %ld , old: %ld (diff: %ld ) , timeout: %ld => timout= %d\n",
// 	       keyTime.tv_sec*1000+keyTime.tv_usec/1000,
// 	       m_oldKeyTime.tv_sec*1000+m_oldKeyTime.tv_usec/1000,
// 	       keyTime.tv_sec*1000+keyTime.tv_usec/1000 -
// 	       m_oldKeyTime.tv_sec*1000+m_oldKeyTime.tv_usec/1000,
// 	       m_timeout,!timeoutNotReached);

	unsigned char key = 0;
	if(msg == CRCInput::RC_1)
	{
			key = '1';
	}
	if(msg == CRCInput::RC_2)
	{
		if(m_oldKey == 'a' && timeoutNotReached)
			key = 'b';
		else if(m_oldKey == 'b' && timeoutNotReached)
			key = 'c';
		else if(m_oldKey == 'c' && timeoutNotReached)
			key = '2';
		else
			key = 'a';
	}
	else if(msg == CRCInput::RC_3)
	{
		if(m_oldKey == 'd' && timeoutNotReached)
			key = 'e';
		else if(m_oldKey == 'e' && timeoutNotReached)
			key = 'f';
		else if(m_oldKey == 'f' && timeoutNotReached)
			key = '3';
		else
			key = 'd';
	}
	else if(msg == CRCInput::RC_4)
	{
		if(m_oldKey == 'g' && timeoutNotReached)
			key = 'h';
		else if(m_oldKey == 'h' && timeoutNotReached)
			key = 'i';
		else if(m_oldKey == 'i' && timeoutNotReached)
			key = '4';
		else
			key = 'g';
	}
	else if(msg == CRCInput::RC_5)
	{
		if(m_oldKey == 'j' && timeoutNotReached)
			key = 'k';
		else if(m_oldKey == 'k' && timeoutNotReached)
			key = 'l';
		else if(m_oldKey == 'l' && timeoutNotReached)
			key = '5';
		else
			key = 'j';
	}
	else if(msg == CRCInput::RC_6)
	{
		if(m_oldKey == 'm' && timeoutNotReached)
			key = 'n';
		else if(m_oldKey == 'n' && timeoutNotReached)
			key = 'o';
		else if(m_oldKey == 'o' && timeoutNotReached)
			key = '6';
		else
			key = 'm';
	}
	else if(msg == CRCInput::RC_7)
	{
		if(m_oldKey == 'p' && timeoutNotReached)
			key = 'q';
		else if(m_oldKey == 'q' && timeoutNotReached)
			key = 'r';
		else if(m_oldKey == 'r' && timeoutNotReached)
			key = 's';
		else if(m_oldKey == 's' && timeoutNotReached)
			key = 's';
		else
			key = 'p';
	}
	else if(msg == CRCInput::RC_8)
	{
		if(m_oldKey == 't' && timeoutNotReached)
			key = 'u';
		else if(m_oldKey == 'u' && timeoutNotReached)
			key = 'v';
		else if(m_oldKey == 'v' && timeoutNotReached)
			key = '8';
		else
			key = 't';
	}
	else if(msg == CRCInput::RC_9)
	{
		if(m_oldKey == 'w' && timeoutNotReached)
			key = 'x';
		else if(m_oldKey == 'x' &&timeoutNotReached)
			key = 'y';
		else if(m_oldKey == 'y' &&timeoutNotReached)
			key = 'z';
		else if(m_oldKey == 'z' && timeoutNotReached)
			key = '9';
		else
			key = 'w';
	}
	else if(msg == CRCInput::RC_0)
	{
		key = '0';
	}
	m_oldKeyTime=keyTime;
	m_oldKey=key;
	return key;
}
//------------------------------------------------------------------------

void SMSKeyInput::resetOldKey()
{
	m_oldKeyTime.tv_sec = 0;
	m_oldKeyTime.tv_usec = 0;
	m_oldKey = 0;
}

unsigned char SMSKeyInput::getOldKey() const
{
	return m_oldKey;
}

const timeval* SMSKeyInput::getOldKeyTime() const
{
 	return &m_oldKeyTime;
}

time_t SMSKeyInput::getOldKeyTimeSec() const
{
	return m_oldKeyTime.tv_sec;
}


int SMSKeyInput::getTimeout() const
{
	return m_timeout;
}

void SMSKeyInput::setTimeout(int timeout)
{
	m_timeout = timeout;
}


//------------------------------------------------------------------------
//------------------------------------------------------------------------

bool comparetolower(const char a, const char b)
{
	return tolower(a) < tolower(b);
};

// sort operators
bool sortByName (const CFile& a, const CFile& b)
{
	if (std::lexicographical_compare(a.Name.begin(), a.Name.end(), b.Name.begin(), b.Name.end(), comparetolower))
		return true;

	if (std::lexicographical_compare(b.Name.begin(), b.Name.end(), a.Name.begin(), a.Name.end(), comparetolower))
		return false;

	return a.Mode < b.Mode;
/*
	int result = __gnu_cxx::lexicographical_compare_3way(a.Name.begin(), a.Name.end(), b.Name.begin(), b.Name.end(), comparetolower);

	if (result == 0)
		return a.Mode < b.Mode;
	else
		return result < 0;
*/
}

bool sortByNameDirsFirst(const CFile& a, const CFile& b)
// Sorts alphabetically with Directories first
{
	int typea, typeb;
	typea = a.getType();
	typeb = b.getType();

	if (typea == CFile::FILE_DIR)
		if (typeb == CFile::FILE_DIR)
			//both directories
			return sortByName(a, b);
		else
			//only a is directory
			return true;
	else if (typeb == CFile::FILE_DIR)
		//only b is directory
		return false;
	else
		//no directory
		return sortByName(a, b);
}

bool sortByType (const CFile& a, const CFile& b)
{
	if(a.Mode == b.Mode)
		return sortByName(a, b);
	else
		return a.Mode < b.Mode;
}

bool sortByDate (const CFile& a, const CFile& b)
{
	if(a.getFileName()=="..")
		return true;
	if(b.getFileName()=="..")
		return false;
	return a.Time < b.Time ;
}

bool sortBySize (const CFile& a, const CFile& b)
{
	if(a.getFileName()=="..")
		return true;
	if(b.getFileName()=="..")
		return false;
	return a.Size < b.Size;
}

bool (* const sortBy[FILEBROWSER_NUMBER_OF_SORT_VARIANTS])(const CFile& a, const CFile& b) =
{
	&sortByName,
	&sortByNameDirsFirst,
	&sortByType,
	&sortByDate,
	&sortBySize
};

const neutrino_locale_t sortByNames[FILEBROWSER_NUMBER_OF_SORT_VARIANTS] =
{
	LOCALE_FILEBROWSER_SORT_NAME,
	LOCALE_FILEBROWSER_SORT_NAMEDIRSFIRST,
	LOCALE_FILEBROWSER_SORT_TYPE,
	LOCALE_FILEBROWSER_SORT_DATE,
	LOCALE_FILEBROWSER_SORT_SIZE
};

//------------------------------------------------------------------------

CFileBrowser::CFileBrowser()
{
	commonInit();
	base = "";
	m_Mode = ModeFile;
}

CFileBrowser::CFileBrowser(const char * const _base, const tFileBrowserMode mode)
{
	commonInit();
	base = _base;
	m_Mode = mode;
}

void CFileBrowser::commonInit()
{
	frameBuffer = CFrameBuffer::getInstance();
	//shoutcast
	sc_init_dir = "/legacy/genrelist?k="  + g_settings.shoutcast_dev_id;

	Filter = NULL;
	use_filter = true;
	Multi_Select = false;
	Dirs_Selectable = false;
	Dir_Mode = false;
	Hide_records = false;
	selected = 0;
	selections.clear();

	x = g_settings.screen_StartX + 20;
	y = g_settings.screen_StartY + 20;

	width = (g_settings.screen_EndX - g_settings.screen_StartX - 40);
	height = (g_settings.screen_EndY - g_settings.screen_StartY - 40);

	theight = g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_TITLE]->getHeight();
	fheight = g_Font[SNeutrinoSettings::FONT_TYPE_FILEBROWSER_ITEM]->getHeight();
	if (fheight == 0)
		fheight = 1; /* avoid div by zero on invalid font */
	foheight = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight()+6; //initial height value for buttonbar; TODO get value from buttonbar

	liststart = 0;
	listmaxshow = std::max(1,(int)(height - theight - 2 * foheight)/fheight);

	//recalc height
	height = theight + listmaxshow * fheight + 2 * foheight;

	m_SMSKeyInput.setTimeout(SMSKEY_TIMEOUT);

}

//------------------------------------------------------------------------


CFileBrowser::~CFileBrowser()
{
}

//------------------------------------------------------------------------

CFile *CFileBrowser::getSelectedFile()
{
	if ((!(filelist.empty())) && (!(filelist[selected].Name.empty())))
		return &filelist[selected];
	else
		return NULL;
}

//------------------------------------------------------------------------

void CFileBrowser::ChangeDir(const std::string & filename, int selection)
{
	std::string newpath;
	if((m_Mode != ModeSC) && (filename == ".."))
	{
		std::string::size_type pos = Path.substr(0,Path.length()-1).rfind('/');

#ifdef ENABLE_MOVIEPLAYER_VLC
		bool is_vlc = (strncmp(Path.c_str(), VLC_URI, strlen(VLC_URI)) == 0);
#endif
		if (pos == std::string::npos)
		{
			newpath = Path;
		}
		else
		{
#ifdef ENABLE_MOVIEPLAYER_VLC
			if (is_vlc && (pos < strlen(VLC_URI) - 1))
				newpath = VLC_URI;
			else
#endif
				newpath = Path.substr(0, pos + 1);
		}

#ifdef ENABLE_MOVIEPLAYER_VLC
		if (strncmp(is_vlc ? &(newpath.c_str()[strlen(VLC_URI)]) : newpath.c_str(), base.c_str(), base.length()) != 0)
			return;
#endif
	}
	else
	{
		newpath=filename;
	}
	if(m_Mode != ModeSC && (newpath.rfind('/') != newpath.length()-1 || newpath.length() == 0))
	{
		newpath += '/';
	}
	filelist.clear();
	Path = newpath;
	name = newpath;
	CFileList allfiles;
	readDir(newpath, &allfiles);
	// filter
	CFileList::iterator file = allfiles.begin();
	for(; file != allfiles.end() ; file++)
	{
		if(Filter != NULL && (!S_ISDIR(file->Mode)) && use_filter)
		{
			if(!Filter->matchFilter(file->Name))
			{
				continue;
			}
			if(Hide_records) {
				int ext_pos = file->Name.rfind('.');
				if( ext_pos > 0) {
					std::string extension = file->Name.substr(ext_pos + 1, name.length() - ext_pos);
					if(strcasecmp(extension.c_str(), "ts") == 0) {
						std::string fname = file->Name.substr(0, ext_pos) + ".xml";
						if(access(fname.c_str(), F_OK) == 0)
							continue;
					}
				}
			}
		}
		if(Dir_Mode && (!S_ISDIR(file->Mode)))
		{
			continue;
		}
		filelist.push_back(*file);
	}
	// sort result
	sort(filelist.begin(), filelist.end(), sortBy[g_settings.filebrowser_sortmethod]);

	selected = 0;
	if ((selection != -1) && (selection < (int)filelist.size()))
		selected = selection;
	paintHead();
	paint();
}

//------------------------------------------------------------------------
bool CFileBrowser::readDir(const std::string & dirname, CFileList* flist)
{
	bool ret;

#ifdef ENABLE_INTERNETRADIO
	if (m_Mode == ModeSC) {
		ret = readDir_sc(dirname, flist);
	}
	else
#endif
#ifdef ENABLE_MOVIEPLAYER_VLC
	if (strncmp(dirname.c_str(), VLC_URI, strlen(VLC_URI)) == 0)
	{
		ret = readDir_vlc(dirname, flist);
	}
	else
#endif
	{
		ret = readDir_std(dirname, flist);
	}
	return ret;
}

#ifdef ENABLE_MOVIEPLAYER_VLC
bool CFileBrowser::readDir_vlc(const std::string & dirname, CFileList* flist)
{
//	printf("readDir_vlc %s\n",dirname.c_str());
	std::string answer="";
	char *dir_escaped = curl_escape(dirname.substr(strlen(VLC_URI)).c_str(), 0);
	std::string url = m_baseurl;
	url += dir_escaped;
	curl_free(dir_escaped);
	std::cout << "[FileBrowser] vlc URL: " << url << std::endl;
	CURL *curl_handle;
	CURLcode httpres;
	/* init the curl session */
	curl_handle = curl_easy_init();
	/* timeout. 15 seconds should be enough */
	curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 15);
	/* specify URL to get */
	curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
	/* send all data to this function  */
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, CurlWriteToString);
	/* we pass our 'chunk' struct to the callback function */
	curl_easy_setopt(curl_handle, CURLOPT_FILE, (void *)&answer);
	/* Generate error if http error >= 400 occurs */
	curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1);
	/* error handling */
	char error[CURL_ERROR_SIZE];
	curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, error);
	/* get it! */
	httpres = curl_easy_perform(curl_handle);
	/* cleanup curl stuff */
	curl_easy_cleanup(curl_handle);

	// std::cout << "Answer:" << std::endl << "----------------" << std::endl << answer << std::endl;

	if (!answer.empty() && httpres == 0)
	{
		xmlDocPtr answer_parser = parseXml(answer.c_str());

		if (answer_parser != NULL) {
			xmlNodePtr element = xmlDocGetRootElement(answer_parser);
			element = element->xmlChildrenNode;
			char *ptr;
			if (element == NULL) {
				printf("[FileBrowser] vlc: Drive is not readable. Possibly no disc inserted\n");
				CFile file;
				file.Mode = S_IFDIR + 0777 ;
				file.Name = dirname + "..";
				file.Size = 0;
				file.Time = 0;
				flist->push_back(file);
			} else {
				while (element) {
					CFile file;
					ptr = xmlGetAttribute(element, "type");
					if (strcmp(ptr, "directory")==0)
						file.Mode = S_IFDIR + 0777 ;
					else
						file.Mode = S_IFREG + 0777 ;

					file.Name = dirname + xmlGetAttribute(element, "name");
					ptr = xmlGetAttribute(element, "size");
					if (ptr)
						file.Size = atoi(ptr);
					else
						file.Size = 0;
					file.Time = 0;

					element = element->xmlNextNode;
					flist->push_back(file);
				}
			}
			xmlFreeDoc(answer_parser);
			return true;
		}
	}

	/* since all CURL error messages use only US-ASCII characters, when can safely print them as if they were UTF-8 encoded */
	if (httpres == 22) {
	    strcat(error, "\nProbably wrong vlc version\nPlease use vlc 0.8.5 or higher");
	}
	DisplayErrorMessage(error); // UTF-8
	CFile file;

	file.Name = dirname + "..";
	file.Mode = S_IFDIR + 0777;
	file.Size = 0;
	file.Time = 0;
	flist->push_back(file);

	return false;
}
#endif /* ENABLE_MOVIEPLAYER_VLC */

#ifdef ENABLE_INTERNETRADIO
bool CFileBrowser::readDir_sc(const std::string & dirname, CFileList* flist)
{
#define GET_SHOUTCAST_TIMEOUT	60
/* how the shoutcast xml interfaces looks/works:
1st step: get http://www.shoutcast.com/sbin/newxml.phtml
example answer:

<genrelist>
...
<genre name="Trance"/>
<genre name=...
...

2nd step: get http://www.shoutcast.com/sbin/newxml.phtml?genre=Trance
example answer:

<stationlist>
<tunein base="/sbin/tunein-station.pls"/>
<station name="TechnoBase.FM - 24h Techno, Dance, Trance, House and More - 128k MP3" mt="audio/mpeg" id="524" br="128" genre="Techno Trance Dance House" ct="We aRe oNe" lc="4466"/>
<station name=...
...

3rd step: get/decode playlist http://www.shoutcast.com/sbin/tunein-station.pls?id=524
and add to neutrino playlist

4th step: play from neutrio playlist
*/
	//shoutcast
	const std::string sc_get_top500 = "/legacy/Top500?k=" + g_settings.shoutcast_dev_id;
	const std::string sc_get_genre = "/legacy/stationsearch?k=" + g_settings.shoutcast_dev_id + "&search=";
	const std::string sc_tune_in_base = "http://yp.shoutcast.com";

//	printf("readDir_sc %s\n",dirname.c_str());
	std::string answer="";
//	char *dir_escaped = curl_escape(dirname.c_str(), 0);
	std::string url = m_baseurl;
//	url += dir_escaped;
//	curl_free(dir_escaped);
	url += dirname;
	std::cout << "[FileBrowser] SC URL: " << url << std::endl;
	CURL *curl_handle;
	CURLcode httpres;
	/* init the curl session */
	curl_handle = curl_easy_init();
	/* specify URL to get */
	curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
	/* send all data to this function  */
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, CurlWriteToString);
	/* we pass our 'chunk' struct to the callback function */
	curl_easy_setopt(curl_handle, CURLOPT_FILE, (void *)&answer);
	/* Generate error if http error >= 400 occurs */
	curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1);
	/* set timeout to 30 seconds */
	curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, GET_SHOUTCAST_TIMEOUT);

	/* error handling */
	char error[CURL_ERROR_SIZE];
	curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, error);
	/* get it! */
	httpres = curl_easy_perform(curl_handle);
	/* cleanup curl stuff */
	curl_easy_cleanup(curl_handle);

	//std::cout << "Answer:" << std::endl << "----------------" << std::endl << answer << std::endl;

	if (!answer.empty() && httpres == 0)
	{
printf("CFileBrowser::readDir_sc: read done, size %d\n", answer.size());
		xmlDocPtr answer_parser = parseXml(answer.c_str());

		if (answer_parser != NULL) {
			char *ptr;
			unsigned char xml_decode = 0;
			xmlNodePtr element = xmlDocGetRootElement(answer_parser);

			if (strcmp(xmlGetName(element), "genrelist") == 0)
				xml_decode = 1;
			else if (strcmp(xmlGetName(element), "stationlist") == 0)
				xml_decode = 2;
			element = element->xmlChildrenNode;

			if (element == NULL) {
				printf("[FileBrowser] SC: Directory cannot be read.\n");
				CFile file;
				file.Mode = S_IFDIR + 0777 ;
				file.Name = dirname + "..";
				file.Size = 0;
				file.Time = 0;
				flist->push_back(file);
			} else {
				char * tunein_base = NULL;

				if (xml_decode == 1) {
					CFile file;
					file.Mode = S_IFDIR + 0777 ;
					file.Name = " Top500"; // use space to have it at the beginning of the list
					file.Url = sc_get_top500;
					file.Size = 0;
					file.Time = 0;
					flist->push_back(file);
				} else if (xml_decode == 2) {
					CFile file2;
					file2.Mode = S_IFDIR + 0777 ;
					file2.Name = "..";
					file2.Url = sc_init_dir;
					file2.Size = 0;
					file2.Time = 0;
					flist->push_back(file2);
				}
				while (element) {
					CFile file;
					if (xml_decode == 1) {
						file.Mode = S_IFDIR + 0777 ;
						file.Name = xmlGetAttribute(element, "name");
						file.Url = sc_get_genre + file.Name;
						file.Size = 0;
						file.Time = 0;
						flist->push_back(file);
					}
					else if (xml_decode == 2) {
						ptr = xmlGetName(element);
						if (ptr != NULL) {
							if (strcmp(ptr, "tunein")==0) {
								ptr = xmlGetAttribute(element, "base");
								if (ptr)
									tunein_base = ptr;
							} else if (strcmp(ptr, "station")==0) {
								ptr = xmlGetAttribute(element, "mt");
								if (ptr && (strcmp(ptr, "audio/mpeg")==0)) {
									file.Mode = S_IFREG + 0777 ;
									file.Name = xmlGetAttribute(element, "name");
									file.Url = sc_tune_in_base + tunein_base + (std::string)"?id=" + xmlGetAttribute(element, "id") + (std::string)"&k=" + g_settings.shoutcast_dev_id;
									//printf("adding %s (%s)\n", file.Name.c_str(), file.Url.c_str());
									ptr = xmlGetAttribute(element, "br");
									if (ptr) {
										file.Size = atoi(ptr);
										file.Time = atoi(ptr);
									} else {
										file.Size = 0;
										file.Time = 0;
									}
									flist->push_back(file);
								}
							}
						}
					}
					element = element->xmlNextNode;
				}
			}
			xmlFreeDoc(answer_parser);
			return true;
		}
	}

	/* since all CURL error messages use only US-ASCII characters, when can safely print them as if they were UTF-8 encoded */
	if (httpres == 22) {
	    strcat(error, "\nProbably wrong link.");
	}
printf("CFileBrowser::readDir_sc: httpres %d error, %s\n", httpres, error);
	DisplayErrorMessage(error); // UTF-8
	CFile file;

	file.Name = dirname + "..";
	file.Mode = S_IFDIR + 0777;
	file.Size = 0;
	file.Time = 0;
	flist->push_back(file);

	return false;
}
#endif

bool CFileBrowser::readDir_std(const std::string & dirname, CFileList* flist)
{
//	printf("readDir_std %s\n",dirname.c_str());
	stat_struct statbuf;
	dirent_struct **namelist;
	int n;

	n = my_scandir(dirname.c_str(), &namelist, 0, my_alphasort);
	if (n < 0)
	{
		perror(("Filebrowser scandir: "+dirname).c_str());
		return false;
	}
	for(int i = 0; i < n;i++)
	{
		CFile file;
		if(strcmp(namelist[i]->d_name,".") != 0)
		{
			file.Name = dirname + namelist[i]->d_name;

//			printf("file.Name: '%s', getFileName: '%s' getPath: '%s'\n",file.Name.c_str(),file.getFileName().c_str(),file.getPath().c_str());
			if(my_stat((file.Name).c_str(),&statbuf) != 0)
				perror("stat error");
			else
			{
				file.Mode = statbuf.st_mode;
				file.Size = statbuf.st_size;
				file.Time = statbuf.st_mtime;
				flist->push_back(file);
			}
		}
		free(namelist[i]);
	}

	free(namelist);

	return true;
}

//------------------------------------------------------------------------

bool CFileBrowser::exec(const char * const dirname)
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	bool res = false;

#ifdef ENABLE_INTERNETRADIO
	if (m_Mode == ModeSC) {
		m_baseurl = base;
	}
#endif

	name = dirname;
	std::replace(name.begin(), name.end(), '\\', '/');

	paintHead();
	ChangeDir(name);
	paint();
	paintFoot();
	frameBuffer->blit();

	int oldselected = selected;

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_FILEBROWSER]);

	bool loop=true;
	while (loop)
	{
		g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd );
		neutrino_msg_t msg_repeatok = msg & ~CRCInput::RC_Repeat;

		if ( msg <= CRCInput::RC_MaxRC )
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_FILEBROWSER]);

		if(!CRCInput::isNumeric(msg))
		{
			m_SMSKeyInput.resetOldKey();
		}

		if (msg == CRCInput::RC_yellow)
		{
			if ((Multi_Select) && (selected < filelist.size()))
			{
				if(filelist[selected].getFileName() != "..")
				{
					if( (S_ISDIR(filelist[selected].Mode) && Dirs_Selectable) || !S_ISDIR(filelist[selected].Mode) )
					{
						filelist[selected].Marked = !filelist[selected].Marked;
						paintItem(selected - liststart);
					}
				}
				msg_repeatok = CRCInput::RC_down;	// jump to next item
			}
		}

		if ((msg == CRCInput::RC_red) || msg == CRCInput::RC_page_down)
		{
			selected += listmaxshow;
			if (selected >= filelist.size()) {
				if (((filelist.size() / listmaxshow) + 1) * listmaxshow == filelist.size() + listmaxshow) // last page has full entries
					selected = 0;
				else
					selected = selected < (((filelist.size() / listmaxshow) + 1) * listmaxshow) ? (filelist.size() - 1) : 0;
			}
			liststart = (selected / listmaxshow) * listmaxshow;
			paint();
		}
		else if ((msg == CRCInput::RC_green) || (msg == CRCInput::RC_page_up) )
		{
			if ((int(selected)-int(listmaxshow))<0)
				selected=filelist.size()-1;
			else
				selected -= listmaxshow;
			liststart = (selected/listmaxshow)*listmaxshow;
			paint();
		}
		else if (msg_repeatok == CRCInput::RC_up)
		{
			int prevselected=selected;
			if(selected==0)
			{
				selected = filelist.size()-1;
			}
			else
				selected--;
			paintItem(prevselected - liststart);
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
		else if (msg_repeatok == CRCInput::RC_down)
		{
			if (!(filelist.empty()))
			{
				int prevselected=selected;
				selected = (selected + 1) % filelist.size();
				paintItem(prevselected - liststart);
				unsigned int oldliststart = liststart;
				liststart = (selected/listmaxshow)*listmaxshow;
				if(oldliststart!=liststart)
					paint();
				else
					paintItem(selected - liststart);
			}
		}
		else if ( ( msg == CRCInput::RC_timeout ) )
		{
			selected = oldselected;
			loop=false;
		}
		else if ( msg == CRCInput::RC_right )
		{
			if (!(filelist.empty()))
			{
				if (S_ISDIR(filelist[selected].Mode))
				{
#ifdef ENABLE_INTERNETRADIO
					if (m_Mode == ModeSC) {
						ChangeDir(filelist[selected].Url);
					} else
#endif
					{
	 					if (filelist[selected].getFileName() != "..") {
							selections.push_back(selected);
							ChangeDir(filelist[selected].Name);
						}
					}
				}
			}
		}
		else if ( msg == CRCInput::RC_left )
		{
#ifdef ENABLE_INTERNETRADIO
			if (m_Mode == ModeSC)
			{
				for(unsigned int i = 0; i < filelist.size();i++) {
					if (S_ISDIR(filelist[i].Mode) && filelist[i].getFileName() == "..") {
						ChangeDir(filelist[i].Url);
						break;
					}
				}
			}
			else
#endif
			if (selections.size() > 0)
			{
				ChangeDir("..",selections.back());
				selections.pop_back();
			} else
			{
				ChangeDir("..");
			}
		}
		else if ( msg == CRCInput::RC_blue )
		{
			if(Filter != NULL)
			{
				use_filter = !use_filter;
				paintFoot();
				ChangeDir(Path);
			}
		}
		else if ( msg == CRCInput::RC_home )
		{
			loop = false;
		}
		else if ( msg == CRCInput::RC_spkr && strncmp(Path.c_str(), VLC_URI, strlen(VLC_URI)) != 0) //Not in vlc mode
		{
			if(".." !=(filelist[selected].getFileName().substr(0,2))) // do not delete that
			{
				std::stringstream _msg;
				_msg << g_Locale->getText(LOCALE_FILEBROWSER_DODELETE1) << " ";
				if (filelist[selected].getFileName().length() > 25)
				{
					_msg << filelist[selected].getFileName().substr(0, 25) << "...";
				}
				else
					_msg << filelist[selected].getFileName();

				_msg << " " << g_Locale->getText(LOCALE_FILEBROWSER_DODELETE2);
				if (ShowMsgUTF(LOCALE_FILEBROWSER_DELETE, _msg.str(), CMessageBox::mbrNo, CMessageBox::mbYes|CMessageBox::mbNo)==CMessageBox::mbrYes)
				{
					recursiveDelete(filelist[selected].Name.c_str());
					if(".ts" ==(filelist[selected].getFileName().substr(filelist[selected].getFileName().length()-3,filelist[selected].getFileName().length())))//if bla.ts
					{
						recursiveDelete((filelist[selected].Name.substr(0,filelist[selected].Name.length()-7)+".xml").c_str());//remove bla.xml von bla.ts
					}
					ChangeDir(Path);

				}
			}
		}
		else if (msg == CRCInput::RC_ok)
		{
			if (!(filelist.empty()))
			{
				if (filelist[selected].getFileName() == "..")
				{
#ifdef ENABLE_INTERNETRADIO
					if (m_Mode == ModeSC)
						ChangeDir(filelist[selected].Url);
					else
#endif
					{
						if (selections.size() > 0)
						{
							ChangeDir("..",selections.back());
							selections.pop_back();
						} else
						{
							std::string::size_type pos = Path.substr(0,Path.length()-1).rfind('/');
							if (pos != std::string::npos) {
								ChangeDir("..");
							}
#if 0 // quick fix for #253, i dont think we ever need to return "/" as selected -- focus
							else {
								loop = false;
								res = true;
								filelist[selected].Name = "/";
							}
#endif
						}
					}
				}
				else
				{
					std::string filename = filelist[selected].Name;
					if ( filename.length() > 1 )
					{
						if((!Multi_Select) && S_ISDIR(filelist[selected].Mode) && !Dir_Mode)
						{
#ifdef ENABLE_INTERNETRADIO
							if (m_Mode == ModeSC)
								ChangeDir(filelist[selected].Url);
							else
#endif
								ChangeDir(filelist[selected].Name);
						}
						else
						{
							filelist[selected].Marked = true;
							loop = false;
							res = true;
						}
					}
				}
			}
		}
		else if (msg==CRCInput::RC_help)
		{
			if (++g_settings.filebrowser_sortmethod >= FILEBROWSER_NUMBER_OF_SORT_VARIANTS)
				g_settings.filebrowser_sortmethod = 0;

			sort(filelist.begin(), filelist.end(), sortBy[g_settings.filebrowser_sortmethod]);

			paint();
			paintFoot();
		}
		else if (CRCInput::isNumeric(msg_repeatok))
		{
			if (!(filelist.empty()))
				SMSInput(msg_repeatok);
		}
		else if (msg == CRCInput::RC_sat || msg == CRCInput::RC_favorites) {
			//FIXME do nothing ?
		}
		else
		{
			if ( CNeutrinoApp::getInstance()->handleMsg( msg, data ) & messages_return::cancel_all )
			{
				loop = false;
			}
		}
		frameBuffer->blit();
	}

	hide();

	selected_filelist.clear();

	if(res && Multi_Select)
	{
		CProgressWindow * progress = new CProgressWindow();
		progress->setTitle(LOCALE_FILEBROWSER_SCAN);
		progress->exec(NULL,"");
		for(unsigned int i = 0; i < filelist.size();i++)
			if(filelist[i].Marked)
			{
				if(S_ISDIR(filelist[i].Mode)) {
#ifdef ENABLE_INTERNETRADIO
					if (m_Mode == ModeSC)
						addRecursiveDir(&selected_filelist,filelist[i].Url, true, progress);
					else
#endif
						addRecursiveDir(&selected_filelist,filelist[i].Name, true, progress);
				} else
					selected_filelist.push_back(filelist[i]);
			}
		progress->hide();
		delete progress;
	}

	return res;
}

//------------------------------------------------------------------------

void CFileBrowser::addRecursiveDir(CFileList * re_filelist, std::string rpath, bool bRootCall, CProgressWindow * progress)
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	int n;

	//printf("addRecursiveDir %s\n",rpath.c_str());

	if (bRootCall) bCancel=false;

	g_RCInput->getMsg_us(&msg, &data, 1);
	if (msg==CRCInput::RC_home)
	{
		// home key cancel scan
		bCancel=true;
	}
	else if (msg!=CRCInput::RC_timeout)
	{
		// other event, save to low priority queue
		g_RCInput->postMsg( msg, data, false );
	}
	if(bCancel)
		return;

	if ((m_Mode != ModeSC) && ((rpath.empty()) || ((*rpath.rbegin()) != '/')))
	{
		rpath += '/';
	}

	CFileList tmplist;
	if(!readDir(rpath, &tmplist))
	{
		perror(("Recursive scandir: "+rpath).c_str());
	}
	else
	{
		n = tmplist.size();
		if(progress)
		{
			progress->showStatusMessageUTF(FILESYSTEM_ENCODING_TO_UTF8_STRING(rpath));
		}
		for(int i = 0; i < n;i++)
		{
			if(progress)
			{
				progress->showGlobalStatus(100/n*i);
			}
			std::string basename = tmplist[i].Name.substr(tmplist[i].Name.rfind('/')+1);
			if( basename != ".." )
			{
				if(Filter != NULL && (!S_ISDIR(tmplist[i].Mode)) && use_filter)
				{
					if(!Filter->matchFilter(tmplist[i].Name))
					{
						continue;
					}
				}
				if(!S_ISDIR(tmplist[i].Mode))
					re_filelist->push_back(tmplist[i]);
				else
					addRecursiveDir(re_filelist,tmplist[i].Name, false, progress);
			}
		}
	}
}


//------------------------------------------------------------------------

void CFileBrowser::hide()
{
	frameBuffer->paintBackgroundBoxRel(x,y, width,height);
	frameBuffer->blit();
}

//------------------------------------------------------------------------

void CFileBrowser::paintItem(unsigned int pos)
{
	int colwidth1, colwidth2, colwidth3, colwidth1_dir, colwidth2_dir;
	int c_rad_small;
	uint8_t color;
	fb_pixel_t bgcolor;
	int ypos = y+ theight+0 + pos*fheight;
	CFile * actual_file = NULL;
	std::string fileicon;

	colwidth2_dir = 180;
	colwidth1_dir = width - 35 - colwidth2_dir - 10;

	if (liststart + pos == selected)
	{
		color   = COL_MENUCONTENTSELECTED;
		bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
		c_rad_small = RADIUS_SMALL;
// 		paintFoot();
	}
	else
	{
		color   = COL_MENUCONTENT;//DARK;
		bgcolor = COL_MENUCONTENT_PLUS_0;//DARK;
		c_rad_small = 0;
	}
	
	frameBuffer->paintBoxRel(x,ypos, width- 15, fheight, COL_MENUCONTENT_PLUS_0);
	frameBuffer->paintBoxRel(x,ypos, width- 15, fheight, bgcolor, c_rad_small);

	if( (liststart + pos) < filelist.size() )
	{
		actual_file = &filelist[liststart+pos];
		if (actual_file->Marked)
		{
			color = COL_MENUCONTENTINACTIVE; //+= 2; FIXME
			bgcolor = (liststart + pos == selected) ? COL_MENUCONTENTSELECTED_PLUS_2 : COL_MENUCONTENT_PLUS_2;
		}

		if (g_settings.filebrowser_showrights == 0 && S_ISREG(actual_file->Mode))
		{
			colwidth2 = 0;
			colwidth3 = 90;
		}
		else
		{
			colwidth2 = 90;
			colwidth3 = 90;
		}
		colwidth1 = width - 35 - colwidth2 - colwidth3 - 10;

		if ( actual_file->Name.length() > 0 )
		{
			if (liststart+pos==selected)
				CVFD::getInstance()->showMenuText(0, FILESYSTEM_ENCODING_TO_UTF8_STRING(actual_file->getFileName()).c_str(), -1, true); // UTF-8

			switch(actual_file->getType())
			{
			case CFile::FILE_CDR:
			case CFile::FILE_MP3:
			case CFile::FILE_OGG:
			case CFile::FILE_WAV:
#ifdef ENABLE_FLAC
			case CFile::FILE_FLAC:
#endif
				fileicon = NEUTRINO_ICON_MP3;
//				color = COL_MENUCONTENT;
				break;

			case CFile::FILE_DIR:
				fileicon = NEUTRINO_ICON_FOLDER;
				break;

			case CFile::FILE_PICTURE:
			case CFile::FILE_TEXT:
			default:
				fileicon = NEUTRINO_ICON_FILE;
			}
			frameBuffer->paintIcon(fileicon, x+5 , ypos + (fheight-16) / 2 );

			g_Font[SNeutrinoSettings::FONT_TYPE_FILEBROWSER_ITEM]->RenderString(x + 35, ypos + fheight, colwidth1 - 10 , FILESYSTEM_ENCODING_TO_UTF8_STRING(actual_file->getFileName()), color, 0, true); // UTF-8

			if( S_ISREG(actual_file->Mode) )
			{
				if (g_settings.filebrowser_showrights != 0)
				{
					const char * attribute = "xwr";
					char modestring[9 + 1];
					for (int m = 8; m >= 0; m--)
					{
						modestring[8 - m] = (actual_file->Mode & (1 << m)) ? attribute[m % 3] : '-';
					}
					modestring[9] = 0;

					g_Font[SNeutrinoSettings::FONT_TYPE_FILEBROWSER_ITEM]->RenderString(x + 35 + colwidth1 , ypos+ fheight, colwidth2 - 10, modestring, color, 0, true); // UTF-8
				}

#define GIGABYTE 1073741824LL
#define MEGABYTE 1048576LL
#define KILOBYTE 1024LL
				char tmpstr[256];
				const char *unit = "";
				int64_t factor = 0;
				if (actual_file->Size >= GIGABYTE)
				{
					factor = GIGABYTE;
					unit = "G";
				}
				else if (actual_file->Size >= MEGABYTE)
				{
					factor = MEGABYTE;
					unit = "M";
				}
				else if (actual_file->Size >= KILOBYTE)
				{
					factor = KILOBYTE;
					unit = "k";
				}
				if (factor)
				{
					int a = actual_file->Size / factor;
					int b = (actual_file->Size - a * factor) * 1000 / factor;
					snprintf(tmpstr, sizeof(tmpstr), "%d.%03d%s", a, b, unit);
				}
				else
					snprintf(tmpstr,sizeof(tmpstr),"%d", (int)actual_file->Size);

				g_Font[SNeutrinoSettings::FONT_TYPE_FILEBROWSER_ITEM]->RenderString(x + 35 + colwidth1 + colwidth2, ypos+ fheight, colwidth3 - 10, tmpstr, color);
			}

			if( S_ISDIR(actual_file->Mode) )
			{
				char timestring[18];
				time_t rawtime;

				rawtime = actual_file->Time;
				strftime(timestring, 18, "%d-%m-%Y %H:%M", gmtime(&rawtime));

				g_Font[SNeutrinoSettings::FONT_TYPE_FILEBROWSER_ITEM]->RenderString(x + 35 + colwidth1_dir, ypos+ fheight, colwidth2_dir - 10, timestring, color);
			}
		}
	}
	else
		frameBuffer->paintBoxRel(x,ypos, width- 15, fheight, COL_MENUCONTENT_PLUS_0/*DARK*/);
}

//------------------------------------------------------------------------

void CFileBrowser::paintHead()
{
	char *l_name;
	int i = 0;
	int l;
	frameBuffer->paintBoxRel(x,y, width,theight+0, COL_MENUHEAD_PLUS_0, RADIUS_MID, CORNER_TOP);
#ifdef ENABLE_INTERNETRADIO
	if(m_Mode == ModeSC)
		l = asprintf(&l_name, "%s %s", g_Locale->getText(LOCALE_AUDIOPLAYER_ADD_SC), FILESYSTEM_ENCODING_TO_UTF8_STRING(name).c_str());
	else
#endif
		l = asprintf(&l_name, "%s %s", g_Locale->getText(LOCALE_FILEBROWSER_HEAD), FILESYSTEM_ENCODING_TO_UTF8_STRING(name).c_str());

	if (l < 1) /* at least 1 for the " " space */
	{
		perror("CFileBrowser::paintHead asprintf");
		return;
	}

	/* too long? Leave out the "Filebrowser" or "Shoutcast" prefix
	 * the allocated space is sufficient since it is surely shorter than before */
	if (g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_TITLE]->getRenderWidth(l_name) > width - 11)
		l = sprintf(l_name, "%s", FILESYSTEM_ENCODING_TO_UTF8_STRING(name).c_str());
	if (l_name[l - 1] == '/')
		l_name[--l] = '\0';

	/* still too long? the last part is probably more interesting than the first part... */
	while ((g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_TITLE]->getRenderWidth(&l_name[i]) > width - 11)
		&& (i < l))
		i++;

	g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_TITLE]->RenderString(x+10,y+theight+1, width-11, &l_name[i], COL_MENUHEAD, 0, true);
	free(l_name);
}

bool chooserDir(char *setting_dir, bool test_dir, const char *action_str, size_t str_leng)
{
	std::string tmp_setting_dir = setting_dir;
	if(chooserDir(tmp_setting_dir, test_dir, action_str)){
		strncpy(setting_dir,tmp_setting_dir.c_str(), str_leng);
		return true;
	}
	return false;
}
bool chooserDir(std::string &setting_dir, bool test_dir, const char *action_str)
{
	const char *wrong_str = "Wrong/unsupported";
		CFileBrowser b;
		b.Dir_Mode=true;
		if (b.exec(setting_dir.c_str())) {
			const char * newdir = b.getSelectedFile()->Name.c_str();
			if(test_dir && check_dir(newdir)){
				printf("%s %s dir %s\n",wrong_str ,action_str, newdir);
				return false;
			}else {
				setting_dir = b.getSelectedFile()->Name;
				return true;
			}
		}
	return false;
}

//------------------------------------------------------------------------

const struct button_label FileBrowserFilterButton[2] =
{
	{ NEUTRINO_ICON_BUTTON_BLUE  , LOCALE_FILEBROWSER_FILTER_INACTIVE },
	{ NEUTRINO_ICON_BUTTON_BLUE  , LOCALE_FILEBROWSER_FILTER_ACTIVE   },
};

void CFileBrowser::paintFoot()
{
	struct button_label FileBrowserButtons[4] =
	{
		{ NEUTRINO_ICON_BUTTON_RED   , LOCALE_FILEBROWSER_NEXTPAGE        },
		{ NEUTRINO_ICON_BUTTON_GREEN , LOCALE_FILEBROWSER_PREVPAGE        },
		{ NEUTRINO_ICON_BUTTON_YELLOW, LOCALE_FILEBROWSER_MARK            },
	};

	const struct button_label FileBrowserButtons2[3] =
	{
		{ NEUTRINO_ICON_BUTTON_OKAY   , LOCALE_FILEBROWSER_SELECT        },
		{ NEUTRINO_ICON_BUTTON_HELP_SMALL , sortByNames[g_settings.filebrowser_sortmethod]        },
		{ NEUTRINO_ICON_BUTTON_MUTE_SMALL, LOCALE_FILEBROWSER_DELETE            },
	};

// 	int iw = 0, ih = 0; 
// 	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_RED, &iw, &ih); 

	//Background
	int by0 = y + height - (2 * foheight );
	frameBuffer->paintBoxRel(x, by0, width, (2 * foheight ), COL_INFOBAR_SHADOW_PLUS_1, RADIUS_MID, CORNER_BOTTOM);
	
	//Second Line (bottom, top)
	int by2 = by0 + foheight;

	if (!(filelist.empty()))
	{
		int idx = 1;
		int num_buttons = Multi_Select ? 3 : 2;
		if (Filter != NULL)
		{
			FileBrowserButtons[num_buttons].button = FileBrowserFilterButton[!use_filter].button;
			FileBrowserButtons[num_buttons].locale = FileBrowserFilterButton[!use_filter].locale;
			num_buttons++;
		}
		//red, green, yellow button
		::paintButtons(x, by0, 0, num_buttons, FileBrowserButtons, width, foheight);

		/* TODO: the changing existence of the OK button makes the sort button
		 *       shift its place :-( */
		num_buttons = 1;
		//OK-Button
		if ((filelist[selected].getType() != CFile::FILE_UNKNOWN) || S_ISDIR(filelist[selected].Mode))
		{
			idx = 0;
			num_buttons++;
		}
		if (strncmp(Path.c_str(), VLC_URI, strlen(VLC_URI)) != 0) // No delete in vlc mode
			num_buttons++;
		/* width-26 to leave room for the SMSinput indicator */
		::paintButtons(x, by2, 0, num_buttons, &(FileBrowserButtons2[idx]), width - 26, foheight);

		if(m_SMSKeyInput.getOldKey()!=0)
		{
			char cKey[2]={m_SMSKeyInput.getOldKey(),0};
			cKey[0] = toupper(cKey[0]);
			int len = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getRenderWidth(cKey);
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(x + width - 10 - len, by2 + foheight, len, cKey, COL_MENUHEAD, 0, true);
		}
	}
}

//------------------------------------------------------------------------

void CFileBrowser::paint()
{
	liststart = (selected/listmaxshow)*listmaxshow;

//	if (filelist[0].Name.length() != 0)
//		frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_HELP, x+ width- 30, y+ 5 );
	CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8, g_Locale->getText(LOCALE_FILEBROWSER_HEAD));

	for(unsigned int count=0;count<listmaxshow;count++)
		paintItem(count);

	//scrollbar
	int ypos = y+ theight;
	int sb = fheight* listmaxshow;
	frameBuffer->paintBoxRel(x+ width- 15,ypos, 15, sb,  COL_MENUCONTENT_PLUS_1);

	int sbc= ((filelist.size()- 1)/ listmaxshow)+ 1;
	int sbs= (selected/listmaxshow);

	frameBuffer->paintBoxRel(x+ width- 13, ypos+ 2+ sbs*(sb-4)/sbc, 11, (sb-4)/sbc, COL_MENUCONTENT_PLUS_3, RADIUS_SMALL);
}

//------------------------------------------------------------------------

void CFileBrowser::SMSInput(const neutrino_msg_t msg)
{
	unsigned char key = m_SMSKeyInput.handleMsg(msg);

	unsigned int i;
	for(i=(selected+1) % filelist.size(); i != selected ; i= (i+1) % filelist.size())
	{
		if(tolower(filelist[i].getFileName()[0]) == key)
		{
			break;
		}
	}
	int prevselected=selected;
	selected=i;
	paintItem(prevselected - liststart);
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

//------------------------------------------------------------------------

void CFileBrowser::recursiveDelete(const char* file)
{
	stat_struct statbuf;
	dirent_struct **namelist;
	int n;
	printf("Delete %s\n", file);
	if(my_lstat(file,&statbuf) == 0)
	{
		if(S_ISDIR(statbuf.st_mode))
		{
			n = my_scandir(file, &namelist, 0, my_alphasort);
			while(n--)
			{
				if(strcmp(namelist[n]->d_name, ".")!=0 && strcmp(namelist[n]->d_name, "..")!=0)
				{
					std::string fullname = (std::string)file + "/" + namelist[n]->d_name;
					recursiveDelete(fullname.c_str());
				}
				free(namelist[n]);
			}
			free(namelist);
			rmdir(file);
		}
		else
		{
			unlink(file);
		}
	}
	else
		perror(file);
}
