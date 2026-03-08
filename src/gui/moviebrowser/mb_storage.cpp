/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

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

	***********************************************************

	Module Name: mb_storage.cpp

	Description: Storage and scan helpers for moviebrowser

	Date:	   Nov 2005
	Author: Guenther@tuxbox.berlios.org
		based on code of Steffen Hehn 'McClean'

	(C) 2009-2015 Stefan Seyfried
	(C) 2016      Sven Hoefer

	outsourced:
	(C) 2026 Thilo Graf 'dbt'
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mb.h"
#include "mb_constants.h"

#include <global.h>
#include <memory>

#include <gui/filebrowser.h>
#include <system/hddstat.h>

#include <dirent.h>
#include <inttypes.h>
#include <stdio.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#define my_scandir scandir64
#define my_alphasort alphasort64
typedef struct stat64 stat_struct;
typedef struct dirent64 dirent_struct;
#define my_stat stat64

bool CMovieBrowser::addDir(std::string& dirname, int* used)
{
	if (dirname.empty()) return false;
	if (dirname == "/") return false;

	MB_DIR newdir;
	newdir.name = dirname;

	if (newdir.name.rfind('/') != newdir.name.length()-1 || newdir.name.length() == 0)
		newdir.name += '/';

	int size = m_dir.size();
	for (int i = 0; i < size; i++)
	{
		if (strcmp(m_dir[i].name.c_str(),newdir.name.c_str()) == 0)
		{
			// string is identical to previous one
			dprintf(DEBUG_DEBUG, "[mb] Dir already in list: %s\n",newdir.name.c_str());
			return (false);
		}
	}
	dprintf(DEBUG_DEBUG, "[mb] new Dir: %s\n",newdir.name.c_str());
	newdir.used = used;
	m_dir.push_back(newdir);
	if (*used == true)
	{
		m_file_info_stale = true; // we got a new Dir, search again for all movies next time
		m_seriename_stale = true;
	}
	return (true);
}

void CMovieBrowser::updateDir(void)
{
	m_dir.clear();
#if 0
	// check if there is a movie dir and if we should use it
	if (g_settings.network_nfs_moviedir[0] != 0)
	{
		std::string name = g_settings.network_nfs_moviedir;
		addDir(name,&m_settings.store.storageDirMovieUsed);
	}
#endif
	// check if there is a record dir and if we should use it
	if (!g_settings.network_nfs_recordingdir.empty())
	{
		addDir(g_settings.network_nfs_recordingdir, &m_settings.store.storageDirRecUsed);
		cHddStat::getInstance()->statOnce();
	}

	for (int i = 0; i < MB_MAX_DIRS; i++)
	{
		if (!m_settings.store.storageDir[i].empty())
			addDir(m_settings.store.storageDir[i],&m_settings.store.storageDirUsed[i]);
	}
}

void CMovieBrowser::loadAllTsFileNamesFromStorage(void)
{

	m_movieSelectionHandler = NULL;
	m_dirNames.clear();
	m_vHandleBrowserList.clear();
	m_vHandleRecordList.clear();
	m_vHandlePlayList.clear();
	m_vHandleSerienames.clear();
	m_vMovieInfo.clear();
	m_vMovieInfo.reserve(256);

	updateDir();

	size_t i, size;
	size = m_dir.size();

	size_t used_dirs = 0;
	for (i = 0; i < size;i++)
	{
		if (*m_dir[i].used)
			used_dirs ++;
	}
	OnSetGlobalMax(used_dirs);

	for (i = 0; i < size; i++)
	{
		if (*m_dir[i].used)
		{
			loadTsFileNamesFromDir(m_dir[i].name);
		}
	}

	dprintf(DEBUG_DEBUG, "[mb] Dir %d, Files: %d, size %zu, used_dirs %zu\n", (int)m_dirNames.size(), (int)m_vMovieInfo.size(), size, used_dirs);
}

bool CMovieBrowser::gotMovie(const char *rec_title)
{

	m_doRefresh = false;
	loadAllTsFileNamesFromStorage();

	bool found = false;
	for (unsigned int i = 0; i < m_vMovieInfo.size(); i++)
	{
		//dprintf(DEBUG_DEBUG, "[mb] search for %s in %s\n", rec_title, m_vMovieInfo[i]->epgTitle.c_str());
		if (strcmp(rec_title, m_vMovieInfo[i]->epgTitle.c_str()) == 0)
		{
			found = true;
			break;
		}
	}
	return found;
}

