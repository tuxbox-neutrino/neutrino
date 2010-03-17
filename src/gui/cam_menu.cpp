/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Kommentar:

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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <dlfcn.h>
#include <sys/mount.h>

#include <global.h>
#include <neutrino.h>
#include <gui/widget/icons.h>
#include "gui/widget/menue.h"
#include "gui/widget/stringinput.h"
#include "gui/widget/messagebox.h"
#include "gui/widget/hintbox.h"
#include "gui/widget/progresswindow.h"

#include "system/setting_helpers.h"
#include "system/settings.h"
#include "system/debug.h"

#include <gui/cam_menu.h>
#include <mymenu.h>
#include <libcoolstream/dvb-ci.h>
#include <sectionsd/edvbstring.h>

void CCAMMenuHandler::init(void)
{
	hintBox = NULL;
	ci = cDvbCi::getInstance();
}

int CCAMMenuHandler::exec(CMenuTarget* parent, const std::string &actionkey)
{
printf("CCAMMenuHandler::exec: actionkey %s\n", actionkey.c_str());
        if (parent)
                parent->hide();

	if(actionkey == "cam1") {
		return doMenu(0);
	} else if(actionkey == "cam2") {
		return doMenu(1);
	} else if(actionkey == "reset1") {
		if(ci->CamPresent(0))
			ci->Reset(0);
	} else if(actionkey == "reset2") {
		if(ci->CamPresent(1))
			ci->Reset(1);
	}

	if(!parent)
		return 0;

	return doMainMenu ();
}

#define OPTIONS_OFF0_ON1_OPTION_COUNT 2
const CMenuOptionChooser::keyval OPTIONS_OFF0_ON1_OPTIONS[OPTIONS_OFF0_ON1_OPTION_COUNT] =
{
        { 0, LOCALE_OPTIONS_OFF },
        { 1, LOCALE_OPTIONS_ON  }
};

