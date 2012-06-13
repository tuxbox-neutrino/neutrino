/*
 * $Id: zapit.cpp,v 1.290.2.50 2003/06/13 10:53:15 digi_casi Exp $
 *
 * zapit - d-box2 linux project
 *
 * (C) 2001, 2002 by Philipp Leusmann <faralla@berlios.de>
 * Copyright (C) 2011 CoolStream International Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/* system headers */
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <syscall.h>

#include <pthread.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* zapit headers */
#include <zapit/cam.h>
#include <zapit/client/msgtypes.h>
#include <zapit/debug.h>
#include <zapit/getservices.h>
#include <zapit/pat.h>
#include <zapit/scanpmt.h>
#include <zapit/scan.h>
#include <zapit/fastscan.h>
#include <zapit/scansdt.h>
#include <zapit/settings.h>
#include <zapit/zapit.h>
#include <xmlinterface.h>

#include <ca_cs.h>

#include <zapit/satconfig.h>
#include <zapit/femanager.h>
#include <dmx.h>
#if HAVE_COOL_HARDWARE
#include <record_cs.h>
#include <playback_cs.h>
#include <pwrmngr.h>
#include <audio_cs.h>
#include <video_cs.h>
#endif
#if HAVE_TRIPLEDRAGON
#include <video_td.h>
#include <audio_td.h>
#endif

#include <driver/abstime.h>
#include "libdvbsub/dvbsub.h"
#include "libtuxtxt/teletext.h"

/* globals */
int sig_delay = 2; // seconds between signal check

/* the bouquet manager */
CBouquetManager *g_bouquetManager = NULL;

//int cam_ci = 2; //  CA_INIT_SC 0 or CA_INIT_CI 1 or CA_INIT_BOTH 2
cCA *ca = NULL;
extern cDemux * pmtDemux;
extern cVideo *videoDecoder;
extern cAudio *audioDecoder;
extern cDemux *audioDemux;
extern cDemux *videoDemux;

cDemux *pcrDemux = NULL;

/* the map which stores the wanted cable/satellites */
scan_list_t scanProviders;

int zapit_debug = 0;

/* transponder scan */
transponder_list_t transponders;

CZapitClient::bouquetMode bouquetMode = CZapitClient::BM_UPDATEBOUQUETS;
//static CZapitClient::scanType scanType = CZapitClient::ST_TVRADIO;

//static TP_params TP;

static bool update_pmt = true;
/******************************************************************************/
CZapit * CZapit::zapit = NULL;
CZapit::CZapit() 
	: configfile(',', false)
{
	started = false;
	pmt_update_fd = -1;
	volume_left = 0, volume_right = 0;
	def_volume_left = 0, def_volume_right = 0;
	audio_mode = 0;
	aspectratio=0;
	mode43=0;
	def_audio_mode = 0;
	playbackStopForced = false;
	standby = true;
	event_mode = true;
	firstzap = true;
	playing = false;
	list_changed = false; // flag to indicate, allchans was changed
	currentMode = 0;
}

CZapit::~CZapit()
{
	Stop();
}

CZapit * CZapit::getInstance()
{
	if(zapit == NULL)
		zapit = new CZapit();
	return zapit;
}

void CZapit::SendEvent(const unsigned int eventID, const void* eventbody, const unsigned int eventbodysize)
{
	if(event_mode)
		eventServer->sendEvent(eventID, CEventServer::INITID_ZAPIT, eventbody, eventbodysize);
}

void CZapit::SaveSettings(bool write)
{
	if (current_channel) {
#if 0
		// now save the lowest channel number with the current channel_id
		int c = ((currentMode & RADIO_MODE) ? g_bouquetManager->radioChannelsBegin() : g_bouquetManager->tvChannelsBegin()).getLowestChannelNumberWithChannelID(current_channel->getChannelID());
//printf("LAST CHAN %d !!!!!!!!!!!\n\n\n", c);
		//FIXME save last TV/RADIO channel id, always ?
		if (c >= 0) {
			if ((currentMode & RADIO_MODE))
				lastChannelRadio = c;
			else
				lastChannelTV = c;
		}
#endif
		if ((currentMode & RADIO_MODE))
			lastChannelRadio = current_channel->getChannelID();
		else
			lastChannelTV = current_channel->getChannelID();
	}

	if (write) {
		configfile.setBool("saveLastChannel", config.saveLastChannel);
		if (config.saveLastChannel) {
			configfile.setInt32("lastChannelMode", (currentMode & RADIO_MODE) ? 1 : 0);
			configfile.setInt64("lastChannelRadio", lastChannelRadio);
			configfile.setInt64("lastChannelTV", lastChannelTV);
			configfile.setInt64("lastChannel", live_channel_id);
		}

		configfile.setBool("writeChannelsNames", config.writeChannelsNames);
		configfile.setBool("makeRemainingChannelsBouquet", config.makeRemainingChannelsBouquet);
		configfile.setInt32("feTimeout", config.feTimeout);

		configfile.setInt32("rezapTimeout", config.rezapTimeout);
		configfile.setBool("scanPids", config.scanPids);
		configfile.setInt32("scanSDT", config.scanSDT);
		configfile.setInt32("cam_ci", config.cam_ci);
#if 0 // unused
		configfile.setBool("fastZap", config.fastZap);
		configfile.setBool("sortNames", config.sortNames);
#endif

		/* FIXME FE global */
		char tempd[12];
		sprintf(tempd, "%3.6f", config.gotoXXLatitude);
		configfile.setString("gotoXXLatitude", tempd);
		sprintf(tempd, "%3.6f", config.gotoXXLongitude);
		configfile.setString("gotoXXLongitude", tempd);

		configfile.setInt32("gotoXXLaDirection", config.gotoXXLaDirection);
		configfile.setInt32("gotoXXLoDirection", config.gotoXXLoDirection);
		configfile.setInt32("repeatUsals", config.repeatUsals);

#if 1
		/* FIXME FE specific */
		configfile.setBool("highVoltage", config.highVoltage);
		configfile.setInt32("lastSatellitePosition", live_fe->getRotorSatellitePosition());
		configfile.setInt32("diseqcRepeats", live_fe->getDiseqcRepeats());
		configfile.setInt32("diseqcType", live_fe->getDiseqcType());
		configfile.setInt32("motorRotationSpeed", config.motorRotationSpeed);
		configfile.setInt32("uni_scr", config.uni_scr);
		configfile.setInt32("uni_qrg", config.uni_qrg);
#endif
		if (configfile.getModifiedFlag())
			configfile.saveConfig(CONFIGFILE);
	}
}

void CZapit::LoadAudioMap()
{
	FILE *audio_config_file = fopen(AUDIO_CONFIG_FILE, "r");
	audio_map.clear();
	if (!audio_config_file) {
		perror(AUDIO_CONFIG_FILE);
		return;
	}
	t_channel_id chan;
	int apid = 0, subpid = 0, ttxpid = 0, ttxpage = 0;
	int mode = 0;
	int volume = 0;
	char s[1000];
	while (fgets(s, 1000, audio_config_file)) {
		sscanf(s, "%llx %d %d %d %d %d %d", &chan, &apid, &mode, &volume, &subpid, &ttxpid, &ttxpage);
		audio_map[chan].apid = apid;
		audio_map[chan].subpid = subpid;
		audio_map[chan].mode = mode;
		audio_map[chan].volume = volume;
		audio_map[chan].ttxpid = ttxpid;
		audio_map[chan].ttxpage = ttxpage;
	}
	fclose(audio_config_file);
}

void CZapit::SaveAudioMap()
{
	FILE *audio_config_file = fopen(AUDIO_CONFIG_FILE, "w");
	if (!audio_config_file) {
		perror(AUDIO_CONFIG_FILE);
		return;
	}
	for (audio_map_iterator_t audio_map_it = audio_map.begin(); audio_map_it != audio_map.end(); audio_map_it++) {
		fprintf(audio_config_file, "%llx %d %d %d %d %d %d\n", (uint64_t) audio_map_it->first,
			(int) audio_map_it->second.apid, (int) audio_map_it->second.mode, (int) audio_map_it->second.volume, 
			(int) audio_map_it->second.subpid, (int) audio_map_it->second.ttxpid, (int) audio_map_it->second.ttxpage);
	}
	fdatasync(fileno(audio_config_file));
	fclose(audio_config_file);
}


void CZapit::LoadSettings()
{
	if (!configfile.loadConfig(CONFIGFILE))
		WARN("%s not found", CONFIGFILE);

	live_channel_id				= configfile.getInt64("lastChannel", 0);
	lastChannelRadio			= configfile.getInt64("lastChannelRadio", 0);
	lastChannelTV				= configfile.getInt64("lastChannelTV", 0);

#if 0 //unused
	config.fastZap				= configfile.getBool("fastZap", 1);
	config.sortNames			= configfile.getBool("sortNames", 0);
	voltageOff				= configfile.getBool("voltageOff", 0);
#endif
	config.saveLastChannel			= configfile.getBool("saveLastChannel", true);
	/* FIXME Channels renum should be done for all channels atm. TODO*/
	//config.makeRemainingChannelsBouquet	= configfile.getBool("makeRemainingChannelsBouquet", 1);
	config.makeRemainingChannelsBouquet	= 1;
	config.scanPids				= configfile.getBool("scanPids", 0);
	config.scanSDT				= configfile.getInt32("scanSDT", 0);
	config.cam_ci				= configfile.getInt32("cam_ci", 2);
	config.rezapTimeout			= configfile.getInt32("rezapTimeout", 1);

	config.feTimeout			= configfile.getInt32("feTimeout", 40);
	config.highVoltage			= configfile.getBool("highVoltage", 0);

	config.gotoXXLatitude 			= strtod(configfile.getString("gotoXXLatitude", "0.0").c_str(), NULL);
	config.gotoXXLongitude			= strtod(configfile.getString("gotoXXLongitude", "0.0").c_str(), NULL);
	config.gotoXXLaDirection		= configfile.getInt32("gotoXXLaDirection", 1);
	config.gotoXXLoDirection		= configfile.getInt32("gotoXXLoDirection", 0);
	config.repeatUsals			= configfile.getInt32("repeatUsals", 0);

	/* FIXME FE specific, to be removed */
	diseqcType				= (diseqc_t)configfile.getInt32("diseqcType", NO_DISEQC);
	config.motorRotationSpeed		= configfile.getInt32("motorRotationSpeed", 18); // default: 1.8 degrees per second
	config.uni_scr				= configfile.getInt32("uni_scr", -1);
	config.uni_qrg				= configfile.getInt32("uni_qrg", 0);

	printf("[zapit.cpp] diseqc type = %d\n", diseqcType);

	CFEManager::getInstance()->loadSettings();
	/**/

	LoadAudioMap();
}

