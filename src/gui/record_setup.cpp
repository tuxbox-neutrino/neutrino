/*
	$port: record_setup.cpp,v 1.7 2010/12/05 22:32:12 tuxbox-cvs Exp $

	record setup implementation - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2009 T. Graf 'dbt'
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


#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>
#include <mymenu.h>

#include "record_setup.h"
#include <gui/filebrowser.h>
#include <gui/followscreenings.h>

#include <gui/widget/icons.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/stringinput_ext.h>
#include <gui/widget/keyboard_input.h>

#include <timerdclient/timerdclient.h>

#include <driver/screen_max.h>
#include <driver/record.h>

#include <system/debug.h>
#include <system/helpers.h>
#include <system/hddstat.h>

CRecordSetup::CRecordSetup()
{
	width = 50;
}

CRecordSetup::~CRecordSetup()
{

}

int CRecordSetup::exec(CMenuTarget* parent, const std::string &actionKey)
{
	dprintf(DEBUG_DEBUG, "init record setup\n");
	int   res = menu_return::RETURN_REPAINT;
	std::string timeshiftDir;

	if (parent)
	{
		parent->hide();
	}

	if(actionKey=="recording")
	{
		CNeutrinoApp::getInstance()->setupRecordingDevice();
		return res;
	}
	else if(actionKey == "help_recording")
	{
		ShowMsg(LOCALE_SETTINGS_HELP, LOCALE_RECORDINGMENU_HELP, CMsgBox::mbrBack, CMsgBox::mbBack);
		return res;
	}
	else if(actionKey == "recordingdir")
	{
		//parent->hide();
		const char *action_str = "recordingdir";
		if(chooserDir(g_settings.network_nfs_recordingdir, true, action_str)){
			printf("New recordingdir: %s (timeshift %s)\n", g_settings.network_nfs_recordingdir.c_str(), g_settings.timeshiftdir.c_str());
			if(g_settings.timeshiftdir.empty())
			{
				timeshiftDir = g_settings.network_nfs_recordingdir + "/.timeshift";
				safe_mkdir(timeshiftDir.c_str());
				printf("New timeshift dir: %s\n", timeshiftDir.c_str());
				CRecordManager::getInstance()->SetTimeshiftDirectory(timeshiftDir);
			}
			cHddStat::getInstance()->setDir(g_settings.network_nfs_recordingdir);
		}
		return res;
	}
	else if(actionKey == "timeshiftdir")
	{
		//parent->hide();
		CFileBrowser b;
		b.Dir_Mode=true;
		if (b.exec(g_settings.timeshiftdir.c_str()))
		{
			const char * newdir = b.getSelectedFile()->Name.c_str();
			printf("New timeshift: selected %s\n", newdir);
			if(check_dir(newdir))
				printf("Wrong/unsupported recording dir %s\n", newdir);
			else
			{
				printf("New timeshift dir: old %s (record %s)\n", g_settings.timeshiftdir.c_str(), g_settings.network_nfs_recordingdir.c_str());
				if(newdir != g_settings.network_nfs_recordingdir)
				{
					printf("New timeshift != rec dir\n");
					g_settings.timeshiftdir = b.getSelectedFile()->Name;
					timeshiftDir = g_settings.timeshiftdir;
				}
				else
				{
					timeshiftDir = g_settings.network_nfs_recordingdir + "/.timeshift";
					g_settings.timeshiftdir = newdir;
					safe_mkdir(timeshiftDir.c_str());
					printf("New timeshift == rec dir\n");
				}
				printf("New timeshift dir: %s\n", timeshiftDir.c_str());
				CRecordManager::getInstance()->SetTimeshiftDirectory(timeshiftDir);
			}
		}
		return res;
	}
#if 0
	if (CNeutrinoApp::getInstance()->recordingstatus)
		DisplayInfoMessage(g_Locale->getText(LOCALE_RECORDINGMENU_RECORD_IS_RUNNING));
	else
#endif
		res = showRecordSetup();

	return res;
}
#if 0 //not used
#define RECORDINGMENU_RECORDING_TYPE_OPTION_COUNT 4
const CMenuOptionChooser::keyval RECORDINGMENU_RECORDING_TYPE_OPTIONS[RECORDINGMENU_RECORDING_TYPE_OPTION_COUNT] =
{
	{ CNeutrinoApp::RECORDING_OFF   , LOCALE_RECORDINGMENU_OFF    },
	{ CNeutrinoApp::RECORDING_SERVER, LOCALE_RECORDINGMENU_SERVER },
	{ CNeutrinoApp::RECORDING_VCR   , LOCALE_RECORDINGMENU_VCR    },
	{ CNeutrinoApp::RECORDING_FILE  , LOCALE_RECORDINGMENU_FILE   }
};

#define CHOOSE_DIRECT_REC_DIR_COUNT 3
const CMenuOptionChooser::keyval CHOOSE_DIRECT_REC_DIR[RECORDINGMENU_RECORDING_TYPE_OPTION_COUNT] =
{
	{0, LOCALE_OPTIONS_OFF},
	{1, LOCALE_NFS_TYPE_NFS},
	{2, LOCALE_NFS_LOCALDIR}
};
#endif

#define END_OF_RECORDING_COUNT 2
const CMenuOptionChooser::keyval END_OF_RECORDING[END_OF_RECORDING_COUNT] =
{
	{0, LOCALE_RECORDINGMENU_END_OF_RECORDING_MAX},
	{1, LOCALE_RECORDINGMENU_END_OF_RECORDING_EPG}
};

const CMenuOptionChooser::keyval timer_followscreenings_options[] =
{
	{CFollowScreenings::FOLLOWSCREENINGS_OFF	,LOCALE_OPTIONS_OFF	},
	{CFollowScreenings::FOLLOWSCREENINGS_ON		,LOCALE_OPTIONS_ON	}
};
size_t timer_followscreenings_options_count = sizeof(timer_followscreenings_options)/sizeof(CMenuOptionChooser::keyval);

int CRecordSetup::showRecordSetup()
{
	CMenuForwarder * mf;
	//menue init
	CMenuWidget* recordingSettings = new CMenuWidget(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_RECORDSETUP);

	recordingSettings->addIntroItems(LOCALE_MAINSETTINGS_RECORDING);
#if 0
	//apply settings
	mf = new CMenuForwarder(LOCALE_RECORDINGMENU_SETUPNOW, true, NULL, this, "recording", CRCInput::RC_red);
	mf->setHint("", LOCALE_MENU_HINT_RECORD_APPLY);
	recordingSettings->addItem(mf);
	recordingSettings->addItem(GenericMenuSeparatorLine);
#endif
	CMenuWidget recordingTsSettings(LOCALE_MAINSETTINGS_RECORDING, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_RECORDSETUP_TIMESHIFT);
	showRecordTimeShiftSetup(&recordingTsSettings);

	CMenuWidget recordingTimerSettings(LOCALE_MAINSETTINGS_RECORDING, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_RECORDSETUP_TIMERSETTINGS);
	showRecordTimerSetup(&recordingTimerSettings);

	bool recstatus = CNeutrinoApp::getInstance()->recordingstatus;
	//record dir
	CMenuForwarder* fRecDir;
	fRecDir = new CMenuForwarder(LOCALE_RECORDINGMENU_DEFDIR, !recstatus, g_settings.network_nfs_recordingdir, this, "recordingdir");
	fRecDir->setHint("", LOCALE_MENU_HINT_RECORD_DIR);
	recordingSettings->addItem(fRecDir);

	CMenuOptionChooser* channel_rec_dir;
	channel_rec_dir = new CMenuOptionChooser(LOCALE_RECORDINGMENU_SAVE_IN_CHANNELDIR, &g_settings.recording_save_in_channeldir, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	channel_rec_dir->setHint("", LOCALE_MENU_HINT_RECORD_CHANDIR);
	recordingSettings->addItem(channel_rec_dir);

	//rec hours
	CMenuOptionNumberChooser * mc = new CMenuOptionNumberChooser(LOCALE_EXTRA_RECORD_TIME, &g_settings.record_hours, true, 1, 24, NULL);
	mc->setNumberFormat(std::string("%d ") + g_Locale->getText(LOCALE_UNIT_SHORT_HOUR));
	mc->setHint("", LOCALE_MENU_HINT_RECORD_TIME);
	recordingSettings->addItem(mc);

	// end of recording
	CMenuOptionChooser* end_of_recording = new CMenuOptionChooser(LOCALE_RECORDINGMENU_END_OF_RECORDING_NAME, &g_settings.recording_epg_for_end, END_OF_RECORDING, END_OF_RECORDING_COUNT, true);
	end_of_recording->setHint("", LOCALE_MENU_HINT_RECORD_END);
	recordingSettings->addItem(end_of_recording);

	// already_found
	CMenuOptionChooser* already_found = new CMenuOptionChooser(LOCALE_RECORDINGMENU_ALREADY_FOUND_CHECK, &g_settings.recording_already_found_check, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	already_found->setHint("", LOCALE_MENU_HINT_RECORD_ALREADY_FOUND_CHECK);
	recordingSettings->addItem(already_found);

	CMenuOptionChooser* slow_warn = new CMenuOptionChooser(LOCALE_RECORDINGMENU_SLOW_WARN, &g_settings.recording_slow_warning, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	slow_warn->setHint("", LOCALE_MENU_HINT_RECORD_SLOW_WARN);
	recordingSettings->addItem(slow_warn);

	CMenuOptionChooser* startstop_msg = new CMenuOptionChooser(LOCALE_RECORDING_STARTSTOP_MSG, &g_settings.recording_startstop_msg, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	startstop_msg->setHint("", LOCALE_MENU_HINT_RECORD_STARTSTOP_MSG);
	recordingSettings->addItem(startstop_msg);

	//filename template
	CKeyboardInput* filename_template = new CKeyboardInput(LOCALE_RECORDINGMENU_FILENAME_TEMPLATE, &g_settings.recording_filename_template, 0, NULL, NULL, LOCALE_RECORDINGMENU_FILENAME_TEMPLATE_HINT, LOCALE_RECORDINGMENU_FILENAME_TEMPLATE_HINT2);
	CMenuForwarder* ft = new CMenuDForwarder(LOCALE_RECORDINGMENU_FILENAME_TEMPLATE, true, g_settings.recording_filename_template, filename_template, NULL, CRCInput::RC_1);
	ft->setHint("", LOCALE_MENU_HINT_RECORD_FILENAME_TEMPLATE);
	recordingSettings->addItem(ft);

	CMenuOptionChooser* cover = new CMenuOptionChooser(LOCALE_RECORDINGMENU_AUTO_COVER, &g_settings.auto_cover, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	cover->setHint("", LOCALE_MENU_HINT_RECORD_AUTO_COVER);
	recordingSettings->addItem(cover);

	recordingSettings->addItem(GenericMenuSeparatorLine);

	//timeshift
	mf = new CMenuForwarder(LOCALE_RECORDINGMENU_TIMESHIFT, true, NULL, &recordingTsSettings, NULL, CRCInput::RC_green);
	mf->setHint("", LOCALE_MENU_HINT_RECORD_TIMESHIFT);
	recordingSettings->addItem(mf);

	//timersettings
	mf = new CMenuForwarder(LOCALE_TIMERSETTINGS_SEPARATOR, true, NULL, &recordingTimerSettings, NULL, CRCInput::RC_yellow);
	mf->setHint("", LOCALE_MENU_HINT_RECORD_TIMER);
	recordingSettings->addItem(mf);

	CMenuWidget recordingaAudioSettings(LOCALE_MAINSETTINGS_RECORDING, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_RECORDSETUP_AUDIOSETTINGS);
	CMenuWidget recordingaDataSettings(LOCALE_MAINSETTINGS_RECORDING, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_RECORDSETUP_DATASETTINGS);

	//audiosettings
	showRecordAudioSetup(&recordingaAudioSettings);
	mf = new CMenuForwarder(LOCALE_RECORDINGMENU_APIDS, true, NULL, &recordingaAudioSettings, NULL, CRCInput::RC_blue);
	mf->setHint("", LOCALE_MENU_HINT_RECORD_APIDS);
	recordingSettings->addItem(mf);

	//datasettings
	showRecordDataSetup(&recordingaDataSettings);
	mf = new CMenuForwarder(LOCALE_RECORDINGMENU_DATA_PIDS, true, NULL, &recordingaDataSettings, NULL,  CRCInput::RC_2);
	mf->setHint("", LOCALE_MENU_HINT_RECORD_DATA);
	recordingSettings->addItem(mf);

	int res = recordingSettings->exec(NULL, "");
	delete recordingSettings;

	/* activate changes */
        CRecordManager::getInstance()->SetDirectory(g_settings.network_nfs_recordingdir);
        CRecordManager::getInstance()->Config(g_settings.recording_stopsectionsd, g_settings.recording_stream_vtxt_pid, g_settings.recording_stream_pmt_pid, g_settings.recording_stream_subtitle_pids);

	return res;
}

