/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2012-2013 defans@bluepeercrew.us

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
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <global.h>
#include <neutrino.h>
#include <pthread.h>
#include <algorithm>    // sort
#include "audiomute.h"
#include "screensaver.h"
#include <system/debug.h>
#include <gui/color_custom.h>
#include <gui/infoclock.h>
#include <zapit/zapit.h>
#include <driver/pictureviewer/pictureviewer.h>

#include <video.h>
extern cVideo * videoDecoder;




using namespace std;

CScreenSaver::CScreenSaver()
{
	thrScreenSaver 	= 0;
	m_frameBuffer 	= CFrameBuffer::getInstance();

	index 		= 0;
	status_mute	= CAudioMute::getInstance()->getStatus();

	scr_clock	= NULL;
	clr.i_color	= COL_DARK_GRAY;
	pip_channel_id	= 0;
	idletime	= time(NULL);
	force_refresh	= false;
}

CScreenSaver::~CScreenSaver()
{
	if(thrScreenSaver)
		pthread_cancel(thrScreenSaver);
	thrScreenSaver = 0;


	if (scr_clock)
		delete scr_clock;
}


CScreenSaver* CScreenSaver::getInstance()
{
	static CScreenSaver * screenSaver = NULL;
	if (!screenSaver)
		screenSaver = new CScreenSaver();

	return screenSaver;
}


void CScreenSaver::Start()
{
	OnBeforeStart();
	status_mute = CAudioMute::getInstance()->getStatus();
	CAudioMute::getInstance()->enableMuteIcon(false);

	if(!CInfoClock::getInstance()->isBlocked())
		CInfoClock::getInstance()->disableInfoClock();




#ifdef ENABLE_PIP
	pip_channel_id = CZapit::getInstance()->GetPipChannelID();
	if (pip_channel_id)
		g_Zapit->stopPip();
#endif

	m_frameBuffer->stopFrame();

	if(!thrScreenSaver)
	{
		//printf("[%s] %s: starting thread\n", __file__, __FUNCTION__);
		pthread_create(&thrScreenSaver, NULL, ScreenSaverPrg, (void*) this);
		pthread_detach(thrScreenSaver);
	}

}

void CScreenSaver::Stop()
{
	if(thrScreenSaver)
	{
		pthread_cancel(thrScreenSaver);
		thrScreenSaver = 0;
	}
	resetIdleTime();

	if (scr_clock){
		scr_clock->Stop();
		delete scr_clock;
		scr_clock = NULL;
	}

#ifdef ENABLE_PIP
	if(pip_channel_id) {
		CNeutrinoApp::getInstance()->StartPip(pip_channel_id);
		pip_channel_id = 0;
	}
#endif

	m_frameBuffer->paintBackground(); //clear entire screen


	CAudioMute::getInstance()->enableMuteIcon(status_mute);
	if (!OnAfterStop.empty()){
		OnAfterStop();
	}else{
		CInfoClock::getInstance()->ClearDisplay(); //provokes reinit
		CInfoClock::getInstance()->enableInfoClock();
	}
}

void* CScreenSaver::ScreenSaverPrg(void* arg)
{
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);

	CScreenSaver * PScreenSaver = static_cast<CScreenSaver*>(arg);

	PScreenSaver->m_frameBuffer->Clear();

	if (g_settings.screensaver_timeout)
	{
		while(PScreenSaver)
		{
			PScreenSaver->ReadDir();
			PScreenSaver->paint();
			int t = g_settings.screensaver_timeout;
			while (t--)
			{
				sleep(1);
				if (PScreenSaver->force_refresh)
				{
					PScreenSaver->force_refresh = false;
					break;
				}
			}
		}
	}
	else
		PScreenSaver->paint();

	return 0;
}

bool CScreenSaver::ReadDir()
{
	bool show_audiocover = false;

	if (CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_audio && g_settings.audioplayer_cover_as_screensaver)
	{
		if (access(COVERDIR_TMP, F_OK) == 0)
		{
			struct dirent **coverlist;
			int n = scandir(COVERDIR_TMP, &coverlist, 0, alphasort);
			if (n > 2) // we always have the "." and ".." entrys
				show_audiocover = true;
		}
	}

	string d;
	if (show_audiocover)
		d = COVERDIR_TMP;
	else
		d = g_settings.screensaver_dir;
	if (d.length() > 1)
	{
		//remove trailing slash
		string::iterator it = d.end() - 1;
		if (*it == '/')
			d.erase(it);
	}

	char *dir_name = (char *) d.c_str();
	struct dirent *dirpointer;
	DIR *dir;
	char curr_ext[5];
	int curr_lenght;
	bool ret = false;

	v_bg_files.clear();

	/* open dir */
	if((dir=opendir(dir_name)) == NULL) {
		fprintf(stderr,"[CScreenSaver] %s - %d : error open dir...\n",  __func__, __LINE__);
		return ret;
	}

	/* read complete dir */
	while((dirpointer=readdir(dir)) != NULL)
	{
		curr_lenght = strlen((*dirpointer).d_name);
		string str = dir_name;
		//printf("%d\n",curr_lenght);
		if(curr_lenght > 4)
		{
			strncpy(curr_ext,(*dirpointer).d_name+(curr_lenght-4),5);

			//printf("%s\n",curr_ext);
			if (strcasecmp(".jpg",curr_ext) && strcasecmp(".png",curr_ext))
				continue;

			str += "/";
			str += (*dirpointer).d_name;

			if ((string) dir_name == ICONSDIR)
			{
				/*
				  backward compatiblity:
				  just add the standard mp3-?.jpg pictures
				  to get the same behavior as usual
				*/
				if (str.find("/mp3-") == string::npos)
					continue;
			}

			v_bg_files.push_back(str);
		}
	}

	sort(v_bg_files.begin(), v_bg_files.end());

	/* close pointer */
	if(closedir(dir) == -1)
		dprintf(DEBUG_NORMAL, "[CScreenSaver]  %s - %d : Error no closed %s\n",  __func__, __LINE__,  dir_name);

	if(!v_bg_files.empty())
		ret = true;
#if 0
	else
		dprintf(DEBUG_NORMAL, "[CScreenSaver]  %s - %d : no picture found\n",  __func__, __LINE__);
#endif

	return ret;
}


