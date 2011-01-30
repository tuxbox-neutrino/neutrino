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

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <math.h>

#include <global.h>
#include <neutrino.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/messagebox.h>

#include <driver/vcrcontrol.h>
#include <driver/encoding.h>
#include <driver/stream2file.h>

#include <daemonc/remotecontrol.h>
#include <zapit/client/zapittools.h>

extern CRemoteControl * g_RemoteControl; /* neutrino.cpp */
int was_record = 0;
extern bool autoshift;
extern bool autoshift_delete;

#define SA struct sockaddr
#define SAI struct sockaddr_in
extern "C" {
#include <driver/genpsi.h>
}

extern t_channel_id rec_channel_id;

static CVCRControl vcrControl;

CVCRControl * CVCRControl::getInstance()
{
	return &vcrControl;
}

//-------------------------------------------------------------------------
CVCRControl::CVCRControl()
{
	Device = NULL;
}

//-------------------------------------------------------------------------
CVCRControl::~CVCRControl()
{
	unregisterDevice();
}

//-------------------------------------------------------------------------
void CVCRControl::unregisterDevice()
{
	if (Device)
	{
		delete Device;
		Device = NULL;
	}
}

//-------------------------------------------------------------------------
void CVCRControl::registerDevice(CDevice * const device)
{
	unregisterDevice();

	Device = device;
	if(CNeutrinoApp::getInstance()->recordingstatus)
		Device->deviceState = CMD_VCR_RECORD;
}

//-------------------------------------------------------------------------
bool CVCRControl::Record(const CTimerd::RecordingInfo * const eventinfo)
{
	int mode = g_Zapit->isChannelTVChannel(eventinfo->channel_id) ? NeutrinoMessages::mode_tv : NeutrinoMessages::mode_radio;

	return Device->Record(eventinfo->channel_id, mode, eventinfo->epgID, eventinfo->epgTitle, eventinfo->apids, eventinfo->epg_starttime);
}

MI_MOVIE_INFO * CVCRControl::GetMovieInfo(void)
{
	if(Device)
		return Device->recMovieInfo;
	return NULL;
}

bool CVCRControl::GetPids(unsigned short *vpid, unsigned short *vtype, unsigned short *apid, unsigned short *atype, unsigned short * apidnum, unsigned short * apids, unsigned short * atypes)
{
	if(Device) {
		if(vpid)
			*vpid = Device->rec_vpid;
		if(vtype)
			*vtype = Device->rec_vtype;
		if(apid)
			*apid = Device->rec_currentapid;
		if(atype)
			*atype = Device->rec_currentac3;
		if(apidnum) {
			*apidnum = Device->rec_numpida;
			for(int i = 0; i < Device->rec_numpida; i++) {
				if(apids)
					apids[i] = Device->rec_apids[i];
				if(atypes)
					atypes[i] = Device->rec_ac3flags[i];
			}
		}
		return true;
	}
	return false;
}

