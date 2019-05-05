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

	Module Name: mb.h

	Description: Implementation of the CMovieBrowser class
	             This class provides a filebrowser window to view, select and start a movies from HD.
	             This class does replace the Filebrowser

	Date:	   Nov 2005

	Author: Guenther@tuxbox.berlios.org
		based on code of Steffen Hehn 'McClean'

	(C) 2009-2014 Stefan Seyfried

	outsourced:
	(C) 2016, Thilo Graf 'dbt'
*/

#ifndef MOVIEBROWSER_H_
#define MOVIEBROWSER_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mb_types.h"

#include <configfile.h>

//#include <string>
#include <vector>
#include <list>

#include <gui/widget/textbox.h>
#include <gui/widget/listframe.h>
#include <driver/movieinfo.h>
#include <driver/file.h>
#include <driver/fb_window.h>
#include <system/ytparser.h>
#include <gui/widget/progresswindow.h>
#include <gui/imdb.h>

#define MAX_NUMBER_OF_BOOKMARK_ITEMS MI_MOVIE_BOOK_USER_MAX // we just use the same size as used in Movie info (MAX_NUMBER_OF_BOOKMARK_ITEMS is used for the number of menu items)
#define MOVIEBROWSER_SETTINGS_FILE          CONFIGDIR "/moviebrowser.conf"

/* percent */
#define MIN_BROWSER_FRAME_HEIGHT 10
#define MAX_BROWSER_FRAME_HEIGHT 80

#define MB_MAX_ROWS LF_MAX_ROWS
#define MB_MAX_DIRS NETWORK_NFS_NR_OF_ENTRIES
/* MB_SETTINGS to be stored in g_settings anytime ....*/
typedef struct
{
	// moviebrowser
	MB_GUI gui; //MB_GUI
	MB_SORTING sorting; //MB_SORTING
	MB_FILTER filter;//MB_FILTER
	MI_PARENTAL_LOCKAGE parentalLockAge ;//MI_PARENTAL_LOCKAGE
	MB_PARENTAL_LOCK parentalLock;//MB_PARENTAL_LOCK

	std::string storageDir[MB_MAX_DIRS];
	int storageDirUsed[MB_MAX_DIRS];
	int storageDirRecUsed;
	int storageDirMovieUsed;

	int reload;
	int remount;
	int ts_only;

	int browser_serie_mode;
	int serie_auto_create;

	/* these variables are used for the listframes */
	int browserFrameHeight;
	int browserRowNr;
	MB_INFO_ITEM browserRowItem[MB_MAX_ROWS];
	int browserRowWidth[MB_MAX_ROWS];
	int browserAdditional;
	int browserCutLongRowText;

	// to be added to config later
	int lastPlayMaxItems;
	int lastPlayRowNr;
	MB_INFO_ITEM lastPlayRow[3];
	int lastPlayRowWidth[3];

	int lastRecordMaxItems;
	int lastRecordRowNr;
	MB_INFO_ITEM lastRecordRow[3];
	int lastRecordRowWidth[3];

	int ytmode;
	int ytorderby;
	int ytresults;
	int ytquality;
	int ytconcconn;
	int ytsearch_history_size;
	int ytsearch_history_max;
	std::string ytregion;
	std::string ytvid;
	std::string ytsearch;
	std::string ytthumbnaildir;
	std::list<std::string> ytsearch_history;
} MB_SETTINGS;

class CMovieBrowser;

class CYTCacheSelectorTarget : public CMenuTarget
{
	private:
		class CMovieBrowser *movieBrowser;
        public:
		CYTCacheSelectorTarget(CMovieBrowser *mb) { movieBrowser = mb; };
		int exec(CMenuTarget* parent, const std::string & actionKey);
};

// Priorities for Developmemt: P1: critical feature, P2: important feature, P3: for next release, P4: looks nice, lets see
class CMovieBrowser : public CMenuTarget, public CProgressSignals
{
	friend class CYTCacheSelectorTarget;

	public: // Variables /////////////////////////////////////////////////
		int Multi_Select;    // for FileBrowser compatibility, not used in MovieBrowser
		int Dirs_Selectable; // for FileBrowser compatibility, not used in MovieBrowser

