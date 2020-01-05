/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Info Clock Window
	based up CComponentsFrmClock
	Copyright (C) 2013-2015, Thilo Graf 'dbt'
	Copyright (C) 2013, Michael Liebmann 'micha-bbg'

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#ifndef __INFOCLOCK__
#define __INFOCLOCK__


#include <gui/components/cc.h>


class CInfoClock : public CComponentsFrmClock
{
	private:
		void		initCCLockItems();
	public:
		CInfoClock();
		virtual ~CInfoClock(){}; // inherited from CComponentsFrmClock
		static		CInfoClock* getInstance();

		bool 		StartInfoClock();
		bool 		StopInfoClock(bool exit_thread = false);
		bool		enableInfoClock(bool enable = true);
		bool		disableInfoClock() {return enableInfoClock(false);}
		void		ClearDisplay();
		void 		setHeight(const int){}//NOTE: dummy member, height is strictly bound to settings

		static void	switchClockOnOff();
};

#endif
