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


#include <global.h>
#include <neutrino.h>
#include <mymenu.h>
#include <gui/widget/icons.h>
#include <gui/widget/menue.h>
#include <driver/screen_max.h>
#include <driver/volume.h>
#include <zapit/zapit.h>

extern CRemoteControl		* g_RemoteControl; /* neutrino.cpp */
extern CAudioSetupNotifier	* audioSetupNotifier;

#include <gui/audio_select.h>
#include <libdvbsub/dvbsub.h>
#include <libtuxtxt/teletext.h>

//
//  -- AUDIO Selector Menue Handler Class
//  -- to be used for calls from Menue
//  -- (2005-08-31 rasc)

CAudioSelectMenuHandler::CAudioSelectMenuHandler()
{
	width = w_max (40, 10);
}

CAudioSelectMenuHandler::~CAudioSelectMenuHandler()
{

}

// -- this is a copy from neutrino.cpp!!
#define AUDIOMENU_ANALOGOUT_OPTION_COUNT 3
const CMenuOptionChooser::keyval AUDIOMENU_ANALOGOUT_OPTIONS[AUDIOMENU_ANALOGOUT_OPTION_COUNT] =
{
	{ 0, LOCALE_AUDIOMENU_STEREO },
	{ 1, LOCALE_AUDIOMENU_MONOLEFT },
	{ 2, LOCALE_AUDIOMENU_MONORIGHT }
};

int CAudioSelectMenuHandler::exec(CMenuTarget* parent, const std::string &actionkey)
{
	int sel= atoi(actionkey.c_str());
	if(sel >= 0) {
		if (g_RemoteControl->current_PIDs.PIDs.selected_apid!= (unsigned int) sel )
		{
			g_RemoteControl->setAPID(sel);
		}
		return menu_return::RETURN_EXIT;
	}

	if (parent)
		parent->hide();

	return doMenu ();
}

