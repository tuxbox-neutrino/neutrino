/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Implementation of CComponent Window class.
	Copyright (C) 2014-2017 Thilo Graf 'dbt'

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


#ifndef __progresswindow__
#define __progresswindow__

#include <gui/components/cc.h>
#include "menue.h"

#define PW_MIN_WIDTH  CCW_PERCENT 50
#define PW_MIN_HEIGHT CCW_PERCENT 20

class CProgressWindow : public CComponentsWindow, public CMenuTarget
{
	private:
		CProgressBar *local_bar, *global_bar;
		CComponentsLabel *status_txt;

		unsigned int global_progress;
		unsigned int local_progress;
		std::string cur_statusText;
		int h_height;

		CProgressBar* getProgressItem();
		void initStatus(const unsigned int prog, const unsigned int max, const std::string &statusText, CProgressBar *pBar);
		void fitItems();

	protected:
		size_t global_max, internal_max;

		void Init(	sigc::signal<void, size_t, size_t, std::string> *statusSignal,
				sigc::signal<void, size_t, size_t, std::string> *localSignal,
				sigc::signal<void, size_t, size_t, std::string> *globalSignal,
				sigc::signal<void, size_t> *globalSet);

	public:
		/**CProgressWindow Constructor
		* @param[in]	parent
		* 	@li 	optional: expects type CComponentsForm * as parent object, default = NULL
		* @param[in]	dx
		* 	@li 	optional: expects type const &int, width of window, default = 0, auto size with declared default values
		* @param[in]	dy
		* @param[in]	localSignal
		* 	@li 	optional: expects pointer of to sigc::signal<void, size_t, size_t, std::string>, defines an optional signal container for
		* 		current changing local values.
		* @param[in]	globalSignal
		* 	@li 	optional: expects pointer of to sigc::signal<void, size_t, size_t, std::string>, defines an optional signal container for
		* 		current changing global values.
		*/
		CProgressWindow(CComponentsForm *parent = NULL,
				const int &dx = PW_MIN_WIDTH,
				const int &dy = PW_MIN_HEIGHT,
				sigc::signal<void,size_t, size_t, std::string> *localSignal = NULL,
				sigc::signal<void, size_t, size_t, std::string> *globalSignal = NULL);

		/**CProgressWindowA Constructor
		* @see		For common arguments and examples, see related constructor(s)\n
		* 		For general parameters see basic class CProgressWindow!
		*/
		CProgressWindow(const neutrino_locale_t title,
				const int &dx = PW_MIN_WIDTH,
				const int &dy = PW_MIN_HEIGHT,
				sigc::signal<void,size_t, size_t, std::string> *localSignal = NULL,
				sigc::signal<void, size_t, size_t, std::string> *globalSignal = NULL);

		/**CProgressWindow Constructor
		* @see		For common arguments and examples, see related constructor(s)\n
		* 		For general parameters see basic class CProgressWindow!
		*/
		CProgressWindow(const std::string &title,
				const int &dx = PW_MIN_WIDTH,
				const int &dy = PW_MIN_HEIGHT,
				sigc::signal<void,size_t, size_t, std::string> *localSignal = NULL,
				sigc::signal<void, size_t, size_t, std::string> *globalSignal = NULL);

		/**Sets titel of window
		* @param[in]	title
		* 	@li 	expects type neutrino_locale_t as window title
		*/
		void setTitle(const neutrino_locale_t title);

		/**Sets titel of window
		* @param[in]	title
		* 	@li 	expects type std::string as window title
		*/
		void setTitle(const std::string & title);

		/**
		* Remove window from screen, restores background.
		*/
		virtual void hide();

		/**
		* Executes the exec contents. In this case it will paint the window and remove possible parents from screen
		* @param[in]	parent
		* 	@li 	optional: expects type CMenuTarget*
		* @param[in]	actionKey
		* 	@li 	optional: without effect
		* @return	int = menu_return::RETURN_REPAINT
		*/
		virtual int exec( CMenuTarget* parent, const std::string & actionKey );

		/**
		* Sets current local progressbar value and show progress in window.
		* @note		For other arguments take a look to related method showStatus()
		* @see		showStatus()
		*/
		void showLocalStatus(const unsigned int prog, const unsigned int max = 100, const std::string &statusText = std::string());

		/**
		* Sets current global progressbar value and show progress in window.
		* @note		For other arguments take a look to related method showStatus()
		* @see		showStatus()
		*/
		void showGlobalStatus(const unsigned int prog, const unsigned int max = 100, const std::string &statusText = std::string());

		/**
		* Gets current progress value
		* @return	unsigned int
		*/
		unsigned int getGlobalStatus(void);

