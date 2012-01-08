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


#include "gui/keybind_setup.h"

#include <global.h>
#include <neutrino.h>
#include <mymenu.h>
#include <neutrino_menue.h>

#include <gui/widget/icons.h>
#include <gui/widget/stringinput.h>

#include "gui/filebrowser.h"

#include <driver/screen_max.h>
#include <driver/screenshot.h>

#include <system/debug.h>


CKeybindSetup::CKeybindSetup()
{
	keySetupNotifier = new CKeySetupNotifier;
	keySetupNotifier->changeNotify(NONEXISTANT_LOCALE, NULL);

	width = w_max (40, 10);
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
		if (fileBrowser.exec("/var/tuxbox/config") == true) {
			CNeutrinoApp::getInstance()->loadKeys(fileBrowser.getSelectedFile()->Name.c_str());
			printf("[neutrino keybind_setup] new keys: %s\n", fileBrowser.getSelectedFile()->Name.c_str());
		}
		return menu_return::RETURN_REPAINT;
	}
	else if(actionKey == "savekeys") {
		CFileBrowser fileBrowser;
		fileBrowser.Dir_Mode = true;
		if (fileBrowser.exec("/var/tuxbox") == true) {
			char  fname[256] = "keys.conf", sname[256];
			CStringInputSMS * sms = new CStringInputSMS(LOCALE_EXTRA_SAVEKEYS, fname, 30, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "abcdefghijklmnopqrstuvwxyz0123456789. ");
			sms->exec(NULL, "");
			sprintf(sname, "%s/%s", fileBrowser.getSelectedFile()->Name.c_str(), fname);
			printf("[neutrino keybind_setup] save keys: %s\n", sname);
			CNeutrinoApp::getInstance()->saveKeys(sname);
			delete sms;
		}
		return menu_return::RETURN_REPAINT;
	}

	res = showKeySetup();
	
	return res;
}

#define KEYBINDINGMENU_REMOTECONTROL_OPTION_COUNT 2
const CMenuOptionChooser::keyval KEYBINDINGMENU_REMOTECONTROL_OPTIONS[KEYBINDINGMENU_REMOTECONTROL_OPTION_COUNT] =
{
	{ CKeybindSetup::REMOTECONTROL_STANDARD, LOCALE_KEYBINDINGMENU_REMOTECONTROL_STANDARD },
	{ CKeybindSetup::REMOTECONTROL_NEO1,     LOCALE_KEYBINDINGMENU_REMOTECONTROL_NEO1     }
};

#define KEYBINDINGMENU_BOUQUETHANDLING_OPTION_COUNT 3
const CMenuOptionChooser::keyval KEYBINDINGMENU_BOUQUETHANDLING_OPTIONS[KEYBINDINGMENU_BOUQUETHANDLING_OPTION_COUNT] =
{
	{ 0, LOCALE_KEYBINDINGMENU_BOUQUETCHANNELS_ON_OK },
	{ 1, LOCALE_KEYBINDINGMENU_BOUQUETLIST_ON_OK     },
	{ 2, LOCALE_KEYBINDINGMENU_ALLCHANNELS_ON_OK     }
};

#define KEYBINDINGMENU_SCREENSHOT_FMT_OPTION_COUNT 2
const CMenuOptionChooser::keyval_ext KEYBINDINGMENU_SCREENSHOT_FMT_OPTIONS[KEYBINDINGMENU_SCREENSHOT_FMT_OPTION_COUNT] =
{
	{ CScreenShot::FORMAT_PNG,   NONEXISTANT_LOCALE, "PNG"  },
	{ CScreenShot::FORMAT_JPG,   NONEXISTANT_LOCALE, "JPEG" }
};

typedef struct key_settings_t
{
	const neutrino_locale_t keydescription;
	int * keyvalue_p;

} key_settings_struct_t;