void CZapit::ConfigFrontend()
{
	int count = CFEManager::getInstance()->getFrontendCount();
	for(int i = 0; i < count; i++) {
		CFrontend * fe = CFEManager::getInstance()->getFE(i);
		if(!fe)
			continue;

		fe->setTimeout(config.feTimeout);
		fe->configUsals(config.gotoXXLatitude, config.gotoXXLongitude,
				config.gotoXXLaDirection, config.gotoXXLoDirection, config.repeatUsals);

		if(!CFEManager::getInstance()->configExist()) {
			INFO("New frontend config not exist");
			fe->configRotor(config.motorRotationSpeed, config.highVoltage);
			fe->configUnicable(config.uni_scr, config.uni_qrg);
			fe->setDiseqcType(diseqcType);
			fe->setDiseqcRepeats(configfile.getInt32("diseqcRepeats", 0));
			fe->setRotorSatellitePosition(configfile.getInt32("lastSatellitePosition", 0));
			fe->setSatellites(CServiceManager::getInstance()->SatelliteList());
		}
	}
}

void CZapit::SendPMT(bool forupdate)
{
	if(!current_channel)
		return;

	CCamManager::getInstance()->Start(current_channel->getChannelID(), CCamManager::PLAY, forupdate);
	int len;
	unsigned char * pmt = current_channel->getRawPmt(len);
	ca->SendPMT(DEMUX_SOURCE_0, pmt, len);
}

void CZapit::SaveChannelPids(CZapitChannel* channel)
{
	if(channel == NULL)
		return;

	printf("[zapit] saving channel, apid %x sub pid %x mode %d volume %d\n", channel->getAudioPid(), dvbsub_getpid(), audio_mode, volume_right);
	audio_map[channel->getChannelID()].apid = channel->getAudioPid();
	audio_map[channel->getChannelID()].mode = audio_mode;
	audio_map[channel->getChannelID()].volume = audioDecoder->getVolume();
	audio_map[channel->getChannelID()].subpid = dvbsub_getpid();
	tuxtx_subtitle_running(&audio_map[channel->getChannelID()].ttxpid, &audio_map[channel->getChannelID()].ttxpage, NULL);
}

void CZapit::RestoreChannelPids(CZapitChannel * channel)
{
	audio_map_set_t * pidmap = GetSavedPids(channel->getChannelID());
	if(pidmap) {
		printf("[zapit] channel found, audio pid %x, subtitle pid %x mode %d volume %d\n",
				pidmap->apid, pidmap->subpid, pidmap->mode, pidmap->volume);

		if(channel->getAudioChannelCount() > 1) {
			for (int i = 0; i < channel->getAudioChannelCount(); i++) {
				if (channel->getAudioChannel(i)->pid == pidmap->apid ) {
					DBG("***** Setting audio!\n");
					channel->setAudioChannel(i);
#if 0
					if(we_playing && (channel->getAudioChannel(i)->audioChannelType != CZapitAudioChannel::MPEG))
						ChangeAudioPid(i);
#endif
				}
			}
		}
#if 0 // to restore saved volume per channel if needed. after first zap its done by neutrino
		if(firstzap) {
			audioDecoder->setVolume(audio_map_it->second.volume, audio_map_it->second.volume);
		}
#endif
		volume_left = volume_right = pidmap->volume;
		audio_mode = pidmap->mode;

		dvbsub_setpid(pidmap->subpid);

		std::string tmplang;
		for (int i = 0 ; i < (int)channel->getSubtitleCount() ; ++i) {
			CZapitAbsSub* s = channel->getChannelSub(i);
			if(s->pId == pidmap->ttxpid) {
				tmplang = s->ISO639_language_code;
				break;
			}
		}
		if(tmplang.empty())
			tuxtx_set_pid(pidmap->ttxpid, pidmap->ttxpage, (char *) channel->getTeletextLang());
		else
			tuxtx_set_pid(pidmap->ttxpid, pidmap->ttxpage, (char *) tmplang.c_str());
	} else {
		volume_left = volume_right = def_volume_left;
		audio_mode = def_audio_mode;
		tuxtx_set_pid(0, 0, (char *) channel->getTeletextLang());
	}
	/* restore saved stereo / left / right channel mode */
	//audioDecoder->setVolume(volume_left, volume_right);
	audioDecoder->setChannel(audio_mode);
}

audio_map_set_t * CZapit::GetSavedPids(const t_channel_id channel_id)
{
	audio_map_iterator_t audio_map_it = audio_map.find(channel_id);
	if(audio_map_it != audio_map.end())
		return &audio_map_it->second;

	return NULL;
}

bool CZapit::TuneChannel(CFrontend * frontend, CZapitChannel * channel, bool &transponder_change)
{
	//CFrontend * frontend = CFEManager::getInstance()->allocateFE(channel);
	if(frontend == NULL) {
		ERROR("Cannot get frontend\n");
		return false;
	}
	transponder_change = false;
#if 0
	if (!(currentMode & RECORD_MODE)) 
#endif
	{
		transponder_change = frontend->setInput(channel, current_is_nvod);
		if(transponder_change && !current_is_nvod) {
			int waitForMotor = frontend->driveToSatellitePosition(channel->getSatellitePosition());
			if(waitForMotor > 0) {
				printf("[zapit] waiting %d seconds for motor to turn satellite dish.\n", waitForMotor);
				SendEvent(CZapitClient::EVT_ZAP_MOTOR, &waitForMotor, sizeof(waitForMotor));
				for(int i = 0; i < waitForMotor; i++) {
					sleep(1);
					if(abort_zapit) {
						abort_zapit = 0;
						return false;
					}
				}
			}
		}

		/* if channel's transponder does not match frontend's tuned transponder ... */
		if (transponder_change /* || current_is_nvod*/) {
			if (frontend->tuneChannel(channel, current_is_nvod) == false) {
				return false;
			}
		}
	} 
#if 0
	else if(!SAME_TRANSPONDER(channel->getChannelID(), live_channel_id))
		return false;
#endif
	return true;
}

bool CZapit::ParsePatPmt(CZapitChannel * channel)
{
	CPat pat(channel->getRecordDemux());
	CPmt pmt(channel->getRecordDemux());
	DBG("looking up pids for channel_id " PRINTF_CHANNEL_ID_TYPE "\n", channel->getChannelID());

	if(!pat.Parse(channel)) {
		printf("[zapit] pat parsing failed\n");
		return false;
	}
	if (!pmt.Parse(channel)) {
		printf("[zapit] pmt parsing failed\n");
		return false;
	}

	return true;
}

bool CZapit::ZapIt(const t_channel_id channel_id, bool forupdate, bool startplayback)
{
	bool transponder_change = false;
	bool failed = false;
	CZapitChannel* newchannel;

	DBG("[zapit] zapto channel id %llx diseqcType %d\n", channel_id, diseqcType);

	abort_zapit = 0;
	if((newchannel = CServiceManager::getInstance()->FindChannel(channel_id, &current_is_nvod)) == NULL) {
		DBG("channel_id " PRINTF_CHANNEL_ID_TYPE " not found", channel_id);
		return false;
	}

	printf("[zapit] zap to %s (%llx tp %llx)\n", newchannel->getName().c_str(), newchannel->getChannelID(), newchannel->getTransponderId());

	CFrontend * fe = CFEManager::getInstance()->allocateFE(newchannel);
	if(fe == NULL) {
		ERROR("Cannot get frontend\n");
		return false;
	}
	live_fe = fe;
	CFEManager::getInstance()->setLiveFE(live_fe);

	sig_delay = 2;
	if (!firstzap && current_channel)
		SaveChannelPids(current_channel);

	/* firstzap right now does nothing but control saving the audio channel */
	firstzap = false;

	pmt_stop_update_filter(&pmt_update_fd);

	ca->SendPMT(0, (unsigned char*) "", 0, CA_SLOT_TYPE_CI);
	StopPlayBack(!forupdate);

	if(!forupdate && current_channel)
		current_channel->resetPids();

	current_channel = newchannel;

	live_channel_id = current_channel->getChannelID();
	SaveSettings(false);

	if(!TuneChannel(live_fe, newchannel, transponder_change)) {
		t_channel_id chid = 0;
		SendEvent(CZapitClient::EVT_TUNE_COMPLETE, &chid, sizeof(t_channel_id));
		return false;
	}
	SendEvent(CZapitClient::EVT_TUNE_COMPLETE, &live_channel_id, sizeof(t_channel_id));

	if (current_channel->getServiceType() == ST_NVOD_REFERENCE_SERVICE) {
		current_is_nvod = true;
		return true;
	}

	failed = !ParsePatPmt(current_channel);

	if ((!failed) && (current_channel->getAudioPid() == 0) && (current_channel->getVideoPid() == 0)) {
		printf("[zapit] neither audio nor video pid found\n");
		failed = true;
	}

	/* start sdt scan even if the service was not found in pat or pmt
	 * if the frontend did not tune, we don't get here, so this is fine */
	if (transponder_change)
		SdtMonitor.Wakeup();

	if (failed)
		return false;

	//current_channel->getCaPmt()->ca_pmt_list_management = transponder_change ? 0x03 : 0x04;

	RestoreChannelPids(current_channel);

	if (startplayback /* && !we_playing*/)
		StartPlayBack(current_channel);

	printf("[zapit] sending capmt....\n");

	SendPMT(forupdate);
	//play:
	int caid = 1;
	SendEvent(CZapitClient::EVT_ZAP_CA_ID, &caid, sizeof(int));

	if (update_pmt)
		pmt_set_update_filter(current_channel, &pmt_update_fd);

	return true;
}

bool CZapit::ZapForRecord(const t_channel_id channel_id)
{
	CZapitChannel* newchannel;
	bool transponder_change;

	if((newchannel = CServiceManager::getInstance()->FindChannel(channel_id)) == NULL) {
		printf("zapit_to_record: channel_id " PRINTF_CHANNEL_ID_TYPE " not found", channel_id);
		return false;
	}
	printf("%s: %s (%llx)\n", __FUNCTION__, newchannel->getName().c_str(), channel_id);

	CFrontend * frontend = CFEManager::getInstance()->allocateFE(newchannel);
	if(frontend == NULL) {
		ERROR("Cannot get frontend\n");
		return false;
	}
	if(!TuneChannel(frontend, newchannel, transponder_change))
		return false;

	if(!ParsePatPmt(newchannel))
		return false;

	return true;
}

void CZapit::SetAudioStreamType(CZapitAudioChannel::ZapitAudioChannelType audioChannelType)
{
	const char *audioStr = "UNKNOWN";
	switch (audioChannelType) {
		case CZapitAudioChannel::AC3:
			audioStr = "AC3";
			audioDecoder->SetStreamType(AUDIO_FMT_DOLBY_DIGITAL);
			break;
		case CZapitAudioChannel::MPEG:
			audioStr = "MPEG2";
			audioDecoder->SetStreamType(AUDIO_FMT_MPEG);
			break;
		case CZapitAudioChannel::AAC:
			audioStr = "AAC";
			audioDecoder->SetStreamType(AUDIO_FMT_AAC);
			break;
		case CZapitAudioChannel::AACPLUS:
			audioStr = "AAC-PLUS";
			audioDecoder->SetStreamType(AUDIO_FMT_AAC_PLUS);
			break;
		case CZapitAudioChannel::DTS:
			audioStr = "DTS";
			audioDecoder->SetStreamType(AUDIO_FMT_DTS);
			break;
		default:
			printf("[zapit] unknown audio channel type 0x%x\n", audioChannelType);
			break;
	}

	printf("[zapit] starting %s audio\n", audioStr);
}

