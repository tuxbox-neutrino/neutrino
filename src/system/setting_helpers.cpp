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

#include <system/setting_helpers.h>

#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include "libnet.h"

#include <coolstream/control.h>

#include <config.h>

#include <global.h>
#include <neutrino.h>
#include <gui/widget/stringinput.h>

// obsolete #include <gui/streaminfo.h>

#include <gui/widget/messagebox.h>
#include <gui/widget/hintbox.h>

#include <gui/plugins.h>
#include <daemonc/remotecontrol.h>
#include <xmlinterface.h>
#include <audio_cs.h>
#include <video_cs.h>
#include <dmx_cs.h>
#include <pwrmngr.h>
#include "libdvbsub/dvbsub.h"
#include "libtuxtxt/teletext.h"
#include <zapit/satconfig.h>

extern CPlugins       * g_PluginList;    /* neutrino.cpp */
extern CRemoteControl * g_RemoteControl; /* neutrino.cpp */
extern cVideo *videoDecoder;
extern cAudio *audioDecoder;

extern cDemux *videoDemux;
extern cDemux *audioDemux;
extern cDemux *pcrDemux;

extern "C" int pinghost( const char *hostname );

CSatelliteSetupNotifier::CSatelliteSetupNotifier()
{
}

/* items1 enabled for advanced diseqc settings, items2 for diseqc != NO_DISEQC, items3 disabled for NO_DISEQC */
bool CSatelliteSetupNotifier::changeNotify(const neutrino_locale_t, void * Data)
{
	std::vector<CMenuItem*>::iterator it;
	int type = *((int*) Data);

	if (type == NO_DISEQC) {
		for(it = items1.begin(); it != items1.end(); it++) {
			//(*it)->init(-1, 0, 0, 0);
			(*it)->setActive(false);
		}
		for(it = items2.begin(); it != items2.end(); it++) {
			//(*it)->init(-1, 0, 0, 0);
			(*it)->setActive(false);
		}
		for(it = items3.begin(); it != items3.end(); it++) {
			//(*it)->init(-1, 0, 0, 0);
			(*it)->setActive(false);
		}
	}
	else if(type < DISEQC_ADVANCED) {
		for(it = items1.begin(); it != items1.end(); it++) {
			//(*it)->init(-1, 0, 0, 0);
			(*it)->setActive(false);
		}
		for(it = items2.begin(); it != items2.end(); it++) {
			//(*it)->init(-1, 0, 0, 0);
			(*it)->setActive(true);
		}
		for(it = items3.begin(); it != items3.end(); it++) {
			//(*it)->init(-1, 0, 0, 0);
			(*it)->setActive(true);
		}
	}
	else if(type == DISEQC_ADVANCED) {
		for(it = items1.begin(); it != items1.end(); it++) {
			//(*it)->init(-1, 0, 0, 0);
			(*it)->setActive(true);
		}
		for(it = items2.begin(); it != items2.end(); it++) {
			//(*it)->init(-1, 0, 0, 0);
			(*it)->setActive(false);
		}
		for(it = items3.begin(); it != items3.end(); it++) {
			//(*it)->init(-1, 0, 0, 0);
			(*it)->setActive(true);
		}
	}
	g_Zapit->setDiseqcType((diseqc_t) type);
	g_Zapit->setDiseqcRepeat( CNeutrinoApp::getInstance()->getScanSettings().diseqcRepeat);
	return true;
}

void CSatelliteSetupNotifier::addItem(int list, CMenuItem* item)
{
	switch(list) {
		case 0:
			items1.push_back(item);
			break;
		case 1:
			items2.push_back(item);
			break;
		case 2:
			items3.push_back(item);
			break;
		default:
			break;
	}
}

bool CSatDiseqcNotifier::changeNotify(const neutrino_locale_t, void * Data)
{
	if (*((int*) Data) == NO_DISEQC)
	{
		satMenu->setActive(true);
		extMenu->setActive(false);
		extMotorMenu->setActive(false);
		repeatMenu->init(-1, 0, 0, 0);
		repeatMenu->setActive(false);
		motorControl->setActive(false);
	}
	else
	if (*((int*) Data) == DISEQC_1_2)
	{
		satMenu->setActive(true);
		extMenu->setActive(true);
		extMotorMenu->setActive(true);
		repeatMenu->init(-1, 0, 0, 0);
		repeatMenu->setActive(true);
		motorControl->setActive(true);
	}
	else
	{
		satMenu->setActive(true);
		extMenu->setActive(true);
		extMotorMenu->setActive(false);
		repeatMenu->init(-1, 0, 0, 0);
		repeatMenu->setActive((*((int*) Data) != DISEQC_1_0));
		motorControl->setActive(false);
	}
	g_Zapit->setDiseqcType( *((diseqc_t*) Data) );
	g_Zapit->setDiseqcRepeat( CNeutrinoApp::getInstance()->getScanSettings().diseqcRepeat);
	return true;
}

CTP_scanNotifier::CTP_scanNotifier(CMenuOptionChooser* i1, CMenuOptionChooser* i2, CMenuForwarder* i3, CMenuForwarder* i4)
{
	toDisable1[0]=i1;
	toDisable1[1]=i2;
	toDisable2[0]=i3;
	toDisable2[1]=i4;
}