		/**
		* Sets current progress value and shows a single progress bar inside window as default, but allows
		* to show a global bar too. If you want to show a global bar, the global max value must be predefined with setGlobalMax() method.
		* A defined global max value causes a gentle display of global progress. This assumes, the numbering fits to the scaned progress.
		* @param[in]	prog
		* 	@li 	expects type unsigned int, describes current progress value
		* @param[in]	max
		* 	@li 	optional: expects type unsigned int, describes maximal progress value, default = 100
		* @param[in]	statusText
		* 	@li 	optional: expects type std::string, describes current status text, default = empty
		*
		* @see		setGlobalMax(), CProgressSignals::OnSetGlobalMax()
		*/
		void showStatus(const unsigned int prog, const unsigned int max = 100, const std::string &statusText = std::string());

		/**
		* Sets current maximal value for global progress in window.
		* @param[in]	global_Max
		* 	@li 	expects type size_t
		*/
		void setGlobalMax(const size_t& global_Max){global_max = global_Max;}

		/**
		* Sets current progress value and show progress in window.
		* @param[in]	text
		* 	@li 	expects type std::string, describes current status text
		*/
		void showStatusMessageUTF(const std::string & text); // UTF-8

		/**
		* Paint window
		* @param[in]	do_save_bg
		* 	@li 	optional: expects type bool, sets background save mode
		*/
		void paint(const bool &do_save_bg = true);
};

class CProgressWindowA : public CProgressWindow
{
	public:
		/**CProgressWindowA Constructor
		* @see		For common arguments and examples, see related constructor(s)\n
		* 		For general parameters see basic class CProgressWindow!
		*
		* @param[in]	status_Signal
		* 	@li 	optional: expects pointer of type sigc::signal<void, size_t, size_t, std::string>, defines an optional signal container for
		* 		current changing values.
		* @param[in]	globalSet
		* 	@li 	optional: expects pointer of type sigc::signal<void, size_t>, defines an optional signal container for global values.
		*/
		CProgressWindowA(const neutrino_locale_t title,
				const int &dx = PW_MIN_WIDTH,
				const int &dy = PW_MIN_HEIGHT,
				sigc::signal<void, size_t, size_t, std::string> *status_Signal = NULL,
				sigc::signal<void, size_t> *globalSet = NULL)
					: CProgressWindow(title, dx, dy, NULL, NULL)
					{
						Init(status_Signal, NULL, NULL, globalSet);
					};

		/**CProgressWindowA Constructor
		* @see		For common arguments and examples, see related constructor(s)!
		* 		For general parameters see basic class CProgressWindow!
		*/
		CProgressWindowA(const std::string &title,
				const int &dx = PW_MIN_WIDTH,
				const int &dy = PW_MIN_HEIGHT,
				sigc::signal<void, size_t, size_t, std::string> *status_Signal = NULL,
				sigc::signal<void, size_t> *globalSet = NULL)
					: CProgressWindow(title, dx, dy, NULL, NULL)
					{
						Init(status_Signal, NULL, NULL, globalSet);
					};
};

class CProgressSignals : public sigc::trackable
{
	public:
		/**CProgressSignals Constructor:
		* Additional class for inherited signal implemantations into classes with used CProgressWindow instances.
		*/
		CProgressSignals()
		{
			//obligatory init of signals. Not really required but just to be safe.
			OnProgress.clear();
			OnLocalProgress.clear();
			OnGlobalProgress.clear();
		};

		/**
		* For general usage for implementations of signals for classes which are using CProgressBar() window instances based on inheritance.
		* @see Take a look into examples to find in progressbar.h
		*/
		sigc::signal<void, size_t, size_t, std::string> OnProgress, OnLocalProgress, OnGlobalProgress;
		sigc::signal<void, size_t> OnSetGlobalMax;
};

#endif

/**CProgressWindow usage with classic method:\n
* This is the general used method to implement progress bar windows.
* @example	Progresswindow
*
* 		void CFooClass::DoCount
* 		{
* 			// Create a CProgressWindow object
* 			CProgressWindow progress;
*
* 			// set possible properties, eg. like here we dont't need a header
* 			status.showHeader(false);
*
* 			// paint window
* 			status.paint();
*
* 			// set possible properties, like current status text
* 			status.showStatusMessageUTF("test progress");
*
* 			// set current progress, call functions, methods or use a while, next loop or what ever
* 			status.showStatus(25)
*
* 			// finally remove window from screen
* 			status.hide();
* 		}
*
* 		// ...That's it. Until now these steps are a classical way inside neutrino, but you can use progress window with signals too.
* 		// Working with signals have the advantage that the implementation could be more compactly, because
* 		// complex constructions within the classes are usually unnecessary,
* 		// beacuse of the signals can be installed where they directly catching the required values. See next examples:
*
* @see
* 		* Progresswindow:Signals:1\n
* 		* Progresswindow:Signals:2\n
* 		* Progresswindow:Adaptive
*/

