/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Generic Timer.
	Copyright (C) 2013-2016, Thilo Graf 'dbt'

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
#include <pthread.h>
#include <errno.h>
#include <system/helpers.h>
#include <system/debug.h>
#include <system/set_threadname.h>

using namespace std;

CComponentsTimer::CComponentsTimer(const int& interval, bool is_nano)
{
	name			= "unnamed";
	tm_thread 		= 0;
	tm_interval 		= interval;
	tm_enable_nano		= is_nano;
	tm_enable 		= false;
	sl_stop_timer 		= sigc::mem_fun(*this, &CComponentsTimer::stopTimer);

	if (interval > 0)
		tm_enable = startTimer();
}

CComponentsTimer::~CComponentsTimer()
{
	stopTimer();
}

int CComponentsTimer::getSleep(long miliseconds)
{
   struct timespec req, rem;

	if(miliseconds > 999){
		req.tv_sec = (time_t)(miliseconds / 1000);
		req.tv_nsec = (miliseconds - ((long)req.tv_sec * 1000)) * 1000000;
	}else{
		req.tv_sec = 0;
		req.tv_nsec = miliseconds * 1000000;
	}

	return nanosleep(&req , &rem);
}

void CComponentsTimer::runSharedTimerAction()
{
	//start loop
	tn = "cc:"+name;
	set_threadname(tn.c_str());
	while(tm_enable && tm_interval > 0 && !OnTimer.empty()) {
		tm_mutex.lock();
		OnTimer();
		if (!tm_enable_nano){
			sleep(tm_interval);
		}else{
			//behavior is different on cst hardware
			long corr_factor = 1;
#if ! HAVE_COOL_HARDWARE 
			corr_factor = 10;
#endif
			int res = getSleep(tm_interval * corr_factor);
			if (res != 0)
				dprintf(DEBUG_NORMAL,"\033[33m[CComponentsTimer] [%s - %d] ERROR: returns [%d] \033[0m\n", __func__, __LINE__, res);
		}
		tm_mutex.unlock();
	}

	if (tm_thread)
		stopThread();
}

//thread handle
void* CComponentsTimer::initThreadAction(void *arg)
{
	CComponentsTimer *timer = static_cast<CComponentsTimer*>(arg);

	timer->runSharedTimerAction();

	return 0;
}

//start up running timer with own thread, return true on succses
void CComponentsTimer::initThread()
{
	if(!tm_thread) {
		void *ptr = static_cast<void*>(this);

		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,0);
		pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS,0);

		int res = pthread_create (&tm_thread, NULL, initThreadAction, ptr);

		if (res != 0){
			dprintf(DEBUG_NORMAL,"\033[33m[CComponentsTimer] [%s - %d] ERROR! pthread_create\033[0m\n", __func__, __LINE__);
			return;
		}else
			dprintf(DEBUG_DEBUG,"\033[33m[CComponentsTimer] [%s - %d] started thread ID:%ld \033[0m\n", __func__, __LINE__, pthread_self());

		if (res == 0)
			CNeutrinoApp::getInstance()->OnBeforeRestart.connect(sigc::retype_return<void>(sl_stop_timer));
	}
}

void CComponentsTimer::stopThread()
{
	//ensure disconnecting possible slots
	while (!sl_stop_timer.empty())
		sl_stop_timer.disconnect();

	while(tm_thread) {
		int thres = pthread_cancel(tm_thread);
		if (thres != 0)
			dprintf(DEBUG_NORMAL,"\033[33m[CComponentsTimer] [%s - %d] ERROR! pthread_cancel, error [%d] %s\033[0m\n", __func__, __LINE__, thres, strerror(thres));

		void* res;
		thres = pthread_join(tm_thread, &res);

		if (res != PTHREAD_CANCELED)
			dprintf(DEBUG_NORMAL, "\033[33m[CComponentsTimer] [%s - %d] ERROR! pthread_join, thread ID:%ld, error [%d] %s\033[0m\n", __func__, __LINE__, pthread_self(), thres, strerror(thres));
		else
			tm_thread = 0;
	}
}

bool CComponentsTimer::startTimer()
{
	initThread();
	if(tm_thread)
		tm_enable = true;

	return tm_enable;
}

bool CComponentsTimer::stopTimer()
{
	tm_enable = false;
	stopThread();
	if(tm_thread == 0){
		if (!OnTimer.empty())
			OnTimer.clear();
		return true;
	}

	return false;
}

void CComponentsTimer::setTimerInterval(const int& interval, bool is_nano)
{
	if (tm_interval == interval && tm_enable_nano == is_nano)
		return;

	tm_enable_nano	= is_nano;
	tm_interval 	= interval;
}
