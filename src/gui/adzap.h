/*
 * adzap.h
 * 
 * (C)2012 by martii
 * 
 * License: GPL
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#ifndef __adzap__
#define __adzap__

#include "widget/menue.h"
#include <zapit/channel.h>
#include <system/localize.h>
#include <string>
#include <semaphore.h>

class CAdZapMenu: public CMenuTarget
{
	private:
		CFrameBuffer * frameBuffer;
		int x;
		int y;
		int width;
		int height;
		int hheight, mheight;	// head/menu font height
		bool running;
		bool armed;
		bool alerted;
		bool monitor;
		struct timespec zapBackTime;
		std::string channelName;
		CMenuForwarder *forwarders[9];
		struct timespec monitorLifeTime;
		t_channel_id channelId;
		sem_t sem;
		CAdZapMenu();
		void Settings();
		void Update();
		void Run(void);
		static void *Run(void *arg);
	public:
		static CAdZapMenu *getInstance();
		int exec(CMenuTarget * parent, const std::string & actionKey);
};
#endif				// __adzap__