bool CZapit::ChangeAudioPid(uint8_t index)
{
	if (!current_channel)
		return false;

	/* stop demux filter */
	if (audioDemux->Stop() < 0)
		return false;

	/* stop audio playback */
	if (audioDecoder->Stop() < 0)
		return false;

	/* update current channel */
	current_channel->setAudioChannel(index);

	/* set bypass mode */
	CZapitAudioChannel *currentAudioChannel = current_channel->getAudioChannel();

	if (!currentAudioChannel) {
		WARN("No current audio channel");
		return false;
	}

	printf("[zapit] change apid to 0x%x\n", current_channel->getAudioPid());
	SetAudioStreamType(currentAudioChannel->audioChannelType);

	/* set demux filter */
	if (audioDemux->pesFilter(current_channel->getAudioPid()) < 0)
		return false;

	/* start demux filter */
	if (audioDemux->Start() < 0)
		return false;

	/* start audio playback */
	if (audioDecoder->Start() < 0)
		return false;

	return true;
}

void CZapit::SetRadioMode(void)
{
	currentMode |= RADIO_MODE;
	currentMode &= ~TV_MODE;
}

void CZapit::SetTVMode(void)
{
	currentMode |= TV_MODE;
	currentMode &= ~RADIO_MODE;
}

int CZapit::getMode(void)
{
	if (currentMode & TV_MODE)
		return CZapitClient::MODE_TV;
	if (currentMode & RADIO_MODE)
		return CZapitClient::MODE_RADIO;
	return 0;
}

void CZapit::SetRecordMode(bool enable)
{
	unsigned int event;
	bool mode = currentMode & RECORD_MODE;

	if(mode == enable)
		return;

	if(enable) {
		currentMode |= RECORD_MODE;
		event = CZapitClient::EVT_RECORDMODE_ACTIVATED;
	} else {
		currentMode &= ~RECORD_MODE;

		int demux = 2;
		if(current_channel)
			demux = current_channel->getRecordDemux();
		ca->SendPMT(demux /*DEMUX_SOURCE_2*/, (unsigned char*) "", 0, CA_SLOT_TYPE_SMARTCARD);
		event = CZapitClient::EVT_RECORDMODE_DEACTIVATED;
	}
	SendEvent(event);
}

bool CZapit::PrepareChannels()
{
	current_channel = 0;

	if (!CServiceManager::getInstance()->LoadServices(false))
		return false;

	INFO("LoadServices: success");
	g_bouquetManager->loadBouquets();
	/* save if services changed (update from sdt, etc) */
	CServiceManager::getInstance()->SaveServices(true, true);
	return true;
}

void CZapit::PrepareScan()
{
	StopPlayBack(true);
        pmt_stop_update_filter(&pmt_update_fd);
	current_channel = 0;
}

bool CZapit::StartScan(int scan_mode)
{
	PrepareScan();

	CServiceScan::getInstance()->Start(CServiceScan::SCAN_PROVIDER, (void *) scan_mode);
	return true;
}

bool CZapit::StartScanTP(TP_params * TPparams)
{
	PrepareScan();

	CServiceScan::getInstance()->Start(CServiceScan::SCAN_TRANSPONDER, (void *) TPparams);
	return true;
}

bool CZapit::StartFastScan(int scan_mode, int opid)
{
	fast_scan_type_t scant;

	scant.type = scan_mode;
	scant.op = (fs_operator_t) opid;

	PrepareScan();

	CServiceScan::getInstance()->Start(CServiceScan::SCAN_FAST, (void *) &scant);
	return true;
}

void CZapit::SendCmdReady(int connfd)
{
	CZapitMessages::responseCmd response;
	response.cmd = CZapitMessages::CMD_READY;
	CBasicServer::send_data(connfd, &response, sizeof(response));
}

