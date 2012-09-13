/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Copyright (C) 2011 CoolStream International Ltd

	License: GPLv2

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation;

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
#include <sys/vfs.h>
#include <errno.h>
#include <math.h>

#include <global.h>
#include <neutrino.h>
#include <gui/filebrowser.h>
#include <gui/movieplayer.h>
#include <gui/nfs.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/messagebox.h>
#include <gui/widget/mountchooser.h>
#include <daemonc/remotecontrol.h>
#include <system/setting_helpers.h>
#include <system/fsmounter.h>
#include <system/helpers.h>


#include <driver/record.h>
#include <zapit/capmt.h>
#include <zapit/channel.h>
#include <zapit/getservices.h>
#include <zapit/zapit.h>
#include <zapit/client/zapittools.h>
#include <eitd/sectionsd.h>

/* TODO:
 * nextRecording / pending recordings - needs testing
 * check/fix askUserOnTimerConflict gui/timerlist.cpp -> getOverlappingTimers lib/timerdclient/timerdclient.cpp
 * check/test is it needed at all and is it possible to use different demux / another recmap for timeshift
 */

extern CRemoteControl * g_RemoteControl; /* neutrino.cpp */

extern "C" {
#include <driver/genpsi.h>
}

//-------------------------------------------------------------------------
CRecordInstance::CRecordInstance(const CTimerd::RecordingInfo * const eventinfo, std::string &dir, bool timeshift, bool stream_vtxt_pid, bool stream_pmt_pid)
{
	channel_id = eventinfo->channel_id;
	epgid = eventinfo->epgID;
	epgTitle = eventinfo->epgTitle;
	epg_time = eventinfo->epg_starttime;
	apidmode = eventinfo->apids;
	recording_id = eventinfo->eventID;

        if (apidmode == TIMERD_APIDS_CONF)
                apidmode = g_settings.recording_audio_pids_default;

	StreamVTxtPid = stream_vtxt_pid;
	StreamPmtPid = stream_pmt_pid;
	Directory = dir;
	autoshift = timeshift;
	numpids = 0;

	cMovieInfo = new CMovieInfo();
	recMovieInfo = new MI_MOVIE_INFO();
	record = NULL;
	tshift_mode = TSHIFT_MODE_OFF;
	rec_stop_msg = g_Locale->getText(LOCALE_RECORDING_STOP);
}

CRecordInstance::~CRecordInstance()
{
	allpids.APIDs.clear();
	recMovieInfo->audioPids.clear();
	delete recMovieInfo;
	delete cMovieInfo;
	delete record;
}

bool CRecordInstance::SaveXml()
{
	int fd;
	std::string xmlfile = std::string(filename) + ".xml";

	if ((fd = open(xmlfile.c_str(), O_SYNC | O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) >= 0) {
		std::string extMessage;
		cMovieInfo->encodeMovieInfoXml(&extMessage, recMovieInfo);
		write(fd, extMessage.c_str(), extMessage.size() /*strlen(info)*/);
		fdatasync(fd);
		close(fd);
		return true;
	}
	perror(xmlfile.c_str());
	return false;
}

void CRecordInstance::WaitRecMsg(time_t StartTime, time_t WaitTime)
{
	return;
	while (time(0) < StartTime + WaitTime)
		usleep(100000);
}

record_error_msg_t CRecordInstance::Start(CZapitChannel * channel)
{
	int fd;
	std::string tsfile;

	time_t msg_start_time = time(0);
	CHintBox hintBox(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_RECORDING_START));
	hintBox.paint();

	tsfile = std::string(filename) + ".ts";

	printf("%s: file %s vpid %x apid %x\n", __FUNCTION__, tsfile.c_str(), allpids.PIDs.vpid, apids[0]);

	fd = open(tsfile.c_str(), O_CREAT | O_RDWR | O_LARGEFILE | O_TRUNC , S_IRWXO | S_IRWXG | S_IRWXU);
	if(fd < 0) {
		perror(tsfile.c_str());
		hintBox.hide();
		return RECORD_INVALID_DIRECTORY;
	}

	if (allpids.PIDs.vpid != 0)
		transfer_pids(allpids.PIDs.vpid, recMovieInfo->VideoType ? EN_TYPE_AVC : EN_TYPE_VIDEO, 0);

	numpids = 0;
	for (unsigned int i = 0; i < recMovieInfo->audioPids.size(); i++) {
		apids[numpids++] = recMovieInfo->audioPids[i].epgAudioPid;
		transfer_pids(recMovieInfo->audioPids[i].epgAudioPid, EN_TYPE_AUDIO, recMovieInfo->audioPids[i].atype);
	}
	genpsi(fd);

	if ((StreamVTxtPid) && (allpids.PIDs.vtxtpid != 0))
		apids[numpids++] = allpids.PIDs.vtxtpid;

	if ((StreamPmtPid) && (allpids.PIDs.pmtpid != 0))
		apids[numpids++] = allpids.PIDs.pmtpid;

	if(record == NULL)
		record = new cRecord(channel->getRecordDemux() /*RECORD_DEMUX*/);

	record->Open();

	if(!record->Start(fd, (unsigned short ) allpids.PIDs.vpid, (unsigned short *) apids, numpids, channel_id)) {
		record->Stop();
		delete record;
		record = NULL;
		close(fd);
		unlink(tsfile.c_str());
		hintBox.hide();
		return RECORD_FAILURE;
	}

	if(!autoshift)
		CFEManager::getInstance()->lockFrontend(frontend);//FIXME testing

	start_time = time(0);
	SaveXml();

	CCamManager::getInstance()->Start(channel->getChannelID(), CCamManager::RECORD);

	//CVFD::getInstance()->ShowIcon(VFD_ICON_CAM1, true);
	WaitRecMsg(msg_start_time, 2);
	hintBox.hide();
	return RECORD_OK;
}

bool CRecordInstance::Stop(bool remove_event)
{
	char buf[FILENAMEBUFFERSIZE]={0};

	struct stat test;
	snprintf(buf,sizeof(buf), "%s.xml", filename);
	if(stat(buf, &test) == 0){
		cMovieInfo->clearMovieInfo(recMovieInfo);
		snprintf(buf,sizeof(buf), "%s.ts", filename);
		recMovieInfo->file.Name = buf;
		cMovieInfo->loadMovieInfo(recMovieInfo);//restore user bookmark
	}

	time_t end_time = time(0);
	recMovieInfo->length = (int) round((double) (end_time - start_time) / (double) 60);

	CHintBox hintBox(LOCALE_MESSAGEBOX_INFO, rec_stop_msg.c_str());
	hintBox.paint();

	printf("%s: channel %llx recording_id %d\n", __FUNCTION__, channel_id, recording_id);
	SaveXml();
	/* Stop do close fd - if started */
	record->Stop();

	if(!autoshift)
		CFEManager::getInstance()->unlockFrontend(frontend);//FIXME testing

        CCamManager::getInstance()->Stop(channel_id, CCamManager::RECORD);

        if((autoshift && g_settings.auto_delete) /* || autoshift_delete*/) {
                snprintf(buf,sizeof(buf), "nice -n 20 rm -f \"%s.ts\" &", filename);
                system(buf);
                snprintf(buf,sizeof(buf), "%s.xml", filename);
                //autoshift_delete = false;
                unlink(buf);
        }
	if(recording_id && remove_event) {
		g_Timerd->stopTimerEvent(recording_id);
		recording_id = 0;
	}
        //CVFD::getInstance()->ShowIcon(VFD_ICON_CAM1, false);
	WaitRecMsg(end_time, 2);
	hintBox.hide();
	return true;
}