bool CTP_scanNotifier::changeNotify(const neutrino_locale_t, void * Data)
{
	int val = *((int*) Data);
//printf("CTP_scanNotifier::changeNotify: data %d\n", val);
//FIXME: test
	//bool set_true_false=CNeutrinoApp::getInstance()->getScanSettings().TP_scan;
	bool set_true_false = (val == 2);
	for (int i=0; i<2; i++)
	{
		toDisable1[i]->setActive(set_true_false);
		toDisable2[i]->setActive(set_true_false);
	}
	return true;
}
CDHCPNotifier::CDHCPNotifier( CMenuForwarder* a1, CMenuForwarder* a2, CMenuForwarder* a3, CMenuForwarder* a4, CMenuForwarder* a5, CMenuForwarder* a6)
{
	toDisable[0] = a1;
	toDisable[1] = a2;
	toDisable[2] = a3;
	toDisable[3] = a4;
	toDisable[4] = a5;
	toEnable[0] = a6;
}


bool CDHCPNotifier::changeNotify(const neutrino_locale_t, void * data)
{
	CNeutrinoApp::getInstance()->networkConfig.inet_static = ((*(int*)(data)) == 0);
	for(int x=0;x<5;x++)
		toDisable[x]->setActive(CNeutrinoApp::getInstance()->networkConfig.inet_static);

	toEnable[0]->setActive(!CNeutrinoApp::getInstance()->networkConfig.inet_static);
	return true;
}

CStreamingNotifier::CStreamingNotifier( CMenuItem* i1, CMenuItem* i2, CMenuItem* i3, CMenuItem* i4, CMenuItem* i5, CMenuItem* i6, CMenuItem* i7, CMenuItem* i8, CMenuItem* i9, CMenuItem* i10, CMenuItem* i11)
{
   toDisable[0]=i1;
   toDisable[1]=i2;
   toDisable[2]=i3;
   toDisable[3]=i4;
   toDisable[4]=i5;
   toDisable[5]=i6;
   toDisable[6]=i7;
   toDisable[7]=i8;
   toDisable[8]=i9;
   toDisable[9]=i10;
   toDisable[10]=i11;
}

bool CStreamingNotifier::changeNotify(const neutrino_locale_t, void *)
{
   if(g_settings.streaming_type==0)
   {
      for (int i=0; i<=10; i++)
        toDisable[i]->setActive(false);
   }
   else if(g_settings.streaming_type==1)
   {
      for (int i=0; i<=10; i++)
        toDisable[i]->setActive(true);
   }
   return true;
}

COnOffNotifier::COnOffNotifier( CMenuItem* a1,CMenuItem* a2,CMenuItem* a3,CMenuItem* a4,CMenuItem* a5)
{
        number = 0;
        if(a1 != NULL){ toDisable[0] =a1;number++;};
        if(a2 != NULL){ toDisable[1] =a2;number++;};
        if(a3 != NULL){ toDisable[2] =a3;number++;};
        if(a4 != NULL){ toDisable[3] =a4;number++;};
        if(a5 != NULL){ toDisable[4] =a5;number++;};
}

bool COnOffNotifier::changeNotify(const neutrino_locale_t, void *Data)
{
   if(*(int*)(Data) == 0)
   {
      for (int i=0; i<number ; i++)
        toDisable[i]->setActive(false);
   }
   else
   {
      for (int i=0; i<number ; i++)
        toDisable[i]->setActive(true);
   }
   return true;
}

CRecordingNotifier::CRecordingNotifier(CMenuItem* i1 , CMenuItem* i2 , CMenuItem* i3 ,
                                       CMenuItem* i4 , CMenuItem* i5 , CMenuItem* i6 ,
                                       CMenuItem* i7 , CMenuItem* i8 , CMenuItem* i9)
{
	toDisable[ 0] = i1;
	toDisable[ 1] = i2;
	toDisable[ 2] = i3;
	toDisable[ 3] = i4;
	toDisable[ 4] = i5;
	toDisable[ 5] = i6;
	toDisable[ 6] = i7;
	toDisable[ 7] = i8;
	toDisable[ 8] = i9;
}
bool CRecordingNotifier::changeNotify(const neutrino_locale_t, void *)
{
   if ((g_settings.recording_type == CNeutrinoApp::RECORDING_OFF) ||
       (g_settings.recording_type == CNeutrinoApp::RECORDING_FILE))
   {
	   for(int i = 0; i < 8; i++) {
		   toDisable[i]->setActive(false);
	   }

	   if (g_settings.recording_type == CNeutrinoApp::RECORDING_FILE)
	   {
		   toDisable[7]->setActive(true);
	   }
   }
   else if (g_settings.recording_type == CNeutrinoApp::RECORDING_SERVER)
   {
	   toDisable[0]->setActive(true);
	   toDisable[1]->setActive(true);
	   toDisable[2]->setActive(true);
	   toDisable[3]->setActive(g_settings.recording_server_wakeup==1);
	   toDisable[4]->setActive(true);
	   toDisable[5]->setActive(true);
	   toDisable[6]->setActive(false);
	   toDisable[7]->setActive(false);
   }
   else if (g_settings.recording_type == CNeutrinoApp::RECORDING_VCR)
   {

	   toDisable[0]->setActive(false);
	   toDisable[1]->setActive(false);
	   toDisable[2]->setActive(false);
	   toDisable[3]->setActive(false);
	   toDisable[4]->setActive(false);
	   toDisable[5]->setActive(false);
	   toDisable[6]->setActive(true);
	   toDisable[7]->setActive(false);
   }

   return true;
}

