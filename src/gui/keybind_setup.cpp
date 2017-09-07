/*
	$port: keybind_setup.cpp,v 1.4 2010/09/20 10:24:12 tuxbox-cvs Exp $

	keybindings setup implementation - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2010 T. Graf 'dbt'
	Homepage: http://www.dbox2-tuning.net/


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

#include <unistd.h>

#include "keybind_setup.h"

#include <global.h>
#include <neutrino.h>
#include <mymenu.h>
#include <neutrino_menue.h>

#include <gui/widget/icons.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/keyboard_input.h>

#include <gui/filebrowser.h>

#include <driver/screen_max.h>
#include <driver/screenshot.h>

#include <system/debug.h>
#include <system/helpers.h>
#include <sys/socket.h>
#include <sys/un.h>

#ifdef IOC_IR_SET_PRI_PROTOCOL
/* define constants instead of #ifdef'ing the corresponding code.
 * the compiler will optimize it away anyway, but the syntax is
 * still checked */
#define RC_HW_SELECT true
#else
#define RC_HW_SELECT false
#ifdef HAVE_COOL_HARDWARE
#warning header coolstream/cs_ir_generic.h not found
#warning you probably have an old driver installation
#warning you´ll be missing the remotecontrol selection feature!
#endif
#endif

CKeybindSetup::CKeybindSetup()
{
	changeNotify(LOCALE_KEYBINDINGMENU_REPEATBLOCKGENERIC, NULL);

	width = 40;
}

CKeybindSetup::~CKeybindSetup()
{
}

int CKeybindSetup::exec(CMenuTarget* parent, const std::string &actionKey)
{
	dprintf(DEBUG_DEBUG, "init keybindings setup\n");
	int   res = menu_return::RETURN_REPAINT;

	if (parent)
	{
		parent->hide();
	}

	if(actionKey == "loadkeys") {
		CFileBrowser fileBrowser;
		CFileFilter fileFilter;
		fileFilter.addFilter("conf");
		fileBrowser.Filter = &fileFilter;
		if (fileBrowser.exec(CONFIGDIR) == true) {
			CNeutrinoApp::getInstance()->loadKeys(fileBrowser.getSelectedFile()->Name.c_str());
			printf("[neutrino keybind_setup] new keys: %s\n", fileBrowser.getSelectedFile()->Name.c_str());
		}
		return menu_return::RETURN_REPAINT;
	}
	else if(actionKey == "savekeys") {
		CFileBrowser fileBrowser;
		fileBrowser.Dir_Mode = true;
		if (fileBrowser.exec("/media") == true) {
			std::string fname = "keys.conf";
			CKeyboardInput * sms = new CKeyboardInput(LOCALE_EXTRA_SAVEKEYS, &fname);
			sms->exec(NULL, "");
			std::string sname = fileBrowser.getSelectedFile()->Name + "/" + fname;
			printf("[neutrino keybind_setup] save keys: %s\n", sname.c_str());
			CNeutrinoApp::getInstance()->saveKeys(sname.c_str());
			delete sms;
		}
		return menu_return::RETURN_REPAINT;
	}

	res = showKeySetup();

	return res;
}

#define KEYBINDINGMENU_REMOTECONTROL_HARDWARE_OPTION_COUNT 4
const CMenuOptionChooser::keyval KEYBINDINGMENU_REMOTECONTROL_HARDWARE_OPTIONS[KEYBINDINGMENU_REMOTECONTROL_HARDWARE_OPTION_COUNT] =
{
	{ CRCInput::RC_HW_COOLSTREAM,   LOCALE_KEYBINDINGMENU_REMOTECONTROL_HARDWARE_COOLSTREAM   },
	{ CRCInput::RC_HW_DBOX,         LOCALE_KEYBINDINGMENU_REMOTECONTROL_HARDWARE_DBOX         },
	{ CRCInput::RC_HW_PHILIPS,      LOCALE_KEYBINDINGMENU_REMOTECONTROL_HARDWARE_PHILIPS      },
	{ CRCInput::RC_HW_TRIPLEDRAGON, LOCALE_KEYBINDINGMENU_REMOTECONTROL_HARDWARE_TRIPLEDRAGON }
};