bool CRecordInstance::Update()
{
        APIDList apid_list;
	APIDList::iterator it;
	bool update = false;

	CZapitChannel * channel = CServiceManager::getInstance()->FindChannel(channel_id);
	if(channel == NULL) {
		printf("%s: channel %llx not found!\n", __FUNCTION__, channel_id);
		return false;
	}

	if(channel->getVideoPid() != allpids.PIDs.vpid) {
		Stop(false);
		MakeFileName(channel);
		GetPids(channel);
		FilterPids(apid_list);
		FillMovieInfo(channel, apid_list);
		record_error_msg_t ret =  Start(channel /*, apid_list*/);
		if(ret == RECORD_OK) {
			CCamManager::getInstance()->Start(channel_id, CCamManager::RECORD, true);
			return true;
		}
		return false;
	}

	GetPids(channel);
	FilterPids(apid_list);

	for(it = apid_list.begin(); it != apid_list.end(); ++it) {
		bool found = false;
		for(unsigned int i = 0; i < numpids; i++) {
			if(apids[i] == it->apid) {
				found = true;
				break;
			}
		}
		if(!found) {
			update = true;
			printf("%s: apid %x not found in recording pids\n", __FUNCTION__, it->apid);
			record->AddPid(it->apid);
			for(unsigned int i = 0; i < allpids.APIDs.size(); i++) {
				if(allpids.APIDs[i].pid == it->apid) {
					EPG_AUDIO_PIDS audio_pids;

					audio_pids.epgAudioPid = allpids.APIDs[i].pid;
					audio_pids.epgAudioPidName = allpids.APIDs[i].desc;
					audio_pids.atype = allpids.APIDs[i].is_ac3 ? 1 : allpids.APIDs[i].is_aac ? 5 : 0;
					audio_pids.selected = 0;
					recMovieInfo->audioPids.push_back(audio_pids);
				}
			}
		}
	}
	if(!update) {
		printf("%s: no update needed\n", __FUNCTION__);
		return false;
	}

	SaveXml();
	CCamManager::getInstance()->Start(channel_id, CCamManager::RECORD, true);

        return true;
}

void CRecordInstance::GetPids(CZapitChannel * channel)
{
	allpids.PIDs.vpid = channel->getVideoPid();
	allpids.PIDs.vtxtpid = channel->getTeletextPid();
	allpids.PIDs.pmtpid = channel->getPmtPid();
	allpids.PIDs.selected_apid = channel->getAudioChannelIndex();
#if 0 // not needed
	allpids.PIDs.pcrpid = channel->getPcrPid();
	allpids.PIDs.privatepid = channel->getPrivatePid();
#endif
	allpids.APIDs.clear();
	for (uint32_t  i = 0; i < channel->getAudioChannelCount(); i++) {
		CZapitClient::responseGetAPIDs response;
		response.pid = channel->getAudioPid(i);
		strncpy(response.desc, channel->getAudioChannel(i)->description.c_str(), DESC_MAX_LEN - 1);
		response.is_ac3 = response.is_aac = 0;
		if (channel->getAudioChannel(i)->audioChannelType == CZapitAudioChannel::AC3) {
			response.is_ac3 = 1;
		} else if (channel->getAudioChannel(i)->audioChannelType == CZapitAudioChannel::AAC) {
			response.is_aac = 1;
		}
		response.component_tag = channel->getAudioChannel(i)->componentTag;
		allpids.APIDs.push_back(response);
	}
	ProcessAPIDnames();
}

void CRecordInstance::ProcessAPIDnames()
{
	bool has_unresolved_ctags = false;

	for(unsigned int count=0; count< allpids.APIDs.size(); count++) {
		//printf("Neutrino: apid name= %s (%s) pid= %X\n", allpids.APIDs[count].desc, getISO639Description( allpids.APIDs[count].desc ), allpids.APIDs[count].pid);
		if (allpids.APIDs[count].component_tag != 0xFF)
			has_unresolved_ctags= true;

		if ( strlen( allpids.APIDs[count].desc ) == 3 )
			strncpy( allpids.APIDs[count].desc, getISO639Description( allpids.APIDs[count].desc ),DESC_MAX_LEN -1 );

		if ( allpids.APIDs[count].is_ac3 && !strstr(allpids.APIDs[count].desc, " (AC3)"))
			strncat(allpids.APIDs[count].desc, " (AC3)", DESC_MAX_LEN - strlen(allpids.APIDs[count].desc) -1);
		else if (allpids.APIDs[count].is_aac && !strstr(allpids.APIDs[count].desc, " (AAC)"))
			strncat(allpids.APIDs[count].desc, " (AAC)", DESC_MAX_LEN - strlen(allpids.APIDs[count].desc) -1);
	}

	if(has_unresolved_ctags && (epgid != 0)) {
		CSectionsdClient::ComponentTagList tags;
		if(CEitManager::getInstance()->getComponentTagsUniqueKey(epgid, tags)) {
			for(unsigned int i=0; i< tags.size(); i++) {
				for(unsigned int j=0; j< allpids.APIDs.size(); j++) {
					if(allpids.APIDs[j].component_tag == tags[i].componentTag) {
						if(!tags[i].component.empty()) {
							strncpy(allpids.APIDs[j].desc, tags[i].component.c_str(), DESC_MAX_LEN -1);
							if (allpids.APIDs[j].is_ac3 && !strstr(allpids.APIDs[j].desc, " (AC3)"))
								strncat(allpids.APIDs[j].desc, " (AC3)", DESC_MAX_LEN - strlen(allpids.APIDs[j].desc)-1);
							else if (allpids.APIDs[j].is_aac && !strstr(allpids.APIDs[j].desc, " (AAC)"))
								strncat(allpids.APIDs[j].desc, " (AAC)", DESC_MAX_LEN - strlen(allpids.APIDs[j].desc)-1);
						}
						allpids.APIDs[j].component_tag = -1;
						break;
					}
				}
			}
		}
	}
}

record_error_msg_t CRecordInstance::Record()
{
	APIDList apid_list;

	printf("%s: channel %llx recording_id %d\n", __FUNCTION__, channel_id, recording_id);
	CZapitChannel * channel = CServiceManager::getInstance()->FindChannel(channel_id);
	if(channel == NULL) {
		printf("%s: channel %llx not found!\n", __FUNCTION__, channel_id);
		return RECORD_INVALID_CHANNEL;
	}

	record_error_msg_t ret = MakeFileName(channel);
	if(ret != RECORD_OK)
		return ret;

	GetPids(channel);
	FilterPids(apid_list);
	FillMovieInfo(channel, apid_list);

	ret = Start(channel /*, apid_list*/);
	//FIXME recording_id (timerd eventID) is 0 means its user recording, in this case timer always added ?
	if(ret == RECORD_OK && recording_id == 0) {
		time_t now = time(NULL);
		int record_end = now+g_settings.record_hours*60*60;
		if (g_settings.recording_epg_for_end)
		{
			int pre=0, post=0;
			CEPGData epgData;
			if (CEitManager::getInstance()->getActualEPGServiceKey(channel_id, &epgData )) {
				g_Timerd->getRecordingSafety(pre, post);
				if (epgData.epg_times.startzeit > 0)
					record_end = epgData.epg_times.startzeit + epgData.epg_times.dauer + post;
			}
		}
		recording_id = g_Timerd->addImmediateRecordTimerEvent(channel_id, now, record_end, epgid, epg_time, apidmode);
		printf("%s: channel %llx -> timer eventID %d\n", __FUNCTION__, channel_id, recording_id);
	}
	return ret;
}