CRecordingNotifier2::CRecordingNotifier2( CMenuItem* i)
{
   toDisable[0]=i;
}
bool CRecordingNotifier2::changeNotify(const neutrino_locale_t, void *)
{
   toDisable[0]->setActive(g_settings.recording_server_wakeup==1);
   return true;
}

bool CRecordingSafetyNotifier::changeNotify(const neutrino_locale_t, void *)
{
	g_Timerd->setRecordingSafety(atoi(g_settings.record_safety_time_before)*60, atoi(g_settings.record_safety_time_after)*60);
   return true;
}

CMiscNotifier::CMiscNotifier( CMenuItem* i1, CMenuItem* i2)
{
   toDisable[0]=i1;
   toDisable[1]=i2;
}
bool CMiscNotifier::changeNotify(const neutrino_locale_t, void *)
{
   toDisable[0]->setActive(!g_settings.shutdown_real);
   toDisable[1]->setActive(!g_settings.shutdown_real);
   return true;
}

bool CLcdNotifier::changeNotify(const neutrino_locale_t, void *)
{
	CVFD::getInstance()->setlcdparameter();
	//CLCD::getInstance()->setAutoDimm(g_settings.lcd_setting[SNeutrinoSettings::LCD_AUTODIMM]);
	return true;
}

int CRfExec::exec(CMenuTarget* /*parent*/, const std::string& /*actionKey*/)
{
	g_RFmod->init();
	return true;
}

bool CRfNotifier::changeNotify(const neutrino_locale_t OptionName, void * val)
{
//printf("CRfNotifier: Option %d val %d\n", OptionName, *((int *)val));
	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_RF_CARRIER)) {
		g_RFmod->setSoundSubCarrier(*((int *)val));
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_RF_ENABLE)) {
		g_RFmod->setSoundEnable(*((int *)val));
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_RF_CHANNEL)) {
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_RF_FINETUNE)) {
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_RF_STANDBY)) {
		g_RFmod->setStandby(*((int *)val));
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_RF_TEST)) {
		g_RFmod->setTestPattern(*((int *)val));
	}
	return true;
}

bool CPauseSectionsdNotifier::changeNotify(const neutrino_locale_t, void * Data)
{
	g_Sectionsd->setPauseScanning((*((int *)Data)) == 0);

	return true;
}

bool CSectionsdConfigNotifier::changeNotify(const neutrino_locale_t, void *)
{
        CNeutrinoApp::getInstance()->SendSectionsdConfig();
        return true;
}

bool CTouchFileNotifier::changeNotify(const neutrino_locale_t, void * data)
{
	if ((*(int *)data) != 0)
	{
		FILE * fd = fopen(filename, "w");
		if (fd)
			fclose(fd);
		else
			return false;
	}
	else
		remove(filename);
	return true;
}

