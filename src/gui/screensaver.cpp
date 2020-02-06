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
#include <driver/radiotext.h>
#include "radiotext_window.h"
#include <system/debug.h>
#include <gui/color_custom.h>
#include <gui/infoclock.h>
#include <zapit/zapit.h>
#include <driver/pictureviewer/pictureviewer.h>
#include <system/set_threadname.h>

#include <hardware/video.h>
extern cVideo * videoDecoder;

static CComponentsFrmClock *scr_clock = NULL;


using namespace std;

CScreenSaver::CScreenSaver()
{
	thrScreenSaver 	= NULL;
	m_frameBuffer 	= CFrameBuffer::getInstance();

	index 		= 0;
	status_mute	= CAudioMute::getInstance()->getStatus();

	clr.i_color	= COL_DARK_GRAY;
	pip_channel_id	= 0;
	idletime	= time(NULL);
	force_refresh	= false;
	thr_exit	= false;

	sl_scr_stop	= sigc::mem_fun(*this, &CScreenSaver::thrExit);
	CNeutrinoApp::getInstance()->OnBeforeRestart.connect(sl_scr_stop);
	CNeutrinoApp::getInstance()->OnShutDown.connect(sl_scr_stop);
}

CScreenSaver::~CScreenSaver()
{
	thrExit();

	if (scr_clock){
		delete scr_clock;
		scr_clock = NULL;
	}
}


CScreenSaver* CScreenSaver::getInstance()
{
	static CScreenSaver * screenSaver = NULL;
	if (!screenSaver)
		screenSaver = new CScreenSaver();

	return screenSaver;
}

void CScreenSaver::thrExit()
{
	if(thrScreenSaver)
	{
		dprintf(DEBUG_NORMAL,"[%s] %s: exit screensaver thread\n", __file__, __func__);
		thr_exit = true;
		thrScreenSaver->join();
		delete thrScreenSaver;
		thrScreenSaver = NULL;
		dprintf(DEBUG_NORMAL,"\033[32m[CScreenSaver] [%s - %d] screensaver thread stopped\033[0m\n", __func__, __LINE__);
	}
}


void CScreenSaver::Start()
{
	if (!OnBeforeStart.empty())
		OnBeforeStart();

	status_mute = CAudioMute::getInstance()->getStatus();
	CAudioMute::getInstance()->enableMuteIcon(false);

	if(!CInfoClock::getInstance()->isBlocked())
		CInfoClock::getInstance()->block();

	if (g_RadiotextWin)
		g_Radiotext->OnAfterDecodeLine.block();

#ifdef ENABLE_PIP
	pip_channel_id = CZapit::getInstance()->GetPipChannelID();
	if (pip_channel_id)
		g_Zapit->stopPip();
#endif

	m_frameBuffer->stopFrame();
	
	if(!thrScreenSaver)
	{
		dprintf(DEBUG_NORMAL,"[%s] %s: starting thread\n", __file__, __func__);
		thr_exit = false;
		thrScreenSaver = new std::thread (ScreenSaverPrg, this);
		std::string tn = "screen_saver";
		set_threadname(tn.c_str());
		dprintf(DEBUG_NORMAL,"\033[32m[CScreenSaver] [%s - %d] thread [%p] [%s] started\033[0m\n", __func__, __LINE__, thrScreenSaver, tn.c_str());
	}

	if (!OnAfterStart.empty())
		OnAfterStart();
}