	private: // Variables
		CFrameBuffer * framebuffer;

		CListFrame* m_pcBrowser;
		CListFrame* m_pcLastPlay;
		CListFrame* m_pcLastRecord;
		CTextBox* m_pcInfo1; // bottom box
		CTextBox* m_pcInfo2; // right box
		CListFrame* m_pcFilter;

		CBox m_cBoxFrame;
		CBox m_cBoxFrameLastPlayList;
		CBox m_cBoxFrameLastRecordList;
		CBox m_cBoxFrameBrowserList;
		CBox m_cBoxFrameInfo1;
		CBox m_cBoxFrameInfo2;
		CBox m_cBoxFrameBookmarkList;
		CBox m_cBoxFrameFilter;
		CBox m_cBoxFrameFootRel;
		CBox m_cBoxFrameTitleRel;

		CComponentsHeader *m_header;
		CComponentsDetailsLine *m_detailsLine;
		CComponentsChannelLogo *m_channelLogo;
		CComponentsPicture *m_movieCover;

		lf_line_types_t m_browserListLines;
		lf_line_types_t m_recordListLines;
		lf_line_types_t m_playListLines;
		lf_line_types_t m_FilterLines;

		std::vector<MI_MOVIE_INFO> m_vMovieInfo;
		std::vector<MI_MOVIE_INFO*> m_vHandleBrowserList;
		std::vector<MI_MOVIE_INFO*> m_vHandleRecordList;
		std::vector<MI_MOVIE_INFO*> m_vHandlePlayList;
		std::vector<std::string> m_dirNames;
		std::vector<MI_MOVIE_INFO*> m_vHandleSerienames;

		unsigned int m_currentBrowserSelection;
		unsigned int m_currentRecordSelection;
		unsigned int m_currentPlaySelection;
		unsigned int m_currentFilterSelection;
		unsigned int m_prevBrowserSelection;
		unsigned int m_prevRecordSelection;
		unsigned int m_prevPlaySelection;

		bool m_showBrowserFiles;
		bool m_showLastRecordFiles;
		bool m_showLastPlayFiles;
		bool m_showMovieInfo;
		bool m_showFilter;
		bool newHeader;
		bool m_doRefresh;
		bool m_doLoadMovies;

		MI_MOVIE_INFO* m_movieSelectionHandler;
		int m_currentStartPos;
		std::string m_selectedDir;
		MB_FOCUS m_windowFocus;

		bool m_file_info_stale; // if this bit is set, MovieBrowser shall reload all movie infos from HD
		bool m_seriename_stale;

		Font* m_pcFontFoot;
		Font* m_pcFontTitle;
		std::string m_textTitle;

		MB_PARENTAL_LOCK m_parentalLock;
		MB_STORAGE_TYPE m_storageType;

		CConfigFile	configfile;
		CMovieInfo m_movieInfo;
		MB_SETTINGS m_settings;
		std::vector<MB_DIR> m_dir;

		CFileList filelist;
		CFileList::iterator filelist_it;
		P_MI_MOVIE_LIST movielist;

		uint64_t old_EpgId;
		std::string old_ChannelName;

		int movieInfoUpdateAll[MB_INFO_MAX_NUMBER];
		int movieInfoUpdateAllIfDestEmptyOnly;

		std::vector<std::string> PicExts;
		std::string getScreenshotName(std::string movie, bool is_dir = false);

		int menu_ret;

		cYTFeedParser ytparser;
		int show_mode;
		CMenuWidget *yt_menue;
		CYTCacheSelectorTarget *ytcache_selector;
		u_int yt_menue_end;
		int yt_pending_offset;
		int yt_completed_offset;
		int yt_failed_offset;
		int yt_pending_end;
		int yt_completed_end;
		int yt_failed_end;
		std::vector<MI_MOVIE_INFO> yt_pending;
		std::vector<MI_MOVIE_INFO> yt_completed;
		std::vector<MI_MOVIE_INFO> yt_failed;
		void loadYTitles(int mode, std::string search = "", std::string id = "");
		bool showYTMenu(bool calledExternally = false);
		void refreshYTMenu();
		CIMDB *imdb;