static const char * const ext_list[] =
{
	"avi", "mkv", "mp4", "flv", "mov", "mpg", "mpeg", "m2ts", "iso"
};

static int ext_list_size = sizeof(ext_list) / sizeof (char *);

bool CMovieBrowser::supportedExtension(CFile &file)
{
	std::string::size_type idx = file.getFileName().rfind('.');

	if (idx == std::string::npos)
		return false;

	std::string ext = file.getFileName().substr(idx+1);
	bool result = (ext == "ts");
	if (!result && !m_settings.ts_only) {
		for (int i = 0; i < ext_list_size; i++) {
			if (!strcasecmp(ext.c_str(), ext_list[i]))
				return true;
		}
	}
	return result;
}

bool CMovieBrowser::addFile(CFile &file, int dirItNr)
{
	if (!S_ISDIR(file.Mode) && !supportedExtension(file)) {
		return false;
	}

	std::unique_ptr<MI_MOVIE_INFO> mi(new MI_MOVIE_INFO());

	mi->file = file;
	if (!m_movieInfo.loadMovieInfo(mi.get())) {
		mi->channelName = std::string(g_Locale->getText(LOCALE_MOVIEPLAYER_HEAD));
		mi->epgTitle = file.getFileName();
	}
	mi->dirItNr = dirItNr;
	m_vMovieInfo.push_back(std::move(mi));
	return true;
}

/************************************************************************
Note: this function is used recursive, do not add any return within the body due to the recursive counter
************************************************************************/
bool CMovieBrowser::loadTsFileNamesFromDir(const std::string & dirname)
{

	static int recursive_counter = 0; // recursive counter to be used to avoid hanging
	bool result = false;

	if (recursive_counter > 10)
	{
		dprintf(DEBUG_DEBUG, "[mb]loadTsFileNamesFromDir: return->recoursive error\n");
		return (false); // do not go deeper than 10 directories
	}

	/* check if directory was already searched once */
	for (int i = 0; i < (int) m_dirNames.size(); i++)
	{
		if (strcmp(m_dirNames[i].c_str(),dirname.c_str()) == 0)
		{
			// string is identical to previous one
			dprintf(DEBUG_DEBUG, "[mb]Dir already in list: %s\n",dirname.c_str());
			return (false);
		}
	}
	/* FIXME hack to fix movie dir path on recursive scan.
	   dirs without files but with subdirs with files will be shown in path filter */
	m_dirNames.push_back(dirname);
	int dirItNr = m_dirNames.size() - 1;

	/* !!!!!! no return statement within the body after here !!!!*/
	recursive_counter++;

	CFileList flist;
	if (readDir(dirname, &flist) == true)
	{
		size_t count = flist.size();
		for (size_t i = 0; i < count; i++)
		{
			if (S_ISDIR(flist[i].Mode)) {
				if (m_settings.ts_only || !CFileBrowser::checkBD(flist[i])) {
					flist[i].Name += '/';
					result |= loadTsFileNamesFromDir(flist[i].Name);
				} else
					result |= addFile(flist[i], dirItNr);
			} else {
				result |= addFile(flist[i], dirItNr);
			}
			if (result)
				OnProgress(i, count, dirname );
		}
		//result = true;
	}
	if (!result)
		m_dirNames.pop_back();

	recursive_counter--;
	return (result);
}

bool CMovieBrowser::readDir(const std::string & dirname, CFileList* flist)
{
	bool result = true;
	stat_struct statbuf;
	dirent_struct **namelist;
	int n;

	n = my_scandir(dirname.c_str(), &namelist, 0, my_alphasort);
	if (n < 0)
	{
		perror(("[mb] scandir: "+dirname).c_str());
		return false;
	}
	CFile file;
	for (int i = 0; i < n;i++)
	{
		if (namelist[i]->d_name[0] != '.')
		{
			file.Name = dirname;
			file.Name += namelist[i]->d_name;

			if (my_stat((file.Name).c_str(),&statbuf) != 0)
				fprintf(stderr, "stat '%s' error: %m\n", file.Name.c_str());
			else
			{
				file.Mode = statbuf.st_mode;
				file.Time = statbuf.st_mtime;
				file.Size = statbuf.st_size;
				flist->push_back(file);
			}
		}
		free(namelist[i]);
	}

	free(namelist);

	return(result);
}