bool CColorSetupNotifier::changeNotify(const neutrino_locale_t, void *)
{
	CFrameBuffer *frameBuffer = CFrameBuffer::getInstance();
//	unsigned char r,g,b;
	//setting colors-..
	frameBuffer->paletteGenFade(COL_MENUHEAD,
	                              convertSetupColor2RGB(g_settings.menu_Head_red, g_settings.menu_Head_green, g_settings.menu_Head_blue),
	                              convertSetupColor2RGB(g_settings.menu_Head_Text_red, g_settings.menu_Head_Text_green, g_settings.menu_Head_Text_blue),
	                              8, convertSetupAlpha2Alpha( g_settings.menu_Head_alpha ) );

	frameBuffer->paletteGenFade(COL_MENUCONTENT,
	                              convertSetupColor2RGB(g_settings.menu_Content_red, g_settings.menu_Content_green, g_settings.menu_Content_blue),
	                              convertSetupColor2RGB(g_settings.menu_Content_Text_red, g_settings.menu_Content_Text_green, g_settings.menu_Content_Text_blue),
	                              8, convertSetupAlpha2Alpha(g_settings.menu_Content_alpha) );


	frameBuffer->paletteGenFade(COL_MENUCONTENTDARK,
	                              convertSetupColor2RGB(int(g_settings.menu_Content_red*0.6), int(g_settings.menu_Content_green*0.6), int(g_settings.menu_Content_blue*0.6)),
	                              convertSetupColor2RGB(g_settings.menu_Content_Text_red, g_settings.menu_Content_Text_green, g_settings.menu_Content_Text_blue),
	                              8, convertSetupAlpha2Alpha(g_settings.menu_Content_alpha) );

	frameBuffer->paletteGenFade(COL_MENUCONTENTSELECTED,
	                              convertSetupColor2RGB(g_settings.menu_Content_Selected_red, g_settings.menu_Content_Selected_green, g_settings.menu_Content_Selected_blue),
	                              convertSetupColor2RGB(g_settings.menu_Content_Selected_Text_red, g_settings.menu_Content_Selected_Text_green, g_settings.menu_Content_Selected_Text_blue),
	                              8, convertSetupAlpha2Alpha(g_settings.menu_Content_Selected_alpha) );

	frameBuffer->paletteGenFade(COL_MENUCONTENTINACTIVE,
	                              convertSetupColor2RGB(g_settings.menu_Content_inactive_red, g_settings.menu_Content_inactive_green, g_settings.menu_Content_inactive_blue),
	                              convertSetupColor2RGB(g_settings.menu_Content_inactive_Text_red, g_settings.menu_Content_inactive_Text_green, g_settings.menu_Content_inactive_Text_blue),
	                              8, convertSetupAlpha2Alpha(g_settings.menu_Content_inactive_alpha) );

	frameBuffer->paletteGenFade(COL_INFOBAR,
	                              convertSetupColor2RGB(g_settings.infobar_red, g_settings.infobar_green, g_settings.infobar_blue),
	                              convertSetupColor2RGB(g_settings.infobar_Text_red, g_settings.infobar_Text_green, g_settings.infobar_Text_blue),
	                              8, convertSetupAlpha2Alpha(g_settings.infobar_alpha) );

/*	frameBuffer->paletteSetColor( COL_INFOBAR_SHADOW,
	                                convertSetupColor2RGB(
	                                    int(g_settings.infobar_red*0.4),
	                                    int(g_settings.infobar_green*0.4),
	                                    int(g_settings.infobar_blue*0.4)),
	                                g_settings.infobar_alpha);
*/
	frameBuffer->paletteGenFade(COL_INFOBAR_SHADOW,
	                              convertSetupColor2RGB(int(g_settings.infobar_red*0.4), int(g_settings.infobar_green*0.4), int(g_settings.infobar_blue*0.4)),
	                              convertSetupColor2RGB(g_settings.infobar_Text_red, g_settings.infobar_Text_green, g_settings.infobar_Text_blue),
	                              8, convertSetupAlpha2Alpha(g_settings.infobar_alpha) );


	frameBuffer->paletteSet();
	return false;
}

bool CAudioSetupNotifier::changeNotify(const neutrino_locale_t OptionName, void *)
{
	//printf("notify: %d\n", OptionName);
#if 0 //FIXME to do ? manual audio delay
	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_AUDIOMENU_PCMOFFSET))
	{
	}
#endif
	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_AUDIOMENU_ANALOG_MODE)) {
		g_Zapit->setAudioMode(g_settings.audio_AnalogMode);
	} else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_AUDIOMENU_ANALOG_OUT)) {
		audioDecoder->EnableAnalogOut(g_settings.analog_out ? true : false);
	} else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_AUDIOMENU_HDMI_DD)) {
		audioDecoder->SetHdmiDD(g_settings.hdmi_dd ? true : false);
	} else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_AUDIOMENU_SPDIF_DD)) {
		audioDecoder->SetSpdifDD(g_settings.spdif_dd ? true : false);
	} else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_AUDIOMENU_AVSYNC)) {
		videoDecoder->SetSyncMode((AVSYNC_TYPE)g_settings.avsync);
		audioDecoder->SetSyncMode((AVSYNC_TYPE)g_settings.avsync);
		videoDemux->SetSyncMode((AVSYNC_TYPE)g_settings.avsync);
		audioDemux->SetSyncMode((AVSYNC_TYPE)g_settings.avsync);
		pcrDemux->SetSyncMode((AVSYNC_TYPE)g_settings.avsync);
	} else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_AUDIOMENU_CLOCKREC)) {
		//.Clock recovery enable/disable
		// FIXME add code here.
	} else { // FIXME atm used for SRS
		audioDecoder->SetSRS(g_settings.srs_enable, g_settings.srs_nmgr_enable, g_settings.srs_algo, g_settings.srs_ref_volume);
	}
	return true;
}

//FIXME
#define IOC_IR_SET_F_DELAY      _IOW(0xDD,  5, unsigned int)            /* set the delay time before the first repetition */
#define IOC_IR_SET_X_DELAY      _IOW(0xDD,  6, unsigned int)            /* set the delay time between all other repetitions */

bool CKeySetupNotifier::changeNotify(const neutrino_locale_t, void *)
{
	unsigned int fdelay = atoi(g_settings.repeat_blocker);
	unsigned int xdelay = atoi(g_settings.repeat_genericblocker);

	g_RCInput->repeat_block = fdelay * 1000;
	g_RCInput->repeat_block_generic = xdelay * 1000;

	int fd = g_RCInput->getFileHandle();
	ioctl(fd, IOC_IR_SET_F_DELAY, fdelay);
	ioctl(fd, IOC_IR_SET_X_DELAY, xdelay);
	return false;
}