#define KEYBINDINGMENU_MODE_LEFT_RIGHT_KEY_TV_COUNT 4
const CMenuOptionChooser::keyval KEYBINDINGMENU_MODE_LEFT_RIGHT_KEY_TV_OPTIONS[KEYBINDINGMENU_MODE_LEFT_RIGHT_KEY_TV_COUNT] =
{
	{ SNeutrinoSettings::ZAP,     LOCALE_KEYBINDINGMENU_MODE_LEFT_RIGHT_KEY_TV_ZAP     },
	{ SNeutrinoSettings::VZAP,    LOCALE_KEYBINDINGMENU_MODE_LEFT_RIGHT_KEY_TV_VZAP    },
	{ SNeutrinoSettings::VOLUME,  LOCALE_KEYBINDINGMENU_MODE_LEFT_RIGHT_KEY_TV_VOLUME  },
	{ SNeutrinoSettings::INFOBAR, LOCALE_KEYBINDINGMENU_MODE_LEFT_RIGHT_KEY_TV_INFOBAR }
};
#if 0 //not used
#define KEYBINDINGMENU_BOUQUETHANDLING_OPTION_COUNT 3
const CMenuOptionChooser::keyval KEYBINDINGMENU_BOUQUETHANDLING_OPTIONS[KEYBINDINGMENU_BOUQUETHANDLING_OPTION_COUNT] =
{
	{ 0, LOCALE_KEYBINDINGMENU_BOUQUETCHANNELS_ON_OK },
	{ 1, LOCALE_KEYBINDINGMENU_BOUQUETLIST_ON_OK     },
	{ 2, LOCALE_KEYBINDINGMENU_ALLCHANNELS_ON_OK     }
};
#endif
typedef struct key_settings_t
{
	const neutrino_locale_t keydescription;
	int * keyvalue_p;
	const neutrino_locale_t hint;

} key_settings_struct_t;

