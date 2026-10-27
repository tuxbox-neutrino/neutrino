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

	Module Name: mb_menu.cpp

	Description: Menus and related helpers for moviebrowser

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
#include "mb_help.h"

#include <global.h>

#include <driver/fontrenderer.h>
#include <driver/display.h>
#include <gui/filebrowser.h>
#include <gui/nfs.h>
#include <gui/widget/icons.h>
#include <gui/widget/menue_options.h>
#include <gui/widget/msgbox.h>
#include <system/helpers.h>

#include <stdio.h>
#include <sys/vfs.h>

#ifdef ENABLE_LCD4LINUX
#include "driver/lcd4l.h"
#endif

static std::string rateFormat(int i)
{
	return to_string(i/10) + "," + to_string(i%10);
}

void CMovieBrowser::showHelp(void)
{
	CMovieHelp help;
	help.exec();
}

#define MAX_STRING 40 // for keybord input

void CMovieBrowser::initBookMarkMenu(CMenuWidget *BookmarkMenu, MI_MOVIE_INFO* movie_info, CIntInput* BookStartIntInput, CIntInput* BookLastIntInput, CIntInput* BookEndIntInput)
{
	BookmarkMenu->addIntroItems(LOCALE_MOVIEBROWSER_BOOK_HEAD);
	BookmarkMenu->addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BOOK_CLEAR_ALL, true, NULL, this, "book_clear_all", CRCInput::RC_red));
	BookmarkMenu->addItem(GenericMenuSeparatorLine);
	BookmarkMenu->addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BOOK_MOVIESTART,    true, BookStartIntInput->getValue(), BookStartIntInput));
	BookmarkMenu->addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BOOK_MOVIEEND,      true, BookEndIntInput->getValue(),   BookEndIntInput));
	BookmarkMenu->addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BOOK_LASTMOVIESTOP, true, BookLastIntInput->getValue(),  BookLastIntInput));
	BookmarkMenu->addItem(GenericMenuSeparatorLine);

	for (int li =0 ; li < MI_MOVIE_BOOK_USER_MAX && li < MAX_NUMBER_OF_BOOKMARK_ITEMS; li++)
	{
		if (movie_info->bookmarks.user[li].name.empty())
			movie_info->bookmarks.user[li].name = g_Locale->getText(LOCALE_MOVIEBROWSER_BOOK_NEW);

		CKeyboardInput *pBookNameInput = new CKeyboardInput(LOCALE_MOVIEBROWSER_EDIT_BOOK,   &movie_info->bookmarks.user[li].name, MAX_STRING, NULL, NULL, LOCALE_MOVIEBROWSER_EDIT_BOOK_NAME_INFO1, LOCALE_MOVIEBROWSER_EDIT_BOOK_NAME_INFO2);
		CIntInput *pBookPosIntInput    = new CIntInput(LOCALE_MOVIEBROWSER_EDIT_BOOK, (int *)&movie_info->bookmarks.user[li].pos,     5, LOCALE_MOVIEBROWSER_EDIT_BOOK_POS_INFO1,  LOCALE_MOVIEBROWSER_EDIT_BOOK_POS_INFO2);
		CIntInput *pBookTypeIntInput   = new CIntInput(LOCALE_MOVIEBROWSER_EDIT_BOOK, (int *)&movie_info->bookmarks.user[li].length, 20, LOCALE_MOVIEBROWSER_EDIT_BOOK_TYPE_INFO1, LOCALE_MOVIEBROWSER_EDIT_BOOK_TYPE_INFO2);
		/* a negative length jumps back, see the hint */
		pBookTypeIntInput->allowNegative();

		CMenuWidget* pBookItemMenu = new CMenuWidget(LOCALE_MOVIEBROWSER_BOOK_HEAD, NEUTRINO_ICON_MOVIEPLAYER);
		pBookItemMenu->addItem(GenericMenuSeparator);
		pBookItemMenu->addItem(new CMenuDForwarder(LOCALE_MOVIEBROWSER_BOOK_NAME,     true,  movie_info->bookmarks.user[li].name,    pBookNameInput));
		pBookItemMenu->addItem(new CMenuDForwarder(LOCALE_MOVIEBROWSER_BOOK_POSITION, true,  pBookPosIntInput->getValue(),  pBookPosIntInput));
		pBookItemMenu->addItem(new CMenuDForwarder(LOCALE_MOVIEBROWSER_BOOK_TYPE,     true,  pBookTypeIntInput->getValue(), pBookTypeIntInput));

		BookmarkMenu->addItem(new CMenuDForwarder("", true, pBookNameInput->getValue(), pBookItemMenu));
	}
}

void CMovieBrowser::initSeriesMenu(CMenuWidget *SeriesMenu, MI_MOVIE_INFO* movie_info, CKeyboardInput *SerieUserInput)
{
	SeriesMenu->addIntroItems(LOCALE_MOVIEBROWSER_SERIE_HEAD);

	SeriesMenu->addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_SERIE_NAME, true, movie_info->serieName, SerieUserInput));

	SeriesMenu->addItem(GenericMenuSeparatorLine);
	for (unsigned int li = 0; li < m_vHandleSerienames.size(); li++)
		SeriesMenu->addItem(new CMenuSelector(m_vHandleSerienames[li]->serieName.c_str(), true, movie_info->serieName));
}