bool CZapit::ParseCommand(CBasicMessage::Header &rmsg, int connfd)
{
	DBG("cmd %d (version %d) received\n", rmsg.cmd, rmsg.version);

	if ((standby) && ((rmsg.cmd != CZapitMessages::CMD_SET_VOLUME)
		&& (rmsg.cmd != CZapitMessages::CMD_MUTE)
		&& (rmsg.cmd != CZapitMessages::CMD_REGISTEREVENTS)
		&& (rmsg.cmd != CZapitMessages::CMD_IS_TV_CHANNEL)
		&& (rmsg.cmd != CZapitMessages::CMD_SET_STANDBY))) {
		WARN("cmd %d in standby mode", rmsg.cmd);
		//return true;
	}

	switch (rmsg.cmd) {
	case CZapitMessages::CMD_SHUTDOWN:
		return false;

	case CZapitMessages::CMD_ZAPTO:
	{
		CZapitMessages::commandZapto msgZapto;
		CBasicServer::receive_data(connfd, &msgZapto, sizeof(msgZapto)); // bouquet & channel number are already starting at 0!
		ZapTo(msgZapto.bouquet, msgZapto.channel);
		break;
	}

	case CZapitMessages::CMD_ZAPTO_CHANNELNR: {
		CZapitMessages::commandZaptoChannelNr msgZaptoChannelNr;
		CBasicServer::receive_data(connfd, &msgZaptoChannelNr, sizeof(msgZaptoChannelNr)); // bouquet & channel number are already starting at 0!
		ZapTo(msgZaptoChannelNr.channel);
		break;
	}

	case CZapitMessages::CMD_ZAPTO_SERVICEID:
	case CZapitMessages::CMD_ZAPTO_SUBSERVICEID: {
		CZapitMessages::commandZaptoServiceID msgZaptoServiceID;
		CZapitMessages::responseZapComplete msgResponseZapComplete;
		CBasicServer::receive_data(connfd, &msgZaptoServiceID, sizeof(msgZaptoServiceID));
		if(msgZaptoServiceID.record) {
			msgResponseZapComplete.zapStatus = ZapForRecord(msgZaptoServiceID.channel_id);
		} else {
			msgResponseZapComplete.zapStatus = ZapTo(msgZaptoServiceID.channel_id, (rmsg.cmd == CZapitMessages::CMD_ZAPTO_SUBSERVICEID));
		}
		CBasicServer::send_data(connfd, &msgResponseZapComplete, sizeof(msgResponseZapComplete));
		break;
	}

	case CZapitMessages::CMD_ZAPTO_SERVICEID_NOWAIT:
	case CZapitMessages::CMD_ZAPTO_SUBSERVICEID_NOWAIT: {
		CZapitMessages::commandZaptoServiceID msgZaptoServiceID;
		CBasicServer::receive_data(connfd, &msgZaptoServiceID, sizeof(msgZaptoServiceID));
		ZapTo(msgZaptoServiceID.channel_id, (rmsg.cmd == CZapitMessages::CMD_ZAPTO_SUBSERVICEID_NOWAIT));
		break;
	}

#if 0
	case CZapitMessages::CMD_GET_LAST_CHANNEL: {
		CZapitClient::responseGetLastChannel lastchannel;
		lastchannel.channel_id = (currentMode & RADIO_MODE) ? lastChannelRadio : lastChannelTV;
		lastchannel.mode = getMode();
		CBasicServer::send_data(connfd, &lastchannel, sizeof(lastchannel)); // bouquet & channel number are already starting at 0!
		ERROR("CZapitMessages::CMD_GET_LAST_CHANNEL: depreciated command\n");
		break;
	}
#endif
#if 0
	case CZapitMessages::CMD_GET_CURRENT_SATELLITE_POSITION: {
		int32_t currentSatellitePosition = current_channel ? current_channel->getSatellitePosition() : live_fe->getCurrentSatellitePosition();
		CBasicServer::send_data(connfd, &currentSatellitePosition, sizeof(currentSatellitePosition));
		break;
	}
#endif
	case CZapitMessages::CMD_SET_AUDIOCHAN: {
		CZapitMessages::commandSetAudioChannel msgSetAudioChannel;
		CBasicServer::receive_data(connfd, &msgSetAudioChannel, sizeof(msgSetAudioChannel));
		ChangeAudioPid(msgSetAudioChannel.channel);
		break;
	}

	case CZapitMessages::CMD_SET_MODE: {
		CZapitMessages::commandSetMode msgSetMode;
		CBasicServer::receive_data(connfd, &msgSetMode, sizeof(msgSetMode));
		if (msgSetMode.mode == CZapitClient::MODE_TV)
			SetTVMode();
		else if (msgSetMode.mode == CZapitClient::MODE_RADIO)
			SetRadioMode();
		break;
	}

	case CZapitMessages::CMD_GET_MODE: {
		CZapitMessages::responseGetMode msgGetMode;
		msgGetMode.mode = (CZapitClient::channelsMode) getMode();
		CBasicServer::send_data(connfd, &msgGetMode, sizeof(msgGetMode));
		break;
	}

	case CZapitMessages::CMD_GET_CURRENT_SERVICEID: {
		CZapitMessages::responseGetCurrentServiceID msgCurrentSID;
		msgCurrentSID.channel_id = (current_channel != 0) ? current_channel->getChannelID() : 0;
		CBasicServer::send_data(connfd, &msgCurrentSID, sizeof(msgCurrentSID));
		break;
	}

	case CZapitMessages::CMD_GET_CURRENT_SERVICEINFO: {
		CZapitClient::CCurrentServiceInfo msgCurrentServiceInfo;
		memset(&msgCurrentServiceInfo, 0, sizeof(CZapitClient::CCurrentServiceInfo));
		if(current_channel) {
			msgCurrentServiceInfo.onid = current_channel->getOriginalNetworkId();
			msgCurrentServiceInfo.sid = current_channel->getServiceId();
			msgCurrentServiceInfo.tsid = current_channel->getTransportStreamId();
			msgCurrentServiceInfo.vpid = current_channel->getVideoPid();
			msgCurrentServiceInfo.apid = current_channel->getAudioPid();
			msgCurrentServiceInfo.vtxtpid = current_channel->getTeletextPid();
			msgCurrentServiceInfo.pmtpid = current_channel->getPmtPid();
			//msgCurrentServiceInfo.pmt_version = (current_channel->getCaPmt() != NULL) ? current_channel->getCaPmt()->version_number : 0xff;
			msgCurrentServiceInfo.pmt_version = current_channel->getPmtVersion();
			msgCurrentServiceInfo.pcrpid = current_channel->getPcrPid();
			msgCurrentServiceInfo.tsfrequency = live_fe->getFrequency();
			msgCurrentServiceInfo.rate = live_fe->getRate();
			msgCurrentServiceInfo.fec = live_fe->getCFEC();
			msgCurrentServiceInfo.vtype = current_channel->type;
			//msgCurrentServiceInfo.diseqc = current_channel->getDiSEqC();
		}
		if(!msgCurrentServiceInfo.fec)
			msgCurrentServiceInfo.fec = (fe_code_rate)3;
		if (live_fe->getInfo()->type == FE_QPSK)
			msgCurrentServiceInfo.polarisation = live_fe->getPolarization();
		else
			msgCurrentServiceInfo.polarisation = 2;
		CBasicServer::send_data(connfd, &msgCurrentServiceInfo, sizeof(msgCurrentServiceInfo));
		break;
	}

	case CZapitMessages::CMD_GET_DELIVERY_SYSTEM: {
		CZapitMessages::responseDeliverySystem response;
		switch (live_fe->getInfo()->type) {
		case FE_QAM:
			response.system = DVB_C;
			break;
		case FE_QPSK:
			response.system = DVB_S;
			break;
		case FE_OFDM:
			response.system = DVB_T;
			break;
		default:
			WARN("Unknown type %d", live_fe->getInfo()->type);
			return false;

		}
		CBasicServer::send_data(connfd, &response, sizeof(response));
		break;
	}

	case CZapitMessages::CMD_GET_BOUQUETS: {
		CZapitMessages::commandGetBouquets msgGetBouquets;
		CBasicServer::receive_data(connfd, &msgGetBouquets, sizeof(msgGetBouquets));
		sendBouquets(connfd, msgGetBouquets.emptyBouquetsToo, msgGetBouquets.mode); // bouquet & channel number are already starting at 0!
		break;
	}

	case CZapitMessages::CMD_GET_BOUQUET_CHANNELS: {
		CZapitMessages::commandGetBouquetChannels msgGetBouquetChannels;
		CBasicServer::receive_data(connfd, &msgGetBouquetChannels, sizeof(msgGetBouquetChannels));
		sendBouquetChannels(connfd, msgGetBouquetChannels.bouquet, msgGetBouquetChannels.mode, false); // bouquet & channel number are already starting at 0!
		break;
	}
	case CZapitMessages::CMD_GET_BOUQUET_NCHANNELS: {
		CZapitMessages::commandGetBouquetChannels msgGetBouquetChannels;
		CBasicServer::receive_data(connfd, &msgGetBouquetChannels, sizeof(msgGetBouquetChannels));
		sendBouquetChannels(connfd, msgGetBouquetChannels.bouquet, msgGetBouquetChannels.mode, true); // bouquet & channel number are already starting at 0!
		break;
	}

	case CZapitMessages::CMD_GET_CHANNELS: {
		CZapitMessages::commandGetChannels msgGetChannels;
		CBasicServer::receive_data(connfd, &msgGetChannels, sizeof(msgGetChannels));
		sendChannels(connfd, msgGetChannels.mode, msgGetChannels.order); // bouquet & channel number are already starting at 0!
		break;
	}

        case CZapitMessages::CMD_GET_CHANNEL_NAME: {
                t_channel_id requested_channel_id;
                CZapitMessages::responseGetChannelName response;
                CBasicServer::receive_data(connfd, &requested_channel_id, sizeof(requested_channel_id));
		response.name[0] = 0;
		CZapitChannel * channel = (requested_channel_id == 0) ? current_channel :
			CServiceManager::getInstance()->FindChannel(requested_channel_id);
		if(channel) {
			strncpy(response.name, channel->getName().c_str(), CHANNEL_NAME_SIZE);
			response.name[CHANNEL_NAME_SIZE-1] = 0;
		}
                CBasicServer::send_data(connfd, &response, sizeof(response));
                break;
        }
#if 0
       case CZapitMessages::CMD_IS_TV_CHANNEL: {
               t_channel_id                             requested_channel_id;
               CZapitMessages::responseGeneralTrueFalse response;
               CBasicServer::receive_data(connfd, &requested_channel_id, sizeof(requested_channel_id));

		/* if in doubt (i.e. unknown channel) answer yes */
		response.status = true;
		CZapitChannel * channel = CServiceManager::getInstance()->FindChannel(requested_channel_id);
		if(channel)
			/* FIXME: the following check is no even remotely accurate */
			response.status = (channel->getServiceType() != ST_DIGITAL_RADIO_SOUND_SERVICE);
		
		CBasicServer::send_data(connfd, &response, sizeof(response));
		break;
        }
#endif
	case CZapitMessages::CMD_BQ_RESTORE: {
		//2004.08.02 g_bouquetManager->restoreBouquets();
		if(list_changed) {
			PrepareChannels();
			list_changed = 0;
		} else {
			g_bouquetManager->clearAll();
			g_bouquetManager->loadBouquets();
		}
		SendCmdReady(connfd);
		break;
	}

	case CZapitMessages::CMD_REINIT_CHANNELS: {
		// Houdini: save actual channel to restore it later, old version's channel was set to scans.conf initial channel
  		t_channel_id cid= current_channel ? current_channel->getChannelID() : 0;

   		PrepareChannels();

		current_channel = CServiceManager::getInstance()->FindChannel(cid);

		SendCmdReady(connfd);
		SendEvent(CZapitClient::EVT_SERVICES_CHANGED);
		break;
	}

	case CZapitMessages::CMD_RELOAD_CURRENTSERVICES: {
#if 0
		SendCmdReady(connfd);
#endif
		DBG("[zapit] sending EVT_SERVICES_CHANGED\n");
  	        SendEvent(CZapitClient::EVT_SERVICES_CHANGED);
		live_fe->setTsidOnid(0);
		ZapIt(live_channel_id, current_is_nvod);
		//SendEvent(CZapitClient::EVT_BOUQUETS_CHANGED);
  	        break;
  	}
	case CZapitMessages::CMD_SCANSTART: {
		int scan_mode;
		CBasicServer::receive_data(connfd, &scan_mode, sizeof(scan_mode));

		if (!StartScan(scan_mode))
			SendEvent(CZapitClient::EVT_SCAN_FAILED);
		break;
	}
	case CZapitMessages::CMD_SCANSTOP:
		CServiceScan::getInstance()->Abort();
		CServiceScan::getInstance()->Stop();
		break;
#if 0
	case CZapitMessages::CMD_SETCONFIG:
		Zapit_config Cfg;
                CBasicServer::receive_data(connfd, &Cfg, sizeof(Cfg));
		SetConfig(&Cfg);
		break;
	case CZapitMessages::CMD_GETCONFIG:
		SendConfig(connfd);
		break;
#endif
	case CZapitMessages::CMD_REZAP:
		if (currentMode & RECORD_MODE)
			break;
		if(config.rezapTimeout > 0)
			sleep(config.rezapTimeout);
		if(current_channel)
			ZapIt(current_channel->getChannelID());
		break;
        case CZapitMessages::CMD_TUNE_TP: {
			CBasicServer::receive_data(connfd, &TP, sizeof(TP));
			sig_delay = 0;
			TP.feparams.inversion = INVERSION_AUTO;
			const char *name = scanProviders.size() > 0  ? scanProviders.begin()->second.c_str() : "unknown";

			switch (live_fe->getInfo()->type) {
			case FE_QPSK:
			case FE_OFDM: {
				//FIXME check scanProviders.size() !
				t_satellite_position satellitePosition = scanProviders.begin()->first;
				printf("[zapit] tune to sat %s freq %d rate %d fec %d pol %d\n", name, TP.feparams.frequency, TP.feparams.u.qpsk.symbol_rate, TP.feparams.u.qpsk.fec_inner, TP.polarization);
				live_fe->setInput(satellitePosition, TP.feparams.frequency,  TP.polarization);
				live_fe->driveToSatellitePosition(satellitePosition);
				break;
			}
			case FE_QAM:
				printf("[zapit] tune to cable %s freq %d rate %d fec %d\n", name, TP.feparams.frequency, TP.feparams.u.qam.symbol_rate, TP.feparams.u.qam.fec_inner);
				break;
			default:
				WARN("Unknown type %d", live_fe->getInfo()->type);
				return false;
			}
			live_fe->tuneFrequency(&TP.feparams, TP.polarization, true);
		}
		break;
        case CZapitMessages::CMD_SCAN_TP: {
                CBasicServer::receive_data(connfd, &TP, sizeof(TP));
		StartScanTP(&TP);
                break;
        }

	case CZapitMessages::CMD_SCANREADY: {
		CZapitMessages::responseIsScanReady msgResponseIsScanReady;
#if 0 //FIXME used only when scanning done using pzapit client, is it really needed ?
		msgResponseIsScanReady.satellite = curr_sat;
		msgResponseIsScanReady.transponder = found_transponders;
		msgResponseIsScanReady.processed_transponder = processed_transponders;
		msgResponseIsScanReady.services = found_channels;
		msgResponseIsScanReady.scanReady = !CServiceScan::getInstance()->Scanning();
#endif
		CBasicServer::send_data(connfd, &msgResponseIsScanReady, sizeof(msgResponseIsScanReady));
		break;
	}

	case CZapitMessages::CMD_SCANGETSATLIST: {
		uint32_t  satlength;
		CZapitClient::responseGetSatelliteList sat;
		satlength = sizeof(sat);

		satellite_map_t satmap = CServiceManager::getInstance()->SatelliteList();
		for(sat_iterator_t sit = satmap.begin(); sit != satmap.end(); sit++) {
			strncpy(sat.satName, sit->second.name.c_str(), 50);
			sat.satName[49] = 0;
			sat.satPosition = sit->first;
			sat.motorPosition = sit->second.motor_position;
			CBasicServer::send_data(connfd, &satlength, sizeof(satlength));
			CBasicServer::send_data(connfd, (char *)&sat, satlength);
		}
		satlength = SATNAMES_END_MARKER;
		CBasicServer::send_data(connfd, &satlength, sizeof(satlength));
		break;
	}

	case CZapitMessages::CMD_SCANSETSCANSATLIST: {
		bool setfe = true;
		CZapitClient::commandSetScanSatelliteList sat;
		scanProviders.clear();
		while (CBasicServer::receive_data(connfd, &sat, sizeof(sat))) {
			printf("[zapit] adding to scan %s (position %d)\n", sat.satName, sat.position);
			scanProviders[sat.position] = sat.satName;
			if(setfe) {
				/* for test signal */
				CServiceScan::getInstance()->SetFrontend(sat.position);
				live_fe = CServiceScan::getInstance()->GetFrontend();
				setfe = false;
			}
		}
		break;
	}
#if 0
	case CZapitMessages::CMD_SCANSETSCANMOTORPOSLIST: {
		// absolute
		ERROR("CZapitMessages::CMD_SCANSETSCANMOTORPOSLIST: depreciated command\n");
		break;
	}
#endif
	case CZapitMessages::CMD_SCANSETDISEQCTYPE: {
		//FIXME diseqcType is global
#if 0
		CBasicServer::receive_data(connfd, &diseqcType, sizeof(diseqcType));
		live_fe->setDiseqcType(diseqcType);
#endif
		ERROR("CZapitMessages::CMD_SCANSETDISEQCTYPE: depreciated command\n");
		break;
	}

	case CZapitMessages::CMD_SCANSETDISEQCREPEAT: {
#if 0
		uint32_t  repeats;
		CBasicServer::receive_data(connfd, &repeats, sizeof(repeats));
		live_fe->setDiseqcRepeats(repeats);
#endif
		ERROR("CZapitMessages::CMD_SCANSETDISEQCREPEAT: depreciated command\n");
		break;
	}

	case CZapitMessages::CMD_SCANSETBOUQUETMODE:
		CBasicServer::receive_data(connfd, &bouquetMode, sizeof(bouquetMode));
		break;
#if 0
        case CZapitMessages::CMD_SCANSETTYPE:
                //CBasicServer::receive_data(connfd, &scanType, sizeof(scanType));
		ERROR("CZapitMessages::CMD_SCANSETTYPE: depreciated command\n");
                break;
#endif
	case CZapitMessages::CMD_SET_EVENT_MODE: {
		CZapitMessages::commandSetRecordMode msgSetRecordMode;
		CBasicServer::receive_data(connfd, &msgSetRecordMode, sizeof(msgSetRecordMode));
		//printf("[zapit] event mode: %d\n", msgSetRecordMode.activate);fflush(stdout);
		event_mode = msgSetRecordMode.activate;
                break;
	}
	case CZapitMessages::CMD_SET_RECORD_MODE: {
		CZapitMessages::commandSetRecordMode msgSetRecordMode;
		CBasicServer::receive_data(connfd, &msgSetRecordMode, sizeof(msgSetRecordMode));
		printf("[zapit] recording mode: %d\n", msgSetRecordMode.activate);fflush(stdout);
		SetRecordMode(msgSetRecordMode.activate);
		break;
	}

	case CZapitMessages::CMD_GET_RECORD_MODE: {
		CZapitMessages::responseGetRecordModeState msgGetRecordModeState;
		msgGetRecordModeState.activated = (currentMode & RECORD_MODE);
		CBasicServer::send_data(connfd, &msgGetRecordModeState, sizeof(msgGetRecordModeState));
		break;
	}

	case CZapitMessages::CMD_SB_GET_PLAYBACK_ACTIVE: {
		/* FIXME check if needed */
		CZapitMessages::responseGetPlaybackState msgGetPlaybackState;
                msgGetPlaybackState.activated = playing;
		msgGetPlaybackState.activated = videoDecoder->getPlayState();
		CBasicServer::send_data(connfd, &msgGetPlaybackState, sizeof(msgGetPlaybackState));
		break;
	}

	case CZapitMessages::CMD_BQ_ADD_BOUQUET: {
		char * name = CBasicServer::receive_string(connfd);
		g_bouquetManager->addBouquet(name, true);
		CBasicServer::delete_string(name);
		break;
	}

	case CZapitMessages::CMD_BQ_DELETE_BOUQUET: {
		CZapitMessages::commandDeleteBouquet msgDeleteBouquet;
		CBasicServer::receive_data(connfd, &msgDeleteBouquet, sizeof(msgDeleteBouquet)); // bouquet & channel number are already starting at 0!
		g_bouquetManager->deleteBouquet(msgDeleteBouquet.bouquet);
		break;
	}

	case CZapitMessages::CMD_BQ_RENAME_BOUQUET: {
		CZapitMessages::commandRenameBouquet msgRenameBouquet;
		CBasicServer::receive_data(connfd, &msgRenameBouquet, sizeof(msgRenameBouquet)); // bouquet & channel number are already starting at 0!
		char * name = CBasicServer::receive_string(connfd);
		if (msgRenameBouquet.bouquet < g_bouquetManager->Bouquets.size()) {
			g_bouquetManager->Bouquets[msgRenameBouquet.bouquet]->Name = name;
			g_bouquetManager->Bouquets[msgRenameBouquet.bouquet]->bUser = true;
		}
		CBasicServer::delete_string(name);
		break;
	}

	case CZapitMessages::CMD_BQ_EXISTS_BOUQUET: {
		CZapitMessages::responseGeneralInteger responseInteger;

		char * name = CBasicServer::receive_string(connfd);
		responseInteger.number = g_bouquetManager->existsBouquet(name);
		CBasicServer::delete_string(name);
		CBasicServer::send_data(connfd, &responseInteger, sizeof(responseInteger)); // bouquet & channel number are already starting at 0!
		break;
	}
#if 0
	case CZapitMessages::CMD_BQ_EXISTS_CHANNEL_IN_BOUQUET: {
		CZapitMessages::commandExistsChannelInBouquet msgExistsChInBq;
		CZapitMessages::responseGeneralTrueFalse responseBool;
		CBasicServer::receive_data(connfd, &msgExistsChInBq, sizeof(msgExistsChInBq)); // bouquet & channel number are already starting at 0!
		responseBool.status = g_bouquetManager->existsChannelInBouquet(msgExistsChInBq.bouquet, msgExistsChInBq.channel_id);
		CBasicServer::send_data(connfd, &responseBool, sizeof(responseBool));
		break;
	}
#endif
	case CZapitMessages::CMD_BQ_MOVE_BOUQUET: {
		CZapitMessages::commandMoveBouquet msgMoveBouquet;
		CBasicServer::receive_data(connfd, &msgMoveBouquet, sizeof(msgMoveBouquet)); // bouquet & channel number are already starting at 0!
		g_bouquetManager->moveBouquet(msgMoveBouquet.bouquet, msgMoveBouquet.newPos);
		break;
	}

	case CZapitMessages::CMD_BQ_ADD_CHANNEL_TO_BOUQUET: {
		CZapitMessages::commandAddChannelToBouquet msgAddChannelToBouquet;
		CBasicServer::receive_data(connfd, &msgAddChannelToBouquet, sizeof(msgAddChannelToBouquet)); // bouquet & channel number are already starting at 0!
		addChannelToBouquet(msgAddChannelToBouquet.bouquet, msgAddChannelToBouquet.channel_id);
		break;
	}

	case CZapitMessages::CMD_BQ_REMOVE_CHANNEL_FROM_BOUQUET: {
		/* removeChannelFromBouquet, still used in webif */
		CZapitMessages::commandRemoveChannelFromBouquet msgRemoveChannelFromBouquet;
		CBasicServer::receive_data(connfd, &msgRemoveChannelFromBouquet, sizeof(msgRemoveChannelFromBouquet)); // bouquet & channel number are already starting at 0!
		if (msgRemoveChannelFromBouquet.bouquet < g_bouquetManager->Bouquets.size())
			g_bouquetManager->Bouquets[msgRemoveChannelFromBouquet.bouquet]->removeService(msgRemoveChannelFromBouquet.channel_id);
#if 1 // REAL_REMOVE
		bool status = 0;
		for (unsigned int i = 0; i < g_bouquetManager->Bouquets.size(); i++) {
			status = g_bouquetManager->existsChannelInBouquet(i, msgRemoveChannelFromBouquet.channel_id);
			if(status)
				break;
		}
		if(!status) {
			CServiceManager::getInstance()->RemoveChannel(msgRemoveChannelFromBouquet.channel_id);
			current_channel = 0;
			list_changed = 1;
		}
#endif
		break;
	}
#if 0
	case CZapitMessages::CMD_BQ_MOVE_CHANNEL: {
		CZapitMessages::commandMoveChannel msgMoveChannel;
		CBasicServer::receive_data(connfd, &msgMoveChannel, sizeof(msgMoveChannel)); // bouquet & channel number are already starting at 0!
		if (msgMoveChannel.bouquet < g_bouquetManager->Bouquets.size())
			g_bouquetManager->Bouquets[msgMoveChannel.bouquet]->moveService(msgMoveChannel.oldPos, msgMoveChannel.newPos,
					(((currentMode & RADIO_MODE) && msgMoveChannel.mode == CZapitClient::MODE_CURRENT)
					 || (msgMoveChannel.mode==CZapitClient::MODE_RADIO)) ? 2 : 1);
		break;
	}
#endif
	case CZapitMessages::CMD_BQ_SET_LOCKSTATE: {
		CZapitMessages::commandBouquetState msgBouquetLockState;
		CBasicServer::receive_data(connfd, &msgBouquetLockState, sizeof(msgBouquetLockState)); // bouquet & channel number are already starting at 0!
		if (msgBouquetLockState.bouquet < g_bouquetManager->Bouquets.size())
			g_bouquetManager->Bouquets[msgBouquetLockState.bouquet]->bLocked = msgBouquetLockState.state;
		break;
	}

	case CZapitMessages::CMD_BQ_SET_HIDDENSTATE: {
		CZapitMessages::commandBouquetState msgBouquetHiddenState;
		CBasicServer::receive_data(connfd, &msgBouquetHiddenState, sizeof(msgBouquetHiddenState)); // bouquet & channel number are already starting at 0!
		if (msgBouquetHiddenState.bouquet < g_bouquetManager->Bouquets.size())
			g_bouquetManager->Bouquets[msgBouquetHiddenState.bouquet]->bHidden = msgBouquetHiddenState.state;
		break;
	}

	case CZapitMessages::CMD_BQ_RENUM_CHANNELLIST:
		g_bouquetManager->renumServices();
		// 2004.08.02 g_bouquetManager->storeBouquets();
		break;

	case CZapitMessages::CMD_BQ_SAVE_BOUQUETS: {
		CZapitMessages::commandBoolean msgBoolean;
		CBasicServer::receive_data(connfd, &msgBoolean, sizeof(msgBoolean));

		SendCmdReady(connfd);
#if 0
		//if (msgBoolean.truefalse)
		if(list_changed) {
			SendEvent(CZapitClient::EVT_SERVICES_CHANGED);
		} else
			SendEvent(CZapitClient::EVT_BOUQUETS_CHANGED);
#endif
		g_bouquetManager->saveBouquets();
		g_bouquetManager->saveUBouquets();
		g_bouquetManager->renumServices();
		//SendEvent(CZapitClient::EVT_SERVICES_CHANGED);
		SendEvent(CZapitClient::EVT_BOUQUETS_CHANGED);
		if(list_changed) {
			CServiceManager::getInstance()->SaveServices(true);
			list_changed = 0;
		}
		break;
	}
#if 0
        case CZapitMessages::CMD_SET_VIDEO_SYSTEM: {
		CZapitMessages::commandInt msg;
		CBasicServer::receive_data(connfd, &msg, sizeof(msg));
		videoDecoder->SetVideoSystem(msg.val);
                break;
        }
#endif
#if 0
        case CZapitMessages::CMD_SET_NTSC: {
		videoDecoder->SetVideoSystem(8);
                break;
        }
#endif
	case CZapitMessages::CMD_SB_START_PLAYBACK:
		//playbackStopForced = false;
		StartPlayBack(current_channel);
		break;

	case CZapitMessages::CMD_SB_STOP_PLAYBACK:
		StopPlayBack(false);
		SendCmdReady(connfd);
		break;

	case CZapitMessages::CMD_SB_LOCK_PLAYBACK:
		/* hack. if standby true, dont blank video */
		standby = true;
		StopPlayBack(true);
		standby = false;
		playbackStopForced = true;
		SendCmdReady(connfd);
		break;
	case CZapitMessages::CMD_SB_UNLOCK_PLAYBACK:
		playbackStopForced = false;
		StartPlayBack(current_channel);
		SendPMT();
		SendCmdReady(connfd);
		break;
#if 0 
	case CZapitMessages::CMD_SET_DISPLAY_FORMAT: {
		CZapitMessages::commandInt msg;
		CBasicServer::receive_data(connfd, &msg, sizeof(msg));
		videoDecoder->setCroppingMode((video_displayformat_t) msg.val);
		break;
	}
#endif
	case CZapitMessages::CMD_SET_AUDIO_MODE: {
		CZapitMessages::commandInt msg;
		CBasicServer::receive_data(connfd, &msg, sizeof(msg));
		audioDecoder->setChannel((int) msg.val);
		audio_mode = msg.val;
		break;
	}
#if 0
	case CZapitMessages::CMD_GET_AUDIO_MODE: {
		CZapitMessages::commandInt msg;
		msg.val = (int) audio_mode;
		CBasicServer::send_data(connfd, &msg, sizeof(msg));
		break;
	}
#endif

	case CZapitMessages::CMD_SET_ASPECTRATIO: {
		CZapitMessages::commandInt msg;
		CBasicServer::receive_data(connfd, &msg, sizeof(msg));
		aspectratio=(int) msg.val;
		videoDecoder->setAspectRatio(aspectratio, -1);
		break;
	}

	case CZapitMessages::CMD_GET_ASPECTRATIO: {
		CZapitMessages::commandInt msg;
		aspectratio=videoDecoder->getAspectRatio();
		msg.val = aspectratio;
		CBasicServer::send_data(connfd, &msg, sizeof(msg));
		break;
	}

	case CZapitMessages::CMD_SET_MODE43: {
		CZapitMessages::commandInt msg;
		CBasicServer::receive_data(connfd, &msg, sizeof(msg));
		mode43=(int) msg.val;
		videoDecoder->setAspectRatio(-1, mode43);
		break;
	}

#if 0 
	//FIXME howto read aspect mode back?
	case CZapitMessages::CMD_GET_MODE43: {
		CZapitMessages::commandInt msg;
		mode43=videoDecoder->getCroppingMode();
		msg.val = mode43;
		CBasicServer::send_data(connfd, &msg, sizeof(msg));
		break;
	}
#endif

	case CZapitMessages::CMD_GETPIDS: {
		if (current_channel) {
			CZapitClient::responseGetOtherPIDs responseGetOtherPIDs;
			responseGetOtherPIDs.vpid = current_channel->getVideoPid();
			responseGetOtherPIDs.ecmpid = 0; // TODO: remove
			responseGetOtherPIDs.vtxtpid = current_channel->getTeletextPid();
			responseGetOtherPIDs.pmtpid = current_channel->getPmtPid();
			responseGetOtherPIDs.pcrpid = current_channel->getPcrPid();
			responseGetOtherPIDs.selected_apid = current_channel->getAudioChannelIndex();
			/*responseGetOtherPIDs.privatepid = current_channel->getPrivatePid();*/
			CBasicServer::send_data(connfd, &responseGetOtherPIDs, sizeof(responseGetOtherPIDs));
			sendAPIDs(connfd);
		}
		break;
	}
#if 0
	case CZapitMessages::CMD_GET_FE_SIGNAL: {
		CZapitClient::responseFESignal response_FEsig;
		response_FEsig.sig = live_fe->getSignalStrength();
		response_FEsig.snr = live_fe->getSignalNoiseRatio();
		response_FEsig.ber = live_fe->getBitErrorRate();
		CBasicServer::send_data(connfd, &response_FEsig, sizeof(CZapitClient::responseFESignal));
		ERROR("CZapitMessages::CMD_GET_FE_SIGNAL: depreciated command\n");
		break;
	}
#endif
	case CZapitMessages::CMD_SETSUBSERVICES: {
		CZapitClient::commandAddSubServices msgAddSubService;

		t_satellite_position satellitePosition = current_channel ? current_channel->getSatellitePosition() : 0;
		while (CBasicServer::receive_data(connfd, &msgAddSubService, sizeof(msgAddSubService))) {
#if 0
			t_original_network_id original_network_id = msgAddSubService.original_network_id;
			t_service_id          service_id          = msgAddSubService.service_id;
			t_channel_id sub_channel_id = 
				((uint64_t) ( satellitePosition >= 0 ? satellitePosition : (uint64_t)(0xF000+ abs(satellitePosition))) << 48) |
				(uint64_t) CREATE_CHANNEL_ID(msgAddSubService.service_id, msgAddSubService.original_network_id, msgAddSubService.transport_stream_id);
			DBG("NVOD insert %llx\n", sub_channel_id);
			nvodchannels.insert (
					std::pair <t_channel_id, CZapitChannel> (
						sub_channel_id,
						CZapitChannel (
							"NVOD",
							service_id,
							msgAddSubService.transport_stream_id,
							original_network_id,
							1,
							satellitePosition,
							0
							)
						)
					);
#endif
			CZapitChannel * channel = new CZapitChannel (
					"NVOD",
					msgAddSubService.service_id,
					msgAddSubService.transport_stream_id,
					msgAddSubService.original_network_id,
					ST_DIGITAL_TELEVISION_SERVICE,
					satellitePosition,
					0
					);
			CServiceManager::getInstance()->AddNVODChannel(channel);
		}

		current_is_nvod = true;
		break;
	}

	case CZapitMessages::CMD_REGISTEREVENTS:
		eventServer->registerEvent(connfd);
		break;

	case CZapitMessages::CMD_UNREGISTEREVENTS:
		eventServer->unRegisterEvent(connfd);
		break;

	case CZapitMessages::CMD_MUTE: {
		CZapitMessages::commandBoolean msgBoolean;
		CBasicServer::receive_data(connfd, &msgBoolean, sizeof(msgBoolean));
		//printf("[zapit] mute %d\n", msgBoolean.truefalse);
		if (msgBoolean.truefalse)
			audioDecoder->mute();
		else
			audioDecoder->unmute();
		break;
	}

	case CZapitMessages::CMD_SET_VOLUME: {
		CZapitMessages::commandVolume msgVolume;
		CBasicServer::receive_data(connfd, &msgVolume, sizeof(msgVolume));
		audioDecoder->setVolume(msgVolume.left, msgVolume.right);
                volume_left = msgVolume.left;
                volume_right = msgVolume.right;
		break;
	}
        case CZapitMessages::CMD_GET_VOLUME: {
                CZapitMessages::commandVolume msgVolume;
                msgVolume.left = volume_left;
                msgVolume.right = volume_right;
                CBasicServer::send_data(connfd, &msgVolume, sizeof(msgVolume));
                break;
        }
	case CZapitMessages::CMD_GET_MUTE_STATUS: {
		CZapitMessages::commandBoolean msgBoolean;
		msgBoolean.truefalse = audioDecoder->getMuteStatus();
		CBasicServer::send_data(connfd, &msgBoolean, sizeof(msgBoolean));
		break;
	}
	case CZapitMessages::CMD_SET_STANDBY: {
		CZapitMessages::commandBoolean msgBoolean;
		CBasicServer::receive_data(connfd, &msgBoolean, sizeof(msgBoolean));
		if (msgBoolean.truefalse) {
			// if(currentMode & RECORD_MODE) videoDecoder->freeze();
			enterStandby();
			SendCmdReady(connfd);
		} else
			leaveStandby();
		break;
	}
#if 0
	case CZapitMessages::CMD_NVOD_SUBSERVICE_NUM: {
		CZapitMessages::commandInt msg;
		CBasicServer::receive_data(connfd, &msg, sizeof(msg));
	}
#endif
	case CZapitMessages::CMD_SEND_MOTOR_COMMAND: {
		CZapitMessages::commandMotor msgMotor;
		CBasicServer::receive_data(connfd, &msgMotor, sizeof(msgMotor));
		printf("[zapit] received motor command: %x %x %x %x %x %x\n", msgMotor.cmdtype, msgMotor.address, msgMotor.cmd, msgMotor.num_parameters, msgMotor.param1, msgMotor.param2);
		if(msgMotor.cmdtype > 0x20)
		live_fe->sendMotorCommand(msgMotor.cmdtype, msgMotor.address, msgMotor.cmd, msgMotor.num_parameters, msgMotor.param1, msgMotor.param2);
		// TODO !!
		//else if(current_channel)
		//  live_fe->satFind(msgMotor.cmdtype, current_channel);
		break;
	}

	default:
		WARN("unknown command %d (version %d)", rmsg.cmd, CZapitMessages::ACTVERSION);
		break;
	}

	DBG("cmd %d processed\n", rmsg.cmd);

	return true;
}

