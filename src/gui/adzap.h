/*
	adzap.h

	(C) 2012-2013 by martii
	(C) 2016 Sven Hoefer (svenhoefer)

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

#ifndef __adzap__
#define __adzap__

#include "widget/menue.h"
#include <zapit/channel.h>
#include <system/localize.h>
#include <string>
#include <semaphore.h>

class CAdZapMenu: public CMenuTarget, CChangeObserver
{
	private:
		CFrameBuffer * frameBuffer;
		int width;
		bool armed;
		bool alerted;
		bool monitor;
		struct timespec ts;
		struct timespec zapBackTime;
		std::string channelName;
		CMenuForwarder *forwarders[9];
		CMenuOptionNumberChooser *nc;
		CChannelEventList evtlist;
		struct timespec monitorLifeTime;
		t_channel_id channelId;
		sem_t sem;
		CAdZapMenu();
		void Init();
		time_t getMonitorLifeTime();
		void ShowMenu();
		void Update();
		void Run(void);
		void WriteData(void);
		void RemoveData(void);
		static void *Run(void *arg);
	public:
		static CAdZapMenu *getInstance();
		int exec(CMenuTarget * parent, const std::string & actionKey);
		bool changeNotify(const neutrino_locale_t, void * data);
		bool isActive() { return (armed || monitor); };
		void Zap_On_Activation(t_channel_id channel_id);
};
#endif // __adzap__
