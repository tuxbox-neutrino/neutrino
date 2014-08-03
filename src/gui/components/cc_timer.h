/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2013, Thilo Graf 'dbt'

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

#ifndef __CC_TIMER__
#define __CC_TIMER__


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sigc++/signal.h>

//! Member of CComponents. Provides a generic timer class
/*!

*/

class CComponentsTimer : public sigc::trackable
{
	private:
		///thread
		pthread_t  tm_thread;
		///refresh interval in seconds
		int tm_interval;
		///init function to start timer in own thread
		static void* initTimerThread(void *arg);

	public:
		///class constructor, parameter interval sets the interval in seconds, default value=1 (1 sec)
		CComponentsTimer(const int& interval = 1);
		~CComponentsTimer();

		///start timer thread, returns true on success, if false causes log output
		bool startTimer();
		///stop timer thread, returns true on success, if false causes log output
		bool stopTimer();

		///returns true, if timer is running in thread
		bool isRun() const {return tm_thread == 0 ? false:true;};
		///set another interval in seconds
		void setTimerIntervall(const int& seconds){tm_interval = seconds;};

		///signal for timer event, use this in your class where ever you need time controled actions
		///for more details see also CComponentsSignals for similar handlings
		sigc::signal<void> OnTimer;
};

#endif