void CZapit::addChannelToBouquet(const unsigned int bouquet, const t_channel_id channel_id)
{
	//DBG("addChannelToBouquet(%d, %d)\n", bouquet, channel_id);

	CZapitChannel* chan = CServiceManager::getInstance()->FindChannel(channel_id);

	if (chan != NULL)
		if (bouquet < g_bouquetManager->Bouquets.size())
			g_bouquetManager->Bouquets[bouquet]->addService(chan);
		else
			WARN("bouquet not found");
	else
		WARN("channel_id not found in channellist");
}

bool CZapit::send_data_count(int connfd, int data_count)
{
	CZapitMessages::responseGeneralInteger responseInteger;
	responseInteger.number = data_count;
	if (CBasicServer::send_data(connfd, &responseInteger, sizeof(responseInteger)) == false) {
		ERROR("could not send any return");
		return false;
	}
	return true;
}

void CZapit::sendAPIDs(int connfd)
{
	if (!send_data_count(connfd, current_channel->getAudioChannelCount()))
		return;
	for (uint32_t  i = 0; i < current_channel->getAudioChannelCount(); i++) {
		CZapitClient::responseGetAPIDs response;
		response.pid = current_channel->getAudioPid(i);
		strncpy(response.desc, current_channel->getAudioChannel(i)->description.c_str(), DESC_MAX_LEN);
		response.is_ac3 = response.is_aac = 0;
		if (current_channel->getAudioChannel(i)->audioChannelType == CZapitAudioChannel::AC3) {
			response.is_ac3 = 1;
		} else if (current_channel->getAudioChannel(i)->audioChannelType == CZapitAudioChannel::AAC) {
			response.is_aac = 1;
		}
		response.component_tag = current_channel->getAudioChannel(i)->componentTag;

		if (CBasicServer::send_data(connfd, &response, sizeof(response)) == false) {
			ERROR("could not send any return");
			return;
		}
	}
}

