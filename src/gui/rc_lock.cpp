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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>

#include <gui/rc_lock.h>

#include <driver/display.h>
#include <gui/widget/msgbox.h>


const std::string CRCLock::NO_USER_INPUT = "NO_USER_INPUT";

CRCLock::CRCLock()
{
	locked = false;
	lockIcon = NULL;
}

CRCLock::~CRCLock()
{
	if (lockIcon)
	{
		delete lockIcon;
		lockIcon = NULL;
	}
}

CRCLock* CRCLock::getInstance()
{
	static CRCLock* me = NULL;
	if (!me)
		me = new CRCLock();
	return me;
}

int CRCLock::exec(CMenuTarget* parent, const std::string &actionKey)
{
	if (locked)
	{
		printf("CRCLock::%s: already locked; exiting\n", __func__);
		return menu_return::RETURN_EXIT_ALL;
	}

	if (parent)
		parent->hide();

	bool no_input = (actionKey == NO_USER_INPUT);
	std::string key_unlock = CRCInput::getKeyName((neutrino_msg_t) g_settings.key_unlock);
	char lock_msg[1024];
	snprintf(lock_msg, sizeof(lock_msg)-1, g_Locale->getText(LOCALE_RCLOCK_LOCKMSG), key_unlock.c_str());

	if (ShowMsg(LOCALE_RCLOCK_TITLE, lock_msg, CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbCancel,
			 NEUTRINO_ICON_INFO, 450, no_input ? 5 : -1, no_input) == CMsgBox::mbrCancel)
		return menu_return::RETURN_EXIT_ALL;

	locked = true;
	lockRC();
	locked = false;

	ShowMsg(LOCALE_RCLOCK_TITLE, LOCALE_RCLOCK_UNLOCKMSG, CMsgBox::mbrBack, CMsgBox::mbBack,
			NEUTRINO_ICON_INFO, 450, no_input ? 5 : -1);
	return  menu_return::RETURN_EXIT_ALL;
}

void CRCLock::lockRC()
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	uint64_t timeoutEnd;

	// -- Loop until release key pressed
	// -- Release key sequence: [red] [g_settings.key_unlock] within 5 secs
	printf("CRCLock::%s: locking remote control\n", __func__);
	while (1)
	{
		timeoutEnd = CRCInput::calcTimeoutEnd(9999999);
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);

		if (msg == NeutrinoMessages::UNLOCK_RC)
			break;

		if (msg == CRCInput::RC_red)
		{
			timeoutEnd = CRCInput::calcTimeoutEnd(5);
			g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);

			if (msg == (neutrino_msg_t) g_settings.key_unlock)
				break;
		}

		if (msg == CRCInput::RC_timeout)
			continue;

		if (msg > CRCInput::RC_MaxRC)
		{
			CNeutrinoApp::getInstance()->handleMsg(msg, data);
		}
		else
		{
			if (lockIcon == NULL)
			{
				lockIcon = new CComponentsPicture(
					g_settings.screen_StartX + OFFSET_INNER_MID,
					g_settings.screen_StartY + OFFSET_INNER_MID,
					NEUTRINO_ICON_RCLOCK);
				lockIcon->doPaintBg(false);
			}
			lockIcon->paint(CC_SAVE_SCREEN_YES);

			CVFD::getInstance()->showRCLock();
			// showRCLock blocks box for 2 seconds,
			// so we eat all messages created during this time.
			g_RCInput->clearRCMsg();

			lockIcon->hide();
		}
	}
	printf("CRCLock::%s: unlocking remote control\n", __func__);

	return;
}