void CRecordSetup::showRecordTimerSetup(CMenuWidget *menu_timersettings)
{
	//recording start/end correcture
	int pre = 0,post = 0;
	g_Timerd->getRecordingSafety(pre,post);
	g_settings.record_safety_time_before = pre/60;
	g_settings.record_safety_time_after = post/60;

	menu_timersettings->addIntroItems(LOCALE_TIMERSETTINGS_SEPARATOR);

	std::string nf = "%d ";
	nf += g_Locale->getText(LOCALE_UNIT_SHORT_MINUTE);

	//start
	CMenuOptionNumberChooser *ch = new CMenuOptionNumberChooser(LOCALE_TIMERSETTINGS_RECORD_SAFETY_TIME_BEFORE,
		&g_settings.record_safety_time_before, true, 0, 99, this);
	ch->setNumberFormat(nf);
	ch->setHint("", LOCALE_MENU_HINT_RECORD_TIMEBEFORE);
	menu_timersettings->addItem(ch);

	//end
	ch = new CMenuOptionNumberChooser(LOCALE_TIMERSETTINGS_RECORD_SAFETY_TIME_AFTER,
		&g_settings.record_safety_time_after, true, 0, 99, this);
	ch->setNumberFormat(nf);
	ch->setHint("", LOCALE_MENU_HINT_RECORD_TIMEAFTER);
	menu_timersettings->addItem(ch);

	//announce
	CMenuOptionChooser* chzapAnnounce = new CMenuOptionChooser(LOCALE_RECORDINGMENU_ZAP_ON_ANNOUNCE,
		&g_settings.recording_zap_on_announce, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	chzapAnnounce->setHint("", LOCALE_MENU_HINT_RECORD_ZAP);
	menu_timersettings->addItem(chzapAnnounce);

	//zapto
	ch = new CMenuOptionNumberChooser(LOCALE_MISCSETTINGS_ZAPTO_PRE_TIME,
		&g_settings.zapto_pre_time, true, 0, 10);
	ch->setHint("", LOCALE_MENU_HINT_RECORD_ZAP_PRE_TIME);
	ch->setNumberFormat(nf);
	menu_timersettings->addItem(ch);

	menu_timersettings->addItem(GenericMenuSeparatorLine);

	//allow followscreenings
	CMenuOptionChooser* followscreenings = new CMenuOptionChooser(LOCALE_TIMERSETTINGS_FOLLOWSCREENINGS, &g_settings.timer_followscreenings, timer_followscreenings_options, timer_followscreenings_options_count, true);
	followscreenings->setHint("", LOCALE_MENU_HINT_TIMER_FOLLOWSCREENINGS);
	menu_timersettings->addItem(followscreenings);
}


void CRecordSetup::showRecordAudioSetup(CMenuWidget *menu_audiosettings)
{
	//default recording audio pids
	//CMenuWidget * apidMenu = new CMenuWidget(LOCALE_RECORDINGMENU_APIDS, NEUTRINO_ICON_AUDIO);
	//CMenuForwarder* fApidMenu = new CMenuForwarder(LOCALE_RECORDINGMENU_APIDS ,true, NULL, apidMenu);
	g_settings.recording_audio_pids_std = ( g_settings.recording_audio_pids_default & TIMERD_APIDS_STD ) ? 1 : 0 ;
	g_settings.recording_audio_pids_alt = ( g_settings.recording_audio_pids_default & TIMERD_APIDS_ALT ) ? 1 : 0 ;
	g_settings.recording_audio_pids_ac3 = ( g_settings.recording_audio_pids_default & TIMERD_APIDS_AC3 ) ? 1 : 0 ;

	//audio pids
	CMenuOptionChooser* aoj1 = new CMenuOptionChooser(LOCALE_RECORDINGMENU_APIDS_STD, &g_settings.recording_audio_pids_std, MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true, this);
	CMenuOptionChooser* aoj2 = new CMenuOptionChooser(LOCALE_RECORDINGMENU_APIDS_ALT, &g_settings.recording_audio_pids_alt, MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true, this);
	CMenuOptionChooser* aoj3 = new CMenuOptionChooser(LOCALE_RECORDINGMENU_APIDS_AC3, &g_settings.recording_audio_pids_ac3, MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true, this);

	aoj1->setHint("", LOCALE_MENU_HINT_RECORD_APID_STD);
	aoj2->setHint("", LOCALE_MENU_HINT_RECORD_APID_ALT);
	aoj3->setHint("", LOCALE_MENU_HINT_RECORD_APID_AC3);

	menu_audiosettings->addIntroItems(LOCALE_RECORDINGMENU_APIDS);
	menu_audiosettings->addItem(aoj1);
	menu_audiosettings->addItem(aoj2);
	menu_audiosettings->addItem(aoj3);
}

void CRecordSetup::showRecordDataSetup(CMenuWidget *menu_datasettings)
{
	//recording data pids

	//teletext pids
	CMenuOptionChooser* doj1 = new CMenuOptionChooser(LOCALE_RECORDINGMENU_VTXT_PID, &g_settings.recording_stream_vtxt_pid, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this);
	CMenuOptionChooser* doj2 = new CMenuOptionChooser(LOCALE_RECORDINGMENU_DVBSUB_PIDS, &g_settings.recording_stream_subtitle_pids, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this);

	doj1->setHint("", LOCALE_MENU_HINT_RECORD_DATA_VTXT);
	doj2->setHint("", LOCALE_MENU_HINT_RECORD_DATA_DVBSUB);

	menu_datasettings->addIntroItems(LOCALE_RECORDINGMENU_DATA_PIDS);
	menu_datasettings->addItem(doj1);
	menu_datasettings->addItem(doj2);
}

void CRecordSetup::showRecordTimeShiftSetup(CMenuWidget *menu_ts)
{
	menu_ts->addIntroItems(LOCALE_RECORDINGMENU_TIMESHIFT);

	//timeshift dir
	bool recstatus = CNeutrinoApp::getInstance()->recordingstatus;
	CMenuForwarder* fTsDir = new CMenuForwarder(LOCALE_RECORDINGMENU_TSDIR, !recstatus, g_settings.timeshiftdir, this, "timeshiftdir");
	fTsDir->setHint("", LOCALE_MENU_HINT_RECORD_TDIR);
	menu_ts->addItem(fTsDir);

	if (1) //has_hdd
	{
		CMenuOptionChooser * mc = new CMenuOptionChooser(LOCALE_EXTRA_TIMESHIFT_PAUSE, &g_settings.timeshift_pause, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
		mc->setHint("", LOCALE_MENU_HINT_RECORD_TIMESHIFT_PAUSE);
		menu_ts->addItem(mc);

		CMenuOptionNumberChooser * mn = new CMenuOptionNumberChooser(LOCALE_EXTRA_AUTO_TIMESHIFT, &g_settings.auto_timeshift, true, 0, 300, NULL);
		mn->setHint("", LOCALE_MENU_HINT_RECORD_TIMESHIFT_AUTO);
		menu_ts->addItem(mn);

		mc = new CMenuOptionChooser(LOCALE_EXTRA_AUTO_DELETE, &g_settings.auto_delete, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
		mc->setHint("", LOCALE_MENU_HINT_RECORD_TIMESHIFT_DELETE);
		menu_ts->addItem(mc);

		mc = new CMenuOptionChooser(LOCALE_EXTRA_TEMP_TIMESHIFT, &g_settings.temp_timeshift, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
		mc->setHint("", LOCALE_MENU_HINT_RECORD_TIMESHIFT_TEMP);
		menu_ts->addItem(mc);

		//rec hours
		mn = new CMenuOptionNumberChooser(LOCALE_EXTRA_RECORD_TIME_TS, &g_settings.timeshift_hours, true, 1, 24, NULL);
		mn->setNumberFormat(std::string("%d ") + g_Locale->getText(LOCALE_UNIT_SHORT_HOUR));
		mn->setHint("", LOCALE_MENU_HINT_RECORD_TIME_TS);
		menu_ts->addItem(mn);
	}
}

bool CRecordSetup::changeNotify(const neutrino_locale_t OptionName, void * /*data*/)
{
	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_TIMERSETTINGS_RECORD_SAFETY_TIME_BEFORE) ||
			ARE_LOCALES_EQUAL(OptionName, LOCALE_TIMERSETTINGS_RECORD_SAFETY_TIME_AFTER)) {
		g_Timerd->setRecordingSafety(g_settings.record_safety_time_before*60, g_settings.record_safety_time_after*60);
	} else if(ARE_LOCALES_EQUAL(OptionName, LOCALE_RECORDINGMENU_APIDS_STD) ||
			ARE_LOCALES_EQUAL(OptionName, LOCALE_RECORDINGMENU_APIDS_ALT) ||
			ARE_LOCALES_EQUAL(OptionName, LOCALE_RECORDINGMENU_APIDS_AC3)) {
		g_settings.recording_audio_pids_default = ( (g_settings.recording_audio_pids_std ? TIMERD_APIDS_STD : 0) |
				(g_settings.recording_audio_pids_alt ? TIMERD_APIDS_ALT : 0) |
				(g_settings.recording_audio_pids_ac3 ? TIMERD_APIDS_AC3 : 0));
	}
	return false;
}