void CRecordInstance::FilterPids(APIDList & apid_list)
{
        apid_list.clear();

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
                for(unsigned int i = 0; i < allpids.APIDs.size(); i++) {
                        if (allpids.APIDs[i].pid < apid_min && !allpids.APIDs[i].is_ac3) {
                                apid_min = allpids.APIDs[i].pid;
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
                for(APIDList::iterator it = apid_list.begin(); it != apid_list.end(); ++it)
                        printf("Record APID 0x%X %d\n",it->apid, it->ac3);
        }
}

void CRecordInstance::FillMovieInfo(CZapitChannel * channel, APIDList & apid_list)
{
	std::string info1, info2;

	cMovieInfo->clearMovieInfo(recMovieInfo);

	std::string tmpstring = channel->getName();

	if (tmpstring.empty())
		recMovieInfo->epgChannel = "unknown";
	else
		recMovieInfo->epgChannel = tmpstring;

	tmpstring = "not available";
	if (epgid != 0) {
		CEPGData epgdata;
		if (CEitManager::getInstance()->getEPGid(epgid, epg_time, &epgdata)) {
			tmpstring = epgdata.title;
			info1 = epgdata.info1;
			info2 = epgdata.info2;

			recMovieInfo->parentalLockAge = epgdata.fsk;
			if( !epgdata.contentClassification.empty() )
				recMovieInfo->genreMajor = epgdata.contentClassification[0];

			recMovieInfo->length = epgdata.epg_times.dauer	/ 60;

			printf("fsk:%d, Genre:%d, Dauer: %d\r\n",recMovieInfo->parentalLockAge,recMovieInfo->genreMajor,recMovieInfo->length);
		}
	} else if (!epgTitle.empty()) {
		tmpstring = epgTitle;
	}
	recMovieInfo->epgTitle		= tmpstring;
	recMovieInfo->epgId		= channel->getChannelID();
	recMovieInfo->epgInfo1		= info1;
	recMovieInfo->epgInfo2		= info2;
	recMovieInfo->epgEpgId		= epgid;
	recMovieInfo->epgMode		= g_Zapit->getMode();
	recMovieInfo->epgVideoPid	= allpids.PIDs.vpid;
	recMovieInfo->VideoType		= channel->type;

	EPG_AUDIO_PIDS audio_pids;
	APIDList::iterator it;
	for(unsigned int i= 0; i< allpids.APIDs.size(); i++) {
		for(it = apid_list.begin(); it != apid_list.end(); ++it) {
			if(allpids.APIDs[i].pid == it->apid) {
				audio_pids.epgAudioPid = allpids.APIDs[i].pid;
				audio_pids.epgAudioPidName = allpids.APIDs[i].desc;
				audio_pids.atype = allpids.APIDs[i].is_ac3 ? 1 : allpids.APIDs[i].is_aac ? 5 : 0;
				audio_pids.selected = (audio_pids.epgAudioPid == channel->getAudioPid()) ? 1 : 0;
				recMovieInfo->audioPids.push_back(audio_pids);
			}
		}
	}
	/* FIXME sometimes no apid in xml ?? */
	if(recMovieInfo->audioPids.empty() && !allpids.APIDs.empty()) {
		int i = 0;
		audio_pids.epgAudioPid = allpids.APIDs[i].pid;
		audio_pids.epgAudioPidName = allpids.APIDs[i].desc;
		audio_pids.atype = allpids.APIDs[i].is_ac3 ? 1 : allpids.APIDs[i].is_aac ? 5 : 0;
		audio_pids.selected = 1;
		recMovieInfo->audioPids.push_back(audio_pids);
	}
	recMovieInfo->epgVTXPID = allpids.PIDs.vtxtpid;
}

record_error_msg_t CRecordInstance::MakeFileName(CZapitChannel * channel)
{
	std::string ext_channel_name;
	unsigned int pos;

	if(check_dir(Directory.c_str())) {
		/* check if Directory and network_nfs_recordingdir the same */
		if(strcmp(g_settings.network_nfs_recordingdir, Directory.c_str())) {
			/* not the same, check network_nfs_recordingdir and return error if not ok */
			if(check_dir(g_settings.network_nfs_recordingdir))
				return RECORD_INVALID_DIRECTORY;
			/* fallback to g_settings.network_nfs_recordingdir */
			Directory = std::string(g_settings.network_nfs_recordingdir);
		}
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

	ext_channel_name = channel->getName();

	if (!(ext_channel_name.empty())) {
		strcpy(&(filename[pos]), UTF8_TO_FILESYSTEM_ENCODING(ext_channel_name.c_str()));
		ZapitTools::replace_char(&filename[pos]);

		if (!autoshift && g_settings.recording_save_in_channeldir) {
			struct stat statInfo;
			int res = stat(filename,&statInfo);
			if (res == -1) {
				if (errno == ENOENT) {
					res = safe_mkdir(filename);
					if (res == 0)
						strncat(filename,"/",FILENAMEBUFFERSIZE - strlen(filename) -1);
					else
						perror("[vcrcontrol] mkdir");
				} else
					perror("[vcrcontrol] stat");
			} else
				// directory exists
				strncat(filename,"/",FILENAMEBUFFERSIZE - strlen(filename)-1);
		} else
			strncat(filename, "_",FILENAMEBUFFERSIZE - strlen(filename)-1);
	}

	pos = strlen(filename);
	if (g_settings.recording_epg_for_filename) {
		if(epgid != 0) {
			CShortEPGData epgdata;
			if(CEitManager::getInstance()->getEPGidShort(epgid, &epgdata)) {
				if (!(epgdata.title.empty())) {
					strcpy(&(filename[pos]), epgdata.title.c_str());
					ZapitTools::replace_char(&filename[pos]);
				}
			}
		} else if (!epgTitle.empty()) {
			strcpy(&(filename[pos]), epgTitle.c_str());
			ZapitTools::replace_char(&filename[pos]);
		}
	}

	pos = strlen(filename);
	time_t t = time(NULL);
	pos += strftime(&(filename[pos]), sizeof(filename) - pos - 1, "%Y%m%d_%H%M%S", localtime(&t));

	if(autoshift)
		strncat(filename, "_temp",FILENAMEBUFFERSIZE - strlen(filename)-1);

	return RECORD_OK;
}

void CRecordInstance::GetRecordString(std::string &str)
{
	CZapitChannel * channel = CServiceManager::getInstance()->FindChannel(channel_id);
	if(channel == NULL) {
		printf("%s: channel %llx not found!\n", __FUNCTION__, channel_id);
		str = "Unknown channel : " + GetEpgTitle();
		return;
	}
	char stime[15];
	strftime(stime, sizeof(stime), "%H:%M:%S ", localtime(&start_time));
	str = stime + channel->getName() + ": " + GetEpgTitle();
}

//-------------------------------------------------------------------------
CRecordManager * CRecordManager::manager = NULL;
OpenThreads::Mutex CRecordManager::sm;

CRecordManager::CRecordManager()
{
	StreamVTxtPid = false;
	StreamPmtPid = false;
	StopSectionsd = false;
	//recordingstatus = 0;
	recmap.clear();
	nextmap.clear();
	autoshift = false;
	shift_timer = 0;
}

CRecordManager::~CRecordManager()
{
	for(recmap_iterator_t it = recmap.begin(); it != recmap.end(); it++) {
		CRecordInstance * inst = it->second;
		inst->Stop();
		delete inst;
	}
	recmap.clear();
	for(nextmap_iterator_t it = nextmap.begin(); it != nextmap.end(); it++) {
		/* Note: CTimerd::RecordingInfo is a class! => typecast to avoid destructor call */
		delete[] (unsigned char *) (*it);
	}
	nextmap.clear();
}

CRecordManager * CRecordManager::getInstance()
{
	sm.lock();

	if(manager == NULL)
		manager = new CRecordManager();

	sm.unlock();
	return manager;
}

CRecordInstance * CRecordManager::FindInstance(t_channel_id channel_id)
{
	for (recmap_iterator_t it = recmap.begin(); it != recmap.end(); it++) {
		if (it->second->GetChannelId() == channel_id)
			return it->second;
	}
	return NULL;
}

CRecordInstance * CRecordManager::FindInstanceID(int recid)
{
	for (recmap_iterator_t it = recmap.begin(); it != recmap.end(); it++) {
		if (it->second->GetRecordingId() == recid)
			return it->second;
	}
	return NULL;
}

CRecordInstance * CRecordManager::FindTimeshift()
{
	for (recmap_iterator_t it = recmap.begin(); it != recmap.end(); it++) {
		if (it->second->Timeshift())
			return it->second;
	}
	return NULL;
}

MI_MOVIE_INFO * CRecordManager::GetMovieInfo(t_channel_id channel_id, bool timeshift)
{
	//FIXME copy MI_MOVIE_INFO ?
	MI_MOVIE_INFO * mi = NULL;

	mutex.lock();
	CRecordInstance * inst = NULL;
	if (timeshift)
		inst = FindTimeshift();
	if (inst == NULL)
		inst = FindInstance(channel_id);
	if (inst)
		mi = inst->GetMovieInfo();
	mutex.unlock();
	return mi;
}

const std::string CRecordManager::GetFileName(t_channel_id channel_id, bool timeshift)
{
	std::string filename;
	CRecordInstance * inst = NULL;
	if (timeshift)
		inst = FindTimeshift();
	if (inst == NULL)
		inst = FindInstance(channel_id);

	if(inst)
		filename = inst->GetFileName();
	return filename;
}

/* return record mode mask, for channel_id not 0, or global */
int CRecordManager::GetRecordMode(const t_channel_id channel_id)
{
#if 0
	if (RecordingStatus(channel_id) || IsTimeshift(channel_id))
	{
		if (RecordingStatus(channel_id) && !IsTimeshift(channel_id))
			return RECMODE_REC;
		if (channel_id == 0)
		{
			int records	= GetRecordCount();
			if (IsTimeshift(channel_id) && (records == 1))
				return RECMODE_TSHIFT;
			else if (IsTimeshift(channel_id) && (records > 1))
				return RECMODE_REC_TSHIFT;
			else
				return RECMODE_OFF;
		} else
		{
			if (IsTimeshift(channel_id))
				return RECMODE_TSHIFT;
		}
	}
	return RECMODE_OFF;
#endif
	int recmode = RECMODE_OFF;
	mutex.lock();
	if (channel_id == 0) {
		/* we can have only one timeshift instance, if there are more - some is record */
		if ((!autoshift && !recmap.empty()) || recmap.size() > 1)
			recmode |= RECMODE_REC;
		if (autoshift)
			recmode |= RECMODE_TSHIFT;
	} else {
		for (recmap_iterator_t it = recmap.begin(); it != recmap.end(); it++) {
			if (it->second->GetChannelId() == channel_id) {
				if (it->second->Timeshift())
					recmode |= RECMODE_TSHIFT;
				else
					recmode |= RECMODE_REC;
			}
		}
	}
	mutex.unlock();
	return recmode;
}

bool CRecordManager::Record(const t_channel_id channel_id, const char * dir, bool timeshift)
{
	CTimerd::RecordingInfo	eventinfo;
	CEPGData		epgData;

	eventinfo.eventID = 0;
	eventinfo.channel_id = channel_id;
	if (CEitManager::getInstance()->getActualEPGServiceKey(channel_id, &epgData )) {
		eventinfo.epgID = epgData.eventID;
		eventinfo.epg_starttime = epgData.epg_times.startzeit;
		strncpy(eventinfo.epgTitle, epgData.title.c_str(), EPG_TITLE_MAXLEN-1);
		eventinfo.epgTitle[EPG_TITLE_MAXLEN-1]=0;
	}
	else {
		eventinfo.epgID = 0;
		eventinfo.epg_starttime = 0;
		strcpy(eventinfo.epgTitle, "");
	}
	eventinfo.apids = TIMERD_APIDS_CONF;
	eventinfo.recordingDir[0] = 0;

	return Record(&eventinfo, dir, timeshift);
}

bool CRecordManager::Record(const CTimerd::RecordingInfo * const eventinfo, const char * dir, bool timeshift)
{
	CRecordInstance * inst = NULL;
	record_error_msg_t error_msg = RECORD_OK;
	/* for now, empty eventinfo.recordingDir means this is direct record, FIXME better way ?
	 * neutrino check if this channel_id already recording, may be not needed */
	bool direct_record = timeshift || strlen(eventinfo->recordingDir) == 0;

	printf("%s channel_id %llx epg: %llx, apidmode 0x%X\n", __FUNCTION__,
	       eventinfo->channel_id, eventinfo->epgID, eventinfo->apids);

#if 0
	if(!CheckRecording(eventinfo))
		return false;
#endif

#if 1 // FIXME test
	StopSectionsd = false;
	if( !recmap.empty() )
		StopSectionsd = true;
#endif
	RunStartScript();

	mutex.lock();
#if 0
	inst = FindInstance(eventinfo->channel_id);
	if(inst) {
		if(direct_record) {
			error_msg = RECORD_BUSY;
		} else {
			CTimerd::RecordingInfo * evt = new CTimerd::RecordingInfo(*eventinfo);
			printf("%s add %llx : %s to pending\n", __FUNCTION__, evt->channel_id, evt->epgTitle);
			nextmap.push_back((CTimerd::RecordingInfo *)evt);
		}
	} else
#endif
	if(recmap.size() < RECORD_MAX_COUNT) {
		CFrontend * frontend = NULL;
		if(CutBackNeutrino(eventinfo->channel_id, frontend)) {
			std::string newdir;
			if(dir && strlen(dir))
				newdir = std::string(dir);
			else if(strlen(eventinfo->recordingDir))
				newdir = std::string(eventinfo->recordingDir);
			else
				newdir = Directory;

			inst = new CRecordInstance(eventinfo, newdir, timeshift, StreamVTxtPid, StreamPmtPid);

			inst->frontend = frontend;
			error_msg = inst->Record();
			if(error_msg == RECORD_OK) {
				recmap.insert(recmap_pair_t(inst->GetRecordingId(), inst));
				if(timeshift)
					autoshift = true;
#if 0
				// mimic old behavior for start/stop menu option chooser, still actual ?
				t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
				if(eventinfo->channel_id == live_channel_id)
					recordingstatus = 1;
#endif
			} else {
				delete inst;
			}
		} else if(!direct_record) {
			CTimerd::RecordingInfo * evt = new CTimerd::RecordingInfo(*eventinfo);
			printf("%s add %llx : %s to pending\n", __FUNCTION__, evt->channel_id, evt->epgTitle);
			nextmap.push_back((CTimerd::RecordingInfo *)evt);
		}
	} else
		error_msg = RECORD_BUSY;

	mutex.unlock();

	if (error_msg == RECORD_OK)
		return true;

	printf("[recordmanager] %s: error code: %d\n", __FUNCTION__, error_msg);
	/* RestoreNeutrino must be called always if record start failed */
	RunStopScript();
	RestoreNeutrino();

	/* FIXME show timeshift start error or not ? */
	//if(!timeshift)
	{
		//FIXME: Use better error message
		DisplayErrorMessage(g_Locale->getText(
				      error_msg == RECORD_BUSY ? LOCALE_STREAMING_BUSY :
				      error_msg == RECORD_INVALID_DIRECTORY ? LOCALE_STREAMING_DIR_NOT_WRITABLE :
				      LOCALE_STREAMING_WRITE_ERROR )); // UTF-8
	}

	return false;
}

bool CRecordManager::StartAutoRecord()
{
	printf("%s: starting to %s\n", __FUNCTION__, TimeshiftDirectory.c_str());
	g_RCInput->killTimer (shift_timer);
	t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
	return Record(live_channel_id, TimeshiftDirectory.c_str(), true);
}

bool CRecordManager::StopAutoRecord(bool lock)
{
	printf("%s: autoshift %d\n", __FUNCTION__, autoshift);

	g_RCInput->killTimer (shift_timer);

	if(!autoshift)
		return false;

	if (lock)
		mutex.lock();

	CRecordInstance * inst = FindTimeshift();
	if (inst)
		StopInstance(inst);

	if (lock)
		mutex.unlock();

	return (inst != NULL);
}

#if 0
bool CRecordManager::CheckRecording(const CTimerd::RecordingInfo * const eventinfo)
{
	t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
	/* FIXME check if frontend used for timeshift the same and will zap ?? */
	if(/*(eventinfo->channel_id == live_channel_id) ||*/ !SAME_TRANSPONDER(eventinfo->channel_id, live_channel_id))
		StopAutoRecord();

	return true;
}
#endif

void CRecordManager::StartNextRecording()
{
	CTimerd::RecordingInfo * eventinfo = NULL;
	printf("%s: pending count %d\n", __FUNCTION__, nextmap.size());

	for(nextmap_iterator_t it = nextmap.begin(); it != nextmap.end(); it++) {
		eventinfo = *it;
#if 0
		bool tested = true;
		if( !recmap.empty() ) {
			CRecordInstance * inst = FindInstance(eventinfo->channel_id);
			/* same channel recording and not auto - skip */
			if(inst && !inst->Timeshift())
				tested = false;
			/* there is only auto-record which can be stopped */
			else if(recmap.size() == 1 && autoshift)
				tested = true;
			else {
				/* there are some recordings, test any (first) for now */
				recmap_iterator_t fit = recmap.begin();
				t_channel_id channel_id = fit->second->GetChannelId();
				tested = (SAME_TRANSPONDER(channel_id, eventinfo->channel_id));
			}
		}
		if(tested) 
#endif
		CZapitChannel * channel = CServiceManager::getInstance()->FindChannel(eventinfo->channel_id);
		if (channel && CFEManager::getInstance()->canTune(channel))
		{
			//MountDirectory(eventinfo->recordingDir);//FIXME in old neutrino startNextRecording commented
			bool ret = Record(eventinfo);
			if(ret) {
				delete[] (unsigned char *) eventinfo;
				it = nextmap.erase(it++);
				if (it == nextmap.end())
					break;
			}
		}
	}
}

/* return true, if there are any recording running for this channel id, or global if id is 0 */
bool CRecordManager::RecordingStatus(const t_channel_id channel_id)
{
	bool ret = false;

	mutex.lock();

	if(channel_id) {
		CRecordInstance * inst = FindInstance(channel_id);
		ret = (inst != NULL);
	} else
		ret = !recmap.empty();

	mutex.unlock();
	return ret;
}

bool CRecordManager::TimeshiftOnly()
{
	mutex.lock();
	int count = recmap.size();
	mutex.unlock();
	return (autoshift && (count == 1));
}

/* FIXME no check for multi-tuner */
bool CRecordManager::SameTransponder(const t_channel_id channel_id)
{
	bool same = true;
	mutex.lock();
	int count = recmap.size();
	if(count) {
		if(autoshift && count == 1)
			same = true;
		else {
			recmap_iterator_t fit = recmap.begin();
			t_channel_id id = fit->second->GetChannelId();
			same = (SAME_TRANSPONDER(channel_id, id));
		}
	}
	mutex.unlock();
	return same;
}

void CRecordManager::StopInstance(CRecordInstance * inst, bool remove_event)
{
	/* first erase - then stop, because Stop() reset recording_id to 0 */
	recmap.erase(inst->GetRecordingId());
	inst->Stop(remove_event);

	if(inst->Timeshift())
		autoshift = false;

#if 0
	t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
	if(inst->GetChannelId() == live_channel_id)
		recordingstatus = 0;
#endif
	delete inst;
}

bool CRecordManager::Stop(const t_channel_id channel_id)
{
	printf("%s: %llx\n", __FUNCTION__, channel_id);

	mutex.lock();

	/* FIXME stop all ? show list ? */
	CRecordInstance * inst = FindInstance(channel_id);
	if(inst != NULL)
		StopInstance(inst);
	else
		printf("%s: channel %llx not recording\n", __FUNCTION__, channel_id);

	mutex.unlock();

	StopPostProcess();

	return (inst != NULL);
}

bool CRecordManager::Stop(const CTimerd::RecordingStopInfo * recinfo)
{
	bool ret = false;

	printf("%s: eventID %d channel_id %llx\n", __FUNCTION__, recinfo->eventID, recinfo->channel_id);

	mutex.lock();

	CRecordInstance * inst = FindInstanceID(recinfo->eventID);
	if(inst != NULL && recinfo->eventID == inst->GetRecordingId()) {
		StopInstance(inst, false);
		ret = true;
	} else {
		for(nextmap_iterator_t it = nextmap.begin(); it != nextmap.end(); it++) {
			if((*it)->eventID == recinfo->eventID) {
				printf("%s: removing pending eventID %d channel_id %llx\n", __FUNCTION__, recinfo->eventID, recinfo->channel_id);
				/* Note: CTimerd::RecordingInfo is a class! => typecast to avoid destructor call */
				delete[] (unsigned char *) (*it);
				nextmap.erase(it);
				ret = true;
				break;
			}
		}
	}
	if(!ret)
		printf("%s: eventID %d channel_id %llx : not found\n", __FUNCTION__, recinfo->eventID, recinfo->channel_id);

	mutex.unlock();

	StopPostProcess();

	return ret;
}

void CRecordManager::StopPostProcess()
{
	RestoreNeutrino();
	StartNextRecording();
	RunStopScript();
}

bool CRecordManager::Update(const t_channel_id channel_id)
{
	mutex.lock();

	CRecordInstance * inst = FindInstance(channel_id);
	if(inst != NULL)
		inst->Update();
	else
		printf("%s: channel %llx not recording\n", __FUNCTION__, channel_id);

	mutex.unlock();
	return (inst != NULL);
}

int CRecordManager::handleMsg(const neutrino_msg_t msg, neutrino_msg_data_t data)
{
	if(msg == NeutrinoMessages::EVT_ZAP_COMPLETE) {
		g_RCInput->killTimer (shift_timer);
		if (g_settings.auto_timeshift) {
			int delay = g_settings.auto_timeshift;
			shift_timer = g_RCInput->addTimer(delay*1000*1000, true);
			g_InfoViewer->handleMsg(NeutrinoMessages::EVT_RECORDMODE, 1);
		}
	}
	else if ((msg == NeutrinoMessages::EVT_TIMER)) {
		if(data == shift_timer) {
			shift_timer = 0;
			StartAutoRecord();
			return messages_return::handled;
		}
	}
	return messages_return::unhandled;
}

#if 0
bool CRecordManager::IsTimeshift(t_channel_id channel_id)
{
	bool ret = false;
	CRecordInstance * inst;
	mutex.lock();
	if (channel_id != 0)
	{
		inst = FindInstance(channel_id);
		if(inst && inst->tshift_mode)
			ret = true;
		else
			ret = false;
	} else
	{
		for(recmap_iterator_t it = recmap.begin(); it != recmap.end(); it++)
		{
			if(it->second->tshift_mode)
			{
				mutex.unlock();
				return true;
			}
		}
	}
	mutex.unlock();
	return ret;
}

void CRecordManager::SetTimeshiftMode(CRecordInstance * inst, int mode)
{
	mutex.lock();
	/* reset all instances mode ? */
	for(recmap_iterator_t it = recmap.begin(); it != recmap.end(); it++)
		it->second->tshift_mode = TSHIFT_MODE_OFF;

	mutex.unlock();
	if (inst)
		inst->tshift_mode = mode;
}
#endif

void CRecordManager::StartTimeshift()
{
	if(g_RemoteControl->is_video_started)
	{
		std::string tmode = "ptimeshift"; // already recording, pause
		bool res = true;
		t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
#if 0
		if(RecordingStatus(live_channel_id))
		{
			tmode = "ptimeshift"; // already recording, pause
			if(GetRecordMode(live_channel_id) == RECMODE_TSHIFT)
				SetTimeshiftMode(FindInstance(live_channel_id), TSHIFT_MODE_PAUSE);
		} else
		{
			if(g_settings.temp_timeshift)
			{
				res = StartAutoRecord();
				SetTimeshiftMode(FindInstance(live_channel_id), TSHIFT_MODE_TEMPORAER);
			} else
			{
				res = Record(live_channel_id);
				SetTimeshiftMode(FindInstance(live_channel_id), TSHIFT_MODE_PERMANET);
			}
			tmode = "timeshift"; // record just started
		}
#endif
		/* start temporary timeshift if enabled and not running, but dont start second record */
		if (g_settings.temp_timeshift) {
			if (!FindTimeshift()) {
				res = StartAutoRecord();
				tmode = "timeshift"; // record just started
			}
		}
		else if (!RecordingStatus(live_channel_id)) {
			res = Record(live_channel_id);
			tmode = "timeshift"; // record just started
		}

		if(res)
		{
			CMoviePlayerGui::getInstance().exec(NULL, tmode);
			if(g_settings.temp_timeshift)
				ShowMenu();
		}
	}
}

int CRecordManager::exec(CMenuTarget* parent, const std::string & actionKey )
{
	if(parent)
		parent->hide();

	if(actionKey == "StopAll")
	{
		char rec_msg[256];
		char rec_msg1[256];
		int records = recmap.size();

		snprintf(rec_msg1, sizeof(rec_msg1)-1, "%s", g_Locale->getText(LOCALE_RECORDINGMENU_MULTIMENU_ASK_STOP_ALL));
		snprintf(rec_msg, sizeof(rec_msg)-1, rec_msg1, records);
		if(ShowMsgUTF(LOCALE_SHUTDOWN_RECODING_QUERY, rec_msg,
			CMessageBox::mbrYes, CMessageBox::mbYes | CMessageBox::mbNo, NULL, 450, 30, false) == CMessageBox::mbrYes)
		{
			snprintf(rec_msg1, sizeof(rec_msg1)-1, "%s", g_Locale->getText(LOCALE_RECORDINGMENU_MULTIMENU_INFO_STOP_ALL));

			int i = 0;
			mutex.lock();
			for(recmap_iterator_t it = recmap.begin(); it != recmap.end(); it++)
			{
				CRecordInstance * inst = it->second;
				t_channel_id channel_id = inst->GetChannelId();

				snprintf(rec_msg, sizeof(rec_msg)-1, rec_msg1, records-i, records);
				inst->SetStopMessage(rec_msg);

				printf("CRecordManager::exec(ExitAll line %d) found channel %llx recording_id %d\n", __LINE__, channel_id, inst->GetRecordingId());
				g_Timerd->stopTimerEvent(inst->GetRecordingId());
				i++;
			}
			mutex.unlock();
		}
		return menu_return::RETURN_EXIT_ALL;
	} else if(actionKey == "Record")
	{
		printf("[neutrino] direct record\n");
		t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();

		bool tostart = true;
		CRecordInstance * inst = FindInstance(live_channel_id);
		if (inst) {
			std::string title;
			inst->GetRecordString(title);
			tostart = (ShowMsgUTF(LOCALE_RECORDING_IS_RUNNING, title.c_str(),
						CMessageBox::mbrYes, CMessageBox::mbYes | CMessageBox::mbNo, NULL, 450, 30, false) == CMessageBox::mbrYes);
		}
		if (tostart) {
			CRecordManager::getInstance()->Record(live_channel_id);

			if(!g_InfoViewer->is_visible) // show Infoviewer
				CNeutrinoApp::getInstance()->showInfo();

			return menu_return::RETURN_EXIT_ALL;
		}
#if 0
		else
			DisplayInfoMessage(g_Locale->getText(LOCALE_RECORDING_IS_RUNNING));
#endif
	} else if(actionKey == "Timeshift")
	{
		StartTimeshift();
		return menu_return::RETURN_EXIT_ALL;
	} else if(actionKey == "Stop_record")
	{
		if(!CRecordManager::getInstance()->RecordingStatus()) {
			ShowHintUTF(LOCALE_MAINMENU_RECORDING_STOP, g_Locale->getText(LOCALE_RECORDINGMENU_RECORD_IS_NOT_RUNNING), 450, 2);
			return menu_return::RETURN_EXIT_ALL;
		}
	}

	ShowMenu();
	return menu_return::RETURN_REPAINT;
}

bool CRecordManager::ShowMenu(void)
{
	int select = -1, rec_count = recmap.size();
	char cnt[5];
	CMenuForwarderNonLocalized * item;
	CMenuForwarder * iteml;
	t_channel_id channel_ids[RECORD_MAX_COUNT] = { 0 };	/* initialization avoids false "might */
	int recording_ids[RECORD_MAX_COUNT] = { 0 };		/* be used uninitialized" warning */

	CMenuSelectorTarget * selector = new CMenuSelectorTarget(&select);

	CMenuWidget menu(LOCALE_MAINMENU_RECORDING, NEUTRINO_ICON_SETTINGS /*, width*/);
	menu.addIntroItems(NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, CMenuWidget::BTN_TYPE_CANCEL);

	// Record / Timeshift
	t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();

	int rec_mode		= GetRecordMode(live_channel_id);
	bool status_ts		= rec_mode & RECMODE_TSHIFT;
	//bool status_rec		= rec_mode & RECMODE_REC;

	//record item
	iteml = new CMenuForwarder(LOCALE_RECORDINGMENU_MULTIMENU_REC_AKT, true /*!status_rec*/, NULL,
			this, "Record", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED);
	//if no recordings are running, set the focus to the record menu item
	menu.addItem(iteml, rec_count == 0 ? true: false);

	//timeshift item
	iteml = new CMenuForwarder(LOCALE_RECORDINGMENU_MULTIMENU_TIMESHIFT, !status_ts, NULL,
			this, "Timeshift", CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW);
	menu.addItem(iteml, false);

	if(rec_count > 0)
	{
		menu.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MAINMENU_RECORDING_STOP));
		mutex.lock();

		int i = 0 , shortcut = 1;
		for(recmap_iterator_t it = recmap.begin(); it != recmap.end(); it++) {
			CRecordInstance * inst = it->second;

			channel_ids[i] = inst->GetChannelId();
			recording_ids[i] = inst->GetRecordingId();

			std::string title;
			inst->GetRecordString(title);

			const char* mode_icon = NULL;
			//if (inst->tshift_mode)
			if (inst->Timeshift())
				mode_icon = NEUTRINO_ICON_AUTO_SHIFT;

			sprintf(cnt, "%d", i);
			//define stop key if only one record is running, otherwise define shortcuts
			neutrino_msg_t rc_key = CRCInput::convertDigitToKey(shortcut++);
			std::string btn_icon = NEUTRINO_ICON_BUTTON_OKAY;
			if (rec_count == 1){
				rc_key = CRCInput::RC_stop;
				btn_icon = NEUTRINO_ICON_BUTTON_STOP;
			}
			item = new CMenuForwarderNonLocalized(title.c_str(), true, NULL, selector, cnt, rc_key, NULL, mode_icon);
			item->setItemButton(btn_icon, true);

			//if only one recording is running, set the focus to this menu item
			menu.addItem(item, rec_count == 1 ? true: false);
			i++;
			if (i >= RECORD_MAX_COUNT)
				break;
		}
		if(i > 1) //menu item "stopp all records"
		{
			menu.addItem(GenericMenuSeparatorLine);
			iteml = new CMenuForwarder(LOCALE_RECORDINGMENU_MULTIMENU_STOP_ALL, true, NULL,
					this, "StopAll", CRCInput::RC_stop);
			iteml->setItemButton(NEUTRINO_ICON_BUTTON_STOP, true);

			//if more than one recording is running, set the focus to menu item 'stopp all records'
			menu.addItem(iteml, rec_count > 1 ? true: false);
		}
		mutex.unlock();
	}

	menu.exec(NULL, "");
	delete selector;

	if (select >= 0 && select < RECORD_MAX_COUNT) {
		/* in theory, timer event can expire while we in menu ? lock and check again */
		mutex.lock();
		CRecordInstance * inst = FindInstanceID(recording_ids[select]);
		if(inst == NULL || recording_ids[select] != inst->GetRecordingId()) {
			printf("%s: channel %llx event id %d not found\n", __FUNCTION__, channel_ids[select], recording_ids[select]);
			mutex.unlock();
			return false;
		}
		mutex.unlock();
		return AskToStop(channel_ids[select], recording_ids[select]);
	}
	return false;
}

