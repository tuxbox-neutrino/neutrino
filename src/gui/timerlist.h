/*
	Neutrino-GUI  -   DBoxII-Project

	Timerliste by Zwen
	
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

#ifndef __timerlist__
#define __timerlist__

#include <timerdclient/timerdtypes.h>

#include <driver/display.h>
#include <gui/widget/menue.h>
#include <gui/widget/listhelpers.h>

#include <string>
#include <vector>

class CFrameBuffer;
class CTimerdClient;
class CTimerList : public CMenuTarget, public CListHelpers
{
	private:
		CFrameBuffer	*frameBuffer;
		int 		x;
		int 		y;
		int 		width;
		int 		height;
		int		font_height;
		int		item_height;
		int		header_height;
		int		footer_height;
		int		selected;
		int		liststart;
		unsigned int	listmaxshow;
		bool		visible;
		CComponentsHeader *header;
		int		httpConnectTimeout;

		CTimerdClient *Timer;
		CTimerd::TimerList timerlist;             // List of timers		
		CTimerd::responseGetTimer timerNew;
		int timerNew_standby_on;
		std::string timerNew_channel_name;
		std::string m_weekdaysStr;
		std::string timerNew_recordingDir;
		std::string timerNew_message;
		std::string timerNew_pluginName;

		int timer_apids_dflt;
		int timer_apids_std;
		int timer_apids_ac3;
		int timer_apids_alt;
		std::string timer_recordingDir;

		void paintItem(int pos);
		void paint();
		void paintHead();
		void paintFoot();
		void hide();
		int modifyTimer();
		int newTimer();
		/* todo: properly import the enum CVFD::MODES */
		CVFD::MODES saved_displaymode;
		bool RemoteBoxSetup();
		void RemoteBoxSelect();
		std::string RemoteBoxConnectUrl(std::string _rbname);
		bool RemoteBoxChanExists(t_channel_id channel_id);
		bool LocalBoxChanExists(t_channel_id channel_id);
		int rem_pre,rem_post;
		int item_offset;
		bool changed;
		int  bselected;
		CMenuWidget *remboxmenu;
		bool askUserOnRemoteTimerConflict(time_t announceTime, time_t stopTime, char * remotebox_ip);

	public:
		CTimerList();
		~CTimerList();
		void updateEvents(void);
		int  show();
		int  exec(CMenuTarget* parent, const std::string & actionKey);
		static const char * convertTimerType2String(const CTimerd::CTimerEventTypes type); // UTF-8
		static std::string convertTimerRepeat2String(const CTimerd::CTimerEventRepeat rep); // UTF-8
		static std::string convertChannelId2String(const t_channel_id id); // UTF-8
		void RemoteBoxTimerList(CTimerd::TimerList &timerlist);
};

bool askUserOnTimerConflict(time_t announceTime, time_t stopTime, t_channel_id channel_id = 0);

#endif