void CZapit::internalSendChannels(int connfd, ZapitChannelList* channels, const unsigned int first_channel_nr, bool nonames)
{
	int data_count = channels->size();
	if (!send_data_count(connfd, data_count))
		return;

	for (uint32_t  i = 0; i < channels->size();i++) {
		if(nonames) {
			CZapitClient::responseGetBouquetNChannels response;
			response.nr = first_channel_nr + i;

			if (CBasicServer::send_data(connfd, &response, sizeof(response)) == false) {
				ERROR("could not send any return");
				if (CBasicServer::send_data(connfd, &response, sizeof(response)) == false) {
					ERROR("could not send any return, stop");
					return;
				}
			}
		} else {
			CZapitClient::responseGetBouquetChannels response;
			strncpy(response.name, ((*channels)[i]->getName()).c_str(), CHANNEL_NAME_SIZE);
			response.name[CHANNEL_NAME_SIZE-1] = 0;
			//printf("internalSendChannels: name %s\n", response.name);
			response.satellitePosition = (*channels)[i]->getSatellitePosition();
			response.channel_id = (*channels)[i]->getChannelID();
			response.nr = first_channel_nr + i;

			if (CBasicServer::send_data(connfd, &response, sizeof(response)) == false) {
				ERROR("could not send any return");
				DBG("current: %d name %s total %d\n", i, response.name, data_count);
				if (CBasicServer::send_data(connfd, &response, sizeof(response)) == false) {
					ERROR("could not send any return, stop");
					return;
				}
			}
		}
	}
}