bool CRecordManager::AskToStop(const t_channel_id channel_id, const int recid)
{
	int recording_id = 0;
	std::string title;
	CRecordInstance * inst;

	mutex.lock();
	if (recid)
		inst = FindInstanceID(recid);
	else
		inst = FindInstance(channel_id);

	if(inst) {
		recording_id = inst->GetRecordingId();
		inst->GetRecordString(title);
	}
	mutex.unlock();
	if(inst == NULL)
		return false;

	if(ShowMsgUTF(LOCALE_SHUTDOWN_RECODING_QUERY, title.c_str(),
				CMessageBox::mbrYes, CMessageBox::mbYes | CMessageBox::mbNo, NULL, 450, 30, false) == CMessageBox::mbrYes) {
		g_Timerd->stopTimerEvent(recording_id);
		return true;
	}
	return false;
}

bool CRecordManager::RunStartScript(void)
{
	//FIXME only if no recordings yet or always ?
	if(RecordingStatus())
		return false;

	puts("[neutrino.cpp] executing " NEUTRINO_RECORDING_START_SCRIPT ".");
	if (file_exists(NEUTRINO_RECORDING_START_SCRIPT) && my_system(NEUTRINO_RECORDING_START_SCRIPT,NULL,NULL) != 0) {
		perror(NEUTRINO_RECORDING_START_SCRIPT " failed");
		return false;
	}
	return true;
}