const key_settings_struct_t key_settings[CKeybindSetup::KEYBINDS_COUNT] =
{                                                                                                            
	{LOCALE_KEYBINDINGMENU_TVRADIOMODE,   	&g_settings.key_tvradio_mode,		},
	{LOCALE_KEYBINDINGMENU_POWEROFF,      	&g_settings.key_power_off,		},
	{LOCALE_KEYBINDINGMENU_PAGEUP, 		&g_settings.key_channelList_pageup,	},
	{LOCALE_KEYBINDINGMENU_PAGEDOWN, 	&g_settings.key_channelList_pagedown, 	},
	{LOCALE_EXTRA_KEY_LIST_START, 		&g_settings.key_list_start, 		},
	{LOCALE_EXTRA_KEY_LIST_END,	 	&g_settings.key_list_end,		},
	{LOCALE_KEYBINDINGMENU_CANCEL,		&g_settings.key_channelList_cancel,	},
	{LOCALE_KEYBINDINGMENU_SORT,		&g_settings.key_channelList_sort,	},
	{LOCALE_KEYBINDINGMENU_ADDRECORD,	&g_settings.key_channelList_addrecord,	},
	{LOCALE_KEYBINDINGMENU_ADDREMIND,	&g_settings.key_channelList_addremind,	},
	{LOCALE_KEYBINDINGMENU_BOUQUETUP,	&g_settings.key_bouquet_up, 		},
	{LOCALE_KEYBINDINGMENU_BOUQUETDOWN,	&g_settings.key_bouquet_down, 		},
	{LOCALE_KEYBINDINGMENU_CHANNELUP,	&g_settings.key_quickzap_up,		},
	{LOCALE_KEYBINDINGMENU_CHANNELDOWN,	&g_settings.key_quickzap_down,  	},
	{LOCALE_KEYBINDINGMENU_SUBCHANNELUP,	&g_settings.key_subchannel_up,  	},
	{LOCALE_KEYBINDINGMENU_SUBCHANNELDOWN,	&g_settings.key_subchannel_down,	},
	{LOCALE_KEYBINDINGMENU_ZAPHISTORY,	&g_settings.key_zaphistory, 		},
	{LOCALE_KEYBINDINGMENU_LASTCHANNEL,	&g_settings.key_lastchannel,		},
	{LOCALE_MPKEY_REWIND,			&g_settings.mpkey_rewind,		}, 
	{LOCALE_MPKEY_FORWARD,			&g_settings.mpkey_forward,  		},
	{LOCALE_MPKEY_PAUSE,			&g_settings.mpkey_pause, 		},
	{LOCALE_MPKEY_STOP,			&g_settings.mpkey_stop,			},
	{LOCALE_MPKEY_PLAY,			&g_settings.mpkey_play,			},
	{LOCALE_MPKEY_AUDIO,			&g_settings.mpkey_audio, 		},
	{LOCALE_MPKEY_TIME,			&g_settings.mpkey_time,			},
	{LOCALE_MPKEY_BOOKMARK,			&g_settings.mpkey_bookmark, 		},
	{LOCALE_EXTRA_KEY_TIMESHIFT,		&g_settings.key_timeshift,  		},
	{LOCALE_MPKEY_PLUGIN,			&g_settings.mpkey_plugin,		},
	{LOCALE_EXTRA_KEY_PLUGIN,		&g_settings.key_plugin,			},
	{LOCALE_EXTRA_KEY_UNLOCK,		&g_settings.key_unlock,			},
	{LOCALE_EXTRA_KEY_SCREENSHOT,		&g_settings.key_screenshot,			},
};


