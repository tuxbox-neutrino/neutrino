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
#include <zapit/zapit.h>
#include <system/helpers.h>

extern CRemoteControl		* g_RemoteControl; /* neutrino.cpp */
extern CAudioSetupNotifier	* audioSetupNotifier;

#include <gui/audio_select.h>
#include <gui/movieplayer.h>
#include <libdvbsub/dvbsub.h>
#include <libtuxtxt/teletext.h>

//
//  -- AUDIO Selector Menue Handler Class
//  -- to be used for calls from Menue
//  -- (2005-08-31 rasc)

CAudioSelectMenuHandler::CAudioSelectMenuHandler()
{
	AudioSelector = NULL;
	width = 40;
	mp = &CMoviePlayerGui::getInstance();
	if (IS_WEBCHAN(g_Zapit->getCurrentServiceID()))
		mp = &CMoviePlayerGui::getInstance(true);
}

CAudioSelectMenuHandler::~CAudioSelectMenuHandler()
{

}

#if !HAVE_SPARK_HARDWARE && !HAVE_DUCKBOX_HARDWARE
// -- this is a copy from neutrino.cpp!!
#define AUDIOMENU_ANALOGOUT_OPTION_COUNT 3
const CMenuOptionChooser::keyval AUDIOMENU_ANALOGOUT_OPTIONS[AUDIOMENU_ANALOGOUT_OPTION_COUNT] =
{
	{ 0, LOCALE_AUDIOMENU_STEREO },
	{ 1, LOCALE_AUDIOMENU_MONOLEFT },
	{ 2, LOCALE_AUDIOMENU_MONORIGHT }
};
#endif

int CAudioSelectMenuHandler::exec(CMenuTarget* parent, const std::string &actionkey)
{
	int sel = -1;
	if (AudioSelector) {
		sel = AudioSelector->getSelected();
		sel -= apid_offset;
		if (sel < 0 || sel >= p_count)
			return menu_return::RETURN_NONE;
	}

	if (actionkey == "-" || actionkey == "+") {
		if (actionkey == "-") {
			if (perc_val[sel] == 0)
				return menu_return::RETURN_NONE;
			perc_val[sel]--;
		} else {
			if (perc_val[sel] == 999)
				return menu_return::RETURN_NONE;
			perc_val[sel]++;
		}
		perc_str[sel] = to_string(perc_val[sel]) + "%";

#if !HAVE_SPARK_HARDWARE && !HAVE_DUCKBOX_HARDWARE
		int vol =  CZapit::getInstance()->GetVolume();
		/* keep resulting volume = (vol * percent)/100 not more than 115 */
		if (vol * perc_val[sel] > 11500)
			perc_val[sel] = 11500 / vol;
#endif
		CZapit::getInstance()->SetPidVolume(chan, apid[sel], perc_val[sel]);
		if (sel == sel_apid)
			CZapit::getInstance()->SetVolumePercent(perc_val[sel]);

		return menu_return::RETURN_REPAINT;
	}

	if (actionkey == "s") {
		if (mp->Playing()) {
			mp->setAPID(sel);
		} else if (g_RemoteControl->current_PIDs.PIDs.selected_apid != (unsigned int) sel ) {
			g_RemoteControl->setAPID(sel);
		}
		return menu_return::RETURN_EXIT;
	}
	if (actionkey == "x")
		return menu_return::RETURN_EXIT;

	if (mp->Playing())
		playback = mp->getPlayback();
	if (parent)
		parent->hide();

	return doMenu ();
}

int CAudioSelectMenuHandler::doMenu ()
{
	AudioSelector = new CMenuWidget(LOCALE_AUDIOSELECTMENUE_HEAD, NEUTRINO_ICON_AUDIO, width);

	CSubtitleChangeExec SubtitleChanger(playback);

	//show cancel button if configured in usermenu settings
	if (g_settings.personalize[SNeutrinoSettings::P_UMENU_SHOW_CANCEL])
		AudioSelector->addIntroItems(NONEXISTANT_LOCALE, LOCALE_AUDIOSELECTMENUE_VOLUME, CMenuWidget::BTN_TYPE_CANCEL);
	else
		AudioSelector->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_AUDIOSELECTMENUE_VOLUME));
	apid_offset = AudioSelector->getItemsCount();
	AudioSelector->addKey(CRCInput::RC_right, this, "+");
	AudioSelector->addKey(CRCInput::RC_left, this, "-");
	AudioSelector->addKey(CRCInput::RC_red, this, "x");