bool CRecordManager::RunStopScript(void)
{
	//FIXME only if no recordings left or always ?
	if(RecordingStatus())
		return false;

	puts("[neutrino.cpp] executing " NEUTRINO_RECORDING_ENDED_SCRIPT ".");
	if (file_exists(NEUTRINO_RECORDING_ENDED_SCRIPT) && my_system(NEUTRINO_RECORDING_ENDED_SCRIPT,NULL,NULL) != 0) {
		perror(NEUTRINO_RECORDING_ENDED_SCRIPT " failed");
		return false;
	}
	return true;
}

/* 
 * if we not recording and standby mode -> wakeup zapit
 * check if we can record channel without zap
 * 	if no - change mode and zap
 *		if zap fails - change mode back and return
 *		else if standby stop playback
 *	if yes
 *		zap_to_record
 * if zap ok
 * 	set record mode
 */
bool CRecordManager::CutBackNeutrino(const t_channel_id channel_id, CFrontend * &frontend)
{
	bool ret = true;
	CZapitChannel * channel = CServiceManager::getInstance()->FindChannel(channel_id);
	if (!channel)
		return false;

	int mode = channel->getServiceType() != ST_DIGITAL_RADIO_SOUND_SERVICE ?
		NeutrinoMessages::mode_tv : NeutrinoMessages::mode_radio;

	printf("%s channel_id %llx mode %d\n", __FUNCTION__, channel_id, mode);

	last_mode = CNeutrinoApp::getInstance()->getMode();
	if(last_mode == NeutrinoMessages::mode_standby && recmap.empty()) {
		g_Zapit->setStandby(false); // this zap to live_channel_id
		/* wait for zapit wakeup */
		g_Zapit->getMode();
	}

	t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();

	bool mode_changed = false;
	CFrontend *live_fe = CZapit::getInstance()->GetLiveFrontend();
	frontend = live_fe;
	if(live_channel_id != channel_id) {
		/* first try to get frontend for record with locked live */
		bool unlock = true;
		CFEManager::getInstance()->lockFrontend(live_fe);
		frontend = CFEManager::getInstance()->allocateFE(channel);
		if (frontend == NULL) {
			/* no frontend, try again with unlocked live */
			unlock = false;
			CFEManager::getInstance()->unlockFrontend(live_fe);
			frontend = CFEManager::getInstance()->allocateFE(channel);
		}
		if (frontend == NULL)
			return false;

		/* if allocateFE was successful, full zapTo_serviceID 
		 * needed, if record frontend same as live, and its on different TP */
		bool found = (live_fe != frontend) || SAME_TRANSPONDER(live_channel_id, channel_id);
		if(found) {
			ret = g_Zapit->zapTo_record(channel_id) > 0;
			printf("%s found same tp, zapTo_record channel_id %llx result %d\n", __FUNCTION__, channel_id, ret);
		}
		else {
			printf("%s mode %d last_mode %d getLastMode %d\n", __FUNCTION__, mode, last_mode, CNeutrinoApp::getInstance()->getLastMode());
			StopAutoRecord(false);
			if (mode != last_mode && (last_mode != NeutrinoMessages::mode_standby || mode != CNeutrinoApp::getInstance()->getLastMode())) {
				CNeutrinoApp::getInstance()->handleMsg( NeutrinoMessages::CHANGEMODE , mode | NeutrinoMessages::norezap );
				mode_changed = true;
			}

			ret = g_Zapit->zapTo_serviceID(channel_id) > 0;
			printf("%s zapTo_serviceID channel_id %llx result %d\n", __FUNCTION__, channel_id, ret);
		}
		if (unlock)
			CFEManager::getInstance()->unlockFrontend(live_fe);
	}
	if(ret) {
		if(StopSectionsd) {
			printf("%s: g_Sectionsd->setPauseScanning(true)\n", __FUNCTION__);
			g_Sectionsd->setPauseScanning(true);
		}

		/* after this zapit send EVT_RECORDMODE_ACTIVATED, so neutrino getting NeutrinoMessages::EVT_RECORDMODE */
		g_Zapit->setRecordMode( true );
		if(last_mode == NeutrinoMessages::mode_standby)
			g_Zapit->stopPlayBack();
	}
	if(last_mode == NeutrinoMessages::mode_standby) {
		//CNeutrinoApp::getInstance()->handleMsg( NeutrinoMessages::CHANGEMODE , NeutrinoMessages::mode_standby);
		g_RCInput->postMsg( NeutrinoMessages::CHANGEMODE , last_mode);
	} else if(!ret && mode_changed /*mode != last_mode*/)
		CNeutrinoApp::getInstance()->handleMsg( NeutrinoMessages::CHANGEMODE , last_mode);

	printf("%s channel_id %llx mode %d : result %s\n", __FUNCTION__, channel_id, mode, ret ? "OK" : "BAD");
	return ret;
}