const key_settings_struct_t key_settings[CKeybindSetup::KEYBINDS_COUNT] =
{
	{LOCALE_KEYBINDINGMENU_TVRADIOMODE,   	&g_settings.key_tvradio_mode,		LOCALE_MENU_HINT_KEY_TVRADIOMODE },
	{LOCALE_KEYBINDINGMENU_POWEROFF,      	&g_settings.key_power_off,		LOCALE_MENU_HINT_KEY_POWEROFF },
	{LOCALE_KEYBINDINGMENU_PAGEUP, 		&g_settings.key_pageup,			LOCALE_MENU_HINT_KEY_PAGEUP },
	{LOCALE_KEYBINDINGMENU_PAGEDOWN, 	&g_settings.key_pagedown, 		LOCALE_MENU_HINT_KEY_PAGEDOWN },
	{LOCALE_KEYBINDINGMENU_VOLUMEUP, 	&g_settings.key_volumeup,		LOCALE_MENU_HINT_KEY_VOLUMEUP },
	{LOCALE_KEYBINDINGMENU_VOLUMEDOWN,	&g_settings.key_volumedown, 		LOCALE_MENU_HINT_KEY_VOLUMEDOWN },
	{LOCALE_EXTRA_KEY_LIST_START, 		&g_settings.key_list_start, 		LOCALE_MENU_HINT_KEY_LIST_START },
	{LOCALE_EXTRA_KEY_LIST_END,	 	&g_settings.key_list_end,		LOCALE_MENU_HINT_KEY_LIST_END },
	{LOCALE_KEYBINDINGMENU_CANCEL,		&g_settings.key_channelList_cancel,	LOCALE_MENU_HINT_KEY_CANCEL },
	{LOCALE_KEYBINDINGMENU_SORT,		&g_settings.key_channelList_sort,	LOCALE_MENU_HINT_KEY_SORT },
	{LOCALE_KEYBINDINGMENU_ADDRECORD,	&g_settings.key_channelList_addrecord,	LOCALE_MENU_HINT_KEY_ADDRECORD },
	{LOCALE_KEYBINDINGMENU_ADDREMIND,	&g_settings.key_channelList_addremind,	LOCALE_MENU_HINT_KEY_ADDREMIND },
	{LOCALE_KEYBINDINGMENU_BOUQUETUP,	&g_settings.key_bouquet_up, 		LOCALE_MENU_HINT_KEY_BOUQUETUP },
	{LOCALE_KEYBINDINGMENU_BOUQUETDOWN,	&g_settings.key_bouquet_down, 		LOCALE_MENU_HINT_KEY_BOUQUETDOWN },
	{LOCALE_EXTRA_KEY_CURRENT_TRANSPONDER,	&g_settings.key_current_transponder,	LOCALE_MENU_HINT_KEY_TRANSPONDER },
	{LOCALE_KEYBINDINGMENU_CHANNELUP,	&g_settings.key_quickzap_up,		LOCALE_MENU_HINT_KEY_CHANNELUP },
	{LOCALE_KEYBINDINGMENU_CHANNELDOWN,	&g_settings.key_quickzap_down,  	LOCALE_MENU_HINT_KEY_CHANNELDOWN },
	{LOCALE_KEYBINDINGMENU_SUBCHANNELUP,	&g_settings.key_subchannel_up,  	LOCALE_MENU_HINT_KEY_SUBCHANNELUP },
	{LOCALE_KEYBINDINGMENU_SUBCHANNELDOWN,	&g_settings.key_subchannel_down,	LOCALE_MENU_HINT_KEY_SUBCHANNELDOWN },
	{LOCALE_KEYBINDINGMENU_ZAPHISTORY,	&g_settings.key_zaphistory, 		LOCALE_MENU_HINT_KEY_HISTORY },
	{LOCALE_KEYBINDINGMENU_LASTCHANNEL,	&g_settings.key_lastchannel,		LOCALE_MENU_HINT_KEY_LASTCHANNEL },
	{LOCALE_MPKEY_REWIND,			&g_settings.mpkey_rewind,		LOCALE_MENU_HINT_KEY_MPREWIND },
	{LOCALE_MPKEY_FORWARD,			&g_settings.mpkey_forward,  		LOCALE_MENU_HINT_KEY_MPFORWARD },
	{LOCALE_MPKEY_PAUSE,			&g_settings.mpkey_pause, 		LOCALE_MENU_HINT_KEY_MPPAUSE },
	{LOCALE_MPKEY_STOP,			&g_settings.mpkey_stop,			LOCALE_MENU_HINT_KEY_MPSTOP },
	{LOCALE_MPKEY_PLAY,			&g_settings.mpkey_play,			LOCALE_MENU_HINT_KEY_MPPLAY },
	{LOCALE_MPKEY_AUDIO,			&g_settings.mpkey_audio, 		LOCALE_MENU_HINT_KEY_MPAUDIO },
	{LOCALE_MPKEY_SUBTITLE,			&g_settings.mpkey_subtitle,		LOCALE_MENU_HINT_KEY_MPSUBTITLE },
	{LOCALE_MPKEY_TIME,			&g_settings.mpkey_time,			LOCALE_MENU_HINT_KEY_MPTIME },
	{LOCALE_MPKEY_BOOKMARK,			&g_settings.mpkey_bookmark, 		LOCALE_MENU_HINT_KEY_MPBOOKMARK },
	{LOCALE_MPKEY_GOTO,			&g_settings.mpkey_goto,	 		NONEXISTANT_LOCALE},
	{LOCALE_MPKEY_NEXT_REPEAT_MODE,		&g_settings.mpkey_next_repeat_mode,	NONEXISTANT_LOCALE},
	{LOCALE_MPKEY_PLUGIN,			&g_settings.mpkey_plugin,		LOCALE_MENU_HINT_KEY_MPPLUGIN },
	{LOCALE_EXTRA_KEY_TIMESHIFT,		&g_settings.key_timeshift,  		LOCALE_MENU_HINT_KEY_TIMESHIFT },
	{LOCALE_EXTRA_KEY_UNLOCK,		&g_settings.key_unlock,			LOCALE_MENU_HINT_KEY_UNLOCK},
	{LOCALE_EXTRA_KEY_HELP,			&g_settings.key_help,			NONEXISTANT_LOCALE},
	{LOCALE_EXTRA_KEY_NEXT43MODE,		&g_settings.key_next43mode,		NONEXISTANT_LOCALE},
	{LOCALE_EXTRA_KEY_SWITCHFORMAT,		&g_settings.key_switchformat,		NONEXISTANT_LOCALE},
	{LOCALE_EXTRA_KEY_SCREENSHOT,		&g_settings.key_screenshot,		LOCALE_MENU_HINT_KEY_SCREENSHOT },
	{LOCALE_EXTRA_KEY_PIP_CLOSE,		&g_settings.key_pip_close,		LOCALE_MENU_HINT_KEY_PIP_CLOSE },
	{LOCALE_EXTRA_KEY_PIP_SETUP,		&g_settings.key_pip_setup,		LOCALE_MENU_HINT_KEY_PIP_SETUP },
	{LOCALE_EXTRA_KEY_PIP_SWAP,		&g_settings.key_pip_swap,		LOCALE_MENU_HINT_KEY_PIP_CLOSE },
	{LOCALE_EXTRA_KEY_FORMAT_MODE,		&g_settings.key_format_mode_active,	LOCALE_MENU_HINT_KEY_FORMAT_MODE_ACTIVE },
	{LOCALE_EXTRA_KEY_PIC_MODE,		&g_settings.key_pic_mode_active,	LOCALE_MENU_HINT_KEY_PIC_MODE_ACTIVE },
	{LOCALE_EXTRA_KEY_PIC_SIZE,		&g_settings.key_pic_size_active,	LOCALE_MENU_HINT_KEY_PIC_SIZE_ACTIVE },
	{LOCALE_EXTRA_KEY_RECORD,		&g_settings.key_record,			LOCALE_MENU_HINT_KEY_RECORD },
	{LOCALE_MBKEY_COPY_ONEFILE,		&g_settings.mbkey_copy_onefile,		NONEXISTANT_LOCALE },
	{LOCALE_MBKEY_COPY_SEVERAL,		&g_settings.mbkey_copy_several,		NONEXISTANT_LOCALE },
	{LOCALE_MBKEY_CUT,			&g_settings.mbkey_cut,			NONEXISTANT_LOCALE },
	{LOCALE_MBKEY_TRUNCATE,			&g_settings.mbkey_truncate,		NONEXISTANT_LOCALE },
	{LOCALE_MBKEY_COVER,			&g_settings.mbkey_cover,		LOCALE_MENU_HINT_MBKEY_COVER },
};