int CKeybindSetup::showKeySetup()
{
	//keysetup menu
	CMenuWidget* keySettings = new CMenuWidget(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP);
	
	//keybindings menu
	CMenuWidget* bindSettings = new CMenuWidget(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP_KEYBINDING);
	keySettings->addIntroItems(LOCALE_MAINSETTINGS_KEYBINDING);
	
	//keybindings
	int shortcut = 1;
	showKeyBindSetup(bindSettings);
	keySettings->addItem(new CMenuForwarder(LOCALE_KEYBINDINGMENU_HEAD, true, NULL, bindSettings, NULL, CRCInput::convertDigitToKey(shortcut++)));
	keySettings->addItem(GenericMenuSeparator);
	keySettings->addItem(new CMenuForwarder(LOCALE_EXTRA_LOADKEYS, true, NULL, this, "loadkeys", CRCInput::convertDigitToKey(shortcut++)));
	keySettings->addItem(new CMenuForwarder(LOCALE_EXTRA_SAVEKEYS, true, NULL, this, "savekeys", CRCInput::convertDigitToKey(shortcut++)));

	//rc tuning
	keySetupNotifier = new CKeySetupNotifier;
	CStringInput * keySettings_repeat_genericblocker = new CStringInput(LOCALE_KEYBINDINGMENU_REPEATBLOCKGENERIC, g_settings.repeat_genericblocker, 3, LOCALE_REPEATBLOCKER_HINT_1, LOCALE_REPEATBLOCKER_HINT_2, "0123456789 ", keySetupNotifier);
	CStringInput * keySettings_repeatBlocker = new CStringInput(LOCALE_KEYBINDINGMENU_REPEATBLOCK, g_settings.repeat_blocker, 3, LOCALE_REPEATBLOCKER_HINT_1, LOCALE_REPEATBLOCKER_HINT_2, "0123456789 ", keySetupNotifier);
	keySetupNotifier->changeNotify(NONEXISTANT_LOCALE, NULL);

	keySettings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_KEYBINDINGMENU_RC));
	keySettings->addItem(new CMenuForwarder(LOCALE_KEYBINDINGMENU_REPEATBLOCK, true, g_settings.repeat_blocker, keySettings_repeatBlocker));
	keySettings->addItem(new CMenuForwarder(LOCALE_KEYBINDINGMENU_REPEATBLOCKGENERIC, true, g_settings.repeat_genericblocker, keySettings_repeat_genericblocker));
	keySettings->addItem(new CMenuOptionChooser(LOCALE_KEYBINDINGMENU_REMOTECONTROL, &g_settings.remote_control_hardware, KEYBINDINGMENU_REMOTECONTROL_OPTIONS, KEYBINDINGMENU_REMOTECONTROL_OPTION_COUNT, true));

	
	int res = keySettings->exec(NULL, "");
	keySettings->hide();
	delete keySettings;
	return res;
}


