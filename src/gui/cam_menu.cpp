/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2010, 2012 Stefan Seyfried
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "cam_menu.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <dlfcn.h>
#include <sys/mount.h>

#include <global.h>
#include <neutrino.h>
#include "widget/icons.h"
#include "widget/stringinput.h"
#include "widget/messagebox.h"
#include "widget/progresswindow.h"

#include <system/setting_helpers.h>
#include <system/settings.h>
#include <system/debug.h>

#include <mymenu.h>
#include <eitd/edvbstring.h>
#include <zapit/capmt.h>
#include <zapit/zapit.h>

void CCAMMenuHandler::init(void)
{
	hintBox = NULL;
	ca = cCA::GetInstance();
}

int CCAMMenuHandler::exec(CMenuTarget* parent, const std::string &actionkey)
{
	std::string::size_type loc;
	int slot;

	printf("CCAMMenuHandler::exec: actionkey %s\n", actionkey.c_str());
        if (parent)
                parent->hide();

	if ((loc = actionkey.find("ca_ci_reset", 0)) != std::string::npos) {
		slot = actionkey.at(11) - '0';

		if(ca && ca->ModulePresent(CA_SLOT_TYPE_CI, slot))
			ca->ModuleReset(CA_SLOT_TYPE_CI, slot);
	} else if ((loc = actionkey.find("ca_ci", 0)) != std::string::npos) {
		slot = actionkey.at(5) - '0';
		printf("CCAMMenuHandler::exec: actionkey %s for slot %d\n", actionkey.c_str(), slot);
		return doMenu(slot, CA_SLOT_TYPE_CI);
	} else if ((loc = actionkey.find("ca_sc_reset", 0)) != std::string::npos) {
		slot = actionkey.at(11) - '0';

		if(ca && ca->ModulePresent(CA_SLOT_TYPE_SMARTCARD, slot))
			ca->ModuleReset(CA_SLOT_TYPE_SMARTCARD, slot);
	} else if ((loc = actionkey.find("ca_sc", 0)) != std::string::npos) {
		slot = actionkey.at(5) - '0';
		printf("CCAMMenuHandler::exec: actionkey %s for slot %d\n", actionkey.c_str(), slot);
		return doMenu(slot, CA_SLOT_TYPE_SMARTCARD);
	}

	if(!parent)
		return 0;

	return doMainMenu();
}

