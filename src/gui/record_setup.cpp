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

#include "gui/record_setup.h"
#include "gui/filebrowser.h"

#include <gui/widget/icons.h>
#include <gui/widget/messagebox.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/stringinput_ext.h>

#include <driver/screen_max.h>
#include <driver/record.h>

#include <system/debug.h>


CRecordSetup::CRecordSetup()
{
	width = w_max (50, 10); //%
}

CRecordSetup::~CRecordSetup()
{

}

int CRecordSetup::exec(CMenuTarget* parent, const std::string &actionKey)
{
	dprintf(DEBUG_DEBUG, "init record setup\n");
	int   res = menu_return::RETURN_REPAINT;
	char timeshiftDir[255];

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
		ShowLocalizedMessage(LOCALE_SETTINGS_HELP, LOCALE_RECORDINGMENU_HELP, CMessageBox::mbrBack, CMessageBox::mbBack);
		return res;
	}
	else if(actionKey == "recordingdir") 
	{
		//parent->hide();
		const char *action_str = "recordingdir";
		if(chooserDir(g_settings.network_nfs_recordingdir, true, action_str, sizeof(g_settings.network_nfs_recordingdir)-1)){
			printf("New recordingdir: %s (timeshift %s)\n", g_settings.network_nfs_recordingdir, g_settings.timeshiftdir);
			if(strlen(g_settings.timeshiftdir) == 0) 
			{
				sprintf(timeshiftDir, "%s/.timeshift", g_settings.network_nfs_recordingdir);
				safe_mkdir(timeshiftDir);
				printf("New timeshift dir: %s\n", timeshiftDir);
			}
			CRecordManager::getInstance()->SetTimeshiftDirectory(timeshiftDir);
		}
		return res;
	}
	else if(actionKey == "timeshiftdir") 
	{
		//parent->hide();
		CFileBrowser b;
		b.Dir_Mode=true;
		if (b.exec(g_settings.timeshiftdir)) 
		{
			const char * newdir = b.getSelectedFile()->Name.c_str();
			printf("New timeshift: selected %s\n", newdir);
			if(check_dir(newdir))
				printf("Wrong/unsupported recording dir %s\n", newdir);
			else 
			{
				printf("New timeshift dir: old %s (record %s)\n", g_settings.timeshiftdir, g_settings.network_nfs_recordingdir);
				if(strcmp(newdir, g_settings.network_nfs_recordingdir)) 
				{
					printf("New timeshift != rec dir\n");
					strncpy(g_settings.timeshiftdir, b.getSelectedFile()->Name.c_str(), sizeof(g_settings.timeshiftdir)-1);
					strcpy(timeshiftDir, g_settings.timeshiftdir);
				} 
				else 
				{
					sprintf(timeshiftDir, "%s/.timeshift", g_settings.network_nfs_recordingdir);
					strcpy(g_settings.timeshiftdir, newdir);
					safe_mkdir(timeshiftDir);
					printf("New timeshift == rec dir\n");
				}
				printf("New timeshift dir: %s\n", timeshiftDir);
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

#define END_OF_RECORDING_COUNT 2
const CMenuOptionChooser::keyval END_OF_RECORDING[END_OF_RECORDING_COUNT] =
{
	{0, LOCALE_RECORDINGMENU_END_OF_RECORDING_MAX},
	{1, LOCALE_RECORDINGMENU_END_OF_RECORDING_EPG}
};

int CRecordSetup::showRecordSetup()
{
	//menue init
	CMenuWidget* recordingSettings = new CMenuWidget(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_RECORDSETUP);

	//apply settings
	recordingSettings->addIntroItems(LOCALE_MAINSETTINGS_RECORDING);
	recordingSettings->addItem(new CMenuForwarder(LOCALE_RECORDINGMENU_SETUPNOW, true, NULL, this, "recording", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
	recordingSettings->addItem(GenericMenuSeparatorLine);

	//record dir
	CMenuForwarder* fRecDir = new CMenuForwarder(LOCALE_RECORDINGMENU_DEFDIR, true, g_settings.network_nfs_recordingdir, this, "recordingdir");
	recordingSettings->addItem(fRecDir);
	CMenuOptionChooser* channel_rec_dir = new CMenuOptionChooser(LOCALE_RECORDINGMENU_SAVE_IN_CHANNELDIR, &g_settings.recording_save_in_channeldir, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	recordingSettings->addItem(channel_rec_dir);

	//rec hours
	recordingSettings->addItem(new CMenuOptionNumberChooser(LOCALE_EXTRA_RECORD_TIME, &g_settings.record_hours, true, 1, 24, NULL) );

	// end of recording
	CMenuOptionChooser* end_of_recording = new CMenuOptionChooser(LOCALE_RECORDINGMENU_END_OF_RECORDING_NAME, &g_settings.recording_epg_for_end, END_OF_RECORDING, END_OF_RECORDING_COUNT, true);
	recordingSettings->addItem(end_of_recording);

	//template
	//CStringInput * recordingSettings_filenameTemplate = new CStringInput(LOCALE_RECORDINGMENU_FILENAME_TEMPLATE, &g_settings.recording_filename_template[0], 21, LOCALE_RECORDINGMENU_FILENAME_TEMPLATE_HINT, LOCALE_IPSETUP_HINT_2, "%/-_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ");
	//CMenuForwarder* mf11 = new CMenuForwarder(LOCALE_RECORDINGMENU_FILENAME_TEMPLATE, true, g_settings.recording_filename_template[0],recordingSettings_filenameTemplate);
	recordingSettings->addItem(GenericMenuSeparatorLine);

	//timeshift
	CMenuWidget* recordingTsSettings = new CMenuWidget(LOCALE_MAINSETTINGS_RECORDING, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_RECORDSETUP_TIMESHIFT);
	showRecordTimeShiftSetup(recordingTsSettings);
	recordingSettings->addItem(new CMenuForwarder(LOCALE_RECORDINGMENU_TIMESHIFT, true, NULL, recordingTsSettings, NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));

	//timersettings
	CMenuWidget* recordingTimerSettings = new CMenuWidget(LOCALE_MAINSETTINGS_RECORDING, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_RECORDSETUP_TIMERSETTINGS);
	showRecordTimerSetup(recordingTimerSettings);
	recordingSettings->addItem(new CMenuForwarder(LOCALE_TIMERSETTINGS_SEPARATOR, true, NULL, recordingTimerSettings, NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));

	//audiosettings
	CMenuWidget* recordingaAudioSettings = new CMenuWidget(LOCALE_MAINSETTINGS_RECORDING, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_RECORDSETUP_AUDIOSETTINGS);
	showRecordAudioSetup(recordingaAudioSettings);
	recordingSettings->addItem(new CMenuForwarder(LOCALE_RECORDINGMENU_APIDS, true, NULL, recordingaAudioSettings, NULL, CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE));

	int res = recordingSettings->exec(NULL, "");
	recordingSettings->hide();
	delete recordingSettings;
	return res;
}

void CRecordSetup::showRecordTimerSetup(CMenuWidget *menu_timersettings)
{
	//recording start/end correcture
	int pre,post;
	g_Timerd->getRecordingSafety(pre,post);
	sprintf(g_settings.record_safety_time_before, "%02d", pre/60);
	sprintf(g_settings.record_safety_time_after, "%02d", post/60);

	//start 
	CRecordingSafetyNotifier *RecordingSafetyNotifier = new CRecordingSafetyNotifier;
	CStringInput * timerBefore = new CStringInput(LOCALE_TIMERSETTINGS_RECORD_SAFETY_TIME_BEFORE, g_settings.record_safety_time_before, 2, LOCALE_TIMERSETTINGS_RECORD_SAFETY_TIME_BEFORE_HINT_1, LOCALE_TIMERSETTINGS_RECORD_SAFETY_TIME_BEFORE_HINT_2,"0123456789 ", RecordingSafetyNotifier);
	CMenuForwarder *fTimerBefore = new CMenuForwarder(LOCALE_TIMERSETTINGS_RECORD_SAFETY_TIME_BEFORE, true, g_settings.record_safety_time_before, timerBefore);

	//end
	CStringInput * timerAfter = new CStringInput(LOCALE_TIMERSETTINGS_RECORD_SAFETY_TIME_AFTER, g_settings.record_safety_time_after, 2, LOCALE_TIMERSETTINGS_RECORD_SAFETY_TIME_AFTER_HINT_1, LOCALE_TIMERSETTINGS_RECORD_SAFETY_TIME_AFTER_HINT_2,"0123456789 ", RecordingSafetyNotifier);
	CMenuForwarder *fTimerAfter = new CMenuForwarder(LOCALE_TIMERSETTINGS_RECORD_SAFETY_TIME_AFTER, true, g_settings.record_safety_time_after, timerAfter);

	//announce
	CMenuOptionChooser* chzapAnnounce = new CMenuOptionChooser(LOCALE_RECORDINGMENU_ZAP_ON_ANNOUNCE, &g_settings.recording_zap_on_announce, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	CMenuOptionNumberChooser *chzapCorr = new CMenuOptionNumberChooser(LOCALE_MISCSETTINGS_ZAPTO_PRE_TIME, &g_settings.zapto_pre_time, true, 0, 10);

	menu_timersettings->addIntroItems(LOCALE_TIMERSETTINGS_SEPARATOR);
	menu_timersettings->addItem(fTimerBefore);
	menu_timersettings->addItem(fTimerAfter);
	menu_timersettings->addItem(chzapAnnounce);
	menu_timersettings->addItem(chzapCorr);
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
	CRecAPIDSettingsNotifier * an = new CRecAPIDSettingsNotifier;
	CMenuOptionChooser* aoj1 = new CMenuOptionChooser(LOCALE_RECORDINGMENU_APIDS_STD, &g_settings.recording_audio_pids_std, MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true, an);
	CMenuOptionChooser* aoj2 = new CMenuOptionChooser(LOCALE_RECORDINGMENU_APIDS_ALT, &g_settings.recording_audio_pids_alt, MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true, an);
	CMenuOptionChooser* aoj3 = new CMenuOptionChooser(LOCALE_RECORDINGMENU_APIDS_AC3, &g_settings.recording_audio_pids_ac3, MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true, an);

	menu_audiosettings->addIntroItems(LOCALE_RECORDINGMENU_APIDS);
	menu_audiosettings->addItem(aoj1);
	menu_audiosettings->addItem(aoj2);
	menu_audiosettings->addItem(aoj3);
}

void CRecordSetup::showRecordTimeShiftSetup(CMenuWidget *menu_ts)
{
	menu_ts->addIntroItems(LOCALE_RECORDINGMENU_TIMESHIFT);

	//timeshift dir
	CMenuForwarder* fTsDir = new CMenuForwarder(LOCALE_RECORDINGMENU_TSDIR, true, g_settings.timeshiftdir, this, "timeshiftdir");
	menu_ts->addItem(fTsDir);

	if (1) //has_hdd
	{ 
		menu_ts->addItem(new CMenuOptionChooser(LOCALE_EXTRA_TIMESHIFT_PAUSE, &g_settings.timeshift_pause, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
		menu_ts->addItem(new CMenuOptionNumberChooser(LOCALE_EXTRA_AUTO_TIMESHIFT, &g_settings.auto_timeshift, true, 0, 300, NULL));
		menu_ts->addItem(new CMenuOptionChooser(LOCALE_EXTRA_AUTO_DELETE, &g_settings.auto_delete, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
		menu_ts->addItem(new CMenuOptionChooser(LOCALE_EXTRA_TEMP_TIMESHIFT, &g_settings.temp_timeshift, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	}
}




