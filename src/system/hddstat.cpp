/*
	Neutrino-HD

	License: GPL

	(C) 2013 martii

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

#ifndef __USE_FILE_OFFSET64
#define __USE_FILE_OFFSET64 1
#endif

#include <string>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/vfs.h>
#include <fcntl.h>
#include <time.h>
#include <stdarg.h>
#include <global.h>
#include "hddstat.h"
#include <gui/widget/menue.h>
//#include <system/ytcache.h>
#include <system/set_threadname.h>
#include <driver/record.h>
#include <driver/display.h>

cHddStat *cHddStat::getInstance(void)
{
	static cHddStat *instance = NULL;

	if (!instance)
		instance = new cHddStat;
	return instance;
}

void cHddStat::setDir(std::string &_dir)
{
	pthread_mutex_lock(&mutex);
	dir = _dir;
	pthread_mutex_unlock(&mutex);
}

void *cHddStat::Run(void *arg)
{
	set_threadname("hddstat");
	class cHddStat *caller = (class cHddStat *)arg;

	timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	long long oldperc = -2;
	caller->once = g_settings.hdd_statfs_mode != SNeutrinoSettings::HDD_STATFS_OFF;
	while (caller->running) {
		std::string _dir;
		pthread_mutex_lock(&caller->mutex);
		_dir = caller->dir;
		pthread_mutex_unlock(&caller->mutex);
		long long perc = -1;

		if (caller->once || (g_settings.hdd_statfs_mode == SNeutrinoSettings::HDD_STATFS_ALWAYS)
		  || (g_settings.hdd_statfs_mode == SNeutrinoSettings::HDD_STATFS_RECORDING && (CRecordManager::getInstance()->RecordingStatus() /* || cYTCache::getInstance()->isActive() */ ))) {
			caller->once = false;
			struct statfs st;
			if (statfs(_dir.c_str(), &st) || !st.f_blocks)
				perc = -1;
			else
				perc = 100 * (long long)(st.f_blocks - st.f_bfree) / (long long) st.f_blocks;
		} else
			perc = oldperc;

		if (oldperc != perc) {
			oldperc = perc;
			caller->percent = (int) perc;
			//CVFD::getInstance()->setHddUsage(perc);
		}
		ts.tv_sec += caller->period;
		sem_timedwait(&caller->sem, &ts);
	}
	pthread_exit(NULL);
}

cHddStat::cHddStat(void)
{
	dir = g_settings.network_nfs_recordingdir;
	period = 60;
	percent = -1;
	running = true;
	sem_init(&sem, 0, 0);
	pthread_mutex_init(&mutex, NULL);
	if (pthread_create(&thr, NULL, Run, this))
		running = false;
}

cHddStat::~cHddStat(void)
{
	if (running) {
		running = false;
		sem_post(&sem);
		pthread_join(thr, NULL);
	}
	sem_destroy(&sem);
}

void cHddStat::statOnce(void)
{
	once = true;
	sem_post(&sem);
}
