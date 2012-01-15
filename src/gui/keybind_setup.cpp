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

#ifdef HAVE_COOLSTREAM_NEVIS_IR_H
/* define constants instead of #ifdef'ing the corresponding code.
 * the compiler will optimize it away anyway, but the syntax is
 * still checked */
#define RC_HW_SELECT true
#else
#define RC_HW_SELECT false
#ifdef HAVE_COOL_HARDWARE
#warning header coolstream/nevis_ir.h not found
#warning you probably have an old driver installation
#warning you´ll be missing the remotecontrol selection feature!
#endif
#endif

#include "gui/keybind_setup.h"

#include <global.h>
#include <neutrino.h>
#include <mymenu.h>
#include <neutrino_menue.h>

#include <gui/widget/icons.h>
#include <gui/widget/messagebox.h>
#include <gui/widget/stringinput.h>

#include "gui/filebrowser.h"

#include <driver/screen_max.h>
#ifdef SCREENSHOT
#include <driver/screenshot.h>
#endif

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

#define KEYBINDINGMENU_BOUQUETHANDLING_OPTION_COUNT 3
const CMenuOptionChooser::keyval KEYBINDINGMENU_BOUQUETHANDLING_OPTIONS[KEYBINDINGMENU_BOUQUETHANDLING_OPTION_COUNT] =
{
	{ 0, LOCALE_KEYBINDINGMENU_BOUQUETCHANNELS_ON_OK },
	{ 1, LOCALE_KEYBINDINGMENU_BOUQUETLIST_ON_OK     },
	{ 2, LOCALE_KEYBINDINGMENU_ALLCHANNELS_ON_OK     }
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
	{LOCALE_EXTRA_KEY_CURRENT_TRANSPONDER,	&g_settings.key_current_transponder,	},
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
	{LOCALE_EXTRA_KEY_SCREENSHOT,		&g_settings.key_screenshot,		}
};


int CKeybindSetup::showKeySetup()
{
	//save original rc hardware selection and initialize text strings
	int org_remote_control_hardware = g_settings.remote_control_hardware;
	char RC_HW_str[4][32];
	snprintf(RC_HW_str[CRCInput::RC_HW_COOLSTREAM],   sizeof(RC_HW_str[CRCInput::RC_HW_COOLSTREAM])-1,   "%s", g_Locale->getText(LOCALE_KEYBINDINGMENU_REMOTECONTROL_HARDWARE_COOLSTREAM));
	snprintf(RC_HW_str[CRCInput::RC_HW_DBOX],         sizeof(RC_HW_str[CRCInput::RC_HW_DBOX])-1,         "%s", g_Locale->getText(LOCALE_KEYBINDINGMENU_REMOTECONTROL_HARDWARE_DBOX));
	snprintf(RC_HW_str[CRCInput::RC_HW_PHILIPS],      sizeof(RC_HW_str[CRCInput::RC_HW_PHILIPS])-1,      "%s", g_Locale->getText(LOCALE_KEYBINDINGMENU_REMOTECONTROL_HARDWARE_PHILIPS));
	snprintf(RC_HW_str[CRCInput::RC_HW_TRIPLEDRAGON], sizeof(RC_HW_str[CRCInput::RC_HW_TRIPLEDRAGON])-1, "%s", g_Locale->getText(LOCALE_KEYBINDINGMENU_REMOTECONTROL_HARDWARE_TRIPLEDRAGON));
	char RC_HW_msg[256];
	snprintf(RC_HW_msg, sizeof(RC_HW_msg)-1, "%s", g_Locale->getText(LOCALE_KEYBINDINGMENU_REMOTECONTROL_HARDWARE_MSG_PART1));

	//keysetup menu
	CMenuWidget* keySettings = new CMenuWidget(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP);
	keySettings->addIntroItems(LOCALE_MAINSETTINGS_KEYBINDING);
	
	//keybindings menu
	CMenuWidget* bindSettings = new CMenuWidget(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_KEYBINDING, width, MN_WIDGET_ID_KEYSETUP_KEYBINDING);
	
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
	if (RC_HW_SELECT)
		keySettings->addItem(new CMenuOptionChooser(LOCALE_KEYBINDINGMENU_REMOTECONTROL_HARDWARE, &g_settings.remote_control_hardware, KEYBINDINGMENU_REMOTECONTROL_HARDWARE_OPTIONS, KEYBINDINGMENU_REMOTECONTROL_HARDWARE_OPTION_COUNT, true));
	keySettings->addItem(new CMenuForwarder(LOCALE_KEYBINDINGMENU_REPEATBLOCK, true, g_settings.repeat_blocker, keySettings_repeatBlocker));
	keySettings->addItem(new CMenuForwarder(LOCALE_KEYBINDINGMENU_REPEATBLOCKGENERIC, true, g_settings.repeat_genericblocker, keySettings_repeat_genericblocker));

	int res = keySettings->exec(NULL, "");
	keySettings->hide();

	//check if rc hardware selection has changed before leaving the menu
	if (org_remote_control_hardware != g_settings.remote_control_hardware) {
		g_RCInput->CRCInput::set_rc_hw();
		strcat(RC_HW_msg, RC_HW_str[org_remote_control_hardware]);
		strcat(RC_HW_msg, g_Locale->getText(LOCALE_KEYBINDINGMENU_REMOTECONTROL_HARDWARE_MSG_PART2));
		strcat(RC_HW_msg, RC_HW_str[g_settings.remote_control_hardware]);
		strcat(RC_HW_msg, g_Locale->getText(LOCALE_KEYBINDINGMENU_REMOTECONTROL_HARDWARE_MSG_PART3));
		if(ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, RC_HW_msg, CMessageBox::mbrNo, CMessageBox::mbYes | CMessageBox::mbNo, NEUTRINO_ICON_INFO, 450, 15, true) == CMessageBox::mbrNo) {
			g_settings.remote_control_hardware = org_remote_control_hardware;
			g_RCInput->CRCInput::set_rc_hw();
		}
	}	

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
#ifdef SCREENSHOT
	bindSettings->addItem(new CMenuForwarder(key_settings[KEY_SCREENSHOT].keydescription, true, keychooser[KEY_SCREENSHOT]->getKeyName(), keychooser[KEY_SCREENSHOT]));
#endif
	//bindSettings->addItem(new CMenuOptionChooser(LOCALE_EXTRA_ZAP_CYCLE, &g_settings.zap_cycle, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	bindSettings->addItem(new CMenuOptionChooser(LOCALE_EXTRA_MENU_LEFT_EXIT, &g_settings.menu_left_exit, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	bindSettings->addItem(new CMenuOptionChooser(LOCALE_EXTRA_AUDIO_RUN_PLAYER, &g_settings.audio_run_player, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	bindSettings->addItem(new CMenuOptionChooser(LOCALE_KEYBINDINGMENU_MODE_LEFT_RIGHT_KEY_TV, &g_settings.mode_left_right_key_tv, KEYBINDINGMENU_MODE_LEFT_RIGHT_KEY_TV_OPTIONS, KEYBINDINGMENU_MODE_LEFT_RIGHT_KEY_TV_COUNT, true));
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

	for (int i = KEY_PAGE_UP; i <= KEY_CURRENT_TRANSPONDER; i++)
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