int CCAMMenuHandler::doMainMenu ()
{
	int ret;
	char name1[255], name2[255];
	//cDvbCiSlot *one, *two;
	char str1[255];
	char str2[255];

	//one = ci->GetSlot(0);
	//two = ci->GetSlot(1);

	CMenuWidget* cammenu = new CMenuWidget(LOCALE_CAM_SETTINGS, NEUTRINO_ICON_SETTINGS);
	cammenu->addItem( GenericMenuBack );
	cammenu->addItem( GenericMenuSeparatorLine );

	cammenu->addItem( new CMenuOptionChooser(LOCALE_CAM_RESET_STANDBY, &g_settings.ci_standby_reset, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	cammenu->addItem( GenericMenuSeparatorLine );

	CMenuWidget * tempMenu;
	if(ci->CamPresent(0)) {
		ci->GetName(0, name1);
printf("CCAMMenuHandler::doMenu cam1 name %s\n", name1);
		cammenu->addItem(new CMenuForwarderNonLocalized(name1, true, NULL, this, "cam1", CRCInput::RC_1));
		cammenu->addItem(new CMenuForwarder(LOCALE_CAM_RESET, true, NULL, this, "reset1"));
		cammenu->addItem( GenericMenuSeparatorLine );
	} else {
		sprintf(str1, "%s 1", g_Locale->getText(LOCALE_CAM_EMPTY));
		tempMenu = new CMenuWidget(str1, NEUTRINO_ICON_SETTINGS);
		cammenu->addItem(new CMenuForwarderNonLocalized(str1, false, NULL, tempMenu));
	}

	if(ci->CamPresent(1)) {
		ci->GetName(1, name2);
printf("CCAMMenuHandler::doMenu cam2 name %s\n", name2);
		cammenu->addItem(new CMenuForwarderNonLocalized(name2, true, NULL, this, "cam2", CRCInput::RC_2));
		cammenu->addItem(new CMenuForwarder(LOCALE_CAM_RESET, true, NULL, this, "reset2"));
	} else {
		sprintf(str2, "%s 2", g_Locale->getText(LOCALE_CAM_EMPTY));
		tempMenu = new CMenuWidget(str2, NEUTRINO_ICON_SETTINGS);
		cammenu->addItem(new CMenuForwarderNonLocalized(str2, false, NULL, tempMenu));
	}

	ret = cammenu->exec(NULL, "");
	delete cammenu;
	return ret;
}

#define CI_MSG_TIME 5
int CCAMMenuHandler::handleMsg (const neutrino_msg_t msg, neutrino_msg_data_t data)
{
	int ret = messages_return::handled;
//printf("CCAMMenuHandler::handleMsg: msg 0x%x data 0x%x\n", msg, data);
	int camret = handleCamMsg(msg, data);
	if(camret < 0) {
		ret = messages_return::unhandled;
	}
	return ret;
}

int CCAMMenuHandler::handleCamMsg (const neutrino_msg_t msg, neutrino_msg_data_t data, bool from_menu)
{
	int ret = 0;
	char str[255];
	char cnt[5];
	int i;
	MMI_MENU_LIST_INFO Menu;
	MMI_ENGUIRY_INFO MmiEnquiry;
	int curslot;
	MMI_MENU_LIST_INFO * pMenu = &Menu;
	MMI_ENGUIRY_INFO * pMmiEnquiry = &MmiEnquiry;

//printf("CCAMMenuHandler::handleCamMsg: msg 0x%x data 0x%x\n", msg, data);

	if(msg == NeutrinoMessages::EVT_CI_INSERTED) {
		if(hintBox != NULL) {
			hintBox->hide();
			delete hintBox;
		}
		sprintf(str, "%s %d", g_Locale->getText(LOCACE_CAM_INSERTED), (int) data+1);

		printf("CCAMMenuHandler::handleMsg: %s\n", str);
		hintBox = new CHintBox(LOCALE_MESSAGEBOX_INFO, str);
		hintBox->paint();

		sleep(CI_MSG_TIME);
		hintBox->hide();
		delete hintBox;
		hintBox = NULL;

	} else if (msg == NeutrinoMessages::EVT_CI_REMOVED) {
		if(hintBox != NULL) {
			hintBox->hide();
			delete hintBox;
		}
		sprintf(str, "%s %d", g_Locale->getText(LOCALE_CAM_REMOVED), (int) data+1);

		printf("CCAMMenuHandler::handleMsg: %s\n", str);
		hintBox = new CHintBox(LOCALE_MESSAGEBOX_INFO, str);

		hintBox->paint();

		sleep(CI_MSG_TIME);
		hintBox->hide();
		delete hintBox;
		hintBox = NULL;
	} else if(msg == NeutrinoMessages::EVT_CI_INIT_OK) {
		if(hintBox != NULL) {
			hintBox->hide();
			delete hintBox;
		}
		char name[255] = "Unknown";
		//cDvbCiSlot * slot = ci->GetSlot((int) data);
		//if(slot)
		//	slot->GetName(name);
		ci->GetName((int) data, name);
		sprintf(str, "%s %d: %s", g_Locale->getText(LOCALE_CAM_INIT_OK), (int) data+1, name);

		printf("CCAMMenuHandler::handleMsg: %s\n", str);
		hintBox = new CHintBox(LOCALE_MESSAGEBOX_INFO, str);
		hintBox->paint();
		sleep(CI_MSG_TIME);
		hintBox->hide();
		delete hintBox;
		hintBox = NULL;
	}
	else if(msg == NeutrinoMessages::EVT_CI_MMI_MENU || msg == NeutrinoMessages::EVT_CI_MMI_LIST) {
		bool sublevel = false;
		if(msg != NeutrinoMessages::EVT_CI_MMI_MENU)
			sublevel = true;

		memcpy(pMenu, (MMI_MENU_LIST_INFO*) data, sizeof(MMI_MENU_LIST_INFO));
		free((void *)data);
		curslot = pMenu->slot;

		printf("CCAMMenuHandler::handleCamMsg: slot %d menu ready, title %s choices %d\n", curslot, convertDVBUTF8(pMenu->title, strlen(pMenu->title), 0).c_str(), pMenu->choice_nb);

		if(hintBox) {
			hintBox->hide();
		}

		int selected = -1;
		if(pMenu->choice_nb) {
			CMenuWidget* menu = new CMenuWidget(convertDVBUTF8(pMenu->title, strlen(pMenu->title), 0).c_str(), NEUTRINO_ICON_SETTINGS);

			CMenuSelectorTarget * selector = new CMenuSelectorTarget(&selected);
			int slen = strlen(pMenu->subtitle);
			if(slen) {
				char * sptr = pMenu->subtitle;
				char * tptr = sptr;
				int bpos = 0;
				for(int li = 0; li < slen; li++) {
					if((tptr[li] == 0x8A) || ((bpos > 38) && (tptr[li] == 0x20)) ) {
						bpos = 0;
						tptr[li] = 0;
printf("CCAMMenuHandler::handleCamMsg: subtitle: %s\n", sptr);
						menu->addItem(new CMenuForwarderNonLocalized(convertDVBUTF8(sptr, strlen(sptr), 0).c_str(), false));
						sptr = &tptr[li+1];
					}
					bpos++;
				}
				if(strlen(sptr)) {
printf("CCAMMenuHandler::handleCamMsg: subtitle: %s\n", sptr);
						menu->addItem(new CMenuForwarderNonLocalized(convertDVBUTF8(sptr, strlen(sptr), 0).c_str(), false));
				}
			}
			for(i = 0; i < pMenu->choice_nb; i++) {
				sprintf(cnt, "%d", i);
				if(sublevel)
					menu->addItem(new CMenuForwarderNonLocalized(convertDVBUTF8(pMenu->choice_item[i], strlen(pMenu->choice_item[i]), 0).c_str(), true, NULL, selector, cnt));
				else
					menu->addItem(new CMenuForwarderNonLocalized(convertDVBUTF8(pMenu->choice_item[i], strlen(pMenu->choice_item[i]), 0).c_str(), true, NULL, selector, cnt, CRCInput::convertDigitToKey(i+1)));
			}
			slen = strlen(pMenu->bottom);
			if(slen) {
printf("CCAMMenuHandler::handleCamMsg: bottom: %s\n", pMenu->bottom);
				menu->addItem(new CMenuForwarderNonLocalized(convertDVBUTF8(pMenu->bottom, slen, 0).c_str(), false));
			}

			menu->exec(NULL, "");

			delete menu;
			delete selector;
		} else {
			char lstr[255];
			snprintf(lstr, 255, "%s\n%s\n%s", pMenu->title, pMenu->subtitle, pMenu->bottom);
			if(hintBox)
				delete hintBox;
			//hintBox = new CHintBox(LOCALE_MESSAGEBOX_INFO, convertDVBUTF8(pMenu->title, strlen(pMenu->title), 0).c_str());
			hintBox = new CHintBox(LOCALE_MESSAGEBOX_INFO, convertDVBUTF8(str, strlen(lstr), 0).c_str());
			hintBox->paint();
			sleep(4);//FIXME
			if(!from_menu) {
				delete hintBox;
				hintBox = NULL;
			}
			return 1;
		}

		if(sublevel)
			return 0;

		if(selected >= 0) {
			printf("CCAMMenuHandler::handleCamMsg: selected %d:%s sublevel %s\n", selected, pMenu->choice_item[i], sublevel ? "yes" : "no");
			CI_MenuAnswer(curslot, selected+1);
			timeoutEnd = CRCInput::calcTimeoutEnd(10);
			return 1;
		} else {
			return 2;
		}
	}
	else if(msg == NeutrinoMessages::EVT_CI_MMI_REQUEST_INPUT) {
		memcpy(pMmiEnquiry, (MMI_ENGUIRY_INFO*) data, sizeof(MMI_ENGUIRY_INFO));
		free((void *)data);
		curslot = pMmiEnquiry->slot;
		printf("CCAMMenuHandler::handleCamMsg: slot %d input request, text %s\n", curslot, convertDVBUTF8(pMmiEnquiry->enguiryText, strlen(pMmiEnquiry->enguiryText), 0).c_str());
		if(hintBox)
			hintBox->hide();

		char cPIN[pMmiEnquiry->answerlen+1];
		cPIN[0] = 0;

		CPINInput* PINInput = new CPINInput((char *) convertDVBUTF8(pMmiEnquiry->enguiryText, strlen(pMmiEnquiry->enguiryText), 0).c_str(), cPIN, 4, NONEXISTANT_LOCALE);
		PINInput->exec(NULL, "");
		delete PINInput;

		printf("CCAMMenuHandler::handleCamMsg: input=[%s]\n", cPIN);
		//if(cPIN[0] == 0)
		//	return 0;

		if((int) strlen(cPIN) != pMmiEnquiry->answerlen) {
			printf("CCAMMenuHandler::handleCamMsg: wrong input len\n");
			CI_Answer(curslot, (unsigned char *) cPIN, 0);
			return 0;
		} else {
			CI_Answer(curslot, (unsigned char *) cPIN, pMmiEnquiry->answerlen);
			return 1;
		}
	}
	else if(msg == NeutrinoMessages::EVT_CI_MMI_CLOSE) {
		curslot = (int) data;
		printf("CCAMMenuHandler::handleCamMsg: close request slot: %d\n", curslot);
#if 0
		if(hintBox) {
			hintBox->hide();
			delete hintBox;
			hintBox = NULL;
		}
#endif
		CI_CloseMMI(curslot);
		return 0;
	}
	else if(msg == NeutrinoMessages::EVT_CI_MMI_TEXT) {
		curslot = (int) data;
		printf("CCAMMenuHandler::handleCamMsg: text\n");
	} else
		ret = -1;
//printf("CCAMMenuHandler::handleCamMsg: return %d\n", ret);
	return ret;
}

int CCAMMenuHandler::doMenu (int slot)
{
	int res = menu_return::RETURN_REPAINT;
	neutrino_msg_t msg;
	neutrino_msg_data_t data;
	bool doexit = false;

	while(!doexit) {
		printf("CCAMMenuHandler::doMenu: slot %d\n", slot);

		timeoutEnd = CRCInput::calcTimeoutEnd(10);

		CI_EnterMenu(slot);
		while(true) {
			if(hintBox)
				delete hintBox;
			hintBox = new CHintBox(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_CAM_WAITING));
			hintBox->paint();
			g_RCInput->getMsgAbsoluteTimeout (&msg, &data, &timeoutEnd);
			printf("CCAMMenuHandler::doMenu: msg %x data %x\n", msg, data);
			if (msg == CRCInput::RC_timeout) {
				if(hintBox)
					delete hintBox;

				hintBox = new CHintBox(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_CAM_TIMEOUT));
				hintBox->paint();

				printf("CCAMMenuHandler::doMenu: menu timeout\n");
				sleep(5);
				delete hintBox;
				hintBox = NULL;
				CI_CloseMMI(slot);
				return menu_return::RETURN_REPAINT;
			}
			/* -1 = not our event, 0 = back to top menu, 1 = continue loop, 2 = quit */
			int ret = handleCamMsg(msg, data, true);
			if(ret < 0 && (msg > CRCInput::RC_Messages)) {
				if ( CNeutrinoApp::getInstance()->handleMsg( msg, data ) & ( messages_return::cancel_all | messages_return::cancel_info ) )
				{
					doexit = true;
					res = menu_return::RETURN_EXIT_ALL;
				}
			} else if (ret == 1) {
				timeoutEnd = CRCInput::calcTimeoutEnd(10);
				continue;
			} else if (ret == 2) {
				doexit = true;
				break;
			} else {
				break;
			}
		}
	}
	CI_CloseMMI(slot);
	if(hintBox) {
		delete hintBox;
		hintBox = NULL;
	}
printf("CCAMMenuHandler::doMenu: return\n");
	return res;
}
