/*
	Neutrino-GUI  -   DBoxII-Project

	Diese GUI wurde von Grund auf neu programmiert und sollte nun vom
	Aufbau und auch den Ausbaumoeglichkeiten gut aussehen. Neutrino basiert
	auf der Client-Server Idee, diese GUI ist also von der direkten DBox-
	Steuerung getrennt. Diese wird dann von Daemons uebernommen.


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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include <driver/display.h>

#include <gui/rc_lock.h>

#include <gui/widget/hintbox.h>
#include <gui/widget/messagebox.h>
#include <gui/widget/stringinput.h>

const std::string CRCLock::NO_USER_INPUT = "noUserInput";
bool CRCLock::locked = false;

// -- Menue Handler Interface
// -- Infinite Loop to lock remote control (until release lock key pressed)
// -- 2003-12-01 rasc

int CRCLock::exec(CMenuTarget* parent, const std::string &actionKey)
{
	if(locked)
		return menu_return::RETURN_EXIT_ALL;

	if (parent)
		parent->hide();

	bool no_input = (actionKey == NO_USER_INPUT);
	if (ShowLocalizedMessage(LOCALE_RCLOCK_TITLE, LOCALE_RCLOCK_LOCKMSG,
				 CMessageBox::mbrYes, CMessageBox::mbYes | CMessageBox::mbCancel,
				 NEUTRINO_ICON_INFO,450,no_input ? 5 : -1,no_input) == CMessageBox::mbrCancel)
		return menu_return::RETURN_EXIT_ALL;

	// -- Lockup Box
	locked = true;
	lockBox();
	locked = false;

	ShowLocalizedMessage(LOCALE_RCLOCK_TITLE, LOCALE_RCLOCK_UNLOCKMSG, CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO,450, no_input ? 5 : -1);
	return  menu_return::RETURN_EXIT_ALL;
}

void CRCLock::lockBox()
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	uint64_t timeoutEnd;

	// -- Loop until release key pressed
	// -- Key sequence:  <RED> <DBOX> within 5 secs
	while  (1) {

		timeoutEnd = CRCInput::calcTimeoutEnd(9999999);
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);

		if (msg == NeutrinoMessages::UNLOCK_RC)
			break;

		if (msg == CRCInput::RC_red)  {
			timeoutEnd = CRCInput::calcTimeoutEnd(5);
			g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);

			//if (msg == CRCInput::RC_setup)  break;
			if (msg == (neutrino_msg_t) g_settings.key_unlock)  break;
		}

		if (msg == CRCInput::RC_timeout) continue;

		// -- Zwen told me: Eating only RC events would be nice
		// -- so be it...

		if (msg >  CRCInput::RC_MaxRC) {
			CNeutrinoApp::getInstance()->handleMsg(msg, data);
		} else {
			CVFD::getInstance()->showRCLock();
			// Since showRCLock blocks 2secs for each key we eat all
			// messages created during this time. Hopefully this ok...
			g_RCInput->clearRCMsg();
		}
	}
	return;
}
