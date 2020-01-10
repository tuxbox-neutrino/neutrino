/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
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

#ifndef __CC_TIMER__
#define __CC_TIMER__


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sigc++/signal.h>
#include <sigc++/adaptors/retype_return.h>
#include <thread>
#include <mutex>

/**CComponentsTimer
* 	Member of CComponents. Provides a generic timer class
* @see
* 	CComponentsTimer()
*/
class CComponentsTimer : public sigc::trackable
{
	private:
		///thread
		std::thread  *tm_thread;
		std::mutex tm_mutex;
		///flag to control thread state
		bool tm_enable;

		///refresh interval in seconds
		int64_t tm_interval;

		///runs shared timer action provided inside OnTimer() signal
		static void threadCallback(CComponentsTimer *tm);

		sigc::slot<void> sl_cleanup_timer;

		std::string tn;

	protected: ///name for thread
		std::string tm_thread_name;

		///init function to start/stop timer in own thread
		void initThread();
		void stopThread();

	public:
		/**Constructor for timer class
		*
		* @param[in]  interval
		* 	@li int64_t interval in miliseconds, default value=1000 ms (1 sec)
		* @see
		* 	setTimerInterval();
		*/
		CComponentsTimer(const int64_t& interval = 1000);

		~CComponentsTimer();

		/**Acivate timer
		* @see
		* 	stopTimer()
		*/
		void startTimer();

		/**Disable timer
		* @see
		* 	startTimer()
		*/
		void stopTimer();

		/**Cancel timer thread
		* @see
		* 	stopTimer()
		*/
		void cancelTimerThread(){stopThread();}

		/**get current timer status
		* @return
		*	bool
		*	returns true, if timer is active
		* @see
		* 	startTimer()
		* 	stopTimer()
		*/
		bool isRun() const;

		/**set timer interval
		* @param[in]  interval
		* 	@li int64_t default interval in miliseconds
		* @return
		*	void
		* @see
		* 	tm_interval
		*/
		void setTimerInterval(const int64_t& interval);

		/**set thread name
		* @param[in] thread name
		* @return
		* 	void
		*/
		void setThreadName(const std::string& n) { tm_thread_name = tn = n; }

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
