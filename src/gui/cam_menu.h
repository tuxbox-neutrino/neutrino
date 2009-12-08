/*
	Neutrino-GUI  -   DBoxII-Project


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


#ifndef __cam_menu__
#define __cam_menu__


#include "widget/menue.h"
#include <libcoolstream/dvb-ci.h>

using namespace std;

class CCAMMenuHandler : public CMenuTarget
{
	private:
		CHintBox * hintBox;
		cDvbCi * ci;
		unsigned long long timeoutEnd;
		//int slot;
		int doMenu(int slot);
		int doMainMenu();
		int handleCamMsg (const neutrino_msg_t msg, neutrino_msg_data_t data, bool from_menu = false);
	public:
		void init(void);
		int exec(CMenuTarget* parent,  const std::string &actionkey);
		int handleMsg (const neutrino_msg_t msg, neutrino_msg_data_t data);
};
#endif

