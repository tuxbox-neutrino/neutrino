/*
	Neutrino-GUI  -   DBoxII-Project

	MP3Player by Dirch

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

#include <gui/pictureviewer.h>

#include <global.h>
#include <neutrino.h>

#include <daemonc/remotecontrol.h>

#include <driver/display.h>
#include <driver/fontrenderer.h>
#include <driver/rcinput.h>

#include <gui/audiomute.h>

#ifdef ENABLE_GUI_MOUNT
#include <gui/nfs.h>
#endif

#include <gui/components/cc.h>
#include <gui/widget/buttons.h>
#include <gui/widget/icons.h>
#include <gui/infoclock.h>
#include <gui/widget/menue.h>
#include <gui/widget/msgbox.h>

// remove this
#include <gui/widget/hintbox.h>

#include <gui/pictureviewer_help.h>
#include <gui/widget/stringinput.h>
#include <driver/screen_max.h>
#include <driver/display.h>


#include <system/settings.h>

#include <algorithm>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <zapit/zapit.h>
#include <video.h>
extern cVideo * videoDecoder;


//------------------------------------------------------------------------
bool comparePictureByDate (const CPicture& a, const CPicture& b)
{
	return a.Date < b.Date ;
}
//------------------------------------------------------------------------
extern bool comparetolower(const char a, const char b); /* filebrowser.cpp */
//------------------------------------------------------------------------
bool comparePictureByFilename (const CPicture& a, const CPicture& b)
{
	return std::lexicographical_compare(a.Filename.begin(), a.Filename.end(), b.Filename.begin(), b.Filename.end(), comparetolower);
}
//------------------------------------------------------------------------

CPictureViewerGui::CPictureViewerGui()
{
	frameBuffer = CFrameBuffer::getInstance();

	visible = false;
	selected = 0;
	m_sort = FILENAME;
	m_viewer = new CPictureViewer();
	if (g_settings.network_nfs_picturedir.empty())
		Path = "/";
	else
		Path = g_settings.network_nfs_picturedir;

	picture_filter.addFilter("png");
	picture_filter.addFilter("bmp");
	picture_filter.addFilter("jpg");
	picture_filter.addFilter("jpeg");
	picture_filter.addFilter("gif");
	picture_filter.addFilter("crw");

	decodeT		= 0;
	decodeTflag	= false;
}

//------------------------------------------------------------------------

CPictureViewerGui::~CPictureViewerGui()
{
	playlist.clear();
	delete m_viewer;

	if (decodeT)
	{
		pthread_cancel(decodeT);
		decodeT = 0;
	}
}

//------------------------------------------------------------------------

#define PictureViewerButtons1Count 4
const struct button_label PictureViewerButtons1[PictureViewerButtons1Count] =
{
	{ NEUTRINO_ICON_BUTTON_RED	, LOCALE_AUDIOPLAYER_DELETE	},
	{ NEUTRINO_ICON_BUTTON_GREEN	, LOCALE_AUDIOPLAYER_ADD	},
	{ NEUTRINO_ICON_BUTTON_YELLOW	, LOCALE_AUDIOPLAYER_DELETEALL	},
	{ NEUTRINO_ICON_BUTTON_BLUE	, LOCALE_PICTUREVIEWER_SLIDESHOW }
};

#define PictureViewerButtons2Count 3
struct button_label PictureViewerButtons2[PictureViewerButtons2Count] =
{
	{ NEUTRINO_ICON_BUTTON_5	, LOCALE_PICTUREVIEWER_SORTORDER_DATE	},
	{ NEUTRINO_ICON_BUTTON_OKAY	, LOCALE_PICTUREVIEWER_SHOW		},
	{ NEUTRINO_ICON_BUTTON_MUTE_SMALL, LOCALE_FILEBROWSER_DELETE		}
};

//------------------------------------------------------------------------