void CMovieBrowser::initUpdateMovieInfoMenu(CMenuWidget *MovieInfoMenuUpdate)
{
	for (unsigned int i = 0; i < MB_INFO_MAX_NUMBER; i++)
		movieInfoUpdateAll[i] = 0;
	movieInfoUpdateAllIfDestEmptyOnly = true;

	MovieInfoMenuUpdate->addIntroItems(LOCALE_MOVIEBROWSER_INFO_HEAD_UPDATE);

	MovieInfoMenuUpdate->addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_MENU_SAVE_ALL, true, NULL, this, "save_movie_info_all", CRCInput::RC_red));

	MovieInfoMenuUpdate->addItem(GenericMenuSeparatorLine);

	MovieInfoMenuUpdate->addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_UPDATE_IF_DEST_EMPTY_ONLY, (&movieInfoUpdateAllIfDestEmptyOnly), MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true, NULL, CRCInput::RC_blue));

	MovieInfoMenuUpdate->addItem(GenericMenuSeparatorLine);

	MovieInfoMenuUpdate->addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_INFO_TITLE,             &movieInfoUpdateAll[MB_INFO_TITLE], MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true, NULL, CRCInput::RC_1));
	MovieInfoMenuUpdate->addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_INFO_SERIE,             &movieInfoUpdateAll[MB_INFO_SERIE], MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true, NULL, CRCInput::RC_2));
	MovieInfoMenuUpdate->addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_INFO_INFO1,             &movieInfoUpdateAll[MB_INFO_INFO1], MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true, NULL, CRCInput::RC_3));
	MovieInfoMenuUpdate->addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_INFO_GENRE_MAJOR,       &movieInfoUpdateAll[MB_INFO_MAJOR_GENRE], MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true, NULL, CRCInput::RC_4));
	MovieInfoMenuUpdate->addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_INFO_PARENTAL_LOCKAGE,  &movieInfoUpdateAll[MB_INFO_PARENTAL_LOCKAGE], MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true, NULL, CRCInput::RC_5));
	MovieInfoMenuUpdate->addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_INFO_PRODYEAR,          &movieInfoUpdateAll[MB_INFO_PRODDATE], MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true, NULL, CRCInput::RC_6));
	MovieInfoMenuUpdate->addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_INFO_PRODCOUNTRY,       &movieInfoUpdateAll[MB_INFO_COUNTRY], MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true, NULL, CRCInput::RC_7));
	MovieInfoMenuUpdate->addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_INFO_LENGTH,            &movieInfoUpdateAll[MB_INFO_LENGTH], MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true, NULL, CRCInput::RC_8));
	MovieInfoMenuUpdate->addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_INFO_RATING,            &movieInfoUpdateAll[MB_INFO_RATING], MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true, NULL, CRCInput::RC_9));
	MovieInfoMenuUpdate->addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_INFO_QUALITY,           &movieInfoUpdateAll[MB_INFO_QUALITY], MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true, NULL, CRCInput::RC_0));
}