void CRecordManager::RestoreNeutrino(void)
{
	if(!recmap.empty())
		return;

	/* after this zapit send EVT_RECORDMODE_DEACTIVATED, so neutrino getting NeutrinoMessages::EVT_RECORDMODE */
	g_Zapit->setRecordMode( false );

	if((CNeutrinoApp::getInstance()->getMode() != NeutrinoMessages::mode_standby) && StopSectionsd)
		g_Sectionsd->setPauseScanning(false);
}

bool CRecordManager::IsFileRecord(std::string file)
{
	mutex.lock();
	for(recmap_iterator_t it = recmap.begin(); it != recmap.end(); it++) {
		CRecordInstance * inst = it->second;
		if ((((std::string)inst->GetFileName()) + ".ts") == file) {
			mutex.unlock();
			return true;
		}
	}
	mutex.unlock();
	return false;
}

#if 0
/* should return true, if recordingstatus changed in this function ? */
bool CRecordManager::doGuiRecord()
{
	bool refreshGui = false;
	std::string recDir;

	t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
	if(recordingstatus == 1) {
		bool doRecord = true;
#if 0 //FIXME unused ?
		doRecord = CRecordManager::getInstance()->ChooseRecDir(recDir);
#endif
		printf("%s: start to dir %s\n", __FUNCTION__, recDir.c_str());
		if(!doRecord || (Record(live_channel_id, recDir.c_str()) == false))
		{
			recordingstatus=0;
			refreshGui = true;
		}
	} else {
		int recording_id = 0;
		mutex.lock();
		CRecordInstance * inst = FindInstance(live_channel_id);
		if(inst)
			recording_id = inst->GetRecordingId();
		mutex.unlock();
		if(recording_id)
			g_Timerd->stopTimerEvent(recording_id);
	}
	return refreshGui;
}