// used by driver/rcinput.cpp
bool checkLongPress(uint32_t key)
{
	if (g_settings.longkeypress_duration == LONGKEYPRESS_OFF)
		return false;
	if (key == CRCInput::RC_standby)
		return true;
	key |= CRCInput::RC_Repeat;
	for (unsigned int i = 0; i < CKeybindSetup::KEYBINDS_COUNT; i++)
		if ((uint32_t)*key_settings[i].keyvalue_p == key)
			return true;
	for (std::vector<SNeutrinoSettings::usermenu_t*>::iterator it = g_settings.usermenu.begin(); it != g_settings.usermenu.end(); ++it)
		if (*it && (uint32_t)((*it)->key) == key)
			return true;
	return false;
}

int CKeybindSetup::showKeySetup()
{
#if !HAVE_SPARK_HARDWARE
	//save original rc hardware selection and initialize text strings
	int org_remote_control_hardware = g_settings.remote_control_hardware;
	char RC_HW_str[4][32];
	snprintf(RC_HW_str[CRCInput::RC_HW_COOLSTREAM],   sizeof(RC_HW_str[CRCInput::RC_HW_COOLSTREAM])-1,   "%s", g_Locale->getText(LOCALE_KEYBINDINGMENU_REMOTECONTROL_HARDWARE_COOLSTREAM));
	snprintf(RC_HW_str[CRCInput::RC_HW_DBOX],         sizeof(RC_HW_str[CRCInput::RC_HW_DBOX])-1,         "%s", g_Locale->getText(LOCALE_KEYBINDINGMENU_REMOTECONTROL_HARDWARE_DBOX));
	snprintf(RC_HW_str[CRCInput::RC_HW_PHILIPS],      sizeof(RC_HW_str[CRCInput::RC_HW_PHILIPS])-1,      "%s", g_Locale->getText(LOCALE_KEYBINDINGMENU_REMOTECONTROL_HARDWARE_PHILIPS));
	snprintf(RC_HW_str[CRCInput::RC_HW_TRIPLEDRAGON], sizeof(RC_HW_str[CRCInput::RC_HW_TRIPLEDRAGON])-1, "%s", g_Locale->getText(LOCALE_KEYBINDINGMENU_REMOTECONTROL_HARDWARE_TRIPLEDRAGON));
	char RC_HW_msg[256];
	snprintf(RC_HW_msg, sizeof(RC_HW_msg)-1, "%s", g_Locale->getText(LOCALE_KEYBINDINGMENU_REMOTECONTROL_HARDWARE_MSG_PART1));
#endif

	//keysetup menu
	CMenuWidget* keySettings = new CMenuWidget(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP);
	keySettings->addIntroItems(LOCALE_MAINSETTINGS_KEYBINDING);

	//keybindings menu
	CMenuWidget bindSettings(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP_KEYBINDING);

	//keybindings
	for (int i = 0; i < KEYBINDS_COUNT; i++)
		keychooser[i] = new CKeyChooser((unsigned int *) key_settings[i].keyvalue_p, key_settings[i].keydescription/*as head caption*/, NEUTRINO_ICON_SETTINGS);

	showKeyBindSetup(&bindSettings);
	CMenuForwarder * mf;

	mf = new CMenuForwarder(LOCALE_KEYBINDINGMENU_EDIT, true, NULL, &bindSettings, NULL, CRCInput::RC_red);
	mf->setHint("", LOCALE_MENU_HINT_KEY_BINDING);
	keySettings->addItem(mf);

	mf = new CMenuForwarder(LOCALE_EXTRA_SAVEKEYS, true, NULL, this, "savekeys", CRCInput::RC_green);
	mf->setHint("", LOCALE_MENU_HINT_KEY_SAVE);
	keySettings->addItem(mf);

	mf = new CMenuForwarder(LOCALE_EXTRA_LOADKEYS, true, NULL, this, "loadkeys", CRCInput::RC_yellow);
	mf->setHint("", LOCALE_MENU_HINT_KEY_LOAD);
	keySettings->addItem(mf);

	keySettings->addItem(GenericMenuSeparatorLine);

	//rc tuning
	std::string ms_number_format("%d ");
	ms_number_format += g_Locale->getText(LOCALE_UNIT_SHORT_MILLISECOND);
	CMenuOptionNumberChooser *cc;

	int shortcut = 1;

	cc = new CMenuOptionNumberChooser(LOCALE_KEYBINDINGMENU_LONGKEYPRESS_DURATION,
		&g_settings.longkeypress_duration, true, LONGKEYPRESS_OFF, 9999, NULL,
		CRCInput::convertDigitToKey(shortcut++), NULL, 0, LONGKEYPRESS_OFF, LOCALE_OPTIONS_OFF);
	cc->setNumberFormat(ms_number_format);
	cc->setNumericInput(true);
	cc->setHint("", LOCALE_MENU_HINT_LONGKEYPRESS_DURATION);
	keySettings->addItem(cc);

#if HAVE_SPARK_HARDWARE
	g_settings.accept_other_remotes = access("/etc/lircd_predata_lock", R_OK) ? 1 : 0;
	CMenuOptionChooser *mc = new CMenuOptionChooser(LOCALE_KEYBINDINGMENU_ACCEPT_OTHER_REMOTES,
		&g_settings.accept_other_remotes, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this,
		CRCInput::convertDigitToKey(shortcut++));
	mc->setHint("", LOCALE_MENU_HINT_ACCEPT_OTHER_REMOTES);
	keySettings->addItem(mc);
#endif
#if !HAVE_SPARK_HARDWARE
	if (RC_HW_SELECT) {
		CMenuOptionChooser * mc = new CMenuOptionChooser(LOCALE_KEYBINDINGMENU_REMOTECONTROL_HARDWARE,
			&g_settings.remote_control_hardware, KEYBINDINGMENU_REMOTECONTROL_HARDWARE_OPTIONS, KEYBINDINGMENU_REMOTECONTROL_HARDWARE_OPTION_COUNT, true, NULL,
			CRCInput::convertDigitToKey(shortcut++));
		mc->setHint("", LOCALE_MENU_HINT_KEY_HARDWARE);
		keySettings->addItem(mc);
	}
#endif

	cc = new CMenuOptionNumberChooser(LOCALE_KEYBINDINGMENU_REPEATBLOCK,
		&g_settings.repeat_blocker, true, 0, 999, this,
		CRCInput::convertDigitToKey(shortcut++), NULL, 0, 0, LOCALE_OPTIONS_OFF);
	cc->setNumberFormat(ms_number_format);
	cc->setNumericInput(true);
	cc->setHint("", LOCALE_MENU_HINT_KEY_REPEATBLOCK);
	keySettings->addItem(cc);

	cc = new CMenuOptionNumberChooser(LOCALE_KEYBINDINGMENU_REPEATBLOCKGENERIC,
		&g_settings.repeat_genericblocker, true, 0, 999, this,
		CRCInput::convertDigitToKey(shortcut++), NULL, 0, 0, LOCALE_OPTIONS_OFF);
	cc->setNumberFormat(ms_number_format);
	cc->setNumericInput(true);
	cc->setHint("", LOCALE_MENU_HINT_KEY_REPEATBLOCKGENERIC);
	keySettings->addItem(cc);

	int res = keySettings->exec(NULL, "");

#if !HAVE_SPARK_HARDWARE
	//check if rc hardware selection has changed before leaving the menu
	if (org_remote_control_hardware != g_settings.remote_control_hardware) {
		g_RCInput->CRCInput::set_rc_hw();
		strcat(RC_HW_msg, RC_HW_str[org_remote_control_hardware]);
		strcat(RC_HW_msg, g_Locale->getText(LOCALE_KEYBINDINGMENU_REMOTECONTROL_HARDWARE_MSG_PART2));
		strcat(RC_HW_msg, RC_HW_str[g_settings.remote_control_hardware]);
		strcat(RC_HW_msg, g_Locale->getText(LOCALE_KEYBINDINGMENU_REMOTECONTROL_HARDWARE_MSG_PART3));
		if(ShowMsg(LOCALE_MESSAGEBOX_INFO, RC_HW_msg, CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo, NEUTRINO_ICON_INFO, 450, 15) == CMsgBox::mbrNo) {
			g_settings.remote_control_hardware = org_remote_control_hardware;
			g_RCInput->CRCInput::set_rc_hw();
		}
	}
#endif

	delete keySettings;
	for (int i = 0; i < KEYBINDS_COUNT; i++)
		delete keychooser[i];
	return res;
}