bool CMovieBrowser::delFile(CFile& file)
{
	bool result = true;
	int err = unlink(file.Name.c_str());
	dprintf(DEBUG_DEBUG, "  delete file: %s\r\n",file.Name.c_str());
	if (err)
		result = false;
	return(result);
}

void CMovieBrowser::loadMovies(bool doRefresh)
{
	dprintf(DEBUG_DEBUG, "[mb] loadMovies: \n");

	struct timeval t1, t2;
	gettimeofday(&t1, NULL);

	CProgressWindowA loadBox(LOCALE_MOVIEBROWSER_SCAN_FOR_MOVIES, CCW_PERCENT 50, CCW_PERCENT 10, &OnProgress, &OnSetGlobalMax);
	loadBox.enableShadow();
	loadBox.paint();

	loadAllTsFileNamesFromStorage(); // P1
	m_seriename_stale = true; // we reloded the movie info, so make sure the other list are updated later on as well
	updateSerienames();
	if (m_settings.serie_auto_create == 1)
		autoFindSerie();

	m_file_info_stale = false;

	gettimeofday(&t2, NULL);
	uint64_t duration = ((t2.tv_sec * 1000000ULL + t2.tv_usec) - (t1.tv_sec * 1000000ULL + t1.tv_usec)) / 1000ULL;
	if (duration)
		fprintf(stderr, "\033[33m[CMovieBrowser] %s: %" PRIu64 " ms to scan movies \033[0m\n",__func__, duration);

	loadBox.hide();

	if (doRefresh)
	{
		refreshBrowserList();
		refreshLastPlayList();
		refreshLastRecordList();
		refreshFilterList();
	}

	m_doLoadMovies = false;
}

void CMovieBrowser::loadAllMovieInfo(void)
{
	for (unsigned int i=0; i < m_vMovieInfo.size();i++)
		m_movieInfo.loadMovieInfo(m_vMovieInfo[i].get());
}

void CMovieBrowser::updateSerienames(void)
{
	if (m_seriename_stale == false)
		return;

	m_vHandleSerienames.clear();
	for (unsigned int i = 0; i < m_vMovieInfo.size(); i++)
	{
		MI_MOVIE_INFO *mi = m_vMovieInfo[i].get();
		if (!mi->serieName.empty())
		{
			// current series name is not empty, lets see if we already have it in the list, and if not save it to the list.
			bool found = false;
			for (unsigned int t = 0; t < m_vHandleSerienames.size() && found == false; t++)
			{
				if (strcmp(m_vHandleSerienames[t]->serieName.c_str(), mi->serieName.c_str()) == 0)
					found = true;
			}
			if (found == false)
				m_vHandleSerienames.push_back(mi);
		}
	}
	dprintf(DEBUG_DEBUG, "[mb]->updateSerienames: %d\n", (int)m_vHandleSerienames.size());
	// TODO sort(m_serienames.begin(), m_serienames.end(), my_alphasort);
	m_seriename_stale = false;
}

void CMovieBrowser::autoFindSerie(void)
{
	dprintf(DEBUG_DEBUG, "autoFindSerie\n");
	updateSerienames(); // we have to make sure that the seriename array is up to date, otherwise this does not work
	// if the array is not stale, the function is left immediately
	for (unsigned int i = 0; i < m_vMovieInfo.size(); i++)
	{
		MI_MOVIE_INFO *mi = m_vMovieInfo[i].get();
		// For all movie infos, which do not have a seriename, we try to find one.
		// We search for a movieinfo with seriename, and than we do check if the title is the same
		// in case of same title, we assume both belongs to the same serie
		if (mi->serieName.empty())
		{
			for (unsigned int t=0; t < m_vHandleSerienames.size();t++)
			{
				if (mi->epgTitle == m_vHandleSerienames[t]->epgTitle)
				{
					mi->serieName = m_vHandleSerienames[t]->serieName;
					break; // we  found a maching serie, nothing to do else, leave for(t=0)
				}
			}
		}
	}
}