void CZapit::sendBouquets(int connfd, const bool emptyBouquetsToo, CZapitClient::channelsMode mode)
{
	CZapitClient::responseGetBouquets msgBouquet;
        int curMode;
        switch(mode) {
                case CZapitClient::MODE_TV:
                        curMode = TV_MODE;
                        break;
                case CZapitClient::MODE_RADIO:
                        curMode = RADIO_MODE;
                        break;
                case CZapitClient::MODE_CURRENT:
                default:
                        curMode = currentMode;
                        break;
        }

        for (uint32_t i = 0; i < g_bouquetManager->Bouquets.size(); i++) {
                if (emptyBouquetsToo || (!g_bouquetManager->Bouquets[i]->bHidden && g_bouquetManager->Bouquets[i]->bUser)
                    || ((!g_bouquetManager->Bouquets[i]->bHidden)
                     && (((curMode & RADIO_MODE) && !g_bouquetManager->Bouquets[i]->radioChannels.empty()) ||
                      ((curMode & TV_MODE) && !g_bouquetManager->Bouquets[i]->tvChannels.empty()))))
		{
			msgBouquet.bouquet_nr = i;
			strncpy(msgBouquet.name, g_bouquetManager->Bouquets[i]->Name.c_str(), 30);
			msgBouquet.name[29] = 0;
			msgBouquet.locked     = g_bouquetManager->Bouquets[i]->bLocked;
			msgBouquet.hidden     = g_bouquetManager->Bouquets[i]->bHidden;
			if (CBasicServer::send_data(connfd, &msgBouquet, sizeof(msgBouquet)) == false) {
				ERROR("could not send any return");
				return;
			}
		}
	}
	msgBouquet.bouquet_nr = RESPONSE_GET_BOUQUETS_END_MARKER;
	if (CBasicServer::send_data(connfd, &msgBouquet, sizeof(msgBouquet)) == false) {
		ERROR("could not send end marker");
		return;
	}
}

void CZapit::sendBouquetChannels(int connfd, const unsigned int bouquet, const CZapitClient::channelsMode mode, bool nonames)
{
	if (bouquet >= g_bouquetManager->Bouquets.size()) {
		WARN("invalid bouquet number: %d", bouquet);
		return;
	}

	if (((currentMode & RADIO_MODE) && (mode == CZapitClient::MODE_CURRENT)) || (mode == CZapitClient::MODE_RADIO))
		internalSendChannels(connfd, &(g_bouquetManager->Bouquets[bouquet]->radioChannels), g_bouquetManager->radioChannelsBegin().getNrofFirstChannelofBouquet(bouquet), nonames);
	else
		internalSendChannels(connfd, &(g_bouquetManager->Bouquets[bouquet]->tvChannels), g_bouquetManager->tvChannelsBegin().getNrofFirstChannelofBouquet(bouquet), nonames);
}

void CZapit::sendChannels(int connfd, const CZapitClient::channelsMode mode, const CZapitClient::channelsOrder order)
{
	ZapitChannelList channels;

	if (order == CZapitClient::SORT_BOUQUET) {
		CBouquetManager::ChannelIterator cit = (((currentMode & RADIO_MODE) && (mode == CZapitClient::MODE_CURRENT)) || (mode==CZapitClient::MODE_RADIO)) ? g_bouquetManager->radioChannelsBegin() : g_bouquetManager->tvChannelsBegin();
		for (; !(cit.EndOfChannels()); cit++)
			channels.push_back(*cit);
	}
	else if (order == CZapitClient::SORT_ALPHA)
	{
		// ATTENTION: in this case response.nr does not return the actual number of the channel for zapping!
		if (((currentMode & RADIO_MODE) && (mode == CZapitClient::MODE_CURRENT)) || (mode == CZapitClient::MODE_RADIO)) {
			CServiceManager::getInstance()->GetAllRadioChannels(channels);
		} else {
			CServiceManager::getInstance()->GetAllTvChannels(channels);
		}
		sort(channels.begin(), channels.end(), CmpChannelByChName());
	}

	internalSendChannels(connfd, &channels, 0, false);
}

bool CZapit::StartPlayBack(CZapitChannel *thisChannel)
{
	bool have_pcr = false;
	bool have_audio = false;
	bool have_video = false;
	bool have_teletext = false;

	INFO("standby %d playing %d forced %d", standby, playing, playbackStopForced);
	if(!thisChannel)
		thisChannel = current_channel;

	if ((playbackStopForced == true) || (!thisChannel) || playing)
		return false;

	if(standby) {
		CFEManager::getInstance()->Open();
		return true;
	}

	printf("[zapit] vpid %X apid %X pcr %X\n", thisChannel->getVideoPid(), thisChannel->getAudioPid(), thisChannel->getPcrPid());
	if (thisChannel->getPcrPid() != 0)
		have_pcr = true;
	if (thisChannel->getAudioPid() != 0)
		have_audio = true;
	if ((thisChannel->getVideoPid() != 0) && (currentMode & TV_MODE))
		have_video = true;
	if (thisChannel->getTeletextPid() != 0)
		have_teletext = true;

	if ((!have_audio) && (!have_video) && (!have_teletext))
		return false;
#if 1
	if(have_video && (thisChannel->getPcrPid() == 0x1FFF)) { //FIXME
		thisChannel->setPcrPid(thisChannel->getVideoPid());
		have_pcr = true;
	}
#endif
	/* set demux filters */
	videoDecoder->SetStreamType((VIDEO_FORMAT)thisChannel->type);
//	videoDecoder->SetSync(VIDEO_PLAY_MOTION);

	if (have_pcr) {
		pcrDemux->pesFilter(thisChannel->getPcrPid());
	}
	if (have_audio) {
		audioDemux->pesFilter(thisChannel->getAudioPid());
	}
	if (have_video) {
		videoDemux->pesFilter(thisChannel->getVideoPid());
	}
//	audioDecoder->SetSyncMode(AVSYNC_ENABLED);

#if 0 //FIXME hack ?
	if(thisChannel->getServiceType() == ST_DIGITAL_RADIO_SOUND_SERVICE) {
		audioDecoder->SetSyncMode(AVSYNC_AUDIO_IS_MASTER);
		have_pcr = false;
	}
#endif
	if (have_pcr) {
		printf("[zapit] starting PCR 0x%X\n", thisChannel->getPcrPid());
		pcrDemux->Start();
	}

	/* select audio output and start audio */
	if (have_audio) {
		SetAudioStreamType(thisChannel->getAudioChannel()->audioChannelType);
		audioDemux->Start();
		audioDecoder->Start();
	}

	/* start video */
	if (have_video) {
		videoDecoder->Start(0, thisChannel->getPcrPid(), thisChannel->getVideoPid());
		videoDemux->Start();
	}
#ifdef USE_VBI
	if(have_teletext)
		videoDecoder->StartVBI(thisChannel->getTeletextPid());
#endif
	playing = true;

	return true;
}

bool CZapit::StopPlayBack(bool send_pmt)
{
#if 1
	if(send_pmt) {
		CCamManager::getInstance()->Stop(live_channel_id, CCamManager::PLAY);
		ca->SendPMT(0, (unsigned char*) "", 0, CA_SLOT_TYPE_SMARTCARD);
	}
#endif
#if 0
	if (send_pmt && !(currentMode & RECORD_MODE)) 
		ca->SendPMT(0, (unsigned char*) "", 0, CA_SLOT_TYPE_SMARTCARD);
	CCamManager::getInstance()->Stop(live_channel_id, CCamManager::PLAY);
#endif
	INFO("standby %d playing %d forced %d", standby, playing, playbackStopForced);

	if (!playing)
		return true;

	if (playbackStopForced)
		return false;

	videoDemux->Stop();
	audioDemux->Stop();
	pcrDemux->Stop();
	audioDecoder->Stop();

	/* hack. if standby, dont blank video -> for paused timeshift */
	videoDecoder->Stop(standby ? false : true);
#ifdef USE_VBI
	videoDecoder->StopVBI();
#endif
	playing = false;

	tuxtx_stop_subtitle();
	if(standby)
		dvbsub_pause();
	else
		dvbsub_stop();

	return true;
}

void CZapit::enterStandby(void)
{
	INFO("standby %d recording %d", standby, (currentMode & RECORD_MODE));
	if (standby)
		return;

	standby = true;

	SaveSettings(true);
	SaveAudioMap();
	StopPlayBack(true);

	if(!(currentMode & RECORD_MODE)) {
		pmt_stop_update_filter(&pmt_update_fd);
		CFEManager::getInstance()->Close();
	}
}

void CZapit::leaveStandby(void)
{
	INFO("standby %d recording %d", standby, (currentMode & RECORD_MODE));
	if(!standby)
		return;

	if(!(currentMode & RECORD_MODE)) {
		CFEManager::getInstance()->Open();
	}
	standby = false;
	if (current_channel)
		/* tune channel, with stopped playback to not bypass the parental PIN check */
		ZapIt(live_channel_id, false, false);
}

unsigned CZapit::ZapTo(const unsigned int bouquet, const unsigned int pchannel)
{
	if (bouquet >= g_bouquetManager->Bouquets.size()) {
		WARN("Invalid bouquet %d", bouquet);
		return CZapitClient::ZAP_INVALID_PARAM;
	}

	ZapitChannelList *channels;

	if (currentMode & RADIO_MODE)
		channels = &(g_bouquetManager->Bouquets[bouquet]->radioChannels);
	else
		channels = &(g_bouquetManager->Bouquets[bouquet]->tvChannels);

	if (pchannel >= channels->size()) {
		WARN("Invalid channel %d in bouquet %d", pchannel, bouquet);
		return CZapitClient::ZAP_INVALID_PARAM;
	}

	return ZapTo((*channels)[pchannel]->getChannelID(), false);
}

unsigned int CZapit::ZapTo(t_channel_id channel_id, bool isSubService)
{
	unsigned int result = 0;

	if (!ZapIt(channel_id)) {
		DBG("[zapit] zapit failed, chid %llx\n", channel_id);
		SendEvent((isSubService ? CZapitClient::EVT_ZAP_SUB_FAILED : CZapitClient::EVT_ZAP_FAILED), &channel_id, sizeof(channel_id));
		return result;
	}

	result |= CZapitClient::ZAP_OK;

	DBG("[zapit] zapit OK, chid %llx\n", channel_id);
	if (isSubService) {
		DBG("[zapit] isSubService chid %llx\n", channel_id);
		SendEvent(CZapitClient::EVT_ZAP_SUB_COMPLETE, &channel_id, sizeof(channel_id));
	}
	else if (current_is_nvod) {
		DBG("[zapit] NVOD chid %llx\n", channel_id);
		SendEvent(CZapitClient::EVT_ZAP_COMPLETE_IS_NVOD, &channel_id, sizeof(channel_id));
		result |= CZapitClient::ZAP_IS_NVOD;
	}
	else
		SendEvent(CZapitClient::EVT_ZAP_COMPLETE, &channel_id, sizeof(channel_id));

	return result;
}