int CMovieBrowser::showMovieInfoMenu(MI_MOVIE_INFO* movie_info)
{
	// init bookmark menu
	CMenuWidget bookmarkMenu(LOCALE_MOVIEBROWSER_HEAD, NEUTRINO_ICON_MOVIEPLAYER);
	CIntInput bookStartIntInput(LOCALE_MOVIEBROWSER_EDIT_BOOK, (int *)&movie_info->bookmarks.start,        5, LOCALE_MOVIEBROWSER_EDIT_BOOK_POS_INFO1, LOCALE_MOVIEBROWSER_EDIT_BOOK_POS_INFO2);
	CIntInput bookLastIntInput(LOCALE_MOVIEBROWSER_EDIT_BOOK,  (int *)&movie_info->bookmarks.lastPlayStop, 5, LOCALE_MOVIEBROWSER_EDIT_BOOK_POS_INFO1, LOCALE_MOVIEBROWSER_EDIT_BOOK_POS_INFO2);
	CIntInput bookEndIntInput(LOCALE_MOVIEBROWSER_EDIT_BOOK,   (int *)&movie_info->bookmarks.end,          5, LOCALE_MOVIEBROWSER_EDIT_BOOK_POS_INFO1, LOCALE_MOVIEBROWSER_EDIT_BOOK_POS_INFO2);
	initBookMarkMenu(&bookmarkMenu, movie_info, &bookStartIntInput, &bookLastIntInput, &bookEndIntInput);

	// init series menu
	CMenuWidget serieMenu(LOCALE_MOVIEBROWSER_HEAD, NEUTRINO_ICON_MOVIEPLAYER);
	CKeyboardInput serieUserInput(LOCALE_MOVIEBROWSER_EDIT_SERIE, &movie_info->serieName, MAX_STRING); //TODO: hints
	initSeriesMenu(&serieMenu, movie_info, &serieUserInput);

	// init update movie info menu
	CMenuWidget movieInfoMenuUpdate(LOCALE_MOVIEBROWSER_HEAD, NEUTRINO_ICON_MOVIEPLAYER);
	initUpdateMovieInfoMenu(&movieInfoMenuUpdate);

	/********************************************************************/
	/**  movieInfo  ******************************************************/
#define BUFFER_SIZE 100
	char dirItNr[BUFFER_SIZE];
	char size[BUFFER_SIZE];

	strncpy(dirItNr, m_dirNames[movie_info->dirItNr].c_str(),BUFFER_SIZE-1);
	snprintf(size,BUFFER_SIZE,"%5" PRIu64 "",movie_info->file.Size>>20);

	CKeyboardInput titelUserInput(LOCALE_MOVIEBROWSER_INFO_TITLE,          &movie_info->epgTitle, (movie_info->epgTitle.empty() || (movie_info->epgTitle.size() < MAX_STRING)) ? MAX_STRING : movie_info->epgTitle.size());
	CKeyboardInput channelUserInput(LOCALE_MOVIEBROWSER_INFO_CHANNEL,      &movie_info->channelName, MAX_STRING);
	CKeyboardInput epgUserInput(LOCALE_MOVIEBROWSER_INFO_INFO1,            &movie_info->epgInfo1, MAX_STRING);
	CKeyboardInput countryUserInput(LOCALE_MOVIEBROWSER_INFO_PRODCOUNTRY,  &movie_info->productionCountry, MAX_STRING);

	std::string neverplayed = "---";
	CDateInput     dateUserDateInput(LOCALE_MOVIEBROWSER_INFO_LENGTH,      &movie_info->dateOfLastPlay);
	CDateInput     recUserDateInput(LOCALE_MOVIEBROWSER_INFO_LENGTH,       &movie_info->file.Time);
	CIntInput      lengthUserIntInput(LOCALE_MOVIEBROWSER_INFO_LENGTH,     (int *)&movie_info->length, 3);
	CIntInput      yearUserIntInput(LOCALE_MOVIEBROWSER_INFO_PRODYEAR,     (int *)&movie_info->productionDate, 4);

	CMenuOptionNumberChooser *rate = new CMenuOptionNumberChooser(LOCALE_MOVIEBROWSER_INFO_RATING, &movie_info->rating, true, 0, 100, NULL);
	rate->setNumberFormat(rateFormat);

	CMenuWidget movieInfoMenu(LOCALE_MOVIEBROWSER_HEAD, NEUTRINO_ICON_MOVIEPLAYER);

	movieInfoMenu.addIntroItems(LOCALE_MOVIEBROWSER_INFO_HEAD);
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_MENU_SAVE,           true, NULL, this,                    "save_movie_info",                  CRCInput::RC_red));
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_HEAD_UPDATE,    true, NULL,                          &movieInfoMenuUpdate, NULL,         CRCInput::RC_green));
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_MENU_DATA_REQUEST,   true, NULL, this,                    "get_db_data",                    CRCInput::RC_yellow));
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BOOK_HEAD,           true, NULL,                          &bookmarkMenu, NULL,                CRCInput::RC_blue));
	movieInfoMenu.addItem(GenericMenuSeparatorLine);
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_TITLE,          true, movie_info->epgTitle,          &titelUserInput, NULL,              CRCInput::RC_1));
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_SERIE,          true, movie_info->serieName,         &serieMenu, NULL,                   CRCInput::RC_2));
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_INFO1,          (movie_info->epgInfo1.size() <= MAX_STRING), movie_info->epgInfo1, &epgUserInput, NULL, CRCInput::RC_3));
	movieInfoMenu.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_INFO_GENRE_MAJOR,      &movie_info->genreMajor, GENRE_ALL, GENRE_ALL_COUNT, true, NULL,   CRCInput::RC_4, "", true));
	movieInfoMenu.addItem(GenericMenuSeparatorLine);
	movieInfoMenu.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_INFO_PARENTAL_LOCKAGE, &movie_info->parentalLockAge, MESSAGEBOX_PARENTAL_LOCKAGE_OPTIONS, MESSAGEBOX_PARENTAL_LOCKAGE_OPTION_COUNT, true, NULL, CRCInput::RC_5));
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_PRODYEAR,       true, yearUserIntInput.getValue(),   &yearUserIntInput, NULL,            CRCInput::RC_6));
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_PRODCOUNTRY,    true, movie_info->productionCountry, &countryUserInput, NULL,            CRCInput::RC_7));
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_LENGTH,         true, lengthUserIntInput.getValue(), &lengthUserIntInput, NULL,          CRCInput::RC_8));
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_CHANNEL,        true, movie_info->channelName,       &channelUserInput, NULL,            CRCInput::RC_9));
	movieInfoMenu.addItem(GenericMenuSeparatorLine);
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_PATH,           false, dirItNr));
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_RECORDDATE,     false, recUserDateInput.getValue()));
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_PREVPLAYDATE,   false, (movie_info->dateOfLastPlay == 0 || movie_info->bookmarks.lastPlayStop/60 == 0) ? neverplayed : dateUserDateInput.getValue()));
	std::string last_stop = to_string(movie_info->bookmarks.lastPlayStop/60) + "/" + to_string(movie_info->length);
	last_stop += " min";
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_ELAPSED,        false, last_stop));
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_SIZE,           false, size));
	movieInfoMenu.addItem(GenericMenuSeparatorLine);
	movieInfoMenu.addItem(rate);
	movieInfoMenu.addItem(new CMenuOptionNumberChooser(LOCALE_MOVIEBROWSER_INFO_QUALITY, &movie_info->quality, true, 0, 3, NULL));

	int res = movieInfoMenu.exec(NULL,"");

	return res;
}


int CMovieBrowser::showMovieCutMenu()
{
	CMenuWidget movieCutMenu(LOCALE_MOVIEBROWSER_HEAD, NEUTRINO_ICON_MOVIEPLAYER);
	movieCutMenu.addIntroItems(LOCALE_MOVIEBROWSER_MENU_CUT_HEAD);
	CMenuForwarder *mf;

	mf = new CMenuForwarder(m_movieSelectionHandler->epgTitle, false);
	mf->setHint(NEUTRINO_ICON_HINT_MOVIE, NONEXISTANT_LOCALE);
	movieCutMenu.addItem(mf);
	movieCutMenu.addItem(GenericMenuSeparator);

	mf = new CMenuForwarder(LOCALE_MOVIEBROWSER_MENU_COPY_ONEFILE, true, NULL, this, "copy_onefile", CRCInput::RC_red);
	mf->setHint(NEUTRINO_ICON_HINT_MOVIE, LOCALE_MOVIEBROWSER_HINT_COPY_ONEFILE);
	movieCutMenu.addItem(mf);

	mf = new CMenuForwarder(LOCALE_MOVIEBROWSER_MENU_COPY_SEVERAL, true, NULL, this, "copy_several", CRCInput::RC_green);
	mf->setHint(NEUTRINO_ICON_HINT_MOVIE, LOCALE_MOVIEBROWSER_HINT_COPY_SEVERAL);
	movieCutMenu.addItem(mf);

	mf = new CMenuForwarder(LOCALE_MOVIEBROWSER_MENU_CUT, true, NULL, this, "cut", CRCInput::RC_yellow);
	mf->setHint(NEUTRINO_ICON_HINT_MOVIE, LOCALE_MOVIEBROWSER_HINT_CUT);
	movieCutMenu.addItem(mf);

	mf = new CMenuForwarder(LOCALE_MOVIEBROWSER_MENU_TRUNCATE, true, NULL, this, "truncate", CRCInput::RC_blue);
	mf->setHint(NEUTRINO_ICON_HINT_MOVIE, LOCALE_MOVIEBROWSER_HINT_TRUNCATE);
	movieCutMenu.addItem(mf);

	int res = movieCutMenu.exec(NULL,"");
	return res;
}