int CPictureViewerGui::exec(CMenuTarget* parent, const std::string & actionKey)
{
	audioplayer = false;
	if (actionKey == "audio")
		audioplayer = true;

	selected = 0;

	width = frameBuffer->getScreenWidthRel();
	height = frameBuffer->getScreenHeightRel();

	header_height = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	item_height = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();

        //get footer_height from paintButtons
	buttons1_height = ::paintButtons(0, 0, 0, PictureViewerButtons1Count, PictureViewerButtons1, 0, 0, "", false, COL_MENUFOOT_TEXT, NULL, 0, false);
	buttons2_height = ::paintButtons(0, 0, 0, PictureViewerButtons2Count, PictureViewerButtons2, 0, 0, "", false, COL_MENUFOOT_TEXT, NULL, 0, false);
	footer_height = buttons1_height + buttons2_height;

	listmaxshow = (height - header_height - footer_height - OFFSET_SHADOW)/item_height;
	height = header_height + listmaxshow*item_height + footer_height + OFFSET_SHADOW; // recalc height

	x=getScreenStartX(width);
	y=getScreenStartY(height);

	m_viewer->SetScaling((CPictureViewer::ScalingMode)g_settings.picviewer_scaling);
	m_viewer->SetVisible(g_settings.screen_StartX, g_settings.screen_EndX, g_settings.screen_StartY, g_settings.screen_EndY);

	if (g_settings.video_Format == 3)
		m_viewer->SetAspectRatio(16.0/9);
	else
		m_viewer->SetAspectRatio(4.0/3);

	if (parent)
		parent->hide();

	// remember last mode
	m_LastMode = CNeutrinoApp::getInstance()->getMode();
	// tell neutrino we're in pic_mode
	CNeutrinoApp::getInstance()->handleMsg( NeutrinoMessages::CHANGEMODE , NeutrinoMessages::mode_pic );

	if (!audioplayer) { // !!! why? !!!
		CNeutrinoApp::getInstance()->stopPlayBack(true);

		// blank background screen
		videoDecoder->setBlank(true);
	}

	// Save and Clear background
	bool usedBackground = frameBuffer->getuseBackground();
	if (usedBackground) {
		frameBuffer->saveBackgroundImage();
		frameBuffer->Clear();
	}

	show();

	// free picviewer mem
	m_viewer->Cleanup();

	if (!audioplayer) { // !!! why? !!!
		//g_Zapit->unlockPlayBack();
		CZapit::getInstance()->EnablePlayback(true);
	}

	// Restore previous background
	if (usedBackground) {
		frameBuffer->restoreBackgroundImage();
		frameBuffer->useBackground(true);
		frameBuffer->paintBackground();
	}

	// Restore last mode
	CNeutrinoApp::getInstance()->handleMsg( NeutrinoMessages::CHANGEMODE , m_LastMode );
	if (m_LastMode == NeutrinoMessages::mode_ts)
		videoDecoder->setBlank(false);

	// always exit all
	return menu_return::RETURN_REPAINT;
}

//------------------------------------------------------------------------