//-------------------------------------------------------------------------
void CVCRControl::CDevice::getAPIDs(const unsigned char _apidmode, APIDList & apid_list)
{
	unsigned char apidmode = _apidmode;

        if (apidmode == TIMERD_APIDS_CONF)
                apidmode = g_settings.recording_audio_pids_default;

        apid_list.clear();
        //CZapitClient::responseGetPIDs allpids;
        g_Zapit->getPIDS(allpids);

        // assume smallest apid ist std apid
        if (apidmode & TIMERD_APIDS_STD) {
                uint32_t apid_min=UINT_MAX;
                uint32_t apid_min_idx=0;
                for(unsigned int i = 0; i < allpids.APIDs.size(); i++) {
                        if (allpids.APIDs[i].pid < apid_min && !allpids.APIDs[i].is_ac3) {
                                apid_min = allpids.APIDs[i].pid;
                                apid_min_idx = i;
                        }
                }
                if (apid_min != UINT_MAX) {
                        APIDDesc a = {apid_min, apid_min_idx, false};
                        apid_list.push_back(a);
                }
        }
        if (apidmode & TIMERD_APIDS_ALT) {
                uint32_t apid_min=UINT_MAX;
                uint32_t apid_min_idx=0;
                for(unsigned int i = 0; i < allpids.APIDs.size(); i++) {
                        if (allpids.APIDs[i].pid < apid_min && !allpids.APIDs[i].is_ac3) {
                                apid_min = allpids.APIDs[i].pid;
                                apid_min_idx = i;
                        }
                }
                for(unsigned int i = 0; i < allpids.APIDs.size(); i++) {
                        if (allpids.APIDs[i].pid != apid_min && !allpids.APIDs[i].is_ac3) {
                                APIDDesc a = {allpids.APIDs[i].pid, i, false};
                                apid_list.push_back(a);
                        }
                }
        }
        if (apidmode & TIMERD_APIDS_AC3) {
                bool ac3_found=false;
                for(unsigned int i = 0; i < allpids.APIDs.size(); i++) {
                        if (allpids.APIDs[i].is_ac3) {
                                APIDDesc a = {allpids.APIDs[i].pid, i, true};
                                apid_list.push_back(a);
                                ac3_found=true;
                        }
                }
                // add non ac3 apid if ac3 not found
                if (!(apidmode & TIMERD_APIDS_STD) && !ac3_found) {
                        uint32_t apid_min=UINT_MAX;
                        uint32_t apid_min_idx=0;
                        for(unsigned int i = 0; i < allpids.APIDs.size(); i++) {
                                if (allpids.APIDs[i].pid < apid_min && !allpids.APIDs[i].is_ac3) {
                                        apid_min = allpids.APIDs[i].pid;
                                        apid_min_idx = i;
                                }
                        }
                        if (apid_min != UINT_MAX) {
                                APIDDesc a = {apid_min, apid_min_idx, false};
                                apid_list.push_back(a);
                        }
                }
        }
        // no apid selected use standard
        if (apid_list.empty() && !allpids.APIDs.empty()) {
                uint32_t apid_min=UINT_MAX;
                uint32_t apid_min_idx=0;
                for(unsigned int i = 0; i < allpids.APIDs.size(); i++) {
                        if (allpids.APIDs[i].pid < apid_min && !allpids.APIDs[i].is_ac3) {
                                apid_min = allpids.APIDs[i].pid;
                                apid_min_idx = i;
                        }
                }
                if (apid_min != UINT_MAX) {
                        APIDDesc a = {apid_min, apid_min_idx, false};
                        apid_list.push_back(a);
                }
                for(APIDList::iterator it = apid_list.begin(); it != apid_list.end(); it++)
                        printf("Record APID 0x%X %d\n",it->apid, it->ac3);

        }
}

//-------------------------------------------------------------------------
bool CVCRControl::CVCRDevice::Stop()
{
	deviceState = CMD_VCR_STOP;

	if(last_mode != NeutrinoMessages::mode_scart)
	{
		g_RCInput->postMsg( NeutrinoMessages::VCR_OFF, 0 );
		g_RCInput->postMsg( NeutrinoMessages::CHANGEMODE , last_mode);
	}
	return true;
}

//-------------------------------------------------------------------------
bool CVCRControl::CVCRDevice::Record(const t_channel_id channel_id, int mode, const event_id_t epgid, const std::string& /*epgTitle*/, unsigned char apidmode, const time_t /*epg_time*/)
{
	printf("Record channel_id: " PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS " epg: %llx, apidmode 0x%X mode \n",
	       channel_id, epgid, apidmode);

	// leave menu (if in any)
	g_RCInput->postMsg( CRCInput::RC_timeout, 0 );

	last_mode = CNeutrinoApp::getInstance()->getMode();
	if(mode != last_mode)
		CNeutrinoApp::getInstance()->handleMsg( NeutrinoMessages::CHANGEMODE , mode | NeutrinoMessages::norezap );

	if(channel_id != 0) {
		if(g_Zapit->getCurrentServiceID() != channel_id)
			g_Zapit->zapTo_serviceID(channel_id);
	}
        if(! (apidmode & TIMERD_APIDS_STD)) {
                APIDList apid_list;
                getAPIDs(apidmode,apid_list);
                if(!apid_list.empty()) {
                        if(!apid_list.begin()->ac3)
                                g_Zapit->setAudioChannel(apid_list.begin()->index);
                        else
                                g_Zapit->setAudioChannel(0);
                }
                else
                        g_Zapit->setAudioChannel(0);
        }
        else
                g_Zapit->setAudioChannel(0);

	if(SwitchToScart) {
		// Auf Scart schalten
		CNeutrinoApp::getInstance()->handleMsg( NeutrinoMessages::VCR_ON, 0 );
		// Das ganze nochmal in die queue, da obiges RC_timeout erst in der naechsten ev. loop ausgeführt wird
		// und dann das menu widget das display falsch rücksetzt
		g_RCInput->postMsg( NeutrinoMessages::VCR_ON, 0 );
	}

	deviceState = CMD_VCR_RECORD;

	return true;
}