void CMovieBrowser::changeBrowserHeight(CMenuForwarder* fw1, CMenuForwarder* fw2)
{
	if (m_settings.browserAdditional)
	{
		fw1->setActive(false);
		fw2->setActive(true);
	}
	else
	{
		fw1->setActive(true);
		fw2->setActive(false);
	}

	fw1->paint();
	fw2->paint();
}

void CMovieBrowser::initParentalMenu(CMenuWidget *ParentalMenu)
{
	ParentalMenu->addIntroItems(LOCALE_MOVIEBROWSER_MENU_PARENTAL_LOCK_HEAD);
	ParentalMenu->addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_MENU_PARENTAL_LOCK_ACTIVATED, (int*)(&m_parentalLock), MESSAGEBOX_PARENTAL_LOCK_OPTIONS, MESSAGEBOX_PARENTAL_LOCK_OPTIONS_COUNT, true));
	ParentalMenu->addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_MENU_PARENTAL_LOCK_RATE_HEAD, (int*)(&m_settings.parentalLockAge), MESSAGEBOX_PARENTAL_LOCKAGE_OPTIONS, MESSAGEBOX_PARENTAL_LOCKAGE_OPTION_COUNT, true));
}

void CMovieBrowser::initOptionsDirMenu(CMenuWidget *OptionsMenuDir, std::vector<COnOffNotifier*>& v_notifiers)
{
	OptionsMenuDir->addIntroItems(LOCALE_MOVIEBROWSER_MENU_DIRECTORIES_HEAD);
	OptionsMenuDir->addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_USE_REC_DIR,       (int*)(&m_settings.store.storageDirRecUsed), MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true));
	OptionsMenuDir->addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_DIR, false, g_settings.network_nfs_recordingdir));

	OptionsMenuDir->addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_USE_MOVIE_DIR,     (int*)(&m_settings.store.storageDirMovieUsed), MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true));
	OptionsMenuDir->addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_DIR, false, g_settings.network_nfs_moviedir));
	OptionsMenuDir->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MOVIEBROWSER_DIRECTORIES_ADDITIONAL));

	for (int i = 0; i < MB_MAX_DIRS ; i++)
	{
		CFileChooser *dirInput =  new CFileChooser(&m_settings.store.storageDir[i]);
		CMenuForwarder *forwarder = new CMenuDForwarder(LOCALE_MOVIEBROWSER_DIR,        m_settings.store.storageDirUsed[i], m_settings.store.storageDir[i],      dirInput);

		v_notifiers.push_back(new COnOffNotifier());
		v_notifiers[i]->addItem(forwarder);
		CMenuOptionChooser *chooser =   new CMenuOptionChooser(LOCALE_MOVIEBROWSER_USE_DIR, &m_settings.store.storageDirUsed[i], MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true, v_notifiers[i]);
		OptionsMenuDir->addItem(chooser);

		OptionsMenuDir->addItem(forwarder);

		if (i != (MB_MAX_DIRS - 1))
			OptionsMenuDir->addItem(GenericMenuSeparator);
	}
}

void CMovieBrowser::initOptionsBrowserMenu(CMenuWidget *OptionsMenuBrowser, CIntInput* PlayMaxUserIntInput, CIntInput* RecMaxUserIntInput, CIntInput* BrowserFrameUserIntInputAdd, CIntInput* BrowserFrameUserIntInputGen, CIntInput* BrowserRowNrIntInput)
{
	OptionsMenuBrowser->addIntroItems(LOCALE_MOVIEBROWSER_OPTION_BROWSER);
	OptionsMenuBrowser->addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_LAST_PLAY_MAX_ITEMS,    true, PlayMaxUserIntInput->getValue(),  PlayMaxUserIntInput));
	OptionsMenuBrowser->addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_LAST_RECORD_MAX_ITEMS,  true, RecMaxUserIntInput->getValue(), RecMaxUserIntInput));
	OptionsMenuBrowser->addItem(GenericMenuSeparatorLine);

	CMenuForwarder* fw1 = new CMenuForwarder(LOCALE_MOVIEBROWSER_BROWSER_FRAME_HIGH,     !m_settings.browserAdditional, BrowserFrameUserIntInputGen->getValue(), BrowserFrameUserIntInputGen);
	CMenuForwarder* fw2 = new CMenuForwarder(LOCALE_MOVIEBROWSER_BROWSER_FRAME_HIGH_ADDITIONAL,     m_settings.browserAdditional, BrowserFrameUserIntInputAdd->getValue(), BrowserFrameUserIntInputAdd);

	CMenuOptionChooser *oj = new CMenuOptionChooser(LOCALE_MOVIEBROWSER_BROWSER_ADDITIONAL, (int*)(&m_settings.browserAdditional), MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true);
	oj->OnAfterChangeOption.connect(sigc::bind(sigc::mem_fun(*this, &CMovieBrowser::changeBrowserHeight), fw1, fw2));

	OptionsMenuBrowser->addItem(oj);
	OptionsMenuBrowser->addItem(fw1);
	OptionsMenuBrowser->addItem(fw2);

	OptionsMenuBrowser->addItem(GenericMenuSeparatorLine);
	OptionsMenuBrowser->addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_BROWSER_CUT_LONG_ROWTEXT, (int*)(&m_settings.browserCutLongRowText), MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true));
	OptionsMenuBrowser->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MOVIEBROWSER_BROWSER_ROW_HEAD));
	OptionsMenuBrowser->addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BROWSER_ROW_NR,     true, BrowserRowNrIntInput->getValue(), BrowserRowNrIntInput));
	OptionsMenuBrowser->addItem(GenericMenuSeparator);
	for (int i = 0; i < MB_MAX_ROWS; i++)
	{
		CIntInput* browserRowWidthIntInput = new CIntInput(LOCALE_MOVIEBROWSER_BROWSER_ROW_WIDTH,(int *)&m_settings.browserRowWidth[i], 3, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);
		OptionsMenuBrowser->addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_BROWSER_ROW_ITEM, (int*)(&m_settings.browserRowItem[i]), MESSAGEBOX_BROWSER_ROW_ITEM, MESSAGEBOX_BROWSER_ROW_ITEM_COUNT, true, NULL, CRCInput::convertDigitToKey(i+1), NULL, true, true));
		OptionsMenuBrowser->addItem(new CMenuDForwarder(LOCALE_MOVIEBROWSER_BROWSER_ROW_WIDTH,    true, browserRowWidthIntInput->getValue(),      browserRowWidthIntInput));
		if (i < MB_MAX_ROWS-1)
			OptionsMenuBrowser->addItem(GenericMenuSeparator);
	}
}