int CPictureViewerGui::show()
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	int res = -1;

	CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8, g_Locale->getText(LOCALE_PICTUREVIEWER_HEAD));
	m_state=MENU;

	int timeout;

	bool loop=true;
	bool update=true;

	if (audioplayer)
		m_currentTitle = m_audioPlayer->getAudioPlayerM_current();

	CAudioMute::getInstance()->enableMuteIcon(false);
	CInfoClock::getInstance()->enableInfoClock(false);

	while (loop)
	{
		if (update)
		{
			hide();
			update=false;
			paint();
		}

		if (audioplayer)
			m_audioPlayer->wantNextPlay();

		if (m_state!=SLIDESHOW)
			timeout=50; // egal
		else
		{
			timeout=(m_time+g_settings.picviewer_slide_time-(long)time(NULL))*10;
			if (timeout <0 )
				timeout=1;
		}
		g_RCInput->getMsg( &msg, &data, timeout );

		if ( msg == CRCInput::RC_home)
		{ //Exit after cancel key
			if (m_state!=MENU)
			{
				endView();
				update=true;
			}
			else
				loop=false;
		}
		else if (msg == CRCInput::RC_timeout)
		{
			if (m_state == SLIDESHOW)
			{
				m_time=(long)time(NULL);
				unsigned int next = selected + 1;
				if (next >= playlist.size())
					next = 0;
				view(next);
			}
		}
		else if (msg == CRCInput::RC_left)
		{
			if (!playlist.empty())
			{
				if (m_state == MENU)
				{
					if (selected < listmaxshow)
						selected=playlist.size()-1;
					else
						selected -= listmaxshow;
					liststart = (selected/listmaxshow)*listmaxshow;
					paint();
				}
				else
				{
					view((selected == 0) ? (playlist.size() - 1) : (selected - 1));
				}
			}
		}
		else if (msg == CRCInput::RC_right)
		{
			if (!playlist.empty())
			{
				if (m_state == MENU)
				{
					selected += listmaxshow;
					if (selected >= playlist.size()) {
						if (((playlist.size() / listmaxshow) + 1) * listmaxshow == playlist.size() + listmaxshow)
							selected=0;
						else
							selected = selected < (((playlist.size() / listmaxshow) + 1) * listmaxshow) ? (playlist.size() - 1) : 0;
					}
					liststart = (selected/listmaxshow)*listmaxshow;
					paint();
				}
				else
				{
					unsigned int next = selected + 1;
					if (next >= playlist.size())
						next = 0;
					view(next);
				}
			}
		}
		else if (msg == CRCInput::RC_up)
		{
			if ((m_state == MENU) && (!playlist.empty()))
			{
				int prevselected=selected;
				if (selected==0)
				{
					selected = playlist.size()-1;
				}
				else
					selected--;
				paintItem(prevselected - liststart);
				unsigned int oldliststart = liststart;
				liststart = (selected/listmaxshow)*listmaxshow;
				if (oldliststart!=liststart)
				{
					update=true;
				}
				else
				{
					paintItem(selected - liststart);
				}
			}
		}
		else if (msg == CRCInput::RC_down)
		{
			if ((m_state == MENU) && (!playlist.empty()))
			{
				int prevselected=selected;
				selected = (selected+1)%playlist.size();
				paintItem(prevselected - liststart);
				unsigned int oldliststart = liststart;
				liststart = (selected/listmaxshow)*listmaxshow;
				if (oldliststart!=liststart)
				{
					update=true;
				}
				else
				{
					paintItem(selected - liststart);
				}
			}
		}
		else if (msg == CRCInput::RC_spkr)
		{
			if (!playlist.empty())
			{
				if (m_state == MENU)
				{
					deletePicFile(selected, false);
					update = true;
				}
				else{
					deletePicFile(selected, true);
				}
			}
		}

		else if (msg == CRCInput::RC_ok)
		{
			if (!playlist.empty())
				view(selected);
		}
		else if (msg == CRCInput::RC_red)
		{
			if (m_state == MENU)
			{
				if (!playlist.empty())
				{
					CViewList::iterator p = playlist.begin()+selected;
					playlist.erase(p);
					if (selected >= playlist.size())
						selected = playlist.size()-1;
					update = true;
				}
			}
		}
		else if (msg==CRCInput::RC_green)
		{
			if (m_state == MENU)
			{
				CFileBrowser filebrowser((g_settings.filebrowser_denydirectoryleave) ? g_settings.network_nfs_picturedir.c_str() : "");

				filebrowser.Multi_Select    = true;
				filebrowser.Dirs_Selectable = true;
				filebrowser.Filter          = &picture_filter;

				hide();

				if (filebrowser.exec(Path.c_str()))
				{
					Path = filebrowser.getCurrentDir();
					CFileList::const_iterator files = filebrowser.getSelectedFiles().begin();
					for (; files != filebrowser.getSelectedFiles().end(); ++files)
					{
						if (files->getType() == CFile::FILE_PICTURE)
						{
							CPicture pic;
							pic.Filename = files->Name;
							std::string tmp   = files->Name.substr(files->Name.rfind('/')+1);
							pic.Name     = tmp.substr(0,tmp.rfind('.'));
							pic.Type     = tmp.substr(tmp.rfind('.')+1);
							struct stat statbuf;
							if (stat(pic.Filename.c_str(),&statbuf) != 0)
								fprintf(stderr, "stat '%s' error: %m\n", pic.Filename.c_str());
							pic.Date     = statbuf.st_mtime;
							playlist.push_back(pic);
						}
						else
							printf("Wrong Filetype %s:%d\n",files->Name.c_str(), files->getType());
					}
					if (m_sort == FILENAME)
						std::sort(playlist.begin(), playlist.end(), comparePictureByFilename);
					else if (m_sort == DATE)
						std::sort(playlist.begin(), playlist.end(), comparePictureByDate);
				}
				update=true;
			}
		}
		else if (msg==CRCInput::RC_yellow)
		{
			if (m_state == MENU && !playlist.empty())
			{
				playlist.clear();
				selected=0;
				update=true;
			}
		}
		else if (msg==CRCInput::RC_blue)
		{
			if(!playlist.empty())
			{
				if (m_state == MENU)
				{
					m_time=(long)time(NULL);
					view(selected);
					m_state=SLIDESHOW;
				} else {
					if (m_state == SLIDESHOW)
						m_state = VIEW;
					else
						m_state = SLIDESHOW;
				}
			}
		}
		else if (msg==CRCInput::RC_help)
		{
			if (m_state == MENU)
			{
				showHelp();
				paint();
			}
		}
		else if ( msg == CRCInput::RC_1 )
		{
			if (m_state != MENU && !playlist.empty())
			{
				m_viewer->Zoom(2.0/3);
			}

		}
		else if ( msg == CRCInput::RC_2 )
		{
			if (m_state != MENU && !playlist.empty())
			{
				m_viewer->Move(0,-50);
			}
		}
		else if ( msg == CRCInput::RC_3 )
		{
			if (m_state != MENU && !playlist.empty())
			{
				m_viewer->Zoom(1.5);
			}

		}
		else if ( msg == CRCInput::RC_4 )
		{
			if (m_state != MENU && !playlist.empty())
			{
				m_viewer->Move(-50,0);
			}
		}
		else if ( msg == CRCInput::RC_5 )
		{
			if (m_state==MENU)
			{
				if (!playlist.empty())
				{
					if (m_sort==FILENAME)
					{
						m_sort=DATE;
						std::sort(playlist.begin(),playlist.end(),comparePictureByDate);
					}
					else if (m_sort==DATE)
					{
						m_sort=FILENAME;
						std::sort(playlist.begin(),playlist.end(),comparePictureByFilename);
					}
					update=true;
				}
			}
		}
		else if ( msg == CRCInput::RC_6 )
		{
			if (m_state != MENU && !playlist.empty())
			{
				m_viewer->Move(50,0);
			}
		}
		else if ( msg == CRCInput::RC_8 )
		{
			if (m_state != MENU && !playlist.empty())
			{
				m_viewer->Move(0,50);
			}
		}
		else if (msg==CRCInput::RC_0)
		{
			if (!playlist.empty())
				view(selected, true);
		}
#ifdef ENABLE_GUI_MOUNT
		else if (msg==CRCInput::RC_setup)
		{
			if (m_state==MENU)
			{
				CNFSSmallMenu nfsMenu;
				nfsMenu.exec(this, "");
				update=true;
				CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8, g_Locale->getText(LOCALE_PICTUREVIEWER_HEAD));
			}
		}