//-------------------------------------------------------------------------
void CVCRControl::CFileAndServerDevice::RestoreNeutrino(void)
{
//printf("RestoreNeutrino\n");fflush(stdout);
	/* after this zapit send EVT_RECORDMODE_DEACTIVATED, so neutrino getting NeutrinoMessages::EVT_RECORDMODE */
	g_Zapit->setRecordMode( false );
	if (!g_Zapit->isPlayBackActive() && (CNeutrinoApp::getInstance()->getMode() != NeutrinoMessages::mode_standby))
		g_Zapit->startPlayBack();
	// alten mode wieder herstellen (ausser wen zwischenzeitlich auf oder aus sb geschalten wurde)
	if(CNeutrinoApp::getInstance()->getMode() != last_mode &&
	   CNeutrinoApp::getInstance()->getMode() != NeutrinoMessages::mode_standby &&
	   last_mode != NeutrinoMessages::mode_standby)
		if(!autoshift) 
			g_RCInput->postMsg( NeutrinoMessages::CHANGEMODE , last_mode);

	if(last_mode == NeutrinoMessages::mode_standby &&
		CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_standby )
	{
		//Wenn vorher und jetzt standby, dann die zapit wieder auf sb schalten
		//g_Zapit->setStandby(true);
		//was_record = 1;
	}
	if((last_mode != NeutrinoMessages::mode_standby) && StopSectionsd)
		g_Sectionsd->setPauseScanning(false);
}

void CVCRControl::CFileAndServerDevice::CutBackNeutrino(const t_channel_id channel_id, const int mode)
{
	rec_channel_id = channel_id;
//printf("CutBackNeutrino\n");fflush(stdout);
	g_Zapit->setStandby(false);
	last_mode = CNeutrinoApp::getInstance()->getMode();
	if(last_mode == NeutrinoMessages::mode_standby)
		was_record = 1;
	if (channel_id != 0) {
		if (mode != last_mode && (last_mode != NeutrinoMessages::mode_standby || mode != CNeutrinoApp::getInstance()->getLastMode())) {
			CNeutrinoApp::getInstance()->handleMsg( NeutrinoMessages::CHANGEMODE , mode | NeutrinoMessages::norezap );
			// Wenn wir im Standby waren, dann brauchen wir fürs streamen nicht aufwachen...
			if(last_mode == NeutrinoMessages::mode_standby)
				CNeutrinoApp::getInstance()->handleMsg( NeutrinoMessages::CHANGEMODE , NeutrinoMessages::mode_standby);
		}
		if(g_Zapit->getCurrentServiceID() != channel_id) {
			g_Zapit->zapTo_serviceID(channel_id);
		}
	}
	if(StopSectionsd)		// wenn sectionsd gestoppt werden soll
		g_Sectionsd->setPauseScanning(true);		// sectionsd stoppen

	/* after this zapit send EVT_RECORDMODE_ACTIVATED, so neutrino getting NeutrinoMessages::EVT_RECORDMODE */
	g_Zapit->setRecordMode( true );
	if((last_mode == NeutrinoMessages::mode_standby) || (StopPlayBack && g_Zapit->isPlayBackActive()))
		g_Zapit->stopPlayBack();
}

bool sectionsd_getEPGidShort(event_id_t epgID, CShortEPGData * epgdata);
bool sectionsd_getEPGid(const event_id_t epgID, const time_t startzeit, CEPGData * epgdata);

