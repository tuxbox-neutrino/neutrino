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

/// Basic CComponentSignals class header.
/*!
Basic signal declarations for component sub classes

This class provides some general declarations of signals based up libsigc++.
For more details see: http://libsigc.sourceforge.net/doc.shtml

If you want to use a signal in a class, some steps are required
example:

#include <sigc++/slot.h>

class CYourClass : sigc::trackable //<- not forget, requierd by destructor!
{
	public:
		CYourClass();
		~CYourClass();

		//...your other stuff

		//here is the member we will connect to a signal, here we have a member with one parameter (void*)
		void DoThis(void* arg)
		{
			//do something here
		};

		//this is the member we will use to connect with a signal
		void RunExample()
		{
			CReceiverClass receiver;
				//this object contains the member in which is placed a signal, here: OnBeforePaint
				//note: OnBeforePaint is already inherited, if you use any CComponent Class. Otherwise, it must be done separatly
				//eg: class CNotACComponentClass : public CComponentSignals
				//this also handles the derivation of sigc::trackable

			// here we use a void* parameter, can be filled with any stuff you can cast, you can use also other types
			void *vptr = yourStuff;

			//Example for connection with member function
			//now we can create a slot with a member function of this class
			sigc::slot0<void> sl = sigc::bind<0>(sigc::mem_fun1(*this, &CYourClass::DoThis), vptr);
			//...and connect with signal in the receiver object
			receiver.OnBeforePaint.connect(sl);

			//example for single function
			sigc::slot0<void> s2 = sigc::bind<0>(sigc::ptr_fun1(&DisplayErrorMessage), "test");
			receiver.OnBeforePaint.connect(s2);

			//thats' all. If DoThis(bla) is connected, OnBeforePaint is doing the same like the DoThis(), but in the foreign object
			//for more details take a look into offical libsig2 documentation at http://libsigc.sourceforge.net/libsigc2/docs/
		};
};
*/

#ifndef __CC_SIGNALS_H__
#define __CC_SIGNALS_H____

#include <sigc++/signal.h>


class CComponentsSignals : public sigc::trackable
{
	public:
		CComponentsSignals(){};
		sigc::signal<void> OnError;

		sigc::signal<void> OnBeforePaint;
		sigc::signal<void> OnAfterPaint;
};


#endif