	public:  // Functions //////////////////////////////////////////////////////////7
		CMovieBrowser(); //P1
		~CMovieBrowser(); //P1
		int exec(const char* path); //P1
		int exec(CMenuTarget* parent, const std::string & actionKey);
		std::string getCurrentDir(void); //P1 for FileBrowser compatibility
		CFile* getSelectedFile(void); //P1 for FileBrowser compatibility
		bool getSelectedFiles(CFileList &flist, P_MI_MOVIE_LIST &mlist); //P1 for FileBrowser compatibility
		MI_MOVIE_BOOKMARKS* getCurrentMovieBookmark(void){if(m_movieSelectionHandler == NULL) return NULL; return(&(m_movieSelectionHandler->bookmarks));};
		int getCurrentStartPos(void){return(m_currentStartPos);}; //P1 return start position in [s]
		MI_MOVIE_INFO* getCurrentMovieInfo(void){return(m_movieSelectionHandler);}; //P1 return start position in [s]
		void fileInfoStale(void); // call this function to force the Moviebrowser to reload all movie information from HD

		bool readDir(const std::string & dirname, CFileList* flist);

		bool delFile(CFile& file);
		int  getMenuRet() { return menu_ret; }
		int  getMode() { return show_mode; }
		void setMode(int mode) {
			if (show_mode != mode)
				m_file_info_stale = true;
			show_mode = mode;
		}
		bool gotMovie(const char *rec_title);

	private: //Functions
		///// MovieBrowser init ///////////////
		void init(void); //P1
		void initGlobalSettings(void); //P1
		void initFrames(void);
		void initRows(void);
		void reinit(void); //P1

		///// MovieBrowser Main Window//////////
		int paint(void); //P1
		void refresh(void); //P1
		void hide(void); //P1
		void refreshLastPlayList(void); //P2
		void refreshLastRecordList(void); //P2
		void refreshBrowserList(void); //P1
		void refreshFilterList(void); //P1
		void refreshMovieInfo(void); //P1
		void refreshChannelLogo(void); //P1
		void initMovieCover(void); //P1
		void refreshMovieCover(void); //P1
		void hideMovieCover(void); //P1
		int refreshFoot(bool show = true); //P2
		void refreshTitle(void); //P2
		void refreshInfo(void); // P2
		void refreshLCD(void); // P2
		void refreshDetailsLine(int pos);
		void hideDetailsLine();

		///// Events ///////////////////////////
		bool onButtonPress(neutrino_msg_t msg); // P1
		bool onButtonPressMainFrame(neutrino_msg_t msg); // P1
		bool onButtonPressBrowserList(neutrino_msg_t msg); // P1
		bool onButtonPressLastPlayList(neutrino_msg_t msg); // P2
		bool onButtonPressLastRecordList(neutrino_msg_t msg); // P2
		bool onButtonPressFilterList(neutrino_msg_t msg); // P2
		bool onButtonPressMovieInfoList(neutrino_msg_t msg); // P2
		void markItem(CListFrame *list);
		void scrollBrowserItem(bool next, bool page);
		void onSetFocus(MB_FOCUS new_focus); // P2
		void onSetFocusNext(void); // P2
		void onSetFocusPrev(void); // P2
		void onSetGUIWindow(MB_GUI gui);
		void onSetGUIWindowNext(void);
		void onSetGUIWindowPrev(void);
		bool onDelete(bool cursor_only = false);
		std::string formatDeleteMsg(MI_MOVIE_INFO *movieinfo, int msgFont, const int boxWidth = 450);
		bool onDeleteFile(MI_MOVIE_INFO *movieinfo, bool skipAsk = false);  // P4
		bool onSortMovieInfoHandleList(std::vector<MI_MOVIE_INFO*>& pv_handle_list, MB_INFO_ITEM sort_type, MB_DIRECTION direction);

		///// parse Storage Directories /////////////
		bool addDir(std::string& dirname, int* used);
		void updateDir(void);
		void loadAllTsFileNamesFromStorage(void); // P1
		bool loadTsFileNamesFromDir(const std::string & dirname); // P1
		void getStorageInfo(void); // P3

