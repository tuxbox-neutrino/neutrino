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


#ifndef __infoview__
#define __infoview__

#include <sectionsdclient/sectionsdclient.h>

#include <driver/rcinput.h>
#include <driver/framebuffer.h>
#include <driver/fontrenderer.h>
#include <driver/fade.h>
#include <system/settings.h>
#include "widget/menue.h"
#include <gui/widget/progressbar.h>
#include <string>

class CInfoViewer
{
 private:
	void Init(void);
	CFrameBuffer * frameBuffer;
	
	bool           gotTime;
	bool           recordModeActive;
#ifndef SKIP_CA_STATUS
	bool           CA_Status;
#endif
	
	int            InfoHeightY;
	int            InfoHeightY_Info;
	bool           showButtonBar;
	bool	       fileplay;

	int            BoxEndX;
	int            BoxEndY;
	int            BoxStartX;
	int            BoxStartY;
	int            ButtonWidth;

        // dimensions of radiotext window
        int             rt_dx;
        int             rt_dy;
        int             rt_x;
        int             rt_y;
        int             rt_h;
        int             rt_w;
	
	std::string ChannelName;

	int            ChanNameX;
	int            ChanNameY;
	int            ChanWidth;
	int            ChanHeight;
	int            ChanInfoX;

	/* the position of the button bar */
	int            BBarY;
	int            BBarIconY;
	int            BBarFontY;

	int		asize;
	int		icol_w, icol_h;
	int 		icon_large_width, icon_small_width, icon_xres_width, icon_crypt_width;
	CSectionsdClient::CurrentNextInfo info_CurrentNext;
        t_channel_id   channel_id;

	char           aspectRatio;

	uint32_t           sec_timer_id;
	//uint32_t           fadeTimer;
	COSDFader	fader;

	int time_left_width;
	int time_dot_width;
	int time_width;
	int time_height;
	int info_time_width;
	int bottom_bar_offset;

	bool newfreq ;
	char old_timestr[10];
	static const short bar_width = 72;
	static event_id_t last_curr_id, last_next_id;
	uint64_t timeoutEnd;
	void setInfobarTimeout(int timeout_ext = 0);

	CChannelEventList               evtlist;
	CChannelEventList::iterator     eli;

	int lastsnr, lastsig, lasthdd, lastvar, lasttime;
	CProgressBar *snrscale, *sigscale, *hddscale, *varscale, *timescale;
	int hddwidth;
	bool casysChange;
	bool channellogoChange;
	void paintBackground(int col_Numbox);
	void show_Data( bool calledFromEvent = false );
	void display_Info(const char *current, const char *next, bool UTF8 = true,
			  bool starttimes = true, const int pb_pos = -1,
			  const char *runningStart = NULL, const char *runningRest = NULL,
			  const char *nextStart = NULL, const char *nextDuration = NULL,
			  bool update_current = true, bool update_next = true);
	void paintTime( bool show_dot, bool firstPaint );
	
	void showButton_Audio();
	void showButton_SubServices();
	
	void showIcon_16_9();
	void showIcon_RadioText(bool rt_available) const;
	void showIcon_CA_Status(int);
	void paint_ca_icons(int, char*, int&);
	void paintCA_bar(int,int);
	void showOne_CAIcon(bool);

	void showIcon_VTXT()      const;
	void showRecordIcon(const bool show);
	void showIcon_SubT() const;
	void showIcon_Resolution() const;

	void showFailure();
	void showMotorMoving(int duration);
   	void showLcdPercentOver();
	int showChannelLogo(const t_channel_id logo_channel_id, const int channel_number_width);
	void showSNR();
	void showRadiotext();
	void killRadiotext();
	void showInfoFile();
	//void loop(int fadeValue, bool show_dot ,bool fadeIn);
	void loop(bool show_dot);
	std::string eventname;
	void paintshowButtonBar();
	void show_current_next(bool new_chan, int  epgpos);
	void reset_allScala();
	void check_channellogo_ca_SettingsChange();
 public:
	bool chanready;
	bool	is_visible;
	bool	virtual_zap_mode;
	uint32_t    lcdUpdateTimer;

	CInfoViewer();
	void	showMovieTitle(const int playState, const std::string &title,
				const std::string &g_file_epg, const std::string &g_file_epg1,
				const int duration, const int curr_pos);

	void	start();
	void	showEpgInfo();
	void	showTitle(const int ChanNum, const std::string & Channel, const t_satellite_position satellitePosition, const t_channel_id new_channel_id = 0, const bool calledFromNumZap = false, int epgpos = 0); // Channel must be UTF-8 encoded
	void lookAheadEPG(const int ChanNum, const std::string & Channel, const t_channel_id new_channel_id = 0, const bool calledFromNumZap = false); //alpha: fix for nvod subchannel update
	void	killTitle();
	void	getEPG(const t_channel_id for_channel_id, CSectionsdClient::CurrentNextInfo &info);
	CSectionsdClient::CurrentNextInfo getCurrentNextInfo() const { return info_CurrentNext; }
	
	void	showSubchan();
	void	Set_CA_Status(int Status);
	
	int     handleMsg(const neutrino_msg_t msg, neutrino_msg_data_t data);
	void    clearVirtualZapMode() {virtual_zap_mode = false;}
	void changePB();
	bool SDT_freq_update;
};

class CInfoViewerHandler : public CMenuTarget
{
	public:
		int  exec( CMenuTarget* parent,  const std::string &actionkey);
		int  doMenu();

};
#endif