std::string CVCRControl::CFileAndServerDevice::getCommandString(const CVCRCommand command, const t_channel_id channel_id, const event_id_t epgid, const std::string& epgTitle, unsigned char apidmode)
{
	char tmp[40];
	std::string apids_selected;
	const char * extCommand;
	std::string info1, info2;

	std::string extMessage = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n<neutrino commandversion=\"1\">\n\t<record command=\"";
	switch(command)
	{
	case CMD_VCR_RECORD:
		extCommand = "record";
		break;
	case CMD_VCR_STOP:
		extCommand = "stop";
		break;
	case CMD_VCR_PAUSE:
		extCommand = "pause";
		break;
	case CMD_VCR_RESUME:
		extCommand = "resume";
		break;
	case CMD_VCR_AVAILABLE:
		extCommand = "available";
		break;
	case CMD_VCR_UNKNOWN:
	default:
		extCommand = "unknown";
		printf("[CVCRControl] Unknown Command\n");
	}

	extMessage += extCommand;
	extMessage +=
		"\">\n"
		"\t\t<channelname>";

	CZapitClient::responseGetPIDs pids;
	g_Zapit->getPIDS (pids);
	CZapitClient::CCurrentServiceInfo si = g_Zapit->getCurrentServiceInfo ();

        APIDList apid_list;
        getAPIDs(apidmode,apid_list);
        apids_selected="";
        for(APIDList::iterator it = apid_list.begin(); it != apid_list.end(); it++)
        {
                if(it != apid_list.begin())
                        apids_selected += " ";
                sprintf(tmp, "%u", it->apid);
                apids_selected += tmp;
        }

	std::string tmpstring = g_Zapit->getChannelName(channel_id);
	if (tmpstring.empty())
		extMessage += "unknown";
	else
		extMessage += ZapitTools::UTF8_to_UTF8XML(tmpstring.c_str());

	extMessage += "</channelname>\n\t\t<epgtitle>";

	tmpstring = "not available";
	if (epgid != 0)
	{
		CShortEPGData epgdata;
		//if (g_Sectionsd->getEPGidShort(epgid, &epgdata)) {
		if(sectionsd_getEPGidShort(epgid, &epgdata)) {
//#warning fixme sectionsd should deliver data in UTF-8 format
			tmpstring = epgdata.title;
			info1 = epgdata.info1;
			info2 = epgdata.info2;
		}
	} else if (!epgTitle.empty()) {
		tmpstring = epgTitle;
	}
	extMessage += ZapitTools::UTF8_to_UTF8XML(tmpstring.c_str());

	extMessage += "</epgtitle>\n\t\t<id>";

	sprintf(tmp, PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel_id);
	extMessage += tmp;

	extMessage += "</id>\n\t\t<info1>";
	extMessage += ZapitTools::UTF8_to_UTF8XML(info1.c_str());
	extMessage += "</info1>\n\t\t<info2>";
	extMessage += ZapitTools::UTF8_to_UTF8XML(info2.c_str());
	extMessage += "</info2>\n\t\t<epgid>";
	sprintf(tmp, "%llu", epgid);
	extMessage += tmp;
	extMessage += "</epgid>\n\t\t<mode>";
	sprintf(tmp, "%d", g_Zapit->getMode());
	extMessage += tmp;
	extMessage += "</mode>\n\t\t<videopid>";
	sprintf(tmp, "%u", si.vpid);
	extMessage += tmp;
	extMessage += "</videopid>\n\t\t<audiopids selected=\"";
	extMessage += apids_selected;
	extMessage += "\">\n";
	// super hack :-), der einfachste weg an die apid descriptions ranzukommen
	g_RemoteControl->current_PIDs = pids;
	g_RemoteControl->processAPIDnames();

	for(unsigned int i= 0; i< pids.APIDs.size(); i++)
	{
		extMessage += "\t\t\t<audio pid=\"";
		sprintf(tmp, "%u", pids.APIDs[i].pid);
		extMessage += tmp;
		extMessage += "\" name=\"";
		extMessage += ZapitTools::UTF8_to_UTF8XML(g_RemoteControl->current_PIDs.APIDs[i].desc);
		extMessage += "\"/>\n";
	}
	extMessage +=
		"\t\t</audiopids>\n"
		"\t\t<vtxtpid>";
	sprintf(tmp, "%u", si.vtxtpid);
	extMessage += tmp;
	extMessage +=
		"</vtxtpid>\n"
		"\t</record>\n"
		"</neutrino>\n";

	return extMessage;
}

bool CVCRControl::CFileDevice::Stop()
{
	std::string extMessage = " ";
	time_t end_time = time(0);
//printf("[direct] Stop recording, recMovieInfo %lx\n", recMovieInfo); fflush(stdout);
//FIXME why not save info if shift ?
	//if(!autoshift || autoshift_delete)
	if(recMovieInfo && cMovieInfo) {
		// recMovieInfo->length = (end_time - start_time) / 60;
		recMovieInfo->length = (int) round((double) (end_time - start_time) / (double) 60);
		cMovieInfo->encodeMovieInfoXml(&extMessage, recMovieInfo);
	}
	bool return_value = (::stop_recording(extMessage.c_str()) == STREAM2FILE_OK);

	RestoreNeutrino();

	deviceState = CMD_VCR_STOP;

	if(recMovieInfo) {
		recMovieInfo->audioPids.clear();
		delete recMovieInfo;
		recMovieInfo = NULL;
	}
	if(cMovieInfo) {
		delete cMovieInfo;
		cMovieInfo = NULL;
	}

	return return_value;
}