bool CMovieBrowser::showMenu(bool calledExternally)
{
	if (m_header && m_header->getClockObject())
		m_header->disableClock();

	/* first clear screen */
	framebuffer->paintBackground();

	//  directory menu
	CDirMenu dirMenu(&m_dir);

	// init parental lock menu
	CMenuWidget parentalMenu(LOCALE_MOVIEBROWSER_HEAD, NEUTRINO_ICON_MOVIEPLAYER);
	initParentalMenu(&parentalMenu);

	// init options menu for directories
	CMenuWidget optionsMenuDir(LOCALE_MOVIEBROWSER_HEAD, NEUTRINO_ICON_MOVIEPLAYER);
	std::vector<COnOffNotifier*> notifiers;
	initOptionsDirMenu(&optionsMenuDir, notifiers);

	// init options menu for browser
	int oldRowNr 		= m_settings.browserRowNr;
	int oldFrameHeight 	= m_settings.browserFrameHeight;
	int oldAdditional 	= m_settings.browserAdditional;
	CMenuWidget optionsMenuBrowser(LOCALE_MOVIEBROWSER_HEAD, NEUTRINO_ICON_MOVIEPLAYER);
	CIntInput playMaxUserIntInput(LOCALE_MOVIEBROWSER_LAST_PLAY_MAX_ITEMS,      (int *)&m_settings.lastPlayMaxItems,    3, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);
	CIntInput recMaxUserIntInput(LOCALE_MOVIEBROWSER_LAST_RECORD_MAX_ITEMS,     (int *)&m_settings.lastRecordMaxItems,  3, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);

	CIntInput browserFrameUserIntInputAdd(LOCALE_MOVIEBROWSER_BROWSER_FRAME_HIGH,  (int *)&m_settings.browserFrameHeightAdditional,  3, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);
	CIntInput browserFrameUserIntInputGen(LOCALE_MOVIEBROWSER_BROWSER_FRAME_HIGH,  (int *)&m_settings.browserFrameHeightGeneral,  3, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);

	CIntInput browserRowNrIntInput(LOCALE_MOVIEBROWSER_BROWSER_ROW_NR,          (int *)&m_settings.browserRowNr,        1, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);
	initOptionsBrowserMenu(&optionsMenuBrowser, &playMaxUserIntInput, &recMaxUserIntInput, &browserFrameUserIntInputAdd, &browserFrameUserIntInputGen, &browserRowNrIntInput);

	/********************************************************************/
	/**  options  **************************************************/

	CMenuWidget optionsMenu(LOCALE_MOVIEBROWSER_HEAD, NEUTRINO_ICON_MOVIEPLAYER);
	optionsMenu.addIntroItems(LOCALE_MOVIEBROWSER_MENU_SETTINGS);
	optionsMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_LOAD_DEFAULT, true, NULL, this, "loaddefault",              CRCInput::RC_red));
	optionsMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_OPTION_BROWSER, true, NULL, &optionsMenuBrowser,NULL,       CRCInput::RC_green));
	optionsMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_DIRECTORIES, true, NULL, &optionsMenuDir,NULL,    CRCInput::RC_yellow));
	if (m_parentalLock != MB_PARENTAL_LOCK_OFF)
		optionsMenu.addItem(new CLockedMenuForwarder(LOCALE_MOVIEBROWSER_MENU_PARENTAL_LOCK_HEAD, g_settings.parentallock_pincode, true,  true, NULL, &parentalMenu,NULL,CRCInput::RC_blue));
	else
		optionsMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_MENU_PARENTAL_LOCK_HEAD,   true, NULL, &parentalMenu,NULL,CRCInput::RC_blue));
	optionsMenu.addItem(GenericMenuSeparatorLine);
	optionsMenu.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_RELOAD_AT_START,   (int*)(&m_settings.reload), MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true));
	optionsMenu.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_REMOUNT_AT_START,  (int*)(&m_settings.remount), MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true));
	optionsMenu.addItem(GenericMenuSeparatorLine);
	optionsMenu.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_HIDE_SERIES,       (int*)(&m_settings.browser_serie_mode), MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true));
	optionsMenu.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_SERIE_AUTO_CREATE, (int*)(&m_settings.serie_auto_create), MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true));
	int ts_only = m_settings.ts_only;
	optionsMenu.addItem( new CMenuOptionChooser(LOCALE_MOVIEBROWSER_TS_ONLY,           (int*)(&m_settings.ts_only), MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true ));

	//optionsMenu.addItem(GenericMenuSeparator);

	/********************************************************************/
	/**  main menu ******************************************************/
	CNFSSmallMenu* nfs =    new CNFSSmallMenu();

	if (!calledExternally) {
		CMenuWidget mainMenu(LOCALE_MOVIEBROWSER_HEAD, NEUTRINO_ICON_MOVIEPLAYER);
		mainMenu.addIntroItems(LOCALE_MOVIEBROWSER_MENU_MAIN_HEAD);
		std::string title = g_Locale->getText(LOCALE_MOVIEBROWSER_MENU_EDIT);
		if (m_movieSelectionHandler)
		{
			if (!m_movieSelectionHandler->epgTitle.empty())
				title = m_movieSelectionHandler->epgTitle;
			const size_t max_size = 32;
			if (title.size() > max_size)
				title = title.substr (0, max_size) + " ...";
		}
		mainMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_HEAD,     (m_movieSelectionHandler != NULL), title, this, "show_movie_info_menu",    CRCInput::RC_red));
		mainMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_MENU_CUT_HEAD, (m_movieSelectionHandler != NULL), NULL , this, "show_movie_cut_menu",     CRCInput::RC_green));
		mainMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_DELETE_MOVIE,  (m_movieSelectionHandler != NULL), NULL , this, "delete_movie",            CRCInput::RC_yellow));
		mainMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_SCAN_FOR_MOVIES,                            true, NULL , this, "reload_movie_info",       CRCInput::RC_blue));
		mainMenu.addItem(GenericMenuSeparatorLine);
		mainMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_MENU_SETTINGS,                              true, NULL , &optionsMenu,NULL,               CRCInput::RC_1));
		mainMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_MENU_DIRECTORIES_HEAD,                      true, NULL , &dirMenu,    NULL,               CRCInput::RC_2));