/**CProgressWindow usage with signals
* @example	Progresswindow:Signals:1
*
* 		class CFooClass
* 		{
* 			//declare a signal eg. in header file
* 			private:
* 				// other members...
* 				sigc::signal<void, size_t, size_t, std::string> OnProgress;
* 				void DoCount();
* 				// other members...
* 			public:
* 				// other members...
* 				void DoAnything();
* 				// other members...
* 		};
*
* 		// add the 'OnProgress' signal into a counter methode
* 		void CFooClass::DoCount{
* 			size_t max = 10
* 			for (size_t i = 0; i < max; i++){
* 				OnProgress(i, max, "Test");
* 			}
* 		}
*
* 		void CFooClass::DoAnything{
* 			// inside of methode which calls the progress define a CProgressWindow object and the counter method:
* 			// ...any code
* 			CProgressWindow progress(NULL, 500, 150, &OnProgress);
* 			// paint window
* 			progress.paint(); // paint()
*
* 			// ...
*
* 			DoCount();
*
* 			// ...
*
* 			// finally remove window from screen
* 			progress.hide();
* 		}
*/

/**CProgressWindow usage with inherited signals
* @example	Progresswindow:Signals:2
*
* 		// Another and a recommended way to implement signals is to inherit prepared signals with
* 		// 'class CProgressSignals'. This class contains prepared signals for implemantation and disconnetion of slots
* 		// is performed automatically.
* 		// See next example:
* 		class CFooClass : public CProgressSignals
* 		{
* 			private:
* 				// other members...
* 				void DoCount();
* 				// other members...
* 			public:
* 				// other members...
* 				void DoAnything();
* 				// other members...
* 		};
*
* 		// add the 'OnGlobalProgress' and 'OnLocalProgress' signals into a counter methode
* 		void CFooClass::DoCount();{
* 			size_t max = 10;
* 			for (size_t i = 0; i < max; i++){
* 				OnGlobalProgress(i, max, "Test"); //visualize global progress
* 				for (size_t j = 0; j < max; j++){
* 					OnLocalProgress(ij, max, "Test"); // visualize local progress
* 				}
* 			}
* 		}
*
* 		void CFooClass::DoAnything{
* 			// inside of methode which calls the progress define a CProgressWindow object and the counter method:
* 			// ...any code
* 			CProgressWindow progress(NULL, 500, 150, NULL, &OnLocalProgress, &OnGlobalProgress);
*  			// paint window
* 			progress.paint();
*
* 			// ...
*
* 			void DoCount();
*
* 			// ...
*
* 			// finally remove window from screen
* 			progress.hide();
* 		}
*/

/**CProgressWindow usage with adaptive global progress
* @example	Progresswindow:Adaptive
*
* 		// Use this for local progress views with adaptive global status.
* 		// This allows an adjusted global progress with more 'smoother' progress view. Here the global progress
* 		// is calculated inside the progressbar object itself. This requires a call for the local progress and
* 		// an assignment of global maximum value.
* 		// Note: Global progressbar is only visible if global value > 1!
*
* 		class CFooClass
* 		{
* 			private:
* 				// other members...
* 				// If You want to use signals, declare signals or inherit 'class CProgressSignals' (e.g: 'class CFooClass : public CProgressSignals')
* 				sigc::signal<void, size_t, size_t, std::string> OnProgress;
* 				sigc::signal<void, size_t> OnSetGlobalMax;
* 				// other members...
* 			public:
* 				// other members...
* 				void DoCount();
* 				// other members...
* 		};
*
* 		void CFooClass::DoCount()
* 		{
* 			size_t max = 10
* 			CProgressWindowA pw("Test", CCW_PERCENT 50, CCW_PERCENT 20, NULL, NULL);
* 			pw.paint();
*
* 			// Set the maximal global value:
* 			// Here global value is set directly with 'setGlobalMax()' inside 'DoCount()', but for scanning for global
* 			// values it's also possible to use the 'OnSetGlobalMax()' signal
* 			// outside of counter methode in any other method.
* 			// An example for usage of OnSetGlobalMax() is to find in 'gui/moviebrowser/mp.cpp'
*			// To see the visual difference to general progress take a look into test menu and try out Test 4 and 5.
* 			// Note: Without assigned global max value, no global bar is visible, means: Only local progress is showed!
* 			pw.setGlobalMax(max);
*
* 			for(size_t i = 1; i <= max; i++){
* 				for(size_t j = 1; j<= 8; j++){
* 					pw.showStatus(j, 8, to_string(j));
* 					sleep(1);
* 				}
* 			}
* 			sleep(1);
* 			pw.hide();
* 		}
*/
