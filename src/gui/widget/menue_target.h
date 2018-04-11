/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

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


#ifndef __MENU_TARGET__
#define __MENU_TARGET__

#include <string>
#include <driver/framebuffer.h>

class CMenuTarget
{
	protected:
		std::string *valueString;
		std::string valueStringTmp;
	public:
		CMenuTarget(){ valueStringTmp = std::string(); valueString = &valueStringTmp; }
		virtual ~CMenuTarget(){}
		virtual void hide(){}
		virtual int exec(CMenuTarget* parent, const std::string & actionKey) = 0;
		virtual std::string& getValue(void) { return *valueString; }
		virtual fb_pixel_t getColor(void) { return 0; }
};


#endif /*__MENU_TARGET__*/
