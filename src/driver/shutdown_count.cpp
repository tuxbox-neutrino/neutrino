/*
	Shutdown Counter  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/


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

#include <driver/shutdown_count.h>

#include <global.h>
#include <neutrino.h>
#include <system/settings.h>

#include <sys/timeb.h>
#include <time.h>


SHTDCNT::SHTDCNT()
	: configfile('\t')
{
}

SHTDCNT* SHTDCNT::getInstance()
{
	static SHTDCNT* shtdcnt = NULL;
	if(shtdcnt == NULL)
	{
		shtdcnt = new SHTDCNT();
	}
	return shtdcnt;
}

void* SHTDCNT::TimeThread(void *)
{
	while(1)
	{
		sleep(1);
		SHTDCNT::getInstance()->shutdown_counter();
	}
	return NULL;
}

void SHTDCNT::init()
{
	if (pthread_create (&thrTime, NULL, TimeThread, NULL) != 0 )
	{
		perror("[SHTDCNT]: pthread_create(TimeThread)");
		return ;
	}
}

void SHTDCNT::shutdown_counter()
{
	if (atoi(g_settings.shutdown_count) > 0)
	{
		if ((CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_standby) && (!CNeutrinoApp::getInstance ()->recordingstatus))
		{
			if (shutdown_cnt > 0 )
			{
				shutdown_cnt = shutdown_cnt - 1;
				printf("[SHTDCNT] shutdown counter: %d sec to shutdown\n", shutdown_cnt);
			}
			else
			{
			// send shutdown message
			printf("[SHTDCNT] shutdown counter send NeutrinoMessages::SHUTDOWN\n");
			CNeutrinoApp::getInstance()->handleMsg( NeutrinoMessages::SHUTDOWN, 0);
			}
		}
		else
		{
			// reset counter
			shutdown_cnt = atoi(g_settings.shutdown_count) * 60;
		}
	}
}