void CScreenSaver::Stop()
{
	//exit thread
	thrExit();

	resetIdleTime();

	if (scr_clock){
		std::lock_guard<std::mutex> g(scr_mutex);
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

	CInfoClock::getInstance()->ClearDisplay(); //provokes reinit
	CInfoClock::getInstance()->enableInfoClock();

	if (g_RadiotextWin)
		g_Radiotext->OnAfterDecodeLine.unblock();

	if (!OnAfterStop.empty())
		OnAfterStop();
}

void CScreenSaver::ScreenSaverPrg(CScreenSaver *scr)
{
	scr->m_frameBuffer->Clear();

	if (g_settings.screensaver_timeout)
	{
		while(!scr->thr_exit)
		{
			if (g_settings.screensaver_mode == SCR_MODE_IMAGE)
				scr->ReadDir();

			scr->paint();

			int corr = 1;
#if HAVE_COOL_HARDWARE //time offset
			corr = 10;
#endif
			int t = 1000/corr * g_settings.screensaver_timeout; //sleep and exit handle
			while (t > 0)
			{
				if (!scr->thr_exit)
					this_thread::sleep_for(std::chrono::milliseconds(1));
				t--;

				if (scr->force_refresh) //NOTE: Do we really need this ?
				{
					scr->force_refresh = false;
					break;
				}
			}
		}
	}
	else
		scr->paint();
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
			if (n > 2){ // we always have the "." and ".." entrys
				show_audiocover = true;
			}
			if(n > -1)
			{
				for (int i = 0; i < n; i++)
				{
					free(coverlist[i]);
				}
				free(coverlist);
			}
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
	char curr_ext[6];
	bool ret = false;

	v_bg_files.clear();

	/* open dir */
	if((dir=opendir(dir_name)) == NULL) {
		fprintf(stderr,"[CScreenSaver] %s - %d : error open dir...\n",  __func__, __LINE__);
		return ret;
	}

	/* read complete dir */
	while((dirpointer=readdir(dir)) != NULL) // TODO: use threadsave readdir_r instead readdir
	{
		int curr_lenght = strlen((*dirpointer).d_name);
		string str = dir_name;
		//printf("%d\n",curr_lenght);
		if(curr_lenght > 4)
		{
			strncpy(curr_ext,(*dirpointer).d_name+(curr_lenght-4),sizeof(curr_ext)-1);

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
		if (g_settings.screensaver_mode != SCR_MODE_CLOCK_COLOR && g_settings.screensaver_mode != SCR_MODE_CLOCK)
		{
			std::lock_guard<std::mutex> g(scr_mutex);
			delete scr_clock;
			scr_clock = NULL;
		}
		else
		{
			scr_clock->kill(0);
			scr_clock->clearSavedScreen();
			scr_clock->doPaintBg(false);
		}
	}

	if (g_settings.screensaver_mode == SCR_MODE_IMAGE && v_bg_files.empty())
		m_frameBuffer->paintBackground();

	if (g_settings.screensaver_mode == SCR_MODE_IMAGE && !v_bg_files.empty())
	{
		if( (index >= v_bg_files.size()) || (access(v_bg_files.at(index).c_str(), F_OK)))
		{
			ReadDir();
			index = 0;
			return;
		}

		dprintf(DEBUG_INFO, "[CScreenSaver]  %s - %d : %s\n",  __func__, __LINE__, v_bg_files.at(index).c_str());

		m_frameBuffer->showFrame(v_bg_files.at(index), CFrameBuffer::SHOW_FRAME_FALLBACK_MODE_IMAGE_UNSCALED);

		handleRadioText(g_settings.screensaver_mode_text);

		if (!g_settings.screensaver_random)
			index++;
		else
			index = rand_r(&seed[0]) % v_bg_files.size();

		if(index ==  v_bg_files.size())
			index = 0;
	}
	else
	{
		if (!scr_clock)
		{
			scr_clock = new CComponentsFrmClock(1, 1, NULL, "%H:%M:%S", "%H:%M %S", false, 1, NULL, CC_SHADOW_OFF, COL_BLACK, COL_BLACK);
			scr_clock->setItemName("scr_clock");
			scr_clock->setCornerType(CORNER_NONE);
			scr_clock->setClockFont(g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]);
			scr_clock->enableSaveBg();
			scr_clock->doPaintBg(false);
#if HAVE_COOL_HARDWARE
			paintImage("blackscreen.jpg", 0, 0, m_frameBuffer->getScreenWidth(true), m_frameBuffer->getScreenHeight(true));
#endif
		}
#if !HAVE_COOL_HARDWARE
#if 0 //example for callback
		m_frameBuffer->OnFallbackShowFrame.connect(sigc::bind(sigc::mem_fun(CFrameBuffer::getInstance(),
								&CFrameBuffer::paintBoxRel),
								scr_clock->getXPos(), scr_clock->getYPos(),
								scr_clock->getWidth(), scr_clock->getHeight(),
								COL_BLACK,
								0,
								CORNER_ALL)
												  );
#endif
		m_frameBuffer->showFrame("blackscreen.jpg", CFrameBuffer::SHOW_FRAME_FALLBACK_MODE_CALLBACK | CFrameBuffer::SHOW_FRAME_FALLBACK_MODE_BLACKSCREEN);
#endif

		handleRadioText(g_settings.screensaver_mode_text);

		if (scr_clock)
		{
			scr_clock->setTextColor(clr.i_color);
			//check position and size use only possible available screen size
			int x_cl, y_cl, w_cl, h_cl;
			scr_clock->getDimensions(&x_cl, &y_cl, &w_cl, &h_cl);
			int	x_random = rand_r(&seed[1]) % ((g_settings.screen_EndX - w_cl - g_settings.screen_StartX) + 1) + g_settings.screen_StartX;
			int	y_random = rand_r(&seed[2]) % ((g_settings.screen_EndY - h_cl - g_settings.screen_StartY) + 1) + g_settings.screen_StartY;

			if (g_RadiotextWin)
			{
				// avoid overlapping of clock and radio text window
				int y_min = g_RadiotextWin->getYPos() - scr_clock->getHeight();
				int y_max = g_RadiotextWin->getYPos() + g_RadiotextWin->getHeight();
				while (y_random > (y_min - scr_clock->getHeight()) && y_random < y_max)
					y_random = rand_r(&seed[2]) % ((g_settings.screen_EndY - h_cl - g_settings.screen_StartY) + 1) + g_settings.screen_StartY;
			}

			scr_clock->setPos(x_random, y_random);

			if (!scr_clock->isRun())
				scr_clock->Start();
			else
				scr_clock->paint(true);

			if (g_RadiotextWin && g_settings.screensaver_mode_text)
				scr_clock->allowPaint(g_RadiotextWin->isPainted());
			else
				scr_clock->allowPaint(true);
		}

		if (g_settings.screensaver_mode == SCR_MODE_CLOCK_COLOR)
		{
			srand (time(NULL));
			uint32_t brightness;

			// sorcery, no darkness
			do {
				clr.i_color = rand_r(&seed[3]);
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

void CScreenSaver::handleRadioText(bool enable_paint)
{
	if (!g_RadiotextWin)
		return;

	g_RadiotextWin->clearSavedScreen();

	if (g_settings.screensaver_mode != SCR_MODE_IMAGE)
		g_RadiotextWin->kill(/*COL_BLACK*/); //ensure black paintBackground before repaint

	//check position and size, use only possible available screen size
	int x_rt, y_rt, w_rt, h_rt;
	g_RadiotextWin->getDimensions(&x_rt, &y_rt, &w_rt, &h_rt);
	int rt_x_random = rand_r(&seed[4]) % ((g_settings.screen_EndX - w_rt - g_settings.screen_StartX - OFFSET_SHADOW) + 1) + g_settings.screen_StartX;
	int rt_y_random = rand_r(&seed[5]) % ((g_settings.screen_EndY - h_rt - g_settings.screen_StartY - OFFSET_SHADOW) + 1) + g_settings.screen_StartY;
	g_RadiotextWin->setPos(rt_x_random, rt_y_random);

	g_RadiotextWin->allowPaint(true);

	if (g_RadiotextWin->hasLines())
	{
		if (scr_clock)
			scr_clock->cl_sl_show.block();
		if (enable_paint)
			g_RadiotextWin->CRadioTextGUI::paint(CC_SAVE_SCREEN_YES);
		if (scr_clock)
			scr_clock->cl_sl_show.unblock();
	}

	m_frameBuffer->OnFallbackShowFrame.connect(sigc::mem_fun(g_RadiotextWin, &CRadioTextGUI::hide));
}

void CScreenSaver::hideRadioText()
{
	if (!g_RadiotextWin)
		return;

	g_RadiotextWin->sl_after_decode_line.block();
	
	if (scr_clock)
		scr_clock->cl_sl_show.block();
	
	g_RadiotextWin->kill();
	g_RadiotextWin->hide();
	
	if (scr_clock)
		scr_clock->cl_sl_show.unblock();
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
		|| msg == NeutrinoMessages::EVT_NOEPG_YET
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

CComponentsFrmClock* CScreenSaver::getClockObject()
{
	return scr_clock;
}