bool CVCRControl::CFileDevice::Record(const t_channel_id channel_id, int mode, const event_id_t epgid, const std::string& epgTitle, unsigned char apidmode, const time_t epg_time)
{
	std::string ext_channel_name;
	unsigned short apids[REC_MAX_PIDS];
	unsigned int numpids = 0;
	unsigned int pos;
	char filename[512]; // UTF-8

	printf("Record channel_id: " PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS " epg: %llx, apidmode 0x%X mode %d\n",
	       channel_id, epgid, apidmode, mode);

	CutBackNeutrino(channel_id, mode);

	apids_mode = apidmode;

	CZapitClient::CCurrentServiceInfo si = g_Zapit->getCurrentServiceInfo();

	if (si.vpid != 0)
		transfer_pids(si.vpid, si.vtype ? EN_TYPE_AVC : EN_TYPE_VIDEO, 0);

        APIDList apid_list;
        getAPIDs(apids_mode, apid_list);

        for(APIDList::iterator it = apid_list.begin(); it != apid_list.end(); it++) {
                apids[numpids++] = it->apid;
		transfer_pids(it->apid, EN_TYPE_AUDIO, it->ac3 ? 1 : 0);
        }
#if 0 // FIXME : why this needed ?
        if(!apid_list.empty())
                g_Zapit->setAudioChannel(apid_list.begin()->index);
#endif
#if 0
        CZapitClient::responseGetPIDs allpids;
        g_Zapit->getPIDS(allpids);
#endif

	if ((StreamVTxtPid) && (si.vtxtpid != 0)) {
		apids[numpids++] = si.vtxtpid;
	}
        if ((StreamPmtPid) && (si.pmtpid != 0)) {
                apids[numpids++] = si.pmtpid;
        }

	// Create filename for recording
	pos = Directory.size();
	strcpy(filename, Directory.c_str());

	if ((pos == 0) || (filename[pos - 1] != '/')) {
		filename[pos] = '/';
		pos++;
		filename[pos] = '\0';
	}
	pos = strlen(filename);
#if 0
	time_t t = time(NULL);
	strftime(&(filename[pos]), sizeof(filename) - pos - 1, "%Y%m%d_%H%M%S", localtime(&t));
	strcat(filename, "_");
	pos = strlen(filename);
#endif
	ext_channel_name = g_Zapit->getChannelName(channel_id);
	if (!(ext_channel_name.empty()))
	{
		strcpy(&(filename[pos]), UTF8_TO_FILESYSTEM_ENCODING(ext_channel_name.c_str()));
		ZapitTools::replace_char(&filename[pos]);

		if (!autoshift && g_settings.recording_save_in_channeldir) {
			struct stat statInfo;
			int res = stat(filename,&statInfo);
			if (res == -1) {
				if (errno == ENOENT) {
					res = safe_mkdir(filename);
					if (res == 0) {
						strcat(filename,"/");
					} else {
						perror("[vcrcontrol] mkdir");
					}

				} else {
					perror("[vcrcontrol] stat");
				}
			} else {
				// directory exists
				strcat(filename,"/");
			}

		} else
			strcat(filename, "_");
	}

	pos = strlen(filename);
	if (g_settings.recording_epg_for_filename) {
		if(epgid != 0) {
			CShortEPGData epgdata;
			//if (g_Sectionsd->getEPGidShort(epgid, &epgdata))
			if(sectionsd_getEPGidShort(epgid, &epgdata))
			{
				if (!(epgdata.title.empty()))
				{
					strcpy(&(filename[pos]), epgdata.title.c_str());
					ZapitTools::replace_char(&filename[pos]);
				}
			}
		} else if (!epgTitle.empty()) {
			strcpy(&(filename[pos]), epgTitle.c_str());
			ZapitTools::replace_char(&filename[pos]);
		}
	}
#if 1
	pos = strlen(filename);
	time_t t = time(NULL);
	strftime(&(filename[pos]), sizeof(filename) - pos - 1, "%Y%m%d_%H%M%S", localtime(&t));
	//pos = strlen(filename);
#endif

	start_time = time(0);
	stream2file_error_msg_t error_msg = ::start_recording(filename,
			      getMovieInfoString(CMD_VCR_RECORD, channel_id, epgid, epgTitle, apid_list, epg_time).c_str(),
			      si.vpid, apids, numpids);

	if (error_msg == STREAM2FILE_OK) {
		deviceState = CMD_VCR_RECORD;
		return true;
	}
	else {
		RestoreNeutrino();

		printf("[vcrcontrol] stream2file error code: %d\n", error_msg);
		//FIXME: Use better error message
		DisplayErrorMessage(g_Locale->getText(
				      error_msg == STREAM2FILE_BUSY ? LOCALE_STREAMING_BUSY :
				      error_msg == STREAM2FILE_INVALID_DIRECTORY ? LOCALE_STREAMING_DIR_NOT_WRITABLE :
				      LOCALE_STREAMINGSERVER_NOCONNECT
				      )); // UTF-8

		return false;
	}
}

