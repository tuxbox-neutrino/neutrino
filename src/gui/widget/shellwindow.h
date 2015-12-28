/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Shell window class, visualize of system events on gui screen.

	Implementation:
	Copyright (C) 2013 martii
	gitorious.org/neutrino-mp/martiis-neutrino-mp

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

#ifndef __WIDGET_SHELLWINDOW_H__
#define __WIDGET_SHELLWINDOW_H__
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string>
#include <gui/widget/textbox.h>
#include <sigc++/signal.h>

class CShellWindow : public sigc::trackable
{
	private:
		int mode;
		std::string command;
		int* res;
		CFrameBuffer *frameBuffer;
		CTextBox *textBox;
		void showResult();

	public:
		//shell window modes for handled shell output. //NOTE: mode 0 use only system calls, with unhandled return values and no handled shell output
		enum shellwindow_modes
		{
			/*SYSTEM		= 0, */
			QUIET			= 1, // no window
			VERBOSE 		= 2, // show window
			ACKNOWLEDGE 		= 4, // show result button inside window after execution, no message box NOTE: only in VERBOSE mode
			ACKNOWLEDGE_EVENT	= 8  // same like ACKNOWLEDGE but shows a default error message box or a slot handled action instead default error message box
		};
		CShellWindow(const std::string &Command, const int Mode = 0, int* Res = NULL, bool auto_exec = true);
		~CShellWindow();
		void exec();

		/*!
		signal/event handler runs on loop in exec method
		this allows to use the shell output lines in other objects eg. for evaluation of error or status data
		example for implamentation in your class:
		...your code...
			//assuming in your class is declared a member function named YourMemberFunction(std::string& arg), parameter is a string as rev:
			//Tis function should handle the shell output!

			//declare a slot with return value as 'void' and parameter as 'string', here by rev!
			sigc::slot1<void, string&> sl_shell_output;

			//fill the slot with your member function in your class that do evaluate the output lines
			sl_shell_output = sigc::mem_fun(*this, &CYourClass::YourMemberFunction);

			//create the CShellWindow object in verbose mode, important: parameter 'auto_exec' must be set to 'false', so it is possible to connect the slot before engages the exec() methode
			CShellWindow shell(cmd, (verbose ? CShellWindow::VERBOSE : 0) | (acknowledge ? CShellWindow::ACKNOWLEDGE_MSG : 0), &res, false);

			//connect slot
			shell.OnShellOutputLoop.connect(sl_shell_output);

			//now exec...
			shell.exec();
		...other code...
		*/
		sigc::signal<void, std::string*, int*, bool*> OnShellOutputLoop;

		/*!
		signal/event handler runs after task is finished.
		NOTE: works only with ACKNOWLEDGE_EVENT mode
		After executed task comes a default messages (see method showResult()), but with these slots it is possible to use other messages
		or any desired action without any touching this class.
		Example for implementation in foreign class let show an alternate message than default message:
		...your code...
			//assuming in your foreign class is declared a member function named YourMemberFunction() without return value and int* as parmeter This method should run
			instead the default message.

			//declare a slot with return value as 'void' and wihout any parameter
			sigc::slot<void, int*> sl_result_err;

			//fill the slot with your member function in your class with your action
			sl_result_err = sigc::mem_fun(*this, &CYourClass::YourMemberFunction);

			//create the CShellWindow object in verbose mode, important: parameter 'auto_exec' must be set to 'false', so it is possible to connect the slot before engages the exec() methode
			CShellWindow shell(cmd, (verbose ? CShellWindow::VERBOSE : 0) | (acknowledge ? CShellWindow::ACKNOWLEDGE_MSG : 0), &res, false);

			//connect slot
			shell1.OnResultError.connect(sl_result_err);

			//now exec...
			shell.exec();
		...other code...
		
		Use of OnResultOk is similar with OnResultError.
		*/
		sigc::signal<void, int*> OnResultError;
		sigc::signal<void, int*> OnResultOk;
};

#endif