bool CIPChangeNotifier::changeNotify(const neutrino_locale_t, void * Data)
{
	char ip[16];
	unsigned char _ip[4];
	sscanf((char*) Data, "%hhu.%hhu.%hhu.%hhu", &_ip[0], &_ip[1], &_ip[2], &_ip[3]);

	sprintf(ip, "%hhu.%hhu.%hhu.255", _ip[0], _ip[1], _ip[2]);
	CNeutrinoApp::getInstance()->networkConfig.broadcast = ip;

	CNeutrinoApp::getInstance()->networkConfig.netmask = (_ip[0] == 10) ? "255.0.0.0" : "255.255.255.0";

	sprintf(ip, "%hhu.%hhu.%hhu.1", _ip[0], _ip[1], _ip[2]);
	CNeutrinoApp::getInstance()->networkConfig.nameserver = ip;
	CNeutrinoApp::getInstance()->networkConfig.gateway = ip;

	return true;
}

bool CConsoleDestChangeNotifier::changeNotify(const neutrino_locale_t, void * Data)
{
	g_settings.uboot_console = *(int *)Data;

	return true;
}

bool CTimingSettingsNotifier::changeNotify(const neutrino_locale_t OptionName, void *)
{
	for (int i = 0; i < TIMING_SETTING_COUNT; i++)
	{
		if (ARE_LOCALES_EQUAL(OptionName, timing_setting_name[i]))
		{
			g_settings.timing[i] = 	atoi(g_settings.timing_string[i]);
			return true;
		}
	}
	return false;
}

bool CFontSizeNotifier::changeNotify(const neutrino_locale_t, void *)
{
	CHintBox hintBox(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_FONTSIZE_HINT)); // UTF-8
	hintBox.paint();

	CNeutrinoApp::getInstance()->SetupFonts();

	hintBox.hide();

	return true;
}

bool CRecAPIDSettingsNotifier::changeNotify(const neutrino_locale_t, void *)
{
	g_settings.recording_audio_pids_default = ( (g_settings.recording_audio_pids_std ? TIMERD_APIDS_STD : 0) |
						  (g_settings.recording_audio_pids_alt ? TIMERD_APIDS_ALT : 0) |
						  (g_settings.recording_audio_pids_ac3 ? TIMERD_APIDS_AC3 : 0));
	return true;
}

int CAPIDChangeExec::exec(CMenuTarget* /*parent*/, const std::string & actionKey)
{
	//    printf("CAPIDChangeExec exec: %s\n", actionKey.c_str());
	unsigned int sel= atoi(actionKey.c_str());
	if (g_RemoteControl->current_PIDs.PIDs.selected_apid!= sel )
	{
		g_RemoteControl->setAPID(sel);
	}
	return menu_return::RETURN_EXIT;
}

int CSubtitleChangeExec::exec(CMenuTarget* /*parent*/, const std::string & actionKey)
{
printf("CSubtitleChangeExec::exec: action %s\n", actionKey.c_str());
	if(actionKey == "off") {
		tuxtx_stop_subtitle();
		dvbsub_stop();
		return menu_return::RETURN_EXIT;
	}
	if(!strncmp(actionKey.c_str(), "DVB", 3)) {
		const char *pidptr = strchr(actionKey.c_str(), ':');
		int pid = atoi(pidptr+1);
		tuxtx_stop_subtitle();
		dvbsub_pause();
		dvbsub_start(pid);
	} else {
		const char *ptr = strchr(actionKey.c_str(), ':');
		ptr++;
		int pid = atoi(ptr);
		ptr = strchr(ptr, ':');
		ptr++;
		int page = strtol(ptr, NULL, 16);
		ptr = strchr(ptr, ':');
		ptr++;
printf("CSubtitleChangeExec::exec: TTX, pid %x page %x lang %s\n", pid, page, ptr);
		tuxtx_stop_subtitle();
		tuxtx_set_pid(pid, page, ptr);
		dvbsub_stop();
		tuxtx_main(g_RCInput->getFileHandle(), pid, page);
	}
        return menu_return::RETURN_EXIT;
}

int CNVODChangeExec::exec(CMenuTarget* parent, const std::string & actionKey)
{
	//    printf("CNVODChangeExec exec: %s\n", actionKey.c_str());
	unsigned sel= atoi(actionKey.c_str());
	g_RemoteControl->setSubChannel(sel);

	parent->hide();
	g_InfoViewer->showSubchan();
	return menu_return::RETURN_EXIT;
}

int CStreamFeaturesChangeExec::exec(CMenuTarget* parent, const std::string & actionKey)
{
	//printf("CStreamFeaturesChangeExec exec: %s\n", actionKey.c_str());
	int sel= atoi(actionKey.c_str());

	if(parent != NULL)
		parent->hide();
	// -- obsolete (rasc 2004-06-10)
	// if (sel==-1)
	// {
	// 	CStreamInfo StreamInfo;
	//	StreamInfo.exec(NULL, "");
	// } else
	if(actionKey == "teletext") {
		g_RCInput->postMsg(CRCInput::RC_timeout, 0);
		g_RCInput->postMsg(CRCInput::RC_text, 0);
#if 0
		g_RCInput->clearRCMsg();
		tuxtx_main(g_RCInput->getFileHandle(), frameBuffer->getFrameBufferPointer(), g_RemoteControl->current_PIDs.PIDs.vtxtpid);
		frameBuffer->paintBackground();
		if(!g_settings.cacheTXT)
			tuxtxt_stop();
		g_RCInput->clearRCMsg();
#endif
	}
	else if (sel>=0)
	{
		g_PluginList->startPlugin(sel,0);
	}

	return menu_return::RETURN_EXIT;
}