void CKeybindSetup::showKeyBindSetup(CMenuWidget *bindSettings)
{
	bindSettings->addIntroItems(LOCALE_KEYBINDINGMENU_HEAD);
	
	for (int i = 0; i < KEYBINDS_COUNT; i++)
		keychooser[i] = new CKeyChooser(key_settings[i].keyvalue_p, key_settings[i].keydescription/*as head caption*/, NEUTRINO_ICON_SETTINGS);
	
	//modes
	CMenuWidget* bindSettings_modes = new CMenuWidget(LOCALE_KEYBINDINGMENU_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP_KEYBINDING_MODES);
	showKeyBindModeSetup(bindSettings_modes);
	bindSettings->addItem(new CMenuForwarder(LOCALE_KEYBINDINGMENU_MODECHANGE, true, NULL, bindSettings_modes, NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));

	// channellist keybindings
	CMenuWidget* bindSettings_chlist = new CMenuWidget(LOCALE_KEYBINDINGMENU_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP_KEYBINDING_CHANNELLIST);
	showKeyBindChannellistSetup(bindSettings_chlist);
	bindSettings->addItem(new CMenuForwarder(LOCALE_KEYBINDINGMENU_CHANNELLIST, true, NULL, bindSettings_chlist, NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));

	// Zapping keys quickzap
	CMenuWidget* bindSettings_qzap = new CMenuWidget(LOCALE_KEYBINDINGMENU_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP_KEYBINDING_QUICKZAP);
	showKeyBindQuickzapSetup(bindSettings_qzap);
 	bindSettings->addItem(new CMenuForwarder(LOCALE_KEYBINDINGMENU_QUICKZAP, true, NULL, bindSettings_qzap, NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));

	//movieplayer
	CMenuWidget* bindSettings_mplayer = new CMenuWidget(LOCALE_KEYBINDINGMENU_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP_KEYBINDING_MOVIEPLAYER);
	showKeyBindMovieplayerSetup(bindSettings_mplayer);
	bindSettings->addItem(new CMenuForwarder(LOCALE_MAINMENU_MOVIEPLAYER, true, NULL, bindSettings_mplayer, NULL, CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE));

	//misc
	bindSettings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_KEYBINDINGMENU_MISC));
	//bindSettings->addItem(new CMenuForwarder(keydescription[KEY_PLUGIN], true, NULL, keychooser[KEY_PLUGIN]));
	bindSettings->addItem(new CMenuForwarder(key_settings[KEY_UNLOCK].keydescription, true, keychooser[KEY_UNLOCK]->getKeyName(), keychooser[KEY_UNLOCK]));
	bindSettings->addItem(new CMenuForwarder(key_settings[KEY_SCREENSHOT].keydescription, true, keychooser[KEY_SCREENSHOT]->getKeyName(), keychooser[KEY_SCREENSHOT]));
	bindSettings->addItem(new CMenuOptionNumberChooser(LOCALE_SCREENSHOT_COUNT, &g_settings.screenshot_count, true, 1, 5, NULL));
	bindSettings->addItem(new CMenuOptionChooser(LOCALE_SCREENSHOT_FORMAT, &g_settings.screenshot_format, KEYBINDINGMENU_SCREENSHOT_FMT_OPTIONS, KEYBINDINGMENU_SCREENSHOT_FMT_OPTION_COUNT, true));
	//bindSettings->addItem(new CMenuOptionChooser(LOCALE_EXTRA_ZAP_CYCLE, &g_settings.zap_cycle, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	bindSettings->addItem(new CMenuOptionChooser(LOCALE_EXTRA_MENU_LEFT_EXIT, &g_settings.menu_left_exit, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));

	bindSettings->addItem(new CMenuOptionChooser(LOCALE_EXTRA_AUDIO_RUN_PLAYER, &g_settings.audio_run_player, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
}

void CKeybindSetup::showKeyBindModeSetup(CMenuWidget *bindSettings_modes)
{
	bindSettings_modes->addIntroItems(LOCALE_KEYBINDINGMENU_MODECHANGE);
	
	bindSettings_modes->addItem(new CMenuForwarder(key_settings[KEY_TV_RADIO_MODE].keydescription, true, keychooser[KEY_TV_RADIO_MODE]->getKeyName(), keychooser[KEY_TV_RADIO_MODE], NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
	bindSettings_modes->addItem(new CMenuForwarder(key_settings[KEY_POWER_OFF].keydescription, true, keychooser[KEY_POWER_OFF]->getKeyName(), keychooser[KEY_POWER_OFF], NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));
}

void CKeybindSetup::showKeyBindChannellistSetup(CMenuWidget *bindSettings_chlist)
{
	bindSettings_chlist->addIntroItems(LOCALE_KEYBINDINGMENU_CHANNELLIST);
	CMenuOptionChooser *oj = new CMenuOptionChooser(LOCALE_KEYBINDINGMENU_BOUQUETHANDLING, &g_settings.bouquetlist_mode, KEYBINDINGMENU_BOUQUETHANDLING_OPTIONS, KEYBINDINGMENU_BOUQUETHANDLING_OPTION_COUNT, true );
	bindSettings_chlist->addItem(oj);

	for (int i = KEY_PAGE_UP; i <= KEY_BOUQUET_DOWN; i++)
		bindSettings_chlist->addItem(new CMenuForwarder(key_settings[i].keydescription, true, keychooser[i]->getKeyName(), keychooser[i]));

	bindSettings_chlist->addItem(new CMenuOptionChooser(LOCALE_EXTRA_SMS_CHANNEL, &g_settings.sms_channel, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
}

void CKeybindSetup::showKeyBindQuickzapSetup(CMenuWidget *bindSettings_qzap)
{
	bindSettings_qzap->addIntroItems(LOCALE_KEYBINDINGMENU_QUICKZAP);

	for (int i = KEY_CHANNEL_UP; i <= KEY_LASTCHANNEL; i++)
		bindSettings_qzap->addItem(new CMenuForwarder(key_settings[i].keydescription, true, keychooser[i]->getKeyName(), keychooser[i]));
}

void CKeybindSetup::showKeyBindMovieplayerSetup(CMenuWidget *bindSettings_mplayer)
{
	bindSettings_mplayer->addIntroItems(LOCALE_MAINMENU_MOVIEPLAYER);
	
	for (int i = MPKEY_REWIND; i <= MPKEY_PLUGIN; i++)
		bindSettings_mplayer->addItem(new CMenuForwarder(key_settings[i].keydescription, true, keychooser[i]->getKeyName(), keychooser[i]));
}

