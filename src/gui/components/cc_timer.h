/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2013-2015, Thilo Graf 'dbt'

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
#include <OpenThreads/ScopedLock>
#include <OpenThreads/Thread>
#include <OpenThreads/Condition>

/**CComponentsTimer
* 	Member of CComponents. Provides a generic timer class
* @see
* 	CComponentsTimer()
*/
class CComponentsTimer : public sigc::trackable
{
	private:
		///thread
		pthread_t  tm_thread;

		///refresh interval in seconds
		int tm_interval;
		
		bool tm_enable_nano;

		///init function to init shared timer action
		static void* initThreadAction(void *arg);

		///init function to start/stop timer in own thread
		void initThread();
		void stopThread();

		///runs shared timer action provided inside OnTimer() signal
		void runSharedTimerAction();

		///flag to control thread state
		bool tm_enable;

		///name for the thread
		std::string name;
		std::string tn;
		///mutex for timer
		OpenThreads::Mutex tm_mutex;
		///slot for restart signals
		sigc::slot0<bool> sl_stop_timer;

		///sleep generated with nanosleep
		int getSleep(long miliseconds);
	public:
		/**Constructor for timer class
		*
		* @param[in]  interval
		* 	@li int interval in seconds, default value=1 (1 sec)
		* 	If init value for interval > 0, timer starts immediately
		* 	@li bool default = false as seconds mode, true = nano seconds mode
		* @see
		* 	setTimerInterval();
		*/
		CComponentsTimer(const int& interval = 1, bool is_nano = false);

		~CComponentsTimer();

		/**Starts timer thread
		* @return
		*	bool
		*	returns true, if timer is running in thread
		* @see
		* 	stopTimer()
		*/
		bool startTimer();

		/**Stops timer thread
		* @return
		*	bool
		*	returns true, if timer thread stopped
		* @see
		* 	startTimer()
		*/
		bool stopTimer();

		/**get current timer status
		* @return
		*	bool
		*	returns true, if timer is running in thread
		* @see
		* 	startTimer()
		* 	stopTimer()
		*/
		bool isRun() const {return tm_thread;};

		/**set timer interval
		* @param[in]  interval
		* 	@li int default interval in seconds, if second parameter = true interval is used as nano seconds
		* 	@li bool default = false as seconds mode, true = nano seconds mode
		* @return
		*	void
		* @see
		* 	tm_interval
		*/
		void setTimerInterval(const int& interval, bool is_nano = false);

		/**set thread name
		* @param[in] thread name
		* @return
		* 	void
		*/
		void setThreadName(const std::string& n) { name = tn = n; }

		/**Provides a signal handler to receive any function or methode.
		* Use this in your class where ever you need time controled actions.
		*
		* @param[in]  seconds
		* 	@li int
		* @see
		* 	CComponentsSignals for similar handlings.
		* 	CComponentsFrmClock::startClock()
		*/
		sigc::signal<void> OnTimer;
};

#endif
