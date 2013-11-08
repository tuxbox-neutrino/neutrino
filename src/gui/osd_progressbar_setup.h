/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	progressbar_setup menu
	Suggested by tomworld

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, 
	Boston, MA  02110-1301, USA.

*/

#ifndef __PROGRESSBAR_SETUP__
#define __PROGRESSBAR_SETUP__


#include <string>

#include <gui/widget/menue.h>

class CProgressbarSetup : public CMenuTarget, CChangeObserver
{
private:
	int width;
	int showMenu();

public:
	CProgressbarSetup();
	~CProgressbarSetup();
	int exec(CMenuTarget* parent, const std::string &);
};

#endif
