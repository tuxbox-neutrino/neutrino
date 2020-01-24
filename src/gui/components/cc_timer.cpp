/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Generic Timer.
	Copyright (C) 2013-2019, Thilo Graf 'dbt'

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>

#include "cc_timer.h"
#include "cc_types.h"
#include <errno.h>
#include <system/helpers.h>
#include <system/debug.h>
#include <system/set_threadname.h>
#include <mutex>
#include <chrono>

using namespace std;


CComponentsTimer::CComponentsTimer(const int64_t& interval)
{
	tm_thread_name		= string();
	tm_interval 		= interval;
	tm_enable 		= false;
	tm_ticks		= 0;
	sl_cleanup_timer	= sigc::mem_fun(*this, &CComponentsTimer::stopThread);
	CNeutrinoApp::getInstance()->OnBeforeRestart.connect(sl_cleanup_timer);
	CNeutrinoApp::getInstance()->OnShutDown.connect(sl_cleanup_timer);

	tm_thread 		= NULL;
}

CComponentsTimer::~CComponentsTimer()
{
	stopThread();
}

void CComponentsTimer::threadCallback(CComponentsTimer *tm)
{
	if (!tm)
		return;

	tm->tn = "cc.timer:" + tm->tm_thread_name;
	set_threadname(tm->tn.c_str());

	if (tm->OnTimer.empty())
		dprintf(DEBUG_NORMAL," \033[36m[CComponentsTimer] [%s - %d] Signal OnTimer for %s is empty, no callback defined \033[0m\n", __func__, __LINE__, tm->tn.c_str());

	dprintf(DEBUG_DEBUG,"\033[32m[CComponentsTimer] thread [%p] [%s] [%s - %d] loop start \033[0m\n", tm->tm_thread, tm->tn.c_str(), __func__, __LINE__);

#if HAVE_COOL_HARDWARE //time offset
	const int64_t MAX_COUNT = tm->tm_interval  / 10;
#else
	const int64_t MAX_COUNT = tm->tm_interval;
#endif
	int64_t i = 0;

	TIMER_START();
	tm->tm_ticks = 0;
	while(tm->tm_enable) //exit loop handled in destructor
	{
		if (tm->tm_interval > 0)
		{
			for (i = 0; i < MAX_COUNT; i++)
				i += tm->tm_interval;
			i = 0;

			if (!tm->OnTimer.blocked())
			{
				tm->OnTimer();
				while (i < MAX_COUNT)
				{
					if (tm->tm_enable)
						this_thread::sleep_for(std::chrono::milliseconds(1));
					i++;
				}
			}
			tm->tm_ticks ++;
		}
	}
	dprintf(DEBUG_DEBUG,"\033[32m[CComponentsTimer] thread [%p] [%s] [%s - %d] loop/callback finished\033[0m\n ", tm->tm_thread, tm->tn.c_str(), __func__, __LINE__);
}


//start up running timer with own thread
void CComponentsTimer::initThread()
{
	if (!tm_thread)
	{
		tm_enable = true;
		tm_thread = new std::thread (threadCallback, this);
		tm_mutex.lock();
	}
}

void CComponentsTimer::stopThread()
{
	if (tm_thread) {
		tm_enable = false;
		tm_mutex.unlock();
		tm_thread->join();
		dprintf(DEBUG_DEBUG,"\033[32m[CComponentsTimer] thread [%p] [%s] [%s - %d] joined\033[0m\n", tm_thread, tn.c_str(), __func__, __LINE__);

		delete tm_thread;
		tm_thread = NULL;
		tn.clear();
		dprintf(DEBUG_DEBUG,"\033[32m[CComponentsTimer] thread [%p] [%s] [%s - %d] thread object terminated after %llu ticks\033[0m\n", tm_thread, tn.c_str(), __func__, __LINE__, tm_ticks);
	}
}

void CComponentsTimer::startTimer()
{
	initThread();
}

void CComponentsTimer::stopTimer()
{
	stopThread();
}

bool CComponentsTimer::isRun() const
{
	bool ret = tm_enable && tm_thread;
	return ret;
}

void CComponentsTimer::setTimerInterval(const int64_t& interval)
{
	if (tm_interval == interval)
		return;

	stopTimer();
	tm_interval 	= interval;
	startTimer();
}