int CCAMMenuHandler::doMainMenu()
{
	int ret, cnt;
	char name1[255]={0};
	char str1[255]={0};

	int CiSlots = ca->GetNumberCISlots();

	CMenuWidget* cammenu = new CMenuWidget(LOCALE_CI_SETTINGS, NEUTRINO_ICON_SETTINGS);
	//cammenu->addItem( GenericMenuBack );
	//cammenu->addItem( GenericMenuSeparatorLine );
	cammenu->addIntroItems();

	if(CiSlots) {
		cammenu->addItem( new CMenuOptionChooser(LOCALE_CI_RESET_STANDBY, &g_settings.ci_standby_reset, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
		cammenu->addItem( new CMenuOptionNumberChooser(LOCALE_CI_CLOCK, &g_settings.ci_clock, true, 6, 12, this));
	}
	cammenu->addItem( new CMenuOptionChooser(LOCALE_CI_IGNORE_MSG, &g_settings.ci_ignore_messages, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	cammenu->addItem( GenericMenuSeparatorLine );

	CMenuWidget * tempMenu;
	int i = 0;

	cnt = 0;
	printf("CCAMMenuHandler::doMainMenu CI slots: %d\n", CiSlots);
	while (i < CiSlots && i < 2) {
		if (ca->ModulePresent(CA_SLOT_TYPE_CI, i)) {
			ca->ModuleName(CA_SLOT_TYPE_CI, i, name1);
			printf("CCAMMenuHandler::doMainMenu cam%d name %s\n", i, name1);
			char tmp[32];
			snprintf(tmp, sizeof(tmp), "ca_ci%d", i);

			cammenu->addItem(new CMenuForwarderNonLocalized(name1, true, NULL, this, tmp, CRCInput::RC_1 + cnt++));
			snprintf(tmp, sizeof(tmp), "ca_ci_reset%d", i);
			cammenu->addItem(new CMenuForwarder(LOCALE_CI_RESET, true, NULL, this, tmp));
			memset(name1,0,sizeof(name1));
		} else {
			snprintf(str1, sizeof(str1), "%s %d", g_Locale->getText(LOCALE_CI_EMPTY), i);
			tempMenu = new CMenuWidget(str1, NEUTRINO_ICON_SETTINGS);
			cammenu->addItem(new CMenuForwarderNonLocalized(str1, false, NULL, tempMenu));
			memset(str1,0,sizeof(str1));
		}
		if (i < (CiSlots - 1))
			cammenu->addItem( GenericMenuSeparatorLine );
		i++;
	}

	i = 0;
	int ScNum = ca->GetNumberSmartCardSlots();
	printf("CCAMMenuHandler::doMainMenu sc slots: %d\n", ScNum);

	if(ScNum && CiSlots)
		cammenu->addItem( GenericMenuSeparatorLine );

	while (i < ScNum && i < 2) {
		if (ca->ModulePresent(CA_SLOT_TYPE_SMARTCARD, i)) {
			ca->ModuleName(CA_SLOT_TYPE_SMARTCARD, i, name1);
			printf("CCAMMenuHandler::doMainMenu cam%d name %s\n", i, name1);
			char tmp[32];
			snprintf(tmp, sizeof(tmp), "ca_sc%d", i);

			cammenu->addItem(new CMenuForwarderNonLocalized(name1, true, NULL, this, tmp, CRCInput::RC_1 + cnt++));
#if 0 // FIXME not implemented yet
			snprintf(tmp, sizeof(tmp), "ca_sc_reset%d", i);
			cammenu->addItem(new CMenuForwarder(LOCALE_SC_RESET, true, NULL, this, tmp));
#endif
			memset(name1,0,sizeof(name1));
		} else {
			snprintf(str1, sizeof(str1), "%s %d", g_Locale->getText(LOCALE_SC_EMPTY), i);
			tempMenu = new CMenuWidget(str1, NEUTRINO_ICON_SETTINGS);
			cammenu->addItem(new CMenuForwarderNonLocalized(str1, false, NULL, tempMenu));
			memset(str1,0,sizeof(str1));
		}
		if (i < (ScNum - 1))
			cammenu->addItem( GenericMenuSeparatorLine );
		i++;
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

void CCAMMenuHandler::showHintBox (const neutrino_locale_t /*Caption*/, const char * const Text, uint32_t timeout)
{
	hideHintBox();
	hintBox = new CHintBox(LOCALE_MESSAGEBOX_INFO, Text);
	hintBox->paint();
	if(timeout > 0) {
		sleep(timeout);
		hideHintBox();
	}
}

void CCAMMenuHandler::hideHintBox(void)
{
	if(hintBox != NULL) {
		hintBox->hide();
		delete hintBox;
		hintBox = NULL;
	}
}

int CCAMMenuHandler::handleCamMsg (const neutrino_msg_t msg, neutrino_msg_data_t data, bool from_menu)
{
	int ret = 0;
	char str[255];
	char cnt[5];
	int i;
	MMI_MENU_LIST_INFO Menu;
	MMI_ENGUIRY_INFO MmiEnquiry;
	MMI_MENU_LIST_INFO *pMenu = &Menu;
	MMI_ENGUIRY_INFO *pMmiEnquiry = &MmiEnquiry;
	CA_MESSAGE Msg, *rMsg;

	if (msg != NeutrinoMessages::EVT_CA_MESSAGE)
		return from_menu ? 1 : -1;

	if (g_settings.ci_ignore_messages && !from_menu)
		return 1;

	rMsg	= (CA_MESSAGE *)data;
	if (!rMsg)
		return -1;

	Msg = *rMsg;
	delete rMsg;

	u32 MsgId             = Msg.MsgId;
	CA_SLOT_TYPE SlotType = Msg.SlotType;
	int curslot           = Msg.Slot;

	printf("CCAMMenuHandler::handleCamMsg: msg %d from %s\n", MsgId, from_menu ? "menu" : "neutrino");

	if (SlotType != CA_SLOT_TYPE_SMARTCARD && SlotType != CA_SLOT_TYPE_CI)
		return -1;

	if(MsgId == CA_MESSAGE_MSG_INSERTED) {
		hideHintBox();
		snprintf(str, sizeof(str), "%s %d", 
				g_Locale->getText(SlotType == CA_SLOT_TYPE_CI ? LOCALE_CI_INSERTED : LOCALE_SC_INSERTED), (int)curslot+1);
		printf("CCAMMenuHandler::handleCamMsg: %s\n", str);
		ShowHintUTF(LOCALE_MESSAGEBOX_INFO, str);
#if 0
		showHintBox(LOCALE_MESSAGEBOX_INFO, str, CI_MSG_TIME);
#endif
	} else if (MsgId == CA_MESSAGE_MSG_REMOVED) {
		hideHintBox();

		snprintf(str, sizeof(str), "%s %d", 
				g_Locale->getText(SlotType == CA_SLOT_TYPE_CI ? LOCALE_CI_REMOVED : LOCALE_SC_REMOVED), (int)curslot+1);

		printf("CCAMMenuHandler::handleCamMsg: %s\n", str);
		ShowHintUTF(LOCALE_MESSAGEBOX_INFO, str);
#if 0
		showHintBox(LOCALE_MESSAGEBOX_INFO, str, CI_MSG_TIME);
#endif
	} else if(MsgId == CA_MESSAGE_MSG_INIT_OK) {
		if(hintBox != NULL) {
			hintBox->hide();
			delete hintBox;
			hintBox = NULL;
		}
		char name[255] = "Unknown";
		if (ca)
			ca->ModuleName(SlotType, curslot, name);

		snprintf(str, sizeof(str), "%s %d: %s", 
				g_Locale->getText(SlotType == CA_SLOT_TYPE_CI ? LOCALE_CI_INIT_OK : LOCALE_SC_INIT_OK), (int)curslot+1, name);
		printf("CCAMMenuHandler::handleCamMsg: %s\n", str);
		CCamManager::getInstance()->Start(CZapit::getInstance()->GetCurrentChannelID(), CCamManager::PLAY, true);
		ShowHintUTF(LOCALE_MESSAGEBOX_INFO, str);
#if 0
		showHintBox(LOCALE_MESSAGEBOX_INFO, str, CI_MSG_TIME);
#endif
	} else if(MsgId == CA_MESSAGE_MSG_INIT_FAILED) {
		hideHintBox();

		char name[255] = "Unknown";
		if (ca)
			ca->ModuleName(SlotType, curslot, name);

		snprintf(str, sizeof(str), "%s %d: %s", 
				g_Locale->getText(SlotType == CA_SLOT_TYPE_CI ? LOCALE_CI_INIT_FAILED : LOCALE_SC_INIT_FAILED), (int)curslot+1, name);

		printf("CCAMMenuHandler::handleCamMsg: %s\n", str);
		ShowHintUTF(LOCALE_MESSAGEBOX_INFO, str);
#if 0
		showHintBox(LOCALE_MESSAGEBOX_INFO, str, CI_MSG_TIME);
#endif
	} else if(MsgId == CA_MESSAGE_MSG_MMI_MENU || MsgId == CA_MESSAGE_MSG_MMI_LIST) {
		bool sublevel = false;

		if(MsgId != CA_MESSAGE_MSG_MMI_MENU)
			sublevel = true;

		if (!(Msg.Flags & CA_MESSAGE_HAS_PARAM1_DATA))
			return -1;

		memmove(pMenu, (MMI_MENU_LIST_INFO*)Msg.Msg.Data[0], sizeof(MMI_MENU_LIST_INFO));
		free((void *)Msg.Msg.Data[0]);

		printf("CCAMMenuHandler::handleCamMsg: slot %d menu ready, title %s choices %d\n", curslot, convertDVBUTF8(pMenu->title, strlen(pMenu->title), 0).c_str(), pMenu->choice_nb);

		hideHintBox();

		int selected = -1;
		if(pMenu->choice_nb) {
			CMenuWidget* menu = new CMenuWidget(convertDVBUTF8(pMenu->title, strlen(pMenu->title), 0).c_str(), NEUTRINO_ICON_SETTINGS);
			menu->enableSaveScreen(true);

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
			for(i = 0; (i < pMenu->choice_nb) && (i < MAX_MMI_ITEMS); i++) {
				snprintf(cnt, sizeof(cnt), "%d", i);
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
			int slen = 0;

			hideHintBox();
#if 1
			if(strlen(pMenu->title))
				slen += snprintf(&lstr[slen], 255-slen, "%s", pMenu->title);
			if(strlen(pMenu->subtitle))
				slen += snprintf(&lstr[slen], 255-slen, "\n%s", pMenu->subtitle);
			if(strlen(pMenu->bottom))
				slen += snprintf(&lstr[slen], 255-slen, "\n%s", pMenu->bottom);

			ShowHintUTF(LOCALE_MESSAGEBOX_INFO, convertDVBUTF8(lstr, slen, 0).c_str());
#else
			if(strlen(pMenu->subtitle))
				slen += snprintf(&lstr[slen], 255-slen, "\n%s", pMenu->subtitle);
			if(strlen(pMenu->bottom))
				slen += snprintf(&lstr[slen], 255-slen, "\n%s", pMenu->bottom);
			ShowHintUTF(convertDVBUTF8(pMenu->title, strlen(pMenu->title), 0).c_str(), convertDVBUTF8(lstr, slen, 0).c_str());
#endif
#if 0
			showHintBox(LOCALE_MESSAGEBOX_INFO, convertDVBUTF8(lstr, slen, 0).c_str());
			sleep(4);//FIXME
			if(!from_menu) {
				hideHintBox();
			}
#endif
			return 0;
		}

		if(sublevel)
			return 0;

		if(selected >= 0) {
			printf("CCAMMenuHandler::handleCamMsg: selected %d:%s sublevel %s\n", selected, pMenu->choice_item[i], sublevel ? "yes" : "no");
			ca->MenuAnswer(SlotType, curslot, selected+1);
			timeoutEnd = CRCInput::calcTimeoutEnd(10);
			return 1;
		} else {
			return 2;
		}
	}
	else if(MsgId == CA_MESSAGE_MSG_MMI_REQ_INPUT) {
		if (!(Msg.Flags & CA_MESSAGE_HAS_PARAM1_DATA))
			return -1;

		memmove(pMmiEnquiry, (MMI_ENGUIRY_INFO *)Msg.Msg.Data[0], sizeof(MMI_ENGUIRY_INFO));
		free((void *)Msg.Msg.Data[0]);
		printf("CCAMMenuHandler::handleCamMsg: slot %d input request, text %s\n", curslot, convertDVBUTF8(pMmiEnquiry->enguiryText, strlen(pMmiEnquiry->enguiryText), 0).c_str());
		hideHintBox();

		char ENQAnswer[pMmiEnquiry->answerlen+1];
		ENQAnswer[0] = 0;

		CEnquiryInput *Inquiry = new CEnquiryInput((char *)convertDVBUTF8(pMmiEnquiry->enguiryText, strlen(pMmiEnquiry->enguiryText), 0).c_str(), ENQAnswer, pMmiEnquiry->answerlen, pMmiEnquiry->blind != 0, NONEXISTANT_LOCALE);
		Inquiry->exec(NULL, "");
		delete Inquiry;

		printf("CCAMMenuHandler::handleCamMsg: input=[%s]\n", ENQAnswer);

		if((int) strlen(ENQAnswer) != pMmiEnquiry->answerlen) {
			printf("CCAMMenuHandler::handleCamMsg: wrong input len\n");
			ca->InputAnswer(SlotType, curslot, (unsigned char *)ENQAnswer, 0);
			return 1; //FIXME
		} else {
			ca->InputAnswer(SlotType, curslot, (unsigned char *)ENQAnswer, pMmiEnquiry->answerlen);
			return 1;
		}
	}
	else if(MsgId == CA_MESSAGE_MSG_MMI_CLOSE) {
		printf("CCAMMenuHandler::handleCamMsg: close request slot: %d\n", curslot);
		hideHintBox();
		ca->MenuClose(SlotType, curslot);
		return 0;
	}
	else if(MsgId == CA_MESSAGE_MSG_MMI_TEXT) {
		printf("CCAMMenuHandler::handleCamMsg: text\n");
	} else
		ret = -1;
	//printf("CCAMMenuHandler::handleCamMsg: return %d\n", ret);
	return ret;
}

int CCAMMenuHandler::doMenu(int slot, CA_SLOT_TYPE slotType)
{
	int res = menu_return::RETURN_REPAINT;
	neutrino_msg_t msg;
	neutrino_msg_data_t data;
	bool doexit = false;

	while(!doexit) {
		printf("CCAMMenuHandler::doMenu: enter menu for slot %d\n", slot);

		timeoutEnd = CRCInput::calcTimeoutEnd(10);

		ca->MenuEnter(slotType, slot);
		while(true) {
			showHintBox(LOCALE_MESSAGEBOX_INFO, 
				g_Locale->getText(slotType == CA_SLOT_TYPE_CI ? LOCALE_CI_WAITING : LOCALE_SC_WAITING));

			g_RCInput->getMsgAbsoluteTimeout (&msg, &data, &timeoutEnd);
			printf("CCAMMenuHandler::doMenu: msg %lx data %lx\n", msg, data);
			if (msg == CRCInput::RC_timeout) {
				printf("CCAMMenuHandler::doMenu: menu timeout\n");
				hideHintBox();
				ShowHintUTF(LOCALE_MESSAGEBOX_INFO, 
					g_Locale->getText(slotType == CA_SLOT_TYPE_CI ? LOCALE_CI_TIMEOUT : LOCALE_SC_TIMEOUT));
#if 0
				showHintBox(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_CI_TIMEOUT), 5);
#endif
				ca->MenuClose(slotType, slot);
				return menu_return::RETURN_REPAINT;
			}
			/* -1 = not our event, 0 = back to top menu, 1 = continue loop, 2 = quit */
			int ret = handleCamMsg(msg, data, true);
			printf("CCAMMenuHandler::doMenu: handleCamMsg %d\n", ret);
			if(ret < 0 && (msg > CRCInput::RC_Messages)) {
				if ( CNeutrinoApp::getInstance()->handleMsg( msg, data ) & ( messages_return::cancel_all | messages_return::cancel_info ) )
				{
					doexit = true;
					res = menu_return::RETURN_EXIT_ALL;
				}
			} else if (ret == 1) {
				/* workaround: dont cycle here on timers */
				if (msg != NeutrinoMessages::EVT_TIMER)
					timeoutEnd = CRCInput::calcTimeoutEnd(10);
				continue;
			} else if (ret == 2) {
				doexit = true;
				break;
			} else { // ret == 0
				break;
			}
		}
	}
	ca->MenuClose(slotType, slot);
	hideHintBox();
	printf("CCAMMenuHandler::doMenu: return\n");
	return res;
}

bool CCAMMenuHandler::changeNotify(const neutrino_locale_t OptionName, void * /*Data*/)
{
	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_CI_CLOCK)) {
		printf("CCAMMenuHandler::changeNotify: ci_clock %d\n", g_settings.ci_clock);
		ca->SetTSClock(g_settings.ci_clock * 1000000);
		return true;
	}
	return false;
}
