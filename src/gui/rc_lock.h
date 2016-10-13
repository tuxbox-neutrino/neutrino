/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Copyright (C) 2015 Sven Hoefer (svenhoefer)

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
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


#ifndef _rc_lock_
#define _rc_lock_

#include <gui/widget/menue.h>

#include <string>

class CRCLock: public CMenuTarget
{
private:
	bool locked;
	CComponentsPicture *lockIcon;

	void lockRC();

public:
	CRCLock();
	~CRCLock();
	static CRCLock* getInstance();

	static const std::string NO_USER_INPUT;

	int isLocked() { return locked; }
	int exec(CMenuTarget* parent, const std::string & actionKey);
};

#endif
