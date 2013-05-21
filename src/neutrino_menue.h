/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Headerfile: neutrino_menue.h,
	Copyright (C) 2011 Thilo Graf 'dbt'
	Homepage: http://www.dbox2-tuning.net
	
        License: GPL

        This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Library General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Library General Public License for more details.

	You should have received a copy of the GNU Library General Public
	License along with this library; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.

		
	NOTE for ignorant distributors:
	It's not allowed to distribute any compiled parts of this code, if you don't accept the terms of GPL.
	Please read it and understand it right!
	This means for you: Hold it, if not, leave it! You could face legal action! 
	Otherwise ask the copyright owners, anything else would be theft!
*/


#ifndef __neutrino_menue__
#define __neutrino_menue__


//enums for menu widget indicies, 
enum MN_WIDGET_ID
{
	//settings manager
	MN_WIDGET_ID_SETTINGS_MNGR,

	//network setup
	MN_WIDGET_ID_NETWORKSETUP,
	MN_WIDGET_ID_NETWORKSETUP_NTP,
	MN_WIDGET_ID_NETWORKSETUP_MOUNTS,
	
	//proxysetup
	MN_WIDGET_ID_PROXYSETUP,
	
	//osd setup
	MN_WIDGET_ID_OSDSETUP,
	MN_WIDGET_ID_OSDSETUP_MENUS,
	MN_WIDGET_ID_OSDSETUP_CHANNELLIST,
	MN_WIDGET_ID_OSDSETUP_EVENTLIST,
	MN_WIDGET_ID_OSDSETUP_FONT,
	MN_WIDGET_ID_OSDSETUP_FONTSCALE,
	MN_WIDGET_ID_OSDSETUP_INFOBAR,
	MN_WIDGET_ID_OSDSETUP_MENUCOLORS,
	MN_WIDGET_ID_OSDSETUP_TIMEOUT,
	MN_WIDGET_ID_OSDSETUP_SCREENSHOT,
	MN_WIDGET_ID_OSDSETUP_VOLUME,
	//actually it does not matter, but these 6 entries must be the same order as in menu
	MN_WIDGET_ID_OSDSETUP_FONTSIZE_MENU,
	MN_WIDGET_ID_OSDSETUP_FONTSIZE_CHANNELLIST,
	MN_WIDGET_ID_OSDSETUP_FONTSIZE_EVENTLIST,
	MN_WIDGET_ID_OSDSETUP_FONTSIZE_EPG,
	MN_WIDGET_ID_OSDSETUP_FONTSIZE_INFOBAR,
	MN_WIDGET_ID_OSDSETUP_FONTSIZE_GAMELIST,
	
	//progressbar setup
	MN_WIDGET_ID_PROGRESSBAR,

	//language setup
	MN_WIDGET_ID_LANGUAGESETUP,
	MN_WIDGET_ID_LANGUAGESETUP_LOCALE,
	MN_WIDGET_ID_LANGUAGESETUP_PREFAUDIO_LANGUAGE,
			
	//recording settings
	MN_WIDGET_ID_RECORDSETUP,
	MN_WIDGET_ID_RECORDSETUP_TIMESHIFT,
	MN_WIDGET_ID_RECORDSETUP_TIMERSETTINGS,
	MN_WIDGET_ID_RECORDSETUP_AUDIOSETTINGS,
	MN_WIDGET_ID_RECORDSETUP_DATASETTINGS,
	
	//vfd setup
	MN_WIDGET_ID_VFDSETUP,
	MN_WIDGET_ID_VFDSETUP_LCD_SLIDERS,
	MN_WIDGET_ID_VFDSETUP_LED_SETUP,
	
	//keybind setup
	MN_WIDGET_ID_KEYSETUP,
	MN_WIDGET_ID_KEYSETUP_KEYBINDING,
	MN_WIDGET_ID_KEYSETUP_KEYBINDING_MODES,
	MN_WIDGET_ID_KEYSETUP_KEYBINDING_CHANNELLIST,
	MN_WIDGET_ID_KEYSETUP_KEYBINDING_QUICKZAP,
	MN_WIDGET_ID_KEYSETUP_KEYBINDING_MOVIEPLAYER,
	
	//picture viewer setup
	MN_WIDGET_ID_PVIEWERSETUP,
	
	//audio setup
	MN_WIDGET_ID_AUDIOSETUP,
	
	//misc settings
	MN_WIDGET_ID_MISCSETUP,
	MN_WIDGET_ID_MISCSETUP_GENERAL,
	MN_WIDGET_ID_MISCSETUP_ENERGY,
	MN_WIDGET_ID_MISCSETUP_EPG,
	MN_WIDGET_ID_MISCSETUP_FILEBROWSER,
	MN_WIDGET_ID_MISCSETUP_CHANNELLIST,
	
	//media menu
	MN_WIDGET_ID_MEDIA,
	MN_WIDGET_ID_MEDIA_MOVIEPLAYER,
	
	//parentallock setup
	MN_WIDGET_ID_PLOCKSETUP,
	
	//drive setup
	MN_WIDGET_ID_DRIVESETUP,
	
	//zapit settings (start channel)
	MN_WIDGET_ID_ZAPIT,
	
	//cec setup
	MN_WIDGET_ID_CEC,
	
	//infomenue
	MN_WIDGET_ID_INFOMENUE,
	
	//personalize
	MN_WIDGET_ID_PERSONALIZE,
	//personalize: to personalize option menues
	MN_WIDGET_ID_PERSONALIZE_MAIN,
	MN_WIDGET_ID_PERSONALIZE_SETTINGS,
	MN_WIDGET_ID_PERSONALIZE_SERVICE,
	MN_WIDGET_ID_PERSONALIZE_MEDIA,
	MN_WIDGET_ID_PERSONALIZE_MOVIEPLAYER,
	//personalize: to personalize sub menues
	MN_WIDGET_ID_PERSONALIZE_USERMENU,
	MN_WIDGET_ID_PERSONALIZE_FEATUREKEYS,

	//user menu setup
	MN_WIDGET_ID_USERMENU_RED,
	MN_WIDGET_ID_USERMENU_GREEN,
	MN_WIDGET_ID_USERMENU_YELLOW,
	MN_WIDGET_ID_USERMENU_BLUE,
	
	//update file selector
	MN_WIDGET_ID_IMAGESELECTOR,
	MN_WIDGET_ID_MTDREAD_SELECTOR,
	MN_WIDGET_ID_MTDWRITE_SELECTOR,
	MN_WIDGET_ID_FILESELECTOR,
	
	//software update
	MN_WIDGET_ID_SOFTWAREUPDATE,
	MN_WIDGET_ID_MTDEXPERT,
	
	//software update settings
	MN_WIDGET_ID_SOFTWAREUPDATE_SETTINGS,

	MN_WIDGET_ID_SCAN_MAIN,
	MN_WIDGET_ID_SCAN_FE_SETUP,
	MN_WIDGET_ID_SCAN_AUTO_SCAN,
	MN_WIDGET_ID_SCAN_AUTO_SCAN_ALL,
	MN_WIDGET_ID_SCAN_MANUAL_SCAN,
	MN_WIDGET_ID_SCAN_FAST_SCAN,
	MN_WIDGET_ID_SCAN_CABLE_SCAN,

	MN_WIDGET_ID_TESTMENU,
	MN_WIDGET_ID_TESTMENU_HARDWARE,
	MN_WIDGET_ID_TESTMENU_COMPONENTS,

	MN_WIDGET_ID_MAX
};


#endif