bool CVCRControl::CFileDevice::Update(void)
{
	EPG_AUDIO_PIDS audio_pids;
	std::string extMessage;
	//unsigned short apids[REC_MAX_PIDS];
	//unsigned int numpids = 0;
        APIDList apid_list;
	APIDList::iterator it;
	bool update = false;

	if(!recMovieInfo || !cMovieInfo)
		return false;

        getAPIDs(apids_mode, apid_list);
	CZapitClient::CCurrentServiceInfo si = g_Zapit->getCurrentServiceInfo ();

	g_RemoteControl->current_PIDs = allpids;
	g_RemoteControl->processAPIDnames();

	for(it = apid_list.begin(); it != apid_list.end(); it++) {
		bool found = false;
		for(unsigned int i = 0; i < rec_numpida; i++) {
			if(rec_apids[i] == it->apid) {
				found = true;
				break;
			}
		}
		if(!found) {
			update = true;
			printf("CVCRControl::CFileDevice::Update: apid %x not found in recording pids\n", it->apid);
			for(unsigned int i = 0; i < allpids.APIDs.size(); i++) {
				if(allpids.APIDs[i].pid == it->apid) {
					audio_pids.epgAudioPid = allpids.APIDs[i].pid;
					audio_pids.epgAudioPidName = ZapitTools::UTF8_to_UTF8XML(g_RemoteControl->current_PIDs.APIDs[i].desc);
					audio_pids.atype = allpids.APIDs[i].is_ac3 ? 1 : allpids.APIDs[i].is_aac ? 5 : 0;
					audio_pids.selected = (audio_pids.epgAudioPid == (int) rec_currentapid) ? 1 : 0;
					recMovieInfo->audioPids.push_back(audio_pids);

					if(allpids.APIDs[i].is_ac3)
						rec_ac3flags[rec_numpida] = 1;
					if(allpids.APIDs[i].is_aac)
						rec_ac3flags[rec_numpida] = 5;

					rec_apids[rec_numpida] = allpids.APIDs[i].pid;
					if(rec_apids[rec_numpida] == rec_currentapid)
						rec_currentac3 = allpids.APIDs[i].is_ac3 ? 1 : allpids.APIDs[i].is_aac ? 5 : 0;
					rec_numpida++;
				}

			}
		}
	}
	if(!update) {
		printf("CVCRControl::CFileDevice::Update: no update needed\n");
		return false;
	}

	cMovieInfo->encodeMovieInfoXml(&extMessage, recMovieInfo);
	/* neutrino check if vpid changed, so using 0 to disable vpid restart */
	::update_recording(extMessage.c_str(), 0 /*si.vpid*/, rec_apids, rec_numpida);

	return true;
}

bool sectionsd_getActualEPGServiceKey(const t_channel_id uniqueServiceKey, CEPGData * epgdata);

void CVCRControl::Screenshot(const t_channel_id channel_id, char * fname)
{
	char filename[512]; // UTF-8
	char cmd[512];
	std::string channel_name;
	CEPGData                epgData;
	event_id_t epgid = 0;
	unsigned int pos;

	if(!fname) {
		if(safe_mkdir((char *) "/hdd/screenshots/"))
			return;

		strcpy(filename, "/hdd/screenshots/");

		pos = strlen(filename);
		channel_name = g_Zapit->getChannelName(channel_id);
		if (!(channel_name.empty())) {
			strcpy(&(filename[pos]), UTF8_TO_FILESYSTEM_ENCODING(channel_name.c_str()));
			ZapitTools::replace_char(&filename[pos]);
			strcat(filename, "_");
		}
		pos = strlen(filename);

		//if (g_Sectionsd->getActualEPGServiceKey(channel_id&0xFFFFFFFFFFFFULL, &epgData))
		if(sectionsd_getActualEPGServiceKey(channel_id&0xFFFFFFFFFFFFULL, &epgData))
		{};
			epgid = epgData.eventID;
		if(epgid != 0) {
			CShortEPGData epgdata;
			//if (g_Sectionsd->getEPGidShort(epgid, &epgdata)) {
			if(sectionsd_getEPGidShort(epgid, &epgdata)) {
				if (!(epgdata.title.empty())) {
					strcpy(&(filename[pos]), epgdata.title.c_str());
					ZapitTools::replace_char(&filename[pos]);
				}
			}
		}
		pos = strlen(filename);
		time_t t = time(NULL);
		strftime(&(filename[pos]), sizeof(filename) - pos - 1, "%Y%m%d_%H%M%S", localtime(&t));
		strcat(filename, ".bmp");
	} else
		strcpy(filename, fname);

	sprintf(cmd, "grab -v %s", filename);
printf("Executing %s\n", cmd);
	CHintBox * hintBox = new CHintBox(LOCALE_MESSAGEBOX_INFO, "Saving screenshot..");
	hintBox->paint();
	system(cmd);
	hintBox->hide();
	delete hintBox;
}