unsigned CZapit::ZapTo(const unsigned int pchannel)
{
	CBouquetManager::ChannelIterator cit = ((currentMode & RADIO_MODE) ? g_bouquetManager->radioChannelsBegin() : g_bouquetManager->tvChannelsBegin()).FindChannelNr(pchannel);
	if (!(cit.EndOfChannels()))
		return ZapTo((*cit)->getChannelID(), false);
	else
		return 0;
}

bool CZapit::Start(Z_start_arg *ZapStart_arg)
{
	if(started)
		return false;

	CFEManager::getInstance()->Init();
	live_fe = CFEManager::getInstance()->getFE(0);
	/* load configuration or set defaults if no configuration file exists */
	video_mode = ZapStart_arg->video_mode;

	videoDemux = new cDemux();
	videoDemux->Open(DMX_VIDEO_CHANNEL);

	pcrDemux = new cDemux();
	pcrDemux->Open(DMX_PCR_ONLY_CHANNEL, videoDemux->getBuffer());

	audioDemux = new cDemux();
	audioDemux->Open(DMX_AUDIO_CHANNEL);

	videoDecoder = new cVideo(video_mode, videoDemux->getChannel(), videoDemux->getBuffer());
	videoDecoder->Standby(false);

	audioDecoder = new cAudio(audioDemux->getBuffer(), videoDecoder->GetTVEnc(), NULL /*videoDecoder->GetTVEncSD()*/);
	videoDecoder->SetAudioHandle(audioDecoder->GetHandle());

#ifdef USE_VBI
	videoDecoder->OpenVBI(1);
#endif
	ca = cCA::GetInstance();

	//LoadSettings();
	//LoadAudioMap();

	/* create bouquet manager */
	g_bouquetManager = new CBouquetManager();
	if (!PrepareChannels())
		WARN("error parsing services");
	else
		INFO("channels have been loaded succesfully");

	/* FIXME to transit from old satconfig.conf to new frontend.conf,
	 * ConfigFrontend called after PrepareChannels, as it copy satellitePositions to every fe */ 
	LoadSettings();
	ConfigFrontend();

	if(!CFEManager::getInstance()->configExist())
		CFEManager::getInstance()->saveSettings(true);

	if (configfile.getInt32("lastChannelMode", 0))
		SetRadioMode();
	else
		SetTVMode();

	if(ZapStart_arg->uselastchannel == 0){
		live_channel_id = (currentMode & RADIO_MODE) ? ZapStart_arg->startchannelradio_id : ZapStart_arg->startchanneltv_id ;
		lastChannelRadio = ZapStart_arg->startchannelradio_id;
		lastChannelTV    = ZapStart_arg->startchanneltv_id;
	}

	/* CA_INIT_CI or CA_INIT_SC or CA_INIT_BOTH */
	switch(config.cam_ci){
	  case 2:
		ca->SetInitMask(CA_INIT_BOTH);
	    break;
	  case 1:
		ca->SetInitMask(CA_INIT_CI);
	    break;
	  case 0:
		ca->SetInitMask(CA_INIT_SC);
	    break;
	  default:
		ca->SetInitMask(CA_INIT_BOTH);
	  break;
	}

	// set ci clock to ZapStart_arg->ci_clock
	ca->SetTSClock(ZapStart_arg->ci_clock * 1000000);
	ca->Start();

	eventServer = new CEventServer;
	if (!zapit_server.prepare(ZAPIT_UDS_NAME)) {
		perror(ZAPIT_UDS_NAME);
		return false;
	}

	current_channel = CServiceManager::getInstance()->FindChannel(live_channel_id);

	// some older? hw needs this sleep. e.g. my hd-1c.
	// if sleep is not set -> blackscreen after boot.
	// sleep(1) is ok here. (striper)
#if 0
	sleep(1);
	leaveStandby();
#endif
	started = true;
	int ret = start();
	return (ret == 0);
}

bool CZapit::Stop()
{
	if(!started)
		return false;
	started = false;
	int ret = join();
	return (ret == 0);
}

static bool zapit_parse_command(CBasicMessage::Header &rmsg, int connfd)
{
	return CZapit::getInstance()->ParseCommand(rmsg, connfd);
}

void CZapit::run()
{
#if 0
	time_t stime = time(0);
	time_t curtime;
#endif
	printf("[zapit] starting... tid %ld\n", syscall(__NR_gettid));

	abort_zapit = 0;
#if 0
	CBasicServer zapit_server;

	if (!zapit_server.prepare(ZAPIT_UDS_NAME)) {
		perror(ZAPIT_UDS_NAME);
		return;
	}
#endif
	SdtMonitor.Start();
	while (started && zapit_server.run(zapit_parse_command, CZapitMessages::ACTVERSION, true))
	{
		if (pmt_update_fd != -1) {
			unsigned char buf[4096];
			int ret = pmtDemux->Read(buf, 4095, 10);
			if (ret > 0) {
				pmt_stop_update_filter(&pmt_update_fd);
				printf("[zapit] pmt updated, sid 0x%x new version 0x%x\n", (buf[3] << 8) + buf[4], (buf[5] >> 1) & 0x1F);
				if(current_channel) {
					t_channel_id channel_id = current_channel->getChannelID();
					int vpid = current_channel->getVideoPid();
					int apid = current_channel->getAudioPid();
					CPmt pmt;
					pmt.Parse(current_channel);
					bool apid_found = false;
					/* check if selected audio pid still present */
					for (int i = 0; i <  current_channel->getAudioChannelCount(); i++) {
						if (current_channel->getAudioChannel(i)->pid == apid) {
							apid_found = true;
							break;
						}
					}
					
					if(!apid_found || vpid != current_channel->getVideoPid()) {
						ZapIt(current_channel->getChannelID(), true);
					} else {
						SendPMT(true);
						pmt_set_update_filter(current_channel, &pmt_update_fd);
					}
					SendEvent(CZapitClient::EVT_PMT_CHANGED, &channel_id, sizeof(channel_id));
				}
			}
		}
		/* yuck, don't waste that much cpu time :) */
		usleep(0);
#if 0
		if(!standby && !CServiceScan::getInstance()->Scanning() &&current_channel) {
			curtime = time(0);
			if(sig_delay && (curtime - stime) > sig_delay) {
				stime = curtime;
				uint16_t sig  = live_fe->getSignalStrength();
				//if(sig < 8000)
				if(sig < 28000) {
					printf("[monitor] signal %d, trying to re-tune...\n", sig);
					live_fe->retuneChannel();
				}
			}
		}
#endif
	}

	SaveChannelPids(current_channel);
	SaveSettings(true);
	SaveAudioMap();
	StopPlayBack(true);
	CFEManager::getInstance()->saveSettings(true);

	//CServiceManager::getInstance()->SaveMotorPositions();

	SdtMonitor.Stop();
	INFO("shutdown started");
#ifdef USE_VBI
	videoDecoder->CloseVBI();
#endif
	delete pcrDemux;
	delete pmtDemux;
	delete audioDecoder;
	delete audioDemux;

	INFO("demuxes/decoders deleted");

	delete CFEManager::getInstance();
	INFO("frontend(s) deleted");
	if (ca) {
		INFO("stopping CA");
		ca->Stop();
		delete ca;
	}
#ifdef EXIT_CLEANUP
	INFO("cleanup...");
	delete eventServer;
	delete g_bouquetManager;
	delete CServiceManager::getInstance();
	delete CServiceScan::getInstance();
#endif
	INFO("shutdown complete");
	return;
}

void CZapit::SetConfig(Zapit_config * Cfg)
{
	printf("[zapit] %s...\n", __FUNCTION__);

	config = *Cfg;

	SaveSettings(true);
	ConfigFrontend();
}

void CZapit::SendConfig(int connfd)
{
	printf("[zapit] %s...\n", __FUNCTION__);
	CBasicServer::send_data(connfd, &config, sizeof(config));
}

void CZapit::GetConfig(Zapit_config &Cfg)
{
	printf("[zapit] %s...\n", __FUNCTION__);
	Cfg = config;
}

CZapitSdtMonitor::CZapitSdtMonitor()
{
	sdt_wakeup = false;
	started = false;
}

CZapitSdtMonitor::~CZapitSdtMonitor()
{
	Stop();
}

void CZapitSdtMonitor::Wakeup()
{
	sdt_wakeup = true;
}

bool CZapitSdtMonitor::Start()
{
	started = true;
	int ret = start();
	return (ret == 0);
}

bool CZapitSdtMonitor::Stop()
{
	started = false;
	int ret = join();
	return (ret == 0);
}

void CZapitSdtMonitor::run()
{
	time_t /*tstart,*/ tcur, wtime = 0;
	t_transport_stream_id           transport_stream_id = 0;
	t_original_network_id           original_network_id = 0;
	t_satellite_position            satellitePosition = 0;
	freq_id_t                       freq = 0;
	transponder_id_t 		tpid = 0;

	tcur = time(0);
	//tstart = time(0);
	sdt_tp.clear();
	printf("[zapit] sdt monitor started\n");
	while(started) {
		sleep(1);
		if(sdt_wakeup) {
			sdt_wakeup = 0;
			CZapitChannel * channel = CZapit::getInstance()->GetCurrentChannel();
			if(channel) {
				wtime = time(0);
				transport_stream_id = channel->getTransportStreamId();
				original_network_id = channel->getOriginalNetworkId();
				satellitePosition = channel->getSatellitePosition();
				freq = channel->getFreqId();
				tpid = channel->getTransponderId();
			}
		}
		if(!CZapit::getInstance()->scanSDT())
			continue;

		tcur = time(0);
		if(wtime && ((tcur - wtime) > 2) && !sdt_wakeup) {
			printf("[sdt monitor] wakeup...\n");
			wtime = 0;

			if(CServiceScan::getInstance()->Scanning() || CZapit::getInstance()->Recording())
				continue;

			transponder_list_t::iterator tI = transponders.find(tpid);
			if(tI == transponders.end()) {
				printf("[sdt monitor] tp not found ?!\n");
				continue;
			}
			sdt_tp_map_t::iterator stI = sdt_tp.find(tpid);

			if ((stI != sdt_tp.end()) && ((time_monotonic() - stI->second) < 3600)) {
				printf("[sdt monitor] TP already updated less than an hour ago.\n");
				continue;
			}

			CServiceManager::getInstance()->RemoveCurrentChannels();

#if 0
			int ret = parse_current_sdt(transport_stream_id, original_network_id, satellitePosition, freq);
			if(ret) {
				if(ret == -1)
					printf("[sdt monitor] scanSDT broken ?\n");
				continue;
			}
#endif
			CSdt sdt(satellitePosition, freq, true);
			if(!sdt.Parse(transport_stream_id, original_network_id)) {
				continue;
			}
			sdt_tp.insert(std::pair <transponder_id_t, time_t> (tpid, time_monotonic()));

			bool updated = CServiceManager::getInstance()->SaveCurrentServices(tpid);
			CServiceManager::getInstance()->CopyCurrentServices(tpid);

			if(updated && (CZapit::getInstance()->scanSDT() == 1))
				CZapit::getInstance()->SendEvent(CZapitClient::EVT_SDT_CHANGED);
			if(!updated)
				printf("[sdt monitor] no changes.\n");
			else
				printf("[sdt monitor] found changes.\n");
		}
	}
	return;
}
