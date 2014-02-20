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

#ifndef __SYSTEM_SYSLOAD__H_
#define __SYSTEM_SYSLOAD__H_
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string>
#include <vector>
#include <pthread.h>
#include <semaphore.h>

class cSysLoad
{
	private:
		pthread_t thr;
		cSysLoad();
		static void* Run(void *);
	public:
		int *data;
		size_t data_avail;
		size_t data_size;
		unsigned int period;
		bool running;
		sem_t sem;
		~cSysLoad(void);
		static cSysLoad *getInstance(void);
		int data_last;
};
#endif
