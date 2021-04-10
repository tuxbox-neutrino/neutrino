/*
	update-check

	Copyright (C) 2017 'vanhofen'
	Homepage: http://www.neutrino-images.de/

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

#include <pthread.h>
#include <unistd.h>

#include <global.h>
#include <neutrino.h>

#include <system/set_threadname.h>
#include <gui/update.h>

#include "update_check.h"

#define C4U_SLEEP (6 * 60 * 60) // 6 hours
#define C4U_FLAGFILE FLAGDIR "/.update"

/* ----------------------------------------------------------------- */

CFlashUpdateCheck::CFlashUpdateCheck()
{
	c4u_thread = 0;
}

CFlashUpdateCheck::~CFlashUpdateCheck()
{
	stopThread();
}

CFlashUpdateCheck* CFlashUpdateCheck::getInstance()
{
	static CFlashUpdateCheck * c4u = NULL;
	if (!c4u)
		c4u = new CFlashUpdateCheck();

	return c4u;
}

void CFlashUpdateCheck::startThread()
{
	if (!c4u_thread)
	{
		printf("[CFlashUpdateCheck]: starting thread\n");
		pthread_create(&c4u_thread, NULL, c4u_proc, (void*) NULL);
		pthread_detach(c4u_thread);
	}
}

void CFlashUpdateCheck::stopThread()
{
	if (c4u_thread)
	{
		printf("[CFlashUpdateCheck]: stopping thread\n");
		pthread_cancel(c4u_thread);
	}
	c4u_thread = 0;

	if (access(C4U_FLAGFILE, F_OK) == 0)
		unlink(C4U_FLAGFILE);
}

void* CFlashUpdateCheck::c4u_proc(void*)
{
	set_threadname("n:flashupdatecheck");

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);

	CFlashUpdate flashupdate;

	//printf("[CFlashUpdateCheck] %s: starting loop\n", __FUNCTION__);
	while(1)
	{
		//printf("[CFlashUpdateCheck]: check for updates\n");
		if (flashupdate.checkOnlineVersion())
		{
			//printf("[CFlashUpdateCheck]: update available\n");
			if (FILE *f = fopen(C4U_FLAGFILE, "w"))
				fclose(f);
		}
		else
		{
			if (access(C4U_FLAGFILE, F_OK) == 0)
				unlink(C4U_FLAGFILE);
		}

		sleep(C4U_SLEEP);
	}
	return 0;
}