#if !HAVE_SPARK_HARDWARE && !HAVE_DUCKBOX_HARDWARE
	AudioSelector->addKey(CRCInput::RC_green, this, "x");
#endif
	AudioSelector->addKey(CRCInput::RC_yellow, this, "x");
	AudioSelector->addKey(CRCInput::RC_blue, this, "x");

	bool is_mp = mp->Playing();

	p_count = is_mp ? mp->getAPIDCount() : g_RemoteControl->current_PIDs.APIDs.size();
	sel_apid = is_mp ? mp->getAPID() : g_RemoteControl->current_PIDs.PIDs.selected_apid;

	int _apid[p_count];
	int _perc_val[p_count];
	unsigned int _is_ac3[p_count];
	std::string _perc_str[p_count];
	perc_val = _perc_val;
	perc_str = _perc_str;
	is_ac3 = _is_ac3;
	apid = _apid;
	chan = is_mp ? mp->getChannelId() : 0;

	// -- setup menue due to Audio PIDs
	for (int i = 0; i < p_count; i++)
	{
		if (is_mp) {
			mp->getAPID(i, apid[i], is_ac3[i]);
			perc_val[i] = (is_ac3[i] == 1) ? g_settings.audio_volume_percent_ac3 : g_settings.audio_volume_percent_pcm;
		} else {
			apid[i] = g_RemoteControl->current_PIDs.APIDs[i].pid;
			is_ac3[i] = g_RemoteControl->current_PIDs.APIDs[i].is_ac3;
			perc_val[i] = CZapit::getInstance()->GetPidVolume(chan, apid[i], is_ac3[i]);
		}
		perc_str[i] = to_string(perc_val[i]) + "%";

		CMenuForwarder *fw = new CMenuForwarder(is_mp ? mp->getAPIDDesc(i).c_str() : g_RemoteControl->current_PIDs.APIDs[i].desc,
				true, perc_str[i], this, "s", CRCInput::convertDigitToKey(i + 1));
		fw->setItemButton(NEUTRINO_ICON_BUTTON_OKAY, true);
		fw->setMarked(sel_apid == i);

		AudioSelector->addItem(fw, sel_apid == i);
	}
	unsigned int shortcut_num = p_count;
#if !HAVE_SPARK_HARDWARE && !HAVE_DUCKBOX_HARDWARE
	if (p_count)
		AudioSelector->addItem(GenericMenuSeparatorLine);

	// -- setup menue for to Dual Channel Stereo
	CMenuOptionChooser* oj = new CMenuOptionChooser(LOCALE_AUDIOMENU_ANALOG_MODE,
			&g_settings.audio_AnalogMode,
			AUDIOMENU_ANALOGOUT_OPTIONS, AUDIOMENU_ANALOGOUT_OPTION_COUNT,
			true, audioSetupNotifier, CRCInput::RC_red);

	AudioSelector->addItem( oj );

	oj = new CMenuOptionChooser(LOCALE_AUDIOMENU_ANALOG_OUT, &g_settings.analog_out,
			OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT,
			true, audioSetupNotifier, CRCInput::RC_green);
	AudioSelector->addItem( oj );