		///// Menu ////////////////////////////////////
		bool showMenu(bool calledExternally = false);
		int showMovieInfoMenu(MI_MOVIE_INFO* movie_info); // P2
		int showMovieCutMenu(); // P2
		int  showStartPosSelectionMenu(void); // P2

		///// settings ///////////////////////////////////
		bool loadSettings(MB_SETTINGS* settings); // P2
		bool saveSettings(MB_SETTINGS* settings); // P2
		void defaultSettings(MB_SETTINGS* settings);

		///// EPG_DATA /XML ///////////////////////////////
		void loadMovies(bool doRefresh = true);
		void loadAllMovieInfo(void); // P1
		void saveMovieInfo(std::string* filename, MI_MOVIE_INFO* movie_info); // P2

		// misc
		void showHelp(void);
		bool isFiltered(MI_MOVIE_INFO& movie_info);
		bool isParentalLock(MI_MOVIE_INFO& movie_info);
		bool getMovieInfoItem(MI_MOVIE_INFO& movie_info, MB_INFO_ITEM item, std::string* item_string);
		void updateMovieSelection(void);
		void updateInfoSelection(void);
		void updateFilterSelection(void);
		void updateSerienames(void);
		void autoFindSerie(void);

		void info_hdd_level(bool paint_hdd=false);

		neutrino_locale_t getFeedLocale(void);
		void clearListLines();
		void clearSelection();
		bool supportedExtension(CFile &file);
		bool addFile(CFile &file, int dirItNr);
};

// I tried a lot to use the menu.cpp as ListBox selection, and I got three solution which are all garbage.
//Might be replaced by somebody who is familiar with this stuff .

// CLass to verifiy a menu was selected by the user. There might be better ways to do so.
class CSelectedMenu : public CMenuTarget
{
	public:
		bool selected;
		CSelectedMenu(void){selected = false;};
		inline	int exec(CMenuTarget* /*parent*/, const std::string & /*actionKey*/){selected = true; return menu_return::RETURN_EXIT;};
};

// This Class creates a menue item, which writes its caption to an given string (or an given int value to an given variable).
// The programm could use this class to verify, what menu was selected.
// A good listbox class might do the same. There might be better ways to do so.
#define BUFFER_MAX 20
class CMenuSelector : public CMenuItem
{
	private:
		const char * optionName;
		char * optionValue;
		std::string* optionValueString;
		int  returnIntValue;
		int* returnInt;
		int height;
		char buffer[BUFFER_MAX];
	public:
		CMenuSelector(const char * OptionName, const bool Active = true, char * OptionValue = NULL, int* ReturnInt = NULL,int ReturnIntValue = 0);
		CMenuSelector(const char * OptionName, const bool Active , std::string & OptionValue, int* ReturnInt = NULL,int ReturnIntValue = 0);
		int exec(CMenuTarget* parent);
		int paint(bool selected);
		int getHeight(void) const{return height;};
		bool isSelectable(void) const {	return active;}
};

// CLass to get the menu line selected by the user. There might be better ways to do so.
class CMenuWidgetSelection : public CMenuWidget
{
	public:
		CMenuWidgetSelection(const neutrino_locale_t Name, const std::string & Icon = "", const int mwidth = 30) : CMenuWidget(Name, Icon, mwidth){;};
		int getSelectedLine(void){return no_action ? -1 : selected;};
};

class CFileChooser : public CMenuWidget
{
	private:
		std::string* dirPath;

	public:
		CFileChooser(std::string* path){dirPath= path;};
		int exec(CMenuTarget* parent, const std::string & actionKey);
};

class CDirMenu : public CMenuWidget
{
	private:
		std::vector<MB_DIR>* dirList;
		DIR_STATE dirState[MB_MAX_DIRS];
		std::string dirOptionText[MB_MAX_DIRS];
		int dirNfsMountNr[MB_MAX_DIRS];
		bool changed;

		void updateDirState(void);

	public:
		CDirMenu(std::vector<MB_DIR>* dir_list);
		int exec(CMenuTarget* parent, const std::string & actionKey);
		int show(void);
		bool isChanged(){return changed;};
};

#endif /*MOVIEBROWSER_H_*/
