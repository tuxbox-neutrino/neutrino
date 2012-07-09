/*
 * sectionsd.h (network daemon for SI-sections)
 * (dbox-II-project)
 *
 * (C) 2001 by fnbrd (fnbrd@gmx.de)
 *
 * Copyright (C) 2011-2012 CoolStream International Ltd
 *
 * License: GPLv2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __sectionsd_h__
#define __sectionsd_h__

#include <sys/time.h>

#include <OpenThreads/Thread>
#include <OpenThreads/Condition>
#include <sectionsdclient/sectionsdclient.h>
#include <connection/basicserver.h>
#include "dmx.h"

//#include "SIutils.hpp"
//#include "SIservices.hpp"
//#include "SIevents.hpp"
//#include "SIsections.hpp"
//#include "SIlanguage.hpp"

class CEitManager : public OpenThreads::Thread, public OpenThreads::Mutex
{
	private:
		bool	running;
		CSectionsdClient::epg_config config;
		CBasicServer sectionsd_server;

		static OpenThreads::Mutex m;
		static CEitManager * manager;

		CEitManager();
		/* main thread function */
		void run();
        public:
		~CEitManager();

		static CEitManager * getInstance();

		bool Start();
		bool Stop();
		void SetConfig(CSectionsdClient::epg_config &cfg) { config = cfg; };
};

#endif
