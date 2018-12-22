/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2011 CoolStream International Ltd
	License: GPLv2

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation;

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
#include <hardware/ca.h>


class CCAMMenuHandler : public CMenuTarget, public CChangeObserver
{
	private:
		CHintBox * hintBox;
		cCA *ca;
		uint64_t timeoutEnd;
		uint32_t close_timer;
		int menu_slot;
		int menu_type;
		bool in_menu;
		int doMenu(int slot, CA_SLOT_TYPE slotType);
		int doMainMenu();
		int handleCamMsg (const neutrino_msg_t msg, neutrino_msg_data_t data, int &msgret, bool from_menu = false);
		void hideHintBox(void);
		void showHintBox(const neutrino_locale_t Caption, const char * const Text, uint32_t timeout = 0);
	public:
		void init(void);
		int exec(CMenuTarget* parent,  const std::string &actionkey);
		int handleMsg (const neutrino_msg_t msg, neutrino_msg_data_t data);
		bool changeNotify(const neutrino_locale_t OptionName, void * /*data*/);
};
#endif