bool CRecordManager::changeNotify(const neutrino_locale_t OptionName, void * /*data*/)
{
	bool ret = false;
	if ((ARE_LOCALES_EQUAL(OptionName, LOCALE_MAINMENU_RECORDING_START)) || (ARE_LOCALES_EQUAL(OptionName, LOCALE_MAINMENU_RECORDING)))
	{
		/* called after option (recordingstatus) changed and painted
		 * recordingstatus = 1 -> start live channe, 0 -> stop live channel record */
		if(g_RemoteControl->is_video_started) {
			ret =  doGuiRecord();
		}
		else {
			if(recordingstatus)
				ret = true;
			recordingstatus = 0;
		}
	}
	return ret;
}
#endif
#if 0
/* this is saved copy of neutrino code which seems was not used for some time */
bool CRecordManager::ChooseRecDir(std::string &dir)
{
	bool doRecord = true;

	if(g_settings.recording_choose_direct_rec_dir == 2) {
		CFileBrowser b;
		b.Dir_Mode=true;
		if (b.exec(g_settings.network_nfs_recordingdir)) {
			dir = b.getSelectedFile()->Name;
		}
		else doRecord = false;
	}
	else if(g_settings.recording_choose_direct_rec_dir == 1) {
		int userDecision = -1;
		CMountChooser recDirs(LOCALE_TIMERLIST_RECORDING_DIR,NEUTRINO_ICON_SETTINGS,&userDecision,NULL,g_settings.network_nfs_recordingdir);
		doRecord = false;
		if (recDirs.hasItem()) {
			recDirs.exec(NULL,"");
			if (userDecision != -1) {
				dir = g_settings.network_nfs_local_dir[userDecision];
				doRecord = MountDirectory(dir.c_str());
			}
		} else
			printf("%s: no network devices available\n", __FUNCTION__);
	}
	return doRecord;
}

