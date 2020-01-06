/*
	Neutrino-GUI  -   DBoxII-Project
	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/
			
	Rework:
	Outsourced subchannel select modul for Neutrino-HD
	Copyright (C) 2011 T.Graf 'dbt'
	http://www.dbox2-tuning.net

	License: GPL

	This program is free software; you can redistribute it and/or modify it under the terms of the GNU
	General Public License as published by the Free Software Foundation; either version 2 of the License, 
	or (at your option) any later version.

	This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
	without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along with this program; 
	if not, write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110, USA
		
		
	NOTE for ignorant distributors:
	It's not allowed to distribute any compiled parts of this code, if you don't accept the terms of GPL.
	Please read it and understand it right!
	This means for you: Hold it, if not, leave it! You could face legal action! 
	Otherwise ask the copyright owners, anything else would be theft!
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include <global.h>
#include <neutrino.h>
#include <mymenu.h>

#include "subchannel_select.h"
#include <driver/record.h>

extern CRemoteControl * g_RemoteControl;

CSubChannelSelectMenu::CSubChannelSelectMenu()
{

}

CSubChannelSelectMenu::~CSubChannelSelectMenu()
{

}

int CSubChannelSelectMenu::getNVODMenu(CMenuWidget* menu)
{
	if (menu == NULL)
		return false;
	if (g_RemoteControl->subChannels.empty())
		return false;

	menu->addItem(GenericMenuSeparator);

	int count = 0;
	char nvod_id[5];

	for ( CSubServiceListSorted::iterator e=g_RemoteControl->subChannels.begin(); e!=g_RemoteControl->subChannels.end(); ++e)
	{
		snprintf(nvod_id, sizeof(nvod_id), "%d", count);

		t_channel_id subid = e->getChannelID();
		bool enabled = CRecordManager::getInstance()->SameTransponder(subid);

		if ( !g_RemoteControl->are_subchannels ) 
		{
			char nvod_time_a[50], nvod_time_e[50], nvod_time_x[50];
			char nvod_s[152];
			struct  tm *tmZeit;

			tmZeit= localtime(&e->startzeit);
			snprintf(nvod_time_a, sizeof(nvod_time_a), "%02d:%02d", tmZeit->tm_hour, tmZeit->tm_min);

			time_t endtime = e->startzeit+ e->dauer;
			tmZeit= localtime(&endtime);
			snprintf(nvod_time_e, sizeof(nvod_time_e), "%02d:%02d", tmZeit->tm_hour, tmZeit->tm_min);

			time_t jetzt=time(NULL);
			if (e->startzeit > jetzt) 
			{
				int mins=(e->startzeit- jetzt)/ 60;
				snprintf(nvod_time_x, sizeof(nvod_time_x), g_Locale->getText(LOCALE_NVOD_STARTING), mins);
			}
			else if ( (e->startzeit<= jetzt) && (jetzt < endtime) ) 
			{
				int proz=(jetzt- e->startzeit)*100/ e->dauer;
				snprintf(nvod_time_x, sizeof(nvod_time_x), g_Locale->getText(LOCALE_NVOD_PERCENTAGE), proz);
			}
			else
				nvod_time_x[0]= 0;

			snprintf(nvod_s, sizeof(nvod_s), "%s - %s %s", nvod_time_a, nvod_time_e, nvod_time_x);
			menu->addItem(new CMenuForwarder(nvod_s, enabled, NULL, &NVODChanger, nvod_id), (count == g_RemoteControl->selected_subchannel));
		} 
		else 
		{
			menu->addItem(new CMenuForwarder(e->subservice_name.c_str(), enabled, NULL, &NVODChanger, nvod_id, CRCInput::convertDigitToKey(count)), (count == g_RemoteControl->selected_subchannel));
		}

		count++;
		if (count > 9999)
			break;
	}

	if ( g_RemoteControl->are_subchannels ) {
		menu->addItem(GenericMenuSeparatorLine);
		CMenuOptionChooser* oj = new CMenuOptionChooser(LOCALE_NVODSELECTOR_DIRECTORMODE, &g_RemoteControl->director_mode, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW);
		menu->addItem(oj);
	}
	return true;
}