// 		mainMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_SCAN_FOR_MOVIES,                            true, NULL , this,        "reload_movie_info",CRCInput::RC_3));
// 		mainMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_MENU_NFS_HEAD,                              true, NULL , nfs,         NULL,               CRCInput::RC_setup));

		mainMenu.exec(NULL, " ");
	} else
		optionsMenu.exec(NULL, "");

	// post menu handling
	if (m_parentalLock != MB_PARENTAL_LOCK_OFF_TMP)
		m_settings.parentalLock = m_parentalLock;

	m_settings.browserFrameHeight = m_settings.browserAdditional ? m_settings.browserFrameHeightAdditional : m_settings.browserFrameHeightGeneral;
	if (m_settings.browserFrameHeight < MIN_BROWSER_FRAME_HEIGHT)
		m_settings.browserFrameHeight = MIN_BROWSER_FRAME_HEIGHT;
	if (m_settings.browserFrameHeight > MAX_BROWSER_FRAME_HEIGHT)
		m_settings.browserFrameHeight = MAX_BROWSER_FRAME_HEIGHT;

	if (m_settings.browserRowNr > MB_MAX_ROWS)
		m_settings.browserRowNr = MB_MAX_ROWS;
	if (m_settings.browserRowNr < 1)
		m_settings.browserRowNr = 1;
	for (int i = 0; i < m_settings.browserRowNr; i++)
	{
		if (m_settings.browserRowWidth[i] > 100)
			m_settings.browserRowWidth[i] = 100;
		if (m_settings.browserRowWidth[i] < 1)
			m_settings.browserRowWidth[i] = 1;
	}

	if (!calledExternally) {
		if (ts_only != m_settings.ts_only || dirMenu.isChanged())
			loadMovies(false);

		bool reInitFrames = (
				   m_settings.browserRowNr       != oldRowNr
				|| m_settings.browserFrameHeight != oldFrameHeight
				|| m_settings.browserAdditional  != oldAdditional
		);

		if (reInitFrames) {
			initFrames();
			hide();
			paint();
		} else {
			updateSerienames();
			refreshBrowserList();
			refreshLastPlayList();
			refreshLastRecordList();
			refreshFilterList();
			refreshTitle();
			refreshMovieInfo();
			refreshFoot();
			refreshLCD();
		}
		/* FIXME: refreshXXXList -> setLines -> CListFrame::refresh, too */
		//refresh();
	} else
		saveSettings(&m_settings);

	for (size_t i = 0; i < notifiers.size(); i++)
		delete notifiers[i];

	delete nfs;

	if (m_header && m_header->getClockObject())
		m_header->enableClock();

	return(true);
}