int CMoviePluginChangeExec::exec(CMenuTarget* parent, const std::string & actionKey)
{
	int sel= atoi(actionKey.c_str());
	parent->hide();
	if (sel>=0)
	{
			g_settings.movieplayer_plugin=g_PluginList->getName(sel);
	}
	return menu_return::RETURN_EXIT;
}
int COnekeyPluginChangeExec::exec(CMenuTarget* parent, const std::string & actionKey)
{
	int sel= atoi(actionKey.c_str());
	parent->hide();
	if (sel>=0)
	{
			g_settings.onekey_plugin=g_PluginList->getName(sel);
	}
	return menu_return::RETURN_EXIT;
}

int CUCodeCheckExec::exec(CMenuTarget* /*parent*/, const std::string & /*actionKey*/)
{
#if 0
	std::string text;
	char res[60];

	text = g_Locale->getText(LOCALE_UCODECHECK_AVIA500);
	text += ": ";
	checkFile((char *) UCODEDIR "/avia500.ux", (char*) &res);
	text += res;
	text += '\n';
	text += g_Locale->getText(LOCALE_UCODECHECK_AVIA600);
	text += ": ";
	checkFile(UCODEDIR "/avia600.ux", (char*) &res);
	text += res;
	text += '\n';
	text += g_Locale->getText(LOCALE_UCODECHECK_UCODE);
	text += ": ";
	checkFile(UCODEDIR "/ucode.bin", (char*) &res);
	if (strcmp("not found", res) == 0)
		text += "ucode_0014 (built-in)";
	else
		text += res;
	text += '\n';
	text += g_Locale->getText(LOCALE_UCODECHECK_CAM_ALPHA);
	text += ": ";
	checkFile(UCODEDIR "/cam-alpha.bin", (char*) &res);
	text += res;

	ShowMsgUTF(LOCALE_UCODECHECK_HEAD, text, CMessageBox::mbrBack, CMessageBox::mbBack); // UTF-8
#endif
	return 1;
}

const char * mypinghost(const char * const host)
{
	int retvalue = pinghost(host);
	switch (retvalue)
	{
		case 1: return (g_Locale->getText(LOCALE_PING_OK));
		case 0: return (g_Locale->getText(LOCALE_PING_UNREACHABLE));
		case -1: return (g_Locale->getText(LOCALE_PING_PROTOCOL));
		case -2: return (g_Locale->getText(LOCALE_PING_SOCKET));
	}
	return "";
}

void testNetworkSettings(const char* ip, const char* netmask, const char* broadcast, const char* gateway, const char* nameserver, bool ip_static)
{
	char our_ip[16];
	char our_mask[16];
	char our_broadcast[16];
	char our_gateway[16];
	char our_nameserver[16];
	std::string text;

	if (ip_static) {
		strcpy(our_ip,ip);
		strcpy(our_mask,netmask);
		strcpy(our_broadcast,broadcast);
		strcpy(our_gateway,gateway);
		strcpy(our_nameserver,nameserver);
	}
	else {
		netGetIP((char *) "eth0",our_ip,our_mask,our_broadcast);
		netGetDefaultRoute(our_gateway);
		netGetNameserver(our_nameserver);
	}

	printf("testNw IP       : %s\n", our_ip);
	printf("testNw Netmask  : %s\n", our_mask);
	printf("testNw Broadcast: %s\n", our_broadcast);
	printf("testNw Gateway: %s\n", our_gateway);
	printf("testNw Nameserver: %s\n", our_nameserver);

	text = our_ip;
	text += ": ";
	text += mypinghost(our_ip);
	text += '\n';
	text += g_Locale->getText(LOCALE_NETWORKMENU_GATEWAY);
	text += ": ";
	text += our_gateway;
	text += ' ';
	text += mypinghost(our_gateway);
	text += '\n';
	text += g_Locale->getText(LOCALE_NETWORKMENU_NAMESERVER);
	text += ": ";
	text += our_nameserver;
	text += ' ';
	text += mypinghost(our_nameserver);
	text += "\ndboxupdate.berlios.de: ";
	text += mypinghost("195.37.77.138");

	ShowMsgUTF(LOCALE_NETWORKMENU_TEST, text, CMessageBox::mbrBack, CMessageBox::mbBack); // UTF-8
}

void showCurrentNetworkSettings()
{
	char ip[16];
	char mask[16];
	char broadcast[16];
	char router[16];
	char nameserver[16];
	std::string text;

	netGetIP((char *) "eth0",ip,mask,broadcast);
	if (ip[0] == 0) {
		text = "Network inactive\n";
	}
	else {
		netGetNameserver(nameserver);
		netGetDefaultRoute(router);
		text  = g_Locale->getText(LOCALE_NETWORKMENU_IPADDRESS );
		text += ": ";
		text += ip;
		text += '\n';
		text += g_Locale->getText(LOCALE_NETWORKMENU_NETMASK   );
		text += ": ";
		text += mask;
		text += '\n';
		text += g_Locale->getText(LOCALE_NETWORKMENU_BROADCAST );
		text += ": ";
		text += broadcast;
		text += '\n';
		text += g_Locale->getText(LOCALE_NETWORKMENU_NAMESERVER);
		text += ": ";
		text += nameserver;
		text += '\n';
		text += g_Locale->getText(LOCALE_NETWORKMENU_GATEWAY   );
		text += ": ";
		text += router;
	}
	ShowMsgUTF(LOCALE_NETWORKMENU_SHOW, text, CMessageBox::mbrBack, CMessageBox::mbBack); // UTF-8
}