#endif
		else if (((msg==CRCInput::RC_plus) || (msg==CRCInput::RC_minus)) && decodeTflag)
		{
			// FIXME: do not accept volume-keys while decoding
		}
		// control keys for audioplayer
		else if (audioplayer && msg==CRCInput::RC_pause)
		{
			m_currentTitle = m_audioPlayer->getAudioPlayerM_current();
			m_audioPlayer->pause();
		}
		else if (audioplayer && msg==CRCInput::RC_stop)
		{
			m_currentTitle = m_audioPlayer->getAudioPlayerM_current();
			m_audioPlayer->stop();
		}
		else if (audioplayer && msg==CRCInput::RC_play)
		{
			m_currentTitle = m_audioPlayer->getAudioPlayerM_current();
			if (m_currentTitle > -1)
				m_audioPlayer->play((unsigned int)m_currentTitle);
		}
		else if (audioplayer && msg==CRCInput::RC_forward)
		{
			m_audioPlayer->playNext();
		}
		else if (audioplayer && msg==CRCInput::RC_rewind)
		{
			m_audioPlayer->playPrev();
		}
		else if (msg == NeutrinoMessages::CHANGEMODE)
		{
			if ((data & NeutrinoMessages::mode_mask) !=NeutrinoMessages::mode_pic)
			{
				loop = false;
				m_LastMode=data;
			}
		}
		else if (msg == NeutrinoMessages::RECORD_START ||
				msg == NeutrinoMessages::ZAPTO ||
				msg == NeutrinoMessages::STANDBY_ON ||
				msg == NeutrinoMessages::LEAVE_ALL ||
				msg == NeutrinoMessages::SHUTDOWN ||
				msg == NeutrinoMessages::SLEEPTIMER)
		{
			// Exit for Record/Zapto Timers
			if (m_state != MENU)
				endView();
			loop = false;
			g_RCInput->postMsg(msg, data);
		}
		else if (CNeutrinoApp::getInstance()->listModeKey(msg)) {
			// do nothing
		}
		else
		{
			if ( CNeutrinoApp::getInstance()->handleMsg( msg, data ) & messages_return::cancel_all )
			{
				loop = false;
			}
		}
	}
	hide();

	CAudioMute::getInstance()->enableMuteIcon(true);
	CInfoClock::getInstance()->enableInfoClock(true);

	return(res);
}