void CKeybindSetup::showKeyBindSetup(CMenuWidget *bindSettings)
{
	int shortcut = 1;

	CMenuForwarder * mf;

	bindSettings->addIntroItems(LOCALE_KEYBINDINGMENU_HEAD);

	//modes
	CMenuWidget* bindSettings_modes = new CMenuWidget(LOCALE_KEYBINDINGMENU_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP_KEYBINDING_MODES);
	showKeyBindModeSetup(bindSettings_modes);
	mf = new CMenuDForwarder(LOCALE_KEYBINDINGMENU_MODECHANGE, true, NULL, bindSettings_modes, NULL, CRCInput::RC_red);
	mf->setHint("", LOCALE_MENU_HINT_KEY_MODECHANGE);
	bindSettings->addItem(mf);

	// channellist keybindings
	CMenuWidget* bindSettings_chlist = new CMenuWidget(LOCALE_KEYBINDINGMENU_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP_KEYBINDING_CHANNELLIST);
	showKeyBindChannellistSetup(bindSettings_chlist);
	mf = new CMenuDForwarder(LOCALE_KEYBINDINGMENU_CHANNELLIST, true, NULL, bindSettings_chlist, NULL, CRCInput::RC_green);
	mf->setHint("", LOCALE_MENU_HINT_KEY_CHANNELLIST);
	bindSettings->addItem(mf);

	// Zapping keys quickzap
	CMenuWidget* bindSettings_qzap = new CMenuWidget(LOCALE_KEYBINDINGMENU_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP_KEYBINDING_QUICKZAP);
	showKeyBindQuickzapSetup(bindSettings_qzap);
	mf = new CMenuDForwarder(LOCALE_KEYBINDINGMENU_QUICKZAP, true, NULL, bindSettings_qzap, NULL, CRCInput::RC_yellow);
	mf->setHint("", LOCALE_MENU_HINT_KEY_QUICKZAP);
 	bindSettings->addItem(mf);

	//movieplayer
	CMenuWidget* bindSettings_mplayer = new CMenuWidget(LOCALE_KEYBINDINGMENU_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP_KEYBINDING_MOVIEPLAYER);
	showKeyBindMovieplayerSetup(bindSettings_mplayer);
	mf = new CMenuDForwarder(LOCALE_MAINMENU_MOVIEPLAYER, true, NULL, bindSettings_mplayer, NULL, CRCInput::RC_blue);
	mf->setHint("", LOCALE_MENU_HINT_KEY_MOVIEPLAYER);
	bindSettings->addItem(mf);

	//moviebrowser
	CMenuWidget* bindSettings_mbrowser = new CMenuWidget(LOCALE_KEYBINDINGMENU_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP_KEYBINDING_MOVIEBROWSER);
	showKeyBindMoviebrowserSetup(bindSettings_mbrowser);
	mf = new CMenuDForwarder(LOCALE_MOVIEBROWSER_HEAD, true, NULL, bindSettings_mbrowser, NULL, CRCInput::RC_nokey);
	mf->setHint("", LOCALE_MENU_HINT_KEY_MOVIEBROWSER);
	bindSettings->addItem(mf);

	//video
	bindSettings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_KEYBINDINGMENU_VIDEO));
	for (int i = NKEY_NEXT43MODE; i <= NKEY_SWITCHFORMAT; i++) {
		mf = new CMenuForwarder(key_settings[i].keydescription, true, keychooser[i]->getKeyName(), keychooser[i]);
		mf->setHint("", key_settings[i].hint);
		bindSettings->addItem(mf);
	}

	//navigation
	bindSettings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_KEYBINDINGMENU_NAVIGATION));
	for (int i = NKEY_PAGE_UP; i <= NKEY_PAGE_DOWN; i++) {
		mf = new CMenuForwarder(key_settings[i].keydescription, true, keychooser[i]->getKeyName(), keychooser[i]);
		mf->setHint("", key_settings[i].hint);
		bindSettings->addItem(mf);
	}
	CMenuOptionChooser * mc = new CMenuOptionChooser(LOCALE_EXTRA_MENU_LEFT_EXIT, &g_settings.menu_left_exit, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_KEY_LEFT_EXIT);
	bindSettings->addItem(mc);

	//volume
	bindSettings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_KEYBINDINGMENU_VOLUME));
	for (int i = NKEY_VOLUME_UP; i <= NKEY_VOLUME_DOWN; i++)
		bindSettings->addItem(new CMenuForwarder(key_settings[i].keydescription, true, keychooser[i]->getKeyName(), keychooser[i]));

	//misc
	bindSettings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_KEYBINDINGMENU_MISC));
	//bindSettings->addItem(new CMenuForwarder(keydescription[NKEY_PLUGIN], true, NULL, keychooser[NKEY_PLUGIN]));

	//Special keys
	CMenuWidget* bindSettings_special = new CMenuWidget(LOCALE_KEYBINDINGMENU_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP_KEYBINDING_SPECIAL);
	showKeyBindSpecialSetup(bindSettings_special);
	mf = new CMenuDForwarder(LOCALE_KEYBINDINGMENU_SPECIAL_ACTIVE, true, NULL, bindSettings_special, NULL, CRCInput::convertDigitToKey(shortcut++));
	mf->setHint("", LOCALE_MENU_HINT_KEY_SPECIAL_ACTIVE);
	bindSettings->addItem(mf);

	bindSettings->addItem(new CMenuSeparator());

	// timeshift
	mf = new CMenuForwarder(key_settings[NKEY_TIMESHIFT].keydescription, true, keychooser[NKEY_TIMESHIFT]->getKeyName(), keychooser[NKEY_TIMESHIFT]);
	mf->setHint("", key_settings[NKEY_TIMESHIFT].hint);
	bindSettings->addItem(mf);
	// unlock
	mf = new CMenuForwarder(key_settings[NKEY_UNLOCK].keydescription, true, keychooser[NKEY_UNLOCK]->getKeyName(), keychooser[NKEY_UNLOCK]);
	mf->setHint("", key_settings[NKEY_UNLOCK].hint);
	bindSettings->addItem(mf);
	// screenshot
	mf = new CMenuForwarder(key_settings[NKEY_SCREENSHOT].keydescription, true, keychooser[NKEY_SCREENSHOT]->getKeyName(), keychooser[NKEY_SCREENSHOT]);
	mf->setHint("", key_settings[NKEY_SCREENSHOT].hint);
	bindSettings->addItem(mf);
