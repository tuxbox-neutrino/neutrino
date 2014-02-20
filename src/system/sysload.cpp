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
#include <iostream>
#include <fstream>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/vfs.h>
#include <fcntl.h>
#include <time.h>
#include <stdarg.h>
#include <global.h>
#include <system/set_threadname.h>
#include <system/sysload.h>

static cSysLoad *instance = NULL;

cSysLoad *cSysLoad::getInstance(void)
{
	if (!instance)
		instance = new cSysLoad;
	return instance;
}

void *cSysLoad::Run(void *arg)
{
	set_threadname("sysload");

	class cSysLoad *caller = (class cSysLoad *)arg;
	unsigned long stat_idle = 0, stat_total = 0;

	while (caller->running) {
		std::ifstream in("/proc/stat");
		if (in.is_open()) {
			std::string line;
			while (getline(in, line)) {
				unsigned long _stat_user, _stat_nice, _stat_system, _stat_idle;
				if (4 == sscanf(line.c_str(), "cpu %lu %lu %lu %lu", &_stat_user, &_stat_nice, &_stat_system, &_stat_idle)) {
					if (stat_total) {
						unsigned long div = _stat_user + _stat_nice + _stat_system + _stat_idle - stat_total;
						caller->data_last = (int)(1000 - 1000 * (_stat_idle - stat_idle) / div);
						if (caller->data_avail < caller->data_size) {
							caller->data[caller->data_avail++] = caller->data_last;
						} else {
							memmove(caller->data, caller->data + 1, (caller->data_size - 1) * sizeof(int));
							caller->data[caller->data_size - 1] = caller->data_last;
						}
					}
					stat_idle = _stat_idle;
					stat_total = _stat_user + _stat_nice + _stat_system + _stat_idle;
					break;
				}
			}
			in.close();
		}

		timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += caller->period;
		sem_timedwait(&caller->sem, &ts);
	}
	pthread_exit(NULL);
}

cSysLoad::cSysLoad(void)
{
	data_last = -1;
	data_avail = 0;
	period = 5;
	data_size = 1800/period;
	data = new int[data_size];
	for (unsigned int i = 0; i < data_size; i++)
		data[i] = -1;
	running = true;
	sem_init(&sem, 0, 0);
	if (pthread_create(&thr, NULL, Run, this))
		running = false;
}

cSysLoad::~cSysLoad(void)
{
	if (running) {
		running = false;
		sem_post(&sem);
		pthread_join(thr, NULL);
	}
	sem_destroy(&sem);
	delete[] data;
}
