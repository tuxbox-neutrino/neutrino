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
#include "widget/hintbox.h"
#include <mmi.h>
#include <ca_cs.h>


using namespace std;

class CCAMMenuHandler : public CMenuTarget
{
	private:
		CHintBox * hintBox;
		cCA *ca;
		uint64_t timeoutEnd;
		//int slot;
		int doMenu(int slot, CA_SLOT_TYPE slotType);
		int doMainMenu();
		int handleCamMsg (const neutrino_msg_t msg, neutrino_msg_data_t data, bool from_menu = false);
		void hideHintBox(void);
		void showHintBox(const neutrino_locale_t Caption, const char * const Text, uint32_t timeout = 0);
	public:
		void init(void);
		int exec(CMenuTarget* parent,  const std::string &actionkey);
		int handleMsg (const neutrino_msg_t msg, neutrino_msg_data_t data);
};
#endif