#ifdef ENABLE_PIP
	// pip
	mf = new CMenuForwarder(key_settings[NKEY_PIP_CLOSE].keydescription, true, keychooser[NKEY_PIP_CLOSE]->getKeyName(), keychooser[NKEY_PIP_CLOSE]);
	mf->setHint("", key_settings[NKEY_PIP_CLOSE].hint);
	bindSettings->addItem(mf);
	mf = new CMenuForwarder(key_settings[NKEY_PIP_SETUP].keydescription, true, keychooser[NKEY_PIP_SETUP]->getKeyName(), keychooser[NKEY_PIP_SETUP]);
	mf->setHint("", key_settings[NKEY_PIP_SETUP].hint);
	bindSettings->addItem(mf);
	mf = new CMenuForwarder(key_settings[NKEY_PIP_SWAP].keydescription, true, keychooser[NKEY_PIP_SWAP]->getKeyName(), keychooser[NKEY_PIP_SWAP]);
	mf->setHint("", key_settings[NKEY_PIP_SWAP].hint);
	bindSettings->addItem(mf);
#endif

	bindSettings->addItem(new CMenuForwarder(key_settings[NKEY_HELP].keydescription, true, keychooser[NKEY_HELP]->getKeyName(), keychooser[NKEY_HELP]));
	bindSettings->addItem(new CMenuForwarder(key_settings[NKEY_RECORD].keydescription, true, keychooser[NKEY_RECORD]->getKeyName(), keychooser[NKEY_RECORD]));

	bindSettings->addItem(new CMenuSeparator());
	// left/right keys
	mc = new CMenuOptionChooser(LOCALE_KEYBINDINGMENU_MODE_LEFT_RIGHT_KEY_TV, &g_settings.mode_left_right_key_tv, KEYBINDINGMENU_MODE_LEFT_RIGHT_KEY_TV_OPTIONS, KEYBINDINGMENU_MODE_LEFT_RIGHT_KEY_TV_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_KEY_RIGHT);
	bindSettings->addItem(mc);
}