uint64_t getcurrenttime()
{
	struct timeval tv;
	gettimeofday( &tv, NULL );
	return (uint64_t) tv.tv_usec + (uint64_t)((uint64_t) tv.tv_sec * (uint64_t) 1000000);
}
// USERMENU
#define USERMENU_ITEM_OPTION_COUNT SNeutrinoSettings::ITEM_MAX
const CMenuOptionChooser::keyval USERMENU_ITEM_OPTIONS[USERMENU_ITEM_OPTION_COUNT] =
{
        { SNeutrinoSettings::ITEM_NONE,			LOCALE_USERMENU_ITEM_NONE },
        { SNeutrinoSettings::ITEM_BAR,			LOCALE_USERMENU_ITEM_BAR },
        { SNeutrinoSettings::ITEM_EPG_LIST,		LOCALE_EPGMENU_EVENTLIST },
        { SNeutrinoSettings::ITEM_EPG_SUPER,		LOCALE_EPGMENU_EPGPLUS },
        { SNeutrinoSettings::ITEM_EPG_INFO,		LOCALE_EPGMENU_EVENTINFO },
        { SNeutrinoSettings::ITEM_EPG_MISC,		LOCALE_USERMENU_ITEM_EPG_MISC },
        { SNeutrinoSettings::ITEM_AUDIO_SELECT,		LOCALE_AUDIOSELECTMENUE_HEAD },
        { SNeutrinoSettings::ITEM_SUBCHANNEL,		LOCALE_INFOVIEWER_SUBSERVICE },
        { SNeutrinoSettings::ITEM_RECORD,		LOCALE_TIMERLIST_TYPE_RECORD },
        { SNeutrinoSettings::ITEM_MOVIEPLAYER_MB,	LOCALE_MOVIEBROWSER_HEAD },
        { SNeutrinoSettings::ITEM_TIMERLIST,		LOCALE_TIMERLIST_NAME },
        { SNeutrinoSettings::ITEM_REMOTE,		LOCALE_RCLOCK_MENUEADD },
        { SNeutrinoSettings::ITEM_FAVORITS,		LOCALE_FAVORITES_MENUEADD },
        { SNeutrinoSettings::ITEM_TECHINFO,		LOCALE_EPGMENU_STREAMINFO },
        { SNeutrinoSettings::ITEM_PLUGIN,		LOCALE_TIMERLIST_PLUGIN },
        { SNeutrinoSettings::ITEM_VTXT,			LOCALE_USERMENU_ITEM_VTXT },
#if 0
        { SNeutrinoSettings::ITEM_MOVIEPLAYER_TS,	LOCALE_MAINMENU_MOVIEPLAYER } ,
        { SNeutrinoSettings::ITEM_RESTART_CAMD,		LOCALE_EXTRA_RESTARTCAMD }
#endif
};

