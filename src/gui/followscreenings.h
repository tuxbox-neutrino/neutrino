/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Copyright (C) 2013 martii
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


#ifndef __followscreening_
#define __followscreening_

#include "widget/menue.h"
#include <sys/time.h>
#include <string.h>
#include <timerdclient/timerdclient.h>
#include <sectionsdclient/sectionsdclient.h>

class CFollowScreenings : public CMenuTarget
{
	private:
		CChannelEventList *evtlist;
		CChannelEventList followlist;
		CTimerdClient Timer;
		t_channel_id channel_id;
		time_t starttime;
		time_t stoptime;
		std::string title;
		uint64_t epg_id;
		unsigned char apids;
		bool safety;
		std::string recDir;
		CTimerd::RecordingInfo eventInfo;
		std::vector<CMenuForwarder *> forwarders;
		void updateRightIcon(int i, time_t start, unsigned int duration);
	public:
		enum
		{
			FOLLOWSCREENINGS_OFF	= 0,
			FOLLOWSCREENINGS_ON	= 1,
		};

                CFollowScreenings(const t_channel_id Channel_id, time_t Starttime, time_t Stoptime, const std::string &Title, uint64_t EpgID=0,
			unsigned char Apids=TIMERD_APIDS_STD, bool Safety=false, std::string RecDir="", CChannelEventList *Evtlist=NULL) : CMenuTarget () {
			this->channel_id = Channel_id;
			this->starttime = Starttime;
			this->stoptime = Stoptime;
			this->epg_id = EpgID;
			this->recDir = RecDir;
			this->evtlist = Evtlist;
			this->title = Title;
			this->safety = Safety;
			this->apids = Apids;
		};
		~CFollowScreenings();
		CChannelEventList *getFollowScreenings(void);
		int exec(CMenuTarget *parent, const std::string & actionKey);
		void show();
};
#endif