void CKeybindSetup::showKeyBindModeSetup(CMenuWidget *bindSettings_modes)
{
	CMenuForwarder * mf;
	bindSettings_modes->addIntroItems(LOCALE_KEYBINDINGMENU_MODECHANGE);

	// tv/radio
	mf = new CMenuForwarder(key_settings[NKEY_TV_RADIO_MODE].keydescription, true, keychooser[NKEY_TV_RADIO_MODE]->getKeyName(), keychooser[NKEY_TV_RADIO_MODE], NULL, CRCInput::RC_red);
	mf->setHint("", key_settings[NKEY_TV_RADIO_MODE].hint);
	bindSettings_modes->addItem(mf);

	mf = new CMenuForwarder(key_settings[NKEY_POWER_OFF].keydescription, true, keychooser[NKEY_POWER_OFF]->getKeyName(), keychooser[NKEY_POWER_OFF], NULL, CRCInput::RC_green);
	mf->setHint("", key_settings[NKEY_POWER_OFF].hint);
	bindSettings_modes->addItem(mf);
}

void CKeybindSetup::showKeyBindChannellistSetup(CMenuWidget *bindSettings_chlist)
{
	bindSettings_chlist->addIntroItems(LOCALE_KEYBINDINGMENU_CHANNELLIST);
#if 0
	CMenuOptionChooser *oj = new CMenuOptionChooser(LOCALE_KEYBINDINGMENU_BOUQUETHANDLING, &g_settings.bouquetlist_mode, KEYBINDINGMENU_BOUQUETHANDLING_OPTIONS, KEYBINDINGMENU_BOUQUETHANDLING_OPTION_COUNT, true );
	bindSettings_chlist->addItem(oj);
#endif
	for (int i = NKEY_LIST_START; i <= NKEY_CURRENT_TRANSPONDER; i++) {
		CMenuForwarder * mf = new CMenuForwarder(key_settings[i].keydescription, true, keychooser[i]->getKeyName(), keychooser[i]);
		mf->setHint("", key_settings[i].hint);
		bindSettings_chlist->addItem(mf);
	}

	CMenuOptionChooser * mc = new CMenuOptionChooser(LOCALE_EXTRA_SMS_CHANNEL, &g_settings.sms_channel, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_SMS_CHANNEL);
	bindSettings_chlist->addItem(mc);
}