//------------------------------------------------------------------------

void CPictureViewerGui::hide()
{
	if (visible) {
		frameBuffer->paintBackground();
		visible = false;
	}
}

//------------------------------------------------------------------------

void CPictureViewerGui::paintItem(int pos)
{
//	printf("paintItem{\n");
	int ypos = y + header_height + pos*item_height;

	unsigned int currpos = liststart + pos;

	bool i_selected	= currpos == selected;
	bool i_marked	= false;
	bool i_switch	= false; //(currpos < playlist.size()) && (pos & 1);
	int i_radius	= RADIUS_NONE;

	fb_pixel_t color;
	fb_pixel_t bgcolor;

	getItemColors(color, bgcolor, i_selected, i_marked, i_switch);

	if (i_selected || i_marked)
		i_radius = RADIUS_LARGE;

	if (i_radius)
		frameBuffer->paintBoxRel(x, ypos, width - SCROLLBAR_WIDTH, item_height, COL_MENUCONTENT_PLUS_0);
	frameBuffer->paintBoxRel(x, ypos, width - SCROLLBAR_WIDTH, item_height, bgcolor, i_radius);

	if (currpos < playlist.size())
	{
		std::string tmp = playlist[currpos].Name;
		tmp += " (";
		tmp += playlist[currpos].Type;
		tmp += ')';
		char timestring[18];
		strftime(timestring, 18, "%d-%m-%Y %H:%M", gmtime(&playlist[currpos].Date));
		int w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(timestring);
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x + OFFSET_INNER_MID, ypos + item_height, width - SCROLLBAR_WIDTH - 2*OFFSET_INNER_MID - w, tmp, color, item_height);
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x + width - SCROLLBAR_WIDTH - OFFSET_INNER_MID - w, ypos + item_height, w, timestring, color, item_height);

	}
//	printf("paintItem}\n");
}

//------------------------------------------------------------------------

void CPictureViewerGui::paintHead()
{
	CComponentsHeader header(x, y, width, header_height, LOCALE_PICTUREVIEWER_HEAD, NEUTRINO_ICON_PICTUREVIEWER, CComponentsHeader::CC_BTN_HELP);
	header.enableShadow(CC_SHADOW_RIGHT | CC_SHADOW_CORNER_TOP_RIGHT | CC_SHADOW_CORNER_BOTTOM_RIGHT, -1, true);

#ifdef ENABLE_GUI_MOUNT
	header.setContextButton(NEUTRINO_ICON_BUTTON_MENU);
#endif

	header.paint(CC_SAVE_SCREEN_NO);
}

//------------------------------------------------------------------------

