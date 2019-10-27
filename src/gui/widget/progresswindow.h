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
		void Init(	sigc::signal<void, size_t, size_t, std::string> *statusSignal,
				sigc::signal<void,size_t, size_t, std::string> *localSignal,
				sigc::signal<void, size_t, size_t, std::string> *globalSignal);

		CProgressBar* getProgressItem();
		void initStatus(const unsigned int prog, const unsigned int max, const std::string &statusText, CProgressBar *pBar);
		void fitItems();

	public:
		/**CProgressWindow Constructor
		* @param[in]	parent
		* 	@li 	optional: expects type CComponentsForm * as possible parent object, default = NULL
		* @param[in]	dx
		* 	@li 	optional: expects type const &int, width of window, default = 0, auto size with declared default values
		* @param[in]	dy
		* 	@li 	optional: expects type const &int, height of window, default = 0, auto size with declared default values
		* @param[in]	status_Signal
		* 	@li 	optional: expects type sigc::signal<void, size_t, size_t, std::string>, defines an optional signal container for
		* 		current changing values.
		* @param[in]	localSignal
		* 	@li 	optional: expects type sigc::signal<void, size_t, size_t, std::string>, defines an optional signal container for
		* 		current changing local values.
		* @param[in]	globalSignal
		* 	@li 	optional: expects type sigc::signal<void, size_t, size_t, std::string>, defines an optional signal container for
		* 		current changing global values.
		*
		* @example
		* 		void CFooClass::DoCount{
		* 			//Usage with classic init inside method:
		*
		* 			//Create a CProgressWindow object
		* 			CProgressWindow progress;
		*
		* 			//set possible properties, eg. like here we dont't need a header
		* 			status.showHeader(false);
		*
		* 			//paint window
		* 			status.paint();
		*
		* 			//set possible properties, like current status text
		* 			status.showStatusMessageUTF("test progress");
		*
		* 			//set current progress, call functions, methods or use a while, next loop or what ever
		* 			status.showStatus(25)
		*
		* 			//finally remove window from screen
		* 			status.hide();
		* 		}
		*
		* 		//That's it. Until now these steps are a classical way inside neutrino, but you can use proress window with signals too.
		* 		//Working with signals have the advantage that the implementation could be more compactly, because
		* 		//complex constructions within the classes are usually unnecessary,
		* 		//beacuse of the signals can be installed where they directly catching the required values. See next example:
		*
		* 		class CFooClass
		* 		{
		* 			//Usage with signals:
		* 			//declare a signal eg. in header file
		* 			private:
		* 				//other members...
		* 				sigc::signal<void, size_t, size_t, std::string> OnProgress;
		* 				void DoCount();
		* 				//other members...
		* 			public:
		* 				//other members...
		* 				void DoAnything();
		* 				//other members...
		* 		};
		*
		* 		//add the OnProgress signal into a counter methode
		* 		void CFooClass::DoCount{
		* 			size_t max = 10
		* 			for (size_t i = 0; i < max; i++){
		* 				OnProgress(i, max, "Test");
		* 			}
		* 		}
		*
		* 		void CFooClass::DoAnything{
		* 			//inside of methode which calls the progress define a CProgressWindow object and the counter method:
		* 			//...any code
		* 			CProgressWindow progress(NULL, 500, 150, &OnProgress);
		* 			//paint window
		* 			progress.paint(); // paint()
		*
		* 			//...
		*
		* 			DoCount();
		*
		* 			//...
		*
		* 			//finally remove window from screen
		* 			progress.hide();
		* 		}
		*
		* 		//Another and a recommended way to implement signals is to inherit prepared signals with
		* 		//class CProgressSignals. This class contains prepared signals for implemantation and disconnetion of slots
		* 		//is performed automatically.
		* 		//See next example:
		* 		class CFooClass : public CProgressSignals
		* 		{
		* 			private:
		* 				//other members...
		* 				void DoCount();
		* 				//other members...
		* 			public:
		* 				//other members...
		* 				void DoAnything();
		* 				//other members...
		* 		};
		*
		* 		//add the OnGlobalProgress and OnLocalProgress signals into a counter methode
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
		* 			//inside of methode which calls the progress define a CProgressWindow object and the counter method:
		* 			//...any code
		* 			CProgressWindow progress(NULL, 500, 150, NULL, &OnLocalProgress, &OnGlobalProgress);
		* 			progress.paint(); // paint window
		*
		* 			//...
		*
		* 			void DoCount();
		*
		* 			//...
		*
		* 			//finally remove window from screen
		* 			progress.hide();
		* 		}
		*
		* @note
		* 		Don't use status_Signal at same time with localSignal and globalSignal. \n
		* 		In This case please set prameter 'status_Signal' = NULL
		*/
		CProgressWindow(CComponentsForm *parent = NULL,
				const int &dx = PW_MIN_WIDTH,
				const int &dy = PW_MIN_HEIGHT,
				sigc::signal<void, size_t, size_t, std::string> *status_Signal = NULL,
				sigc::signal<void,size_t, size_t, std::string> *localSignal = NULL,
				sigc::signal<void, size_t, size_t, std::string> *globalSignal = NULL);

		/**CProgressWindow Constructor
		* @param[in]	title
		* 	@li 	expects type neutrino_locale_t as window title
		*
		* @see		For other arguments and examples, see related constructor(s)
		*/
		CProgressWindow(const neutrino_locale_t title,
				const int &dx = PW_MIN_WIDTH,
				const int &dy = PW_MIN_HEIGHT,
				sigc::signal<void, size_t, size_t, std::string> *status_Signal = NULL,
				sigc::signal<void,size_t, size_t, std::string> *localSignal = NULL,
				sigc::signal<void, size_t, size_t, std::string> *globalSignal = NULL);

		/**CProgressWindow Constructor
		* @param[in]	title
		* 	@li 	expects type std::string as window title
		*
		* @see		For other arguments and examples, see related constructor(s)
		*/
		CProgressWindow(const std::string &title,
				const int &dx = PW_MIN_WIDTH,
				const int &dy = PW_MIN_HEIGHT,
				sigc::signal<void, size_t, size_t, std::string> *status_Signal = NULL,
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
		* Sets current progress value and show progress in window.
		* @param[in]	prog
		* 	@li 	expects type unsigned int, describes current progress value
		* @param[in]	max
		* 	@li 	optional: expects type unsigned int, describes maximal progress value, default = 100
		* @param[in]	statusText
		* 	@li 	optional: expects type std::string, describes current status text, default = empty
		*/
		void showStatus(const unsigned int prog, const unsigned int max = 100, const std::string &statusText = std::string());

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
};

#endif