int CMovieBrowser::showStartPosSelectionMenu(void) // P2
{
	const char *unit_short_minute = g_Locale->getText(LOCALE_UNIT_SHORT_MINUTE);
	int pos = -1;
	int result = 0;
	int menu_nr= 0;
	int position[MAX_NUMBER_OF_BOOKMARK_ITEMS] ={0};

	if (m_movieSelectionHandler == NULL) return(result);

	char start_pos[32]; snprintf(start_pos, sizeof(start_pos), "%3d %s",m_movieSelectionHandler->bookmarks.start/60, unit_short_minute);
	char play_pos[32]; 	snprintf(play_pos, sizeof(play_pos), "%3d %s",m_movieSelectionHandler->bookmarks.lastPlayStop/60, unit_short_minute);

	char book[MI_MOVIE_BOOK_USER_MAX][32];

	CMenuWidgetSelection startPosSelectionMenu(LOCALE_MOVIEBROWSER_HEAD, NEUTRINO_ICON_MOVIEPLAYER);
	startPosSelectionMenu.enableFade(false);

	startPosSelectionMenu.addIntroItems(LOCALE_MOVIEBROWSER_START_HEAD, NONEXISTANT_LOCALE, CMenuWidget::BTN_TYPE_CANCEL);

	int off = startPosSelectionMenu.getItemsCount();
	bool got_start_pos = false;

	if (m_movieSelectionHandler->bookmarks.start != 0)
	{
		got_start_pos = true;
		startPosSelectionMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BOOK_MOVIESTART, true, start_pos), true);
		position[menu_nr++] = m_movieSelectionHandler->bookmarks.start;
	}
	if (m_movieSelectionHandler->bookmarks.lastPlayStop != 0)
	{
		got_start_pos = true;
		startPosSelectionMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BOOK_LASTMOVIESTOP, true, play_pos), true);
		position[menu_nr++] = m_movieSelectionHandler->bookmarks.lastPlayStop;
	}

	startPosSelectionMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_START_RECORD_START, true,NULL), got_start_pos ? false : true);
	position[menu_nr++] = 0;

	for (int i = 0; i < MI_MOVIE_BOOK_USER_MAX && menu_nr < MAX_NUMBER_OF_BOOKMARK_ITEMS; i++)
	{
		if (m_movieSelectionHandler->bookmarks.user[i].pos != 0)
		{
			if (m_movieSelectionHandler->bookmarks.user[i].length >= 0)
				position[menu_nr] = m_movieSelectionHandler->bookmarks.user[i].pos;
			else
				position[menu_nr] = m_movieSelectionHandler->bookmarks.user[i].pos + m_movieSelectionHandler->bookmarks.user[i].length;

			snprintf(book[i], sizeof(book[i]),"%5d %s",position[menu_nr]/60, unit_short_minute);
			startPosSelectionMenu.addItem(new CMenuForwarder(m_movieSelectionHandler->bookmarks.user[i].name.c_str(), 	true, book[i]));
			menu_nr++;
		}
	}

	startPosSelectionMenu.exec(NULL, "12345");
	/* check what menu item was ok'd  and set the appropriate play offset*/
	result = startPosSelectionMenu.getSelectedLine();
	result -= off; // sub-text, separator, back, separator-line

	if (result >= 0 && result <= MAX_NUMBER_OF_BOOKMARK_ITEMS)
		pos = position[result];

	dprintf(DEBUG_DEBUG, "[mb] selected bookmark %d position %d \n", result, pos);

	return(pos) ;
}

CMenuSelector::CMenuSelector(const char * OptionName, const bool Active, char * OptionValue, int* ReturnInt,int ReturnIntValue) : CMenuItem()
{
	height     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	optionValueString = NULL;
	optionName = 		OptionName;
	optionValue = 		OptionValue;
	active = 			Active;
	returnIntValue =	ReturnIntValue;
	returnInt = 		ReturnInt;
}

CMenuSelector::CMenuSelector(const char * OptionName, const bool Active, std::string& OptionValue, int* ReturnInt,int ReturnIntValue) : CMenuItem()
{
	height     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	optionValueString = &OptionValue;
	optionName =        OptionName;
	strncpy(buffer,OptionValue.c_str(),BUFFER_MAX-1);
	buffer[BUFFER_MAX-1] = 0;// terminate string
	optionValue =       buffer;
	active =            Active;
	returnIntValue =    ReturnIntValue;
	returnInt =         ReturnInt;
}

int CMenuSelector::exec(CMenuTarget* /*parent*/)
{
	if (returnInt != NULL)
		*returnInt= returnIntValue;

	if (optionValue != NULL && optionName != NULL)
	{
		if (optionValueString == NULL)
			strcpy(optionValue,optionName);
		else
			*optionValueString = optionName;
	}
	return menu_return::RETURN_EXIT;
}

int CMenuSelector::paint(bool selected)
{
	CFrameBuffer * frameBuffer = CFrameBuffer::getInstance();

	fb_pixel_t    color   = COL_MENUCONTENT_TEXT;
	fb_pixel_t    bgcolor = COL_MENUCONTENT_PLUS_0;
	if (selected)
	{
		color   = COL_MENUCONTENTSELECTED_TEXT;
		bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
	}
	if (!active)
	{
		color   = COL_MENUCONTENTINACTIVE_TEXT;
		bgcolor = COL_MENUCONTENTINACTIVE_PLUS_0;
	}

	frameBuffer->paintBoxRel(x, y, dx, height, bgcolor);

	int stringstartposName = x + offx + 10;
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(stringstartposName,   y+height,dx- (stringstartposName - x), optionName, color);

	if (selected)
	{
#ifdef ENABLE_LCD4LINUX
		if (g_settings.lcd4l_support)
			CLCD4l::getInstance()->CreateEventFile(optionName, g_settings.lcd4l_convert);
#endif
		CVFD::getInstance()->showMenuText(0, optionName, -1, true); // UTF-8
	}

	return y+height;
}


/////////////////////////////////////////////////
// MenuTargets
////////////////////////////////////////////////
int CFileChooser::exec(CMenuTarget* parent, const std::string & /*actionKey*/)
{
	if (parent != NULL)
		parent->hide();

	CFileBrowser browser;
	browser.Dir_Mode=true;
	std::string oldPath = *dirPath;
	if (browser.exec(dirPath->c_str())) {
		*dirPath = browser.getSelectedFile()->Name;
		if (check_dir(dirPath->c_str(), true))
			*dirPath = oldPath;
	}

	return menu_return::RETURN_REPAINT;
}

CDirMenu::CDirMenu(std::vector<MB_DIR>* dir_list)
{
	unsigned int i;
	changed = false;
	dirList = dir_list;

	if (dirList->empty())
		return;

	for (i = 0; i < MB_MAX_DIRS; i++)
		dirNfsMountNr[i] = -1;

	for (i = 0; i < dirList->size() && i < MB_MAX_DIRS; i++)
	{
		for (int nfs = 0; nfs < NETWORK_NFS_NR_OF_ENTRIES; nfs++)
		{
			int result = -1;
			if (!g_settings.network_nfs[nfs].local_dir.empty())
				result = (*dirList)[i].name.compare(0,g_settings.network_nfs[nfs].local_dir.size(),g_settings.network_nfs[nfs].local_dir) ;
			dprintf(DEBUG_DEBUG, "[CDirMenu] (nfs%d) %s == (mb%d) %s (%d)\n",nfs,g_settings.network_nfs[nfs].local_dir.c_str(),i,(*dirList)[i].name.c_str(),result);

			if (result == 0)
			{
				dirNfsMountNr[i] = nfs;
				break;
			}
		}
	}
}