void CScreenSaver::paint()
{
	if (scr_clock)
	{
		scr_clock->Stop();
		scr_clock->kill();
	}
	if (g_settings.screensaver_mode == SCR_MODE_IMAGE && v_bg_files.empty())
		m_frameBuffer->paintBackground();

	if (g_settings.screensaver_mode == SCR_MODE_IMAGE && !v_bg_files.empty()){

		if( (index >= v_bg_files.size()) || (access(v_bg_files.at(index).c_str(), F_OK)) )
		{
			ReadDir();
			index = 0;
			return;
		}

		dprintf(DEBUG_INFO, "[CScreenSaver]  %s - %d : %s\n",  __func__, __LINE__, v_bg_files.at(index).c_str());
		m_frameBuffer->showFrame(v_bg_files.at(index));

		if (!g_settings.screensaver_random)
			index++;
		else
			index = rand() % v_bg_files.size();

		if(index ==  v_bg_files.size())
			index = 0;
	}
	else{
		if (!scr_clock){
			scr_clock = new CComponentsFrmClock(1, 1, NULL, "%H:%M:%S", "%H:%M %S", true, 1, NULL, CC_SHADOW_OFF, COL_BLACK, COL_BLACK);
			scr_clock->setCornerType(CORNER_NONE);
			scr_clock->setClockFont(g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]);
			scr_clock->disableSaveBg();
			scr_clock->doPaintBg(false);
		}

		scr_clock->setTextColor(clr.i_color);

		//check position and size use only possible available screen size
		int x_cl, y_cl, w_cl, h_cl;
		scr_clock->getDimensions(&x_cl, &y_cl, &w_cl, &h_cl);
		int x_random = rand() % ((g_settings.screen_EndX - w_cl - g_settings.screen_StartX) + 1) + g_settings.screen_StartX;
		int y_random = rand() % ((g_settings.screen_EndY - h_cl - g_settings.screen_StartY) + 1) + g_settings.screen_StartY;
		scr_clock->setPos(x_random, y_random);
		scr_clock->Start();

		if (g_settings.screensaver_mode == SCR_MODE_CLOCK_COLOR) {
			srand (time(NULL));
			uint32_t brightness;

			// sorcery, no darkness
			do {
				clr.i_color = rand();
				brightness = (unsigned int)clr.uc_color.r * 19595 + (unsigned int)clr.uc_color.g * 38469 + (unsigned int)clr.uc_color.b * 7471;
				//printf("[%s] %s: brightness: %d\n", __file__, __FUNCTION__, brightness>> 16);
			}
			while(brightness >> 16 < 80);

			clr.i_color &= 0x00FFFFFF;
			//printf("[%s] %s: clr.i_color: r %02x g %02x b %02x a %02x\n", __file__, __FUNCTION__, clr.uc_color.r, clr.uc_color.g, clr.uc_color.b, clr.uc_color.a);
		}
		else
			clr.i_color = COL_DARK_GRAY;
	}
}

bool CScreenSaver::canStart()
{
	if (g_settings.screensaver_delay && (time(NULL) - idletime > g_settings.screensaver_delay*60))
		return true;
	return false;
}

bool CScreenSaver::isActive()
{
	if(thrScreenSaver)
		return true;
	return false;
}

bool CScreenSaver::ignoredMsg(neutrino_msg_t msg)
{
	/* screensaver will ignore these msgs */
	if (
		   msg == NeutrinoMessages::EVT_CURRENTEPG
		|| msg == NeutrinoMessages::EVT_NEXTEPG
		|| msg == NeutrinoMessages::EVT_CURRENTNEXT_EPG
		|| msg == NeutrinoMessages::EVT_TIMESET
		|| msg == NeutrinoMessages::EVT_PROGRAMLOCKSTATUS
		|| msg == NeutrinoMessages::EVT_ZAP_GOT_SUBSERVICES
		|| msg == NeutrinoMessages::EVT_ZAP_GOTAPIDS
		|| msg == NeutrinoMessages::EVT_ZAP_GOTPIDS
		|| msg == NeutrinoMessages::EVT_EIT_COMPLETE
		|| msg == NeutrinoMessages::EVT_BACK_ZAP_COMPLETE
	)
		return true;
	return false;
}