bool CRecordManager::MountDirectory(const char *recordingDir)
{
	bool ret = true;

	if (!CFSMounter::isMounted(recordingDir)) {
		for(int i=0 ; i < NETWORK_NFS_NR_OF_ENTRIES ; i++) {
			if (strcmp(g_settings.network_nfs_local_dir[i],recordingDir) == 0) {
				CFSMounter::MountRes mres =
					CFSMounter::mount(g_settings.network_nfs_ip[i].c_str(),
							g_settings.network_nfs_dir[i],
							g_settings.network_nfs_local_dir[i],
							(CFSMounter::FSType) g_settings.network_nfs_type[i],
							g_settings.network_nfs_username[i],
							g_settings.network_nfs_password[i],
							g_settings.network_nfs_mount_options1[i],
							g_settings.network_nfs_mount_options2[i]);
				if (mres != CFSMounter::MRES_OK) {
					const char * merr = mntRes2Str(mres);
					int msglen = strlen(merr) + strlen(recordingDir) + 7;
					char msg[msglen];
					strcpy(msg,merr);
					strcat(msg,"\nDir: ");
					strcat(msg,recordingDir);

					ShowMsgUTF(LOCALE_MESSAGEBOX_ERROR, msg,
							CMessageBox::mbrBack, CMessageBox::mbBack,NEUTRINO_ICON_ERROR, 450, 10); // UTF-8
					ret = false;
				}
				break;
			}
		}
	}

	return ret;
}
#endif

#if 0 // not used, saved in case we needed it
extern bool autoshift_delete;
bool CRecordManager::LinkTimeshift()
{
	if(autoshift) {
		char buf[512];
		autoshift = false;
		sprintf(buf, "ln %s/* %s", timeshiftDir, g_settings.network_nfs_recordingdir);
		system(buf);
		autoshift_delete = true;
	}
}
#endif