int CAudioSelectMenuHandler::doMenu ()
{
	CMenuWidget AudioSelector(LOCALE_AUDIOSELECTMENUE_HEAD, NEUTRINO_ICON_AUDIO, width);

	CSubtitleChangeExec SubtitleChanger;

	//show cancel button if configured in usermenu settings
	if (g_settings.personalize[SNeutrinoSettings::P_UMENU_SHOW_CANCEL])
		AudioSelector.addIntroItems(NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, CMenuWidget::BTN_TYPE_CANCEL);
	else
		AudioSelector.addItem(GenericMenuSeparator);

	unsigned int shortcut_num = 1;

	uint p_count = g_RemoteControl->current_PIDs.APIDs.size();
	// -- setup menue due to Audio PIDs
	for( uint i=0; i < p_count; i++ )
	{
		char apid[5];
		sprintf(apid, "%d", i);
		CMenuForwarderNonLocalized *fw = new CMenuForwarderNonLocalized(g_RemoteControl->current_PIDs.APIDs[i].desc, 
				true, NULL, this, apid, CRCInput::convertDigitToKey(i + 1));
		fw->setItemButton(NEUTRINO_ICON_BUTTON_OKAY, true);
		AudioSelector.addItem(fw, (i == g_RemoteControl->current_PIDs.PIDs.selected_apid));
		shortcut_num = i+1;
	}
	if (p_count)
		AudioSelector.addItem(GenericMenuSeparatorLine);

	// -- setup menue for to Dual Channel Stereo
	CMenuOptionChooser* oj = new CMenuOptionChooser(LOCALE_AUDIOMENU_ANALOG_MODE,
			&g_settings.audio_AnalogMode,
			AUDIOMENU_ANALOGOUT_OPTIONS, AUDIOMENU_ANALOGOUT_OPTION_COUNT,
			true, audioSetupNotifier, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED);

	AudioSelector.addItem( oj );

	oj = new CMenuOptionChooser(LOCALE_AUDIOMENU_ANALOG_OUT, &g_settings.analog_out,
			OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT,
			true, audioSetupNotifier, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN);

	AudioSelector.addItem( oj );

	CChannelList *channelList = CNeutrinoApp::getInstance ()->channelList;
	int curnum = channelList->getActiveChannelNumber();
	CZapitChannel * cc = channelList->getChannel(curnum);

	bool sep_added = false;
	if(cc)
	{
		for (int i = 0 ; i < (int)cc->getSubtitleCount() ; ++i)
		{
			CZapitAbsSub* s = cc->getChannelSub(i);
			if (s->thisSubType == CZapitAbsSub::DVB) {
				CZapitDVBSub* sd = reinterpret_cast<CZapitDVBSub*>(s);
				printf("[neutrino] adding DVB subtitle %s pid %x\n", sd->ISO639_language_code.c_str(), sd->pId);
				if(!sep_added)
				{
					sep_added = true;
					AudioSelector.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_SUBTITLES_HEAD));
				}
				char spid[10];
				snprintf(spid,sizeof(spid), "DVB:%d", sd->pId);
				char item[64];
				snprintf(item,sizeof(item), "DVB: %s (pid %x)", sd->ISO639_language_code.c_str(), sd->pId);
				AudioSelector.addItem(new CMenuForwarderNonLocalized(item /*sd->ISO639_language_code.c_str()*/,
							sd->pId != dvbsub_getpid(), NULL, &SubtitleChanger, spid, CRCInput::convertDigitToKey(++shortcut_num)));
			}
			if (s->thisSubType == CZapitAbsSub::TTX)
			{
				CZapitTTXSub* sd = reinterpret_cast<CZapitTTXSub*>(s);
				printf("[neutrino] adding TTX subtitle %s pid %x mag %X page %x\n", sd->ISO639_language_code.c_str(), sd->pId, sd->teletext_magazine_number, sd->teletext_page_number);
				if(!sep_added)
				{
					sep_added = true;
					AudioSelector.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_SUBTITLES_HEAD));
				}
				char spid[64];
				int page = ((sd->teletext_magazine_number & 0xFF) << 8) | sd->teletext_page_number;
				int pid = sd->pId;
				snprintf(spid,sizeof(spid), "TTX:%d:%03X:%s", sd->pId, page, sd->ISO639_language_code.c_str());
				char item[64];
				snprintf(item,sizeof(item), "TTX: %s (pid %x page %03X)", sd->ISO639_language_code.c_str(), sd->pId, page);
				AudioSelector.addItem(new CMenuForwarderNonLocalized(item /*sd->ISO639_language_code.c_str()*/,
							!tuxtx_subtitle_running(&pid, &page, NULL), NULL, &SubtitleChanger, spid, CRCInput::convertDigitToKey(++shortcut_num)));
			}
		}

		if(sep_added) {
			CMenuForwarder * item = new CMenuForwarder(LOCALE_SUBTITLES_STOP, true, NULL, &SubtitleChanger, "off", CRCInput::RC_stop);
			item->setItemButton(NEUTRINO_ICON_BUTTON_STOP, false);
			AudioSelector.addItem(item);
		}
	}

	AudioSelector.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_AUDIOMENU_VOLUME_ADJUST));

	/* setting volume percent to zapit with channel_id/apid = 0 means current channel and pid */
	CVolume::getInstance()->SetCurrentChannel(0);
	CVolume::getInstance()->SetCurrentPid(0);
	int percent[p_count];
	for (uint i=0; i < p_count; i++) {
		percent[i] = CZapit::getInstance()->GetPidVolume(0, g_RemoteControl->current_PIDs.APIDs[i].pid);
		AudioSelector.addItem(new CMenuOptionNumberChooser(NONEXISTANT_LOCALE, &percent[i],
					i == g_RemoteControl->current_PIDs.PIDs.selected_apid,
					0, 999, CVolume::getInstance(), 0, 0, NONEXISTANT_LOCALE,
					g_RemoteControl->current_PIDs.APIDs[i].desc));
	}

	return AudioSelector.exec(NULL, "");
}