int CUserMenuMenu::exec(CMenuTarget* parent, const std::string & /*actionKey*/)
{
        if(parent != NULL)
                parent->hide();

        CMenuWidget menu (local , NEUTRINO_ICON_KEYBINDING);
        menu.addItem(GenericMenuSeparator);
        menu.addItem(GenericMenuBack);
        menu.addItem(GenericMenuSeparatorLine);

        CStringInputSMS name(LOCALE_USERMENU_NAME,    &g_settings.usermenu_text[button], 11, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz��/- ");
        menu.addItem(new CMenuForwarder(LOCALE_USERMENU_NAME,    true, g_settings.usermenu_text[button],&name));
        menu.addItem(GenericMenuSeparatorLine);

        char text[10];
        for(int item = 0; item < SNeutrinoSettings::ITEM_MAX && item <13; item++) // Do not show more than 13 items
        {
                snprintf(text,10,"%d:",item);
                text[9]=0;// terminate for sure
                //menu.addItem( new CMenuOptionChooser(text, &g_settings.usermenu[button][item], USERMENU_ITEM_OPTIONS, USERMENU_ITEM_OPTION_COUNT,true ));
                menu.addItem( new CMenuOptionChooser(text, &g_settings.usermenu[button][item], USERMENU_ITEM_OPTIONS, USERMENU_ITEM_OPTION_COUNT,true, NULL, CRCInput::RC_nokey, "", true ));
        }

        menu.exec(NULL,"");

        return menu_return::RETURN_REPAINT;
}

bool CTZChangeNotifier::changeNotify(const neutrino_locale_t, void * Data)
{
	bool found = false;
	std::string name, zone;
	printf("CTZChangeNotifier::changeNotify: %s\n", (char *) Data);

        xmlDocPtr parser = parseXmlFile("/etc/timezone.xml");
        if (parser != NULL) {
                xmlNodePtr search = xmlDocGetRootElement(parser)->xmlChildrenNode;
                while (search) {
                        if (!strcmp(xmlGetName(search), "zone")) {
				name = xmlGetAttribute(search, "name");
				if(!strcmp(g_settings.timezone, name.c_str())) {
					zone = xmlGetAttribute(search, "zone");
					found = true;
					break;
				}
                        }
                        search = search->xmlNextNode;
                }
                xmlFreeDoc(parser);
        }
	if(found) {
		printf("Timezone: %s -> %s\n", name.c_str(), zone.c_str());
		std::string cmd = "cp /usr/share/zoneinfo/" + zone + " /etc/localtime";
		printf("exec %s\n", cmd.c_str());
		system(cmd.c_str());
		cmd = ":" + zone;
		setenv("TZ", cmd.c_str(), 1);
	}

	return true;
}

extern Zapit_config zapitCfg;
void loadZapitSettings();
void getZapitConfig(Zapit_config *Cfg);

int CDataResetNotifier::exec(CMenuTarget* /*parent*/, const std::string& actionKey)
{
	bool delete_all = (actionKey == "all");
	bool delete_chan = (actionKey == "channels") || delete_all;
	bool delete_set = (actionKey == "settings") || delete_all;
	neutrino_locale_t msg = delete_all ? LOCALE_RESET_ALL : delete_chan ? LOCALE_RESET_CHANNELS : LOCALE_RESET_SETTINGS;

	int result = ShowMsgUTF(msg, g_Locale->getText(LOCALE_RESET_CONFIRM), CMessageBox::mbrNo, CMessageBox::mbYes | CMessageBox::mbNo);
	if(result != CMessageBox::mbrYes)
		return true;

	if(delete_all) {
		system("rm -f /var/tuxbox/config/zapit/*.conf");
		loadZapitSettings();
		getZapitConfig(&zapitCfg);
	}
	if(delete_set) {
		unlink(NEUTRINO_SETTINGS_FILE);
		//unlink(NEUTRINO_SCAN_SETTINGS_FILE);
		CNeutrinoApp::getInstance()->loadSetup(NEUTRINO_SETTINGS_FILE);
		CNeutrinoApp::getInstance()->saveSetup(NEUTRINO_SETTINGS_FILE);
		CNeutrinoApp::getInstance()->loadColors(NEUTRINO_SETTINGS_FILE);
		//CNeutrinoApp::getInstance()->SetupFonts();
		CNeutrinoApp::getInstance()->SetupTiming();
	}
	if(delete_chan) {
		system("rm -f /var/tuxbox/config/zapit/*.xml");
		g_Zapit->reinitChannels();
	}
	return true;
}

bool CFanControlNotifier::changeNotify(const neutrino_locale_t, void * data)
{
	int cfd, ret;
	//unsigned char speed = (unsigned char) g_settings.fan_speed;
	unsigned int speed = * (int *) data;

	printf("FAN Speed %d\n", speed);
	cfd = open("/dev/cs_control", O_RDONLY);
	if(cfd < 0) {
		perror("Cannot open /dev/cs_control");
		return false;
	}
	ret = ioctl(cfd, IOC_CONTROL_PWM_SPEED, speed);
	close(cfd);
	if(ret < 0) {
		perror("IOC_CONTROL_PWM_SPEED");
		return false;
	}
	return true;
}

extern cCpuFreqManager * cpuFreq;
bool CCpuFreqNotifier::changeNotify(const neutrino_locale_t, void * data)
{
	int freq = * (int *) data;

	printf("CCpuFreqNotifier: %d Mhz\n", freq);
	freq *= 1000*1000;

	cpuFreq->SetCpuFreq(freq);
	return true;
}

bool CScreenPresetNotifier::changeNotify(const neutrino_locale_t /*OptionName*/, void * data)
{
	int preset = * (int *) data;
printf("CScreenPresetNotifier::changeNotify preset %d (setting %d)\n", preset, g_settings.screen_preset);

	g_settings.screen_StartX = g_settings.screen_preset ? g_settings.screen_StartX_lcd : g_settings.screen_StartX_crt;
	g_settings.screen_StartY = g_settings.screen_preset ? g_settings.screen_StartY_lcd : g_settings.screen_StartY_crt;
	g_settings.screen_EndX = g_settings.screen_preset ? g_settings.screen_EndX_lcd : g_settings.screen_EndX_crt;
	g_settings.screen_EndY = g_settings.screen_preset ? g_settings.screen_EndY_lcd : g_settings.screen_EndY_crt;
	CFrameBuffer::getInstance()->Clear();
	return true;
}

bool CAllUsalsNotifier::changeNotify(const neutrino_locale_t /*OptionName*/, void * data)
{
	int onoff = * (int *) data;
printf("CAllUsalsNotifier::changeNotify: %s\n", onoff ? "ON" : "OFF");

	sat_iterator_t sit;

	for (sit = satellitePositions.begin(); sit != satellitePositions.end(); sit++) {
		sit->second.use_usals = onoff;
	}
	return true;
}