void CKeybindSetup::showKeyBindQuickzapSetup(CMenuWidget *bindSettings_qzap)
{
	bindSettings_qzap->addIntroItems(LOCALE_KEYBINDINGMENU_QUICKZAP);

	for (int i = NKEY_CHANNEL_UP; i <= NKEY_LASTCHANNEL; i++) {
		CMenuForwarder * mf = new CMenuForwarder(key_settings[i].keydescription, true, keychooser[i]->getKeyName(), keychooser[i]);
		mf->setHint("", key_settings[i].hint);
		bindSettings_qzap->addItem(mf);
	}
}

void CKeybindSetup::showKeyBindMovieplayerSetup(CMenuWidget *bindSettings_mplayer)
{
	bindSettings_mplayer->addIntroItems(LOCALE_MAINMENU_MOVIEPLAYER);

	for (int i = MPKEY_REWIND; i <= MPKEY_PLUGIN; i++) {
		CMenuForwarder * mf = new CMenuForwarder(key_settings[i].keydescription, true, keychooser[i]->getKeyName(), keychooser[i]);
		mf->setHint("", key_settings[i].hint);
		bindSettings_mplayer->addItem(mf);
	}
}

void CKeybindSetup::showKeyBindMoviebrowserSetup(CMenuWidget *bindSettings_mbrowser)
{
	bindSettings_mbrowser->addIntroItems(LOCALE_MOVIEBROWSER_HEAD);

	for (int i = MBKEY_COPY_ONEFILE; i <= MBKEY_COVER; i++) {
		CMenuForwarder * mf = new CMenuForwarder(key_settings[i].keydescription, true, keychooser[i]->getKeyName(), keychooser[i]);
		mf->setHint("", key_settings[i].hint);
		bindSettings_mbrowser->addItem(mf);
	}

	CMenuOptionChooser * mc = new CMenuOptionChooser(LOCALE_EXTRA_SMS_MOVIE, &g_settings.sms_movie, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_SMS_MOVIE);
	bindSettings_mbrowser->addItem(mc);
}

void CKeybindSetup::showKeyBindSpecialSetup(CMenuWidget *bindSettings_special)
{
	bindSettings_special->addIntroItems(LOCALE_KEYBINDINGMENU_SPECIAL_ACTIVE);

	for (int i = NKEY_FORMAT_MODE; i <= NKEY_PIC_SIZE; i++) {
		CMenuOptionChooser * mf = new CMenuOptionChooser(key_settings[i].keydescription, key_settings[i].keyvalue_p, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
		mf->setHint("", key_settings[i].hint);
		bindSettings_special->addItem(mf);
	}
}

bool CKeybindSetup::changeNotify(const neutrino_locale_t OptionName, void * /* data */)
{
#if HAVE_SPARK_HARDWARE
	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_KEYBINDINGMENU_ACCEPT_OTHER_REMOTES)) {
		struct sockaddr_un sun;
		memset(&sun, 0, sizeof(sun));
		sun.sun_family = AF_UNIX;
		strcpy(sun.sun_path, "/var/run/lirc/lircd");
		int lircd_sock = socket(AF_UNIX, SOCK_STREAM, 0);
		if (lircd_sock > -1) {
			if (!connect(lircd_sock, (struct sockaddr *) &sun, sizeof(sun))) {
				if (g_settings.accept_other_remotes)
					write(lircd_sock, "PREDATA_UNLOCK\n", 16);
				else
					write(lircd_sock, "PREDATA_LOCK\n", 14);
			}
			close(lircd_sock);
		}
		// work around junk data sent by vfd controller
		sleep(2);
		g_RCInput->clearRCMsg();
		return false;
	}
#endif
	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_KEYBINDINGMENU_REPEATBLOCKGENERIC) ||
			ARE_LOCALES_EQUAL(OptionName, LOCALE_KEYBINDINGMENU_REPEATBLOCK)) {
		unsigned int fdelay = g_settings.repeat_blocker;
		unsigned int xdelay = g_settings.repeat_genericblocker;

		g_RCInput->repeat_block = fdelay * 1000;
		g_RCInput->repeat_block_generic = xdelay * 1000;
		g_RCInput->setKeyRepeatDelay(fdelay, xdelay);
	}
	return false;
}

const char *CKeybindSetup::getMoviePlayerButtonName(const neutrino_msg_t key, bool &active, bool return_title)
{
	active = false;
	for (unsigned int i = MPKEY_REWIND; i <= MPKEY_PLUGIN; i++)
	{
		if ((uint32_t)*key_settings[i].keyvalue_p == (unsigned int)key)
		{
			active = true;
			if (!return_title && (key_settings[i].keydescription == LOCALE_MPKEY_PLUGIN))
				return g_settings.movieplayer_plugin.c_str();
			else
				return g_Locale->getText(key_settings[i].keydescription);
		}
	}
	return "";
}