int CDirMenu::exec(CMenuTarget* parent, const std::string & actionKey)
{
	int returnval = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	if (actionKey.empty())
	{
		changed = false;
		return show();
	}
	else if (actionKey.size() == 1)
	{
		dprintf(DEBUG_DEBUG, "[CDirMenu].exec %s\n",actionKey.c_str());
		int number = atoi(actionKey.c_str());
		if (number < MB_MAX_DIRS)
		{
			if (dirState[number] == DIR_STATE_SERVER_DOWN)
			{
				dprintf(DEBUG_DEBUG, "[mb] try to start server: %s %s\n","ether-wake", g_settings.network_nfs[dirNfsMountNr[number]].mac.c_str());
				if (my_system(2, "ether-wake", g_settings.network_nfs[dirNfsMountNr[number]].mac.c_str()) != 0)
					perror("ether-wake failed");

				dirOptionText[number] = "STARTE SERVER";
			}
			else if (dirState[number] == DIR_STATE_NOT_MOUNTED)
			{
				dprintf(DEBUG_DEBUG, "[CDirMenu] try to mount %d,%d\n",number,dirNfsMountNr[number]);
				CFSMounter::MountRes res;
				res = CFSMounter::mount(g_settings.network_nfs[dirNfsMountNr[number]].ip,
						g_settings.network_nfs[dirNfsMountNr[number]].dir,
						g_settings.network_nfs[dirNfsMountNr[number]].local_dir,
						(CFSMounter::FSType)g_settings.network_nfs[dirNfsMountNr[number]].type,
						g_settings.network_nfs[dirNfsMountNr[number]].username,
						g_settings.network_nfs[dirNfsMountNr[number]].password,
						g_settings.network_nfs[dirNfsMountNr[number]].mount_options1,
						g_settings.network_nfs[dirNfsMountNr[number]].mount_options2);
				if (res == CFSMounter::MRES_OK) // if mount is successful we set the state to active in any case
					*(*dirList)[number].used = true;

				// try to mount
				updateDirState();
				changed = true;
			}
			else if (dirState[number] == DIR_STATE_MOUNTED)
			{
				if (*(*dirList)[number].used == true)
					*(*dirList)[number].used = false;
				else
					*(*dirList)[number].used = true;
				//CFSMounter::umount(g_settings.network_nfs_local_dir[dirNfsMountNr[number]]);
				updateDirState();
				changed = true;
			}
		}
	}
	return returnval;
}

extern int pinghost(const std::string &hostname, std::string *ip = NULL);
void CDirMenu::updateDirState(void)
{
	unsigned int drivefree = 0;
	struct statfs s;

	for (unsigned int i = 0; i < dirList->size() && i < MB_MAX_DIRS; i++)
	{
		dirOptionText[i] = "UNBEKANNT";
		dirState[i] = DIR_STATE_UNKNOWN;
		// 1st ping server
		dprintf(DEBUG_DEBUG, "[mb] updateDirState: %d: state %d nfs %d\n", i, dirState[i], dirNfsMountNr[i]);
		if (dirNfsMountNr[i] != -1)
		{
			int retvalue = pinghost(g_settings.network_nfs[dirNfsMountNr[i]].ip);
			if (retvalue == 0)//LOCALE_PING_UNREACHABLE
			{
				dirOptionText[i] = "Server, offline";
				dirState[i] = DIR_STATE_SERVER_DOWN;
			}
			else if (retvalue == 1)//LOCALE_PING_OK
			{
				if (!CFSMounter::isMounted(g_settings.network_nfs[dirNfsMountNr[i]].local_dir))
				{
					dirOptionText[i] = "Not mounted";
					dirState[i] = DIR_STATE_NOT_MOUNTED;
				}
				else
				{
					dirState[i] = DIR_STATE_MOUNTED;
				}
			}
		}
		else
		{
			// not a nfs dir, probably IDE? we accept this so far
			dirState[i] = DIR_STATE_MOUNTED;
		}
		if (dirState[i] == DIR_STATE_MOUNTED)
		{
			if (*(*dirList)[i].used == true)
			{
				if (statfs((*dirList)[i].name.c_str(), &s) >= 0)
				{
					drivefree = (s.f_bfree * s.f_bsize)>>30;
					char tmp[20];
					snprintf(tmp, 19,"%3d GB free",drivefree);
					dirOptionText[i] = tmp;
				}
				else
				{
					dirOptionText[i] = "? GB free";
				}
			}
			else
			{
				dirOptionText[i] = "Inactive";
			}
		}
	}
}

int CDirMenu::show(void)
{
	if (dirList->empty())
		return menu_return::RETURN_REPAINT;

	char tmp[20];

	CMenuWidget dirMenu(LOCALE_MOVIEBROWSER_HEAD, NEUTRINO_ICON_MOVIEPLAYER);
	dirMenu.addIntroItems(LOCALE_MOVIEBROWSER_MENU_DIRECTORIES_HEAD);

	updateDirState();
	for (unsigned int i = 0; i < dirList->size() && i < MB_MAX_DIRS; i++)
	{
		snprintf(tmp, sizeof(tmp),"%d",i);
		dirMenu.addItem(new CMenuForwarder ((*dirList)[i].name.c_str(), (dirState[i] != DIR_STATE_UNKNOWN), dirOptionText[i], this, tmp));
	}
	int ret = dirMenu.exec(NULL," ");
	return ret;
}
