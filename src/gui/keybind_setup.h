/*
	$port: keybind_setup.h,v 1.2 2010/09/07 09:22:36 tuxbox-cvs Exp $

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

#ifndef __keybind_setup__
#define __keybind_setup__

#include <gui/widget/menue.h>
#include <gui/widget/icons.h>
#include <gui/widget/keychooser.h>

#include <system/setting_helpers.h>

#include <string>

class CKeybindSetup : public CMenuTarget
{
	public:	
		enum keynames 
		{
			KEY_TV_RADIO_MODE,
			KEY_POWER_OFF,
			KEY_PAGE_UP,
			KEY_PAGE_DOWN,
			KEY_LIST_START,
			KEY_LIST_END,
			KEY_CANCEL_ACTION,
			KEY_SORT,
			KEY_ADD_RECORD,
			KEY_ADD_REMIND,
			KEY_BOUQUET_UP,
			KEY_BOUQUET_DOWN,
			KEY_CURRENT_TRANSPONDER,
			KEY_CHANNEL_UP,
			KEY_CHANNEL_DOWN,
			KEY_SUBCHANNEL_UP,
			KEY_SUBCHANNEL_DOWN,
			KEY_ZAP_HISTORY,
			KEY_LASTCHANNEL,
			MPKEY_REWIND,
			MPKEY_FORWARD,
			MPKEY_PAUSE,
			MPKEY_STOP,
			MPKEY_PLAY,
			MPKEY_AUDIO,
			MPKEY_TIME,
			MPKEY_BOOKMARK,
			KEY_TIMESHIFT,
			MPKEY_PLUGIN,
			KEY_PLUGIN,
			KEY_UNLOCK,
			KEY_SCREENSHOT,
			
			KEYBINDS_COUNT
		};

	private:
		CFrameBuffer *frameBuffer;
		CKeySetupNotifier      *keySetupNotifier;
		CKeyChooser *keychooser[KEYBINDS_COUNT];
		int width;

		int showKeySetup();
		void showKeyBindSetup(CMenuWidget *bindSettings);
		void showKeyBindModeSetup(CMenuWidget *bindSettings_modes);
		void showKeyBindChannellistSetup(CMenuWidget *bindSettings_chlist);
		void showKeyBindQuickzapSetup(CMenuWidget *bindSettings_qzap);
		void showKeyBindMovieplayerSetup(CMenuWidget *bindSettings_mplayer);

	public:				
		CKeybindSetup();
		~CKeybindSetup();
		int exec(CMenuTarget* parent, const std::string & actionKey);
};


#endif
		