//-------------------------------------------------------------------------
bool CVCRControl::CServerDevice::Stop()
{
	printf("Stop\n");

	bool return_value = sendCommand(CMD_VCR_STOP);
	RestoreNeutrino();

	return return_value;
}

//-------------------------------------------------------------------------
bool CVCRControl::CServerDevice::Record(const t_channel_id channel_id, int mode, const event_id_t epgid, const std::string& epgTitle, unsigned char apids, const time_t /*epg_time*/)
{
	printf("Record channel_id: "
	       PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
	       " epg: %llx, apids 0x%X mode %d\n",
	       channel_id,
	       epgid,
	       apids,
	       mode);

	CutBackNeutrino(channel_id, mode);
	if(!sendCommand(CMD_VCR_RECORD, channel_id, epgid, epgTitle, apids))
	{
		RestoreNeutrino();

		DisplayErrorMessage(g_Locale->getText(LOCALE_STREAMINGSERVER_NOCONNECT));

		return false;
	}
	else {
		//ext_channel_name = g_Zapit->getChannelName(channel_id);
		return true;
	}
}


//-------------------------------------------------------------------------
void CVCRControl::CServerDevice::serverDisconnect()
{
	close(sock_fd);
}

//-------------------------------------------------------------------------
bool CVCRControl::CServerDevice::sendCommand(CVCRCommand command, const t_channel_id channel_id, const event_id_t epgid, const std::string& epgTitle, unsigned char apids)
{
	printf("Send command: %d channel_id: "
	       PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
	       " epgid: %llx\n",
	       command,
	       channel_id,
	       epgid);
	if(serverConnect())
	{
		std::string extMessage = getCommandString(command, channel_id, epgid, epgTitle, apids);

		printf("sending to vcr-client:\n\n%s\n", extMessage.c_str());
		write(sock_fd, extMessage.c_str() , extMessage.length() );

		serverDisconnect();

		deviceState = command;
		return true;
	}
	else
		return false;

}

//-------------------------------------------------------------------------
bool CVCRControl::CServerDevice::serverConnect()
{

	printf("connect to server: %s:%d\n",ServerAddress.c_str(),ServerPort);

	sock_fd=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	SAI servaddr;
	memset(&servaddr,0,sizeof(SAI));
	servaddr.sin_family=AF_INET;
	servaddr.sin_port=htons(ServerPort);
	inet_pton(AF_INET, ServerAddress.c_str(), &servaddr.sin_addr);


	if(connect(sock_fd, (SA *)&servaddr, sizeof(servaddr))==-1)
	{
		perror("[cvcr] -  cannot connect to streamingserver\n");
		return false;
	}

	return true;
}