void CPictureViewerGui::paintFoot()
{
	if (m_sort == FILENAME)
		PictureViewerButtons2[0].locale = LOCALE_PICTUREVIEWER_SORTORDER_FILENAME;
	else
		PictureViewerButtons2[0].locale = LOCALE_PICTUREVIEWER_SORTORDER_DATE;

	int footer_y = y + (height - footer_height - OFFSET_SHADOW);

	// shadow
	frameBuffer->paintBoxRel(x + OFFSET_SHADOW, footer_y + OFFSET_SHADOW, width, footer_height, COL_SHADOW_PLUS_0, RADIUS_LARGE, CORNER_BOTTOM);

	frameBuffer->paintBoxRel(x, footer_y, width, footer_height, COL_MENUFOOT_PLUS_0, RADIUS_LARGE, CORNER_BOTTOM);

	if (!playlist.empty())
	{
		::paintButtons(x, footer_y, width, PictureViewerButtons1Count, PictureViewerButtons1, width, buttons1_height);
		::paintButtons(x, footer_y + buttons1_height, width, PictureViewerButtons2Count, PictureViewerButtons2, width, buttons2_height);
	}
	else
		::paintButtons(x, footer_y, width, 1, &(PictureViewerButtons1[1]), width, buttons1_height);
}

//------------------------------------------------------------------------

void CPictureViewerGui::paintInfo()
{
}

//------------------------------------------------------------------------

void CPictureViewerGui::paint()
{
	liststart = (selected/listmaxshow)*listmaxshow;

	paintHead();
	for (unsigned int count=0; count<listmaxshow; count++)
	{
		paintItem(count);
	}

	//scrollbar
	int total_pages;
	int current_page;
	getScrollBarData(&total_pages, &current_page, playlist.size(), listmaxshow, selected);
	paintScrollBar(x + width - SCROLLBAR_WIDTH, y + header_height, SCROLLBAR_WIDTH, item_height*listmaxshow, total_pages, current_page, CC_SHADOW_ON);

	paintFoot();
	paintInfo();

	visible = true;
}

void CPictureViewerGui::view(unsigned int index, bool unscaled)
{
	if (decodeTflag)
		return;

	m_unscaled = unscaled;
	selected=index;

	CVFD::getInstance()->showMenuText(0, playlist[index].Name.c_str());
	char timestring[19];
	strftime(timestring, 18, "%d-%m-%Y %H:%M", gmtime(&playlist[index].Date));
	//CVFD::getInstance()->showMenuText(1, timestring); //FIXME

	if (m_state==MENU)
		m_state=VIEW;

	//decode and view in a seperate thread
	if (!decodeTflag) {
		decodeTflag=true;
		pthread_create(&decodeT, NULL, decodeThread, (void*) this);
		pthread_detach(decodeT);
	}
}

void* CPictureViewerGui::decodeThread(void *arg)
{
	CPictureViewerGui *PictureViewerGui = (CPictureViewerGui*) arg;

	pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	PictureViewerGui->thrView();

	PictureViewerGui->decodeTflag=false;
	pthread_exit(NULL);
}

void CPictureViewerGui::thrView()
{
	if (m_unscaled)
		m_viewer->DecodeImage(playlist[selected].Filename, true, m_unscaled);

	m_viewer->ShowImage(playlist[selected].Filename, m_unscaled);

#if 0
	//Decode next
	unsigned int next=selected+1;
	if (next > playlist.size()-1)
		next=0;
	if (m_state==VIEW)
		m_viewer->DecodeImage(playlist[next].Filename,true);
	else
		m_viewer->DecodeImage(playlist[next].Filename,false);
#endif
}

void CPictureViewerGui::endView()
{
	if (m_state != MENU)
		m_state=MENU;

	if (decodeTflag)
	{
		decodeTflag=false;
		pthread_cancel(decodeT);
	}
}

void CPictureViewerGui::deletePicFile(unsigned int index, bool mode)
{
	CVFD::getInstance()->showMenuText(0, playlist[index].Name.c_str());
	if (ShowMsg(LOCALE_FILEBROWSER_DELETE, playlist[index].Filename, CMsgBox::mbrNo, CMsgBox::mbYes|CMsgBox::mbNo)==CMsgBox::mbrYes)
	{
		unlink(playlist[index].Filename.c_str());
		printf("[ %s ]  delete file: %s\r\n",__FUNCTION__,playlist[index].Filename.c_str());
		CViewList::iterator p = playlist.begin()+index;
		playlist.erase(p);
		if(mode)
			selected = selected-1;
		if (selected >= playlist.size())
			selected = playlist.size()-1;
	}
}

int CPictureViewerGui::showHelp()
{
	CPictureViewerHelp help(audioplayer);
	return help.exec(NULL, "");
}