#endif

	CZapitChannel * cc = NULL;
	int subtitleCount = 0;
	if (is_mp) {
		subtitleCount = mp->getSubtitleCount();
	} else {
		CChannelList *channelList = CNeutrinoApp::getInstance ()->channelList;
		int curnum = channelList->getActiveChannelNumber();
		cc = channelList->getChannel(curnum);
		subtitleCount = (int)cc->getSubtitleCount();
	}

	bool sep_added = false;
	if (subtitleCount > 0)
	{
		bool sub_active = false;

		for (int i = 0 ; i < subtitleCount ; ++i)
		{
			CZapitAbsSub* s = is_mp ? mp->getChannelSub(i, &s) : cc->getChannelSub(i);
			if (!s)
				continue;

			if (!sep_added)
			{
				sep_added = true;
				AudioSelector->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_SUBTITLES_HEAD));
			}

			bool ena = false;
			bool add = true;
			char spid[64];
			char item[64];

			if (s->thisSubType == CZapitAbsSub::DVB) {
				CZapitDVBSub* sd = reinterpret_cast<CZapitDVBSub*>(s);
				// printf("[neutrino] adding DVB subtitle %s pid %x\n", sd->ISO639_language_code.c_str(), sd->pId);
				snprintf(spid,sizeof(spid), "DVB:%d", sd->pId);
				snprintf(item,sizeof(item), "DVB: %s (pid %x)", sd->ISO639_language_code.c_str(), sd->pId);
				ena = sd->pId != (is_mp ? mp->getCurrentSubPid(CZapitAbsSub::DVB) : dvbsub_getpid());
			} else if (s->thisSubType == CZapitAbsSub::TTX) {
				CZapitTTXSub* sd = reinterpret_cast<CZapitTTXSub*>(s);
				// printf("[neutrino] adding TTX subtitle %s pid %x mag %X page %x\n", sd->ISO639_language_code.c_str(), sd->pId, sd->teletext_magazine_number, sd->teletext_page_number);
				int page = ((sd->teletext_magazine_number & 0xFF) << 8) | sd->teletext_page_number;
				int pid = sd->pId;
				snprintf(spid,sizeof(spid), "TTX:%d:%03X:%s", sd->pId, page, sd->ISO639_language_code.c_str());
				snprintf(item,sizeof(item), "TTX: %s (pid %x page %03X)", sd->ISO639_language_code.c_str(), sd->pId, page);
				ena = !tuxtx_subtitle_running(&pid, &page, NULL);
			} else if (is_mp && s->thisSubType == CZapitAbsSub::SUB) {
				// printf("[neutrino] adding SUB subtitle %s pid %x\n", s->ISO639_language_code.c_str(), s->pId);
				snprintf(spid,sizeof(spid), "SUB:%d", s->pId);
				snprintf(item,sizeof(item), "SUB: %s (pid %x)", s->ISO639_language_code.c_str(), s->pId);
				ena = s->pId != mp->getCurrentSubPid(CZapitAbsSub::SUB);
			} else
				add = false;

			if (add)
				AudioSelector->addItem(new CMenuForwarder(item, ena,
							NULL, &SubtitleChanger, spid, CRCInput::convertDigitToKey(++shortcut_num)));
			if (is_mp)
				delete s;

			sub_active |= !ena;
		}

		if (sub_active) {
			CMenuForwarder * item = new CMenuForwarder(LOCALE_SUBTITLES_STOP, true, NULL, &SubtitleChanger, "off", CRCInput::RC_stop);
			item->setItemButton(NEUTRINO_ICON_BUTTON_STOP, false);
			AudioSelector->addItem(item);
		}
	}

#if 0
	AudioSelector.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_AUDIOMENU_VOLUME_ADJUST));

	/* setting volume percent to zapit with channel_id/apid = 0 means current channel and pid */
	CVolume::getInstance()->SetCurrentChannel(0);
	CVolume::getInstance()->SetCurrentPid(0);
	int percent[p_count+1];//+1 avoid zero size
	for (uint i=0; i < p_count; i++) {
		percent[i] = CZapit::getInstance()->GetPidVolume(0, g_RemoteControl->current_PIDs.APIDs[i].pid, g_RemoteControl->current_PIDs.APIDs[i].is_ac3);
		AudioSelector.addItem(new CMenuOptionNumberChooser(g_RemoteControl->current_PIDs.APIDs[i].desc,
					&percent[i], i == g_RemoteControl->current_PIDs.PIDs.selected_apid,
					0, 999, CVolume::getInstance()));
	}
#endif

	//tonbug
	AudioSelector->addItem(GenericMenuSeparatorLine);
#if !HAVE_SPARK_HARDWARE && !HAVE_DUCKBOX_HARDWARE
	AudioSelector->addItem(new CMenuForwarder(LOCALE_CI_RESET, true, NULL, CNeutrinoApp::getInstance(), "tonbug", CRCInput::convertDigitToKey(++shortcut_num)));
#else
	AudioSelector->addItem(new CMenuForwarder(LOCALE_CI_RESET, true, NULL, CNeutrinoApp::getInstance(), "tonbug", CRCInput::RC_green));
#endif

	int res = AudioSelector->exec(NULL, "");

	delete AudioSelector;
	AudioSelector = NULL;

	return res;
}