//-------------------------------------------------------------------------
std::string CVCRControl::CFileAndServerDevice::getMovieInfoString(const CVCRCommand /*command*/, const t_channel_id channel_id, const event_id_t epgid, const std::string& epgTitle, APIDList apid_list, const time_t epg_time)
{
	std::string extMessage;
	std::string info1, info2;

	if(!cMovieInfo)
		cMovieInfo = new CMovieInfo();
	if(!recMovieInfo)
		recMovieInfo = new MI_MOVIE_INFO();

	cMovieInfo->clearMovieInfo(recMovieInfo);
#if 0
	CZapitClient::responseGetPIDs pids;
	g_Zapit->getPIDS (pids);
#endif
	CZapitClient::CCurrentServiceInfo si = g_Zapit->getCurrentServiceInfo ();

	std::string tmpstring = g_Zapit->getChannelName(channel_id);
	if (tmpstring.empty())
		recMovieInfo->epgChannel = "unknown";
	else
		recMovieInfo->epgChannel = ZapitTools::UTF8_to_UTF8XML(tmpstring.c_str());

	tmpstring = "not available";
	if (epgid != 0) {
		CEPGData epgdata;
		if (sectionsd_getEPGid(epgid, epg_time, &epgdata)) {
			tmpstring = epgdata.title;
			info1 = epgdata.info1;
			info2 = epgdata.info2;

			recMovieInfo->parentalLockAge = epgdata.fsk;
			if(epgdata.contentClassification.size() > 0 )
				recMovieInfo->genreMajor = epgdata.contentClassification[0];

			recMovieInfo->length = epgdata.epg_times.dauer	/ 60;

			printf("fsk:%d, Genre:%d, Dauer: %d\r\n",recMovieInfo->parentalLockAge,recMovieInfo->genreMajor,recMovieInfo->length);
		}
	} else if (!epgTitle.empty()) {
		tmpstring = epgTitle;
	}
	recMovieInfo->epgTitle		= ZapitTools::UTF8_to_UTF8XML(tmpstring.c_str());
	recMovieInfo->epgId		= channel_id;
	recMovieInfo->epgInfo1		= ZapitTools::UTF8_to_UTF8XML(info1.c_str());
	recMovieInfo->epgInfo2		= ZapitTools::UTF8_to_UTF8XML(info2.c_str());
	recMovieInfo->epgEpgId		= epgid ;
	recMovieInfo->epgMode		= g_Zapit->getMode();
	recMovieInfo->epgVideoPid	= si.vpid;
	recMovieInfo->VideoType		= si.vtype;

	rec_vpid = si.vpid;
	rec_vtype = si.vtype;
	rec_currentapid = si.apid;
	memset(rec_apids, 0, sizeof(unsigned short)*REC_MAX_APIDS);
	memset(rec_ac3flags, 0, sizeof(unsigned short)*REC_MAX_APIDS);
	rec_numpida = 0;

	EPG_AUDIO_PIDS audio_pids;
	/* super hack :-), der einfachste weg an die apid descriptions ranzukommen */
	g_RemoteControl->current_PIDs = allpids;
	g_RemoteControl->processAPIDnames();

	APIDList::iterator it;
	for(unsigned int i= 0; i< allpids.APIDs.size(); i++) {
		for(it = apid_list.begin(); it != apid_list.end(); it++) {
			if(allpids.APIDs[i].pid == it->apid) {
				audio_pids.epgAudioPid = allpids.APIDs[i].pid;
				audio_pids.epgAudioPidName = ZapitTools::UTF8_to_UTF8XML(g_RemoteControl->current_PIDs.APIDs[i].desc);
				audio_pids.atype = allpids.APIDs[i].is_ac3 ? 1 : allpids.APIDs[i].is_aac ? 5 : 0;
				audio_pids.selected = (audio_pids.epgAudioPid == (int) rec_currentapid) ? 1 : 0;
				recMovieInfo->audioPids.push_back(audio_pids);

				if(allpids.APIDs[i].is_ac3)
					rec_ac3flags[rec_numpida] = 1;
				if(allpids.APIDs[i].is_aac)
					rec_ac3flags[rec_numpida] = 5;

				rec_apids[rec_numpida] = allpids.APIDs[i].pid;
				if(rec_apids[rec_numpida] == rec_currentapid)
					rec_currentac3 = allpids.APIDs[i].is_ac3 ? 1 : allpids.APIDs[i].is_aac ? 5 : 0;
				rec_numpida++;
			}
		}
	}
	/* FIXME sometimes no apid in xml ?? */
	if(recMovieInfo->audioPids.empty() && allpids.APIDs.size()) {
		int i = 0;
		audio_pids.epgAudioPid = allpids.APIDs[i].pid;
		audio_pids.epgAudioPidName = ZapitTools::UTF8_to_UTF8XML(g_RemoteControl->current_PIDs.APIDs[i].desc);
		audio_pids.atype = allpids.APIDs[i].is_ac3 ? 1 : allpids.APIDs[i].is_aac ? 5 : 0;
		audio_pids.selected = 1;
		recMovieInfo->audioPids.push_back(audio_pids);
	}
	recMovieInfo->epgVTXPID = si.vtxtpid;

	cMovieInfo->encodeMovieInfoXml(&extMessage, recMovieInfo);

	return extMessage;
}
