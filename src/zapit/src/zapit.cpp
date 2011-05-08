/*
 * $Id: zapit.cpp,v 1.290.2.50 2003/06/13 10:53:15 digi_casi Exp $
 *
 * zapit - d-box2 linux project
 *
 * (C) 2001, 2002 by Philipp Leusmann <faralla@berlios.de>
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
#include <csignal>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <syscall.h>

#include <pthread.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* tuxbox headers */
#include <configfile.h>
#include <connection/basicserver.h>

/* zapit headers */
#include <zapit/cam.h>
#include <zapit/client/msgtypes.h>
#include <zapit/debug.h>
#include <zapit/getservices.h>
#include <zapit/pat.h>
#include <zapit/pmt.h>
#include <zapit/scan.h>
#include <zapit/fastscan.h>
#include <zapit/settings.h>
#include <zapit/zapit.h>
#include <xmlinterface.h>

#include <ca_cs.h>

#include <zapit/satconfig.h>
#include <zapit/frontend_c.h>
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
int zapit_ready;
int abort_zapit;

extern void send_ca_id(int);
cCA *ca = NULL;
//cDvbCiSlot *one, *two;
extern cDemux * pmtDemux;

#define AUDIO_CONFIG_FILE "/var/tuxbox/config/zapit/audio.conf"
map<t_channel_id, audio_map_set_t> audio_map;
map<t_channel_id, audio_map_set_t>::iterator audio_map_it;
unsigned int volume_left = 0, volume_right = 0;
unsigned int def_volume_left = 0, def_volume_right = 0;
int audio_mode = 0;
int aspectratio=0;
int mode43=0;
int def_audio_mode = 0;
t_channel_id live_channel_id;
t_channel_id rec_channel_id;
int rezapTimeout;
int feTimeout = 40;
bool fastZap;
bool sortNames;
bool mcemode = false;
bool sortlist = false;
int scan_pids = false;
bool highVoltage = false;
bool voltageOff = false;
bool event_mode = true;
bool firstzap = true;
bool playing = false;
char pipzap = 0;
bool g_list_changed = false; // flag to indicate, allchans was changed
int sig_delay = 2; // seconds between signal check

/* variables for EN 50494 (a.k.a Unicable) */
int uni_scr = -1;	/* the unicable SCR address,     -1 == no unicable */
int uni_qrg = 0;	/* the unicable frequency in MHz, 0 == from spec */
int cam_ci = 2; //  CA_INIT_SC 0 or CA_INIT_CI 1 or CA_INIT_BOTH 2

double gotoXXLatitude, gotoXXLongitude;
int gotoXXLaDirection, gotoXXLoDirection, useGotoXX;
int scanSDT;
int repeatUsals;

int change_audio_pid(uint8_t index);
// SDT
void * monitor_thread(void * arg);
void * sdt_thread(void * arg);
void SaveServices(bool tocopy);
pthread_t tmon, tsdt;
pthread_mutex_t chan_mutex = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
bool sdt_wakeup;

/* the conditional access module */
CCam *cam0 = NULL;
CCam *cam1 = NULL;
/* the configuration file */
CConfigFile config(',', false);
/* the event server */
CEventServer *eventServer = NULL;
/* the dvb frontend device */
CFrontend *frontend = NULL;
/* the current channel */
CZapitChannel *g_current_channel = NULL;
/* the transponder scan xml input */
xmlDocPtr scanInputParser = NULL;
/* the bouquet manager */
CBouquetManager *g_bouquetManager = NULL;

extern cVideo *videoDecoder;
extern cAudio *audioDecoder;
extern cDemux *audioDemux;
extern cDemux *videoDemux;

cDemux *pcrDemux = NULL;

/* the map which stores the wanted cable/satellites */
scan_list_t scanProviders;

/* current zapit mode */
enum {
	TV_MODE = 0x01,
	RADIO_MODE = 0x02,
	RECORD_MODE = 0x04
};

int currentMode;
bool playbackStopForced = false;
int zapit_debug = 0;
int waitForMotor = 0;
int motorRotationSpeed = 0; //in 0.1 degrees per second
diseqc_t diseqcType;

/* near video on demand */
tallchans nvodchannels;         //  tallchans defined in "bouquets.h"
//std::string nvodname;
bool current_is_nvod = false;

/* list of all channels (services) */
tallchans allchans;             //  tallchans defined in "bouquets.h"
tallchans curchans;             //  tallchans defined in "bouquets.h"

/* transponder scan */
transponder_list_t transponders;
pthread_t scan_thread;
extern int found_transponders;
extern int processed_transponders;
extern int found_channels;
extern short curr_sat;
extern short scan_runs;
extern short abort_scan;

CZapitClient::bouquetMode bouquetMode = CZapitClient::BM_UPDATEBOUQUETS;
CZapitClient::scanType scanType = CZapitClient::ST_TVRADIO;

void scan_clean();
bool standby = true;
void * scan_transponder(void * arg);
void mergeServices();
static TP_params TP;

uint32_t  lastChannelRadio;
uint32_t  lastChannelTV;
void setZapitConfig(Zapit_config * Cfg);
void sendConfig(int connfd);

void saveZapitSettings(bool write, bool write_a)
{
	if (g_current_channel) {
		// now save the lowest channel number with the current channel_id
		int c = ((currentMode & RADIO_MODE) ? g_bouquetManager->radioChannelsBegin() : g_bouquetManager->tvChannelsBegin()).getLowestChannelNumberWithChannelID(g_current_channel->getChannelID());
//printf("LAST CHAN %d !!!!!!!!!!!\n\n\n", c);
		if (c >= 0) {
			if ((currentMode & RADIO_MODE))
				lastChannelRadio = c;
			else
				lastChannelTV = c;
		}
	}

	if (write) {
		if (config.getBool("saveLastChannel", true)) {
			config.setInt32("lastChannelMode", (currentMode & RADIO_MODE) ? 1 : 0);
			config.setInt32("lastChannelRadio", lastChannelRadio);
			config.setInt32("lastChannelTV", lastChannelTV);
			config.setInt64("lastChannel", live_channel_id);
		}
		config.setInt32("lastSatellitePosition", frontend->getCurrentSatellitePosition());
		config.setInt32("diseqcRepeats", frontend->getDiseqcRepeats());
		config.setInt32("diseqcType", frontend->getDiseqcType());
		config.setInt32("feTimeout", feTimeout);

		config.setInt32("rezapTimeout", rezapTimeout);
		config.setBool("fastZap", fastZap);
		config.setBool("sortNames", sortNames);
		config.setBool("scanPids", scan_pids);
		config.setBool("highVoltage", highVoltage);

		char tempd[12];
		sprintf(tempd, "%3.6f", gotoXXLatitude);
		config.setString("gotoXXLatitude", tempd);
		sprintf(tempd, "%3.6f", gotoXXLongitude);
		config.setString("gotoXXLongitude", tempd);
		config.setInt32("gotoXXLaDirection", gotoXXLaDirection);
		config.setInt32("gotoXXLoDirection", gotoXXLoDirection);
		//config.setInt32("useGotoXX", useGotoXX);
		config.setInt32("repeatUsals", repeatUsals);

		config.setInt32("uni_scr", uni_scr);
		config.setInt32("uni_qrg", uni_qrg);

		config.setInt32("cam_ci", cam_ci);

		config.setInt32("scanSDT", scanSDT);
		if (config.getModifiedFlag())
			config.saveConfig(CONFIGFILE);

	}
        if (write_a) {
                FILE *audio_config_file = fopen(AUDIO_CONFIG_FILE, "w");
                if (audio_config_file) {
                  for (audio_map_it = audio_map.begin(); audio_map_it != audio_map.end(); audio_map_it++) {
                        fprintf(audio_config_file, "%llx %d %d %d %d %d %d\n", (uint64_t) audio_map_it->first,
                                (int) audio_map_it->second.apid, (int) audio_map_it->second.mode, (int) audio_map_it->second.volume, 
				(int) audio_map_it->second.subpid, (int) audio_map_it->second.ttxpid, (int) audio_map_it->second.ttxpage);
                  }
		  fdatasync(fileno(audio_config_file));
                  fclose(audio_config_file);
                }
        }
}

void load_audio_map()
{
        FILE *audio_config_file = fopen(AUDIO_CONFIG_FILE, "r");
	audio_map.clear();
        if (audio_config_file) {
          t_channel_id chan;
          int apid = 0, subpid = 0, ttxpid = 0, ttxpage = 0;
          int mode = 0;
          int volume = 0;
          char s[1000];
          while (fgets(s, 1000, audio_config_file)) {
            sscanf(s, "%llx %d %d %d %d %d %d", &chan, &apid, &mode, &volume, &subpid, &ttxpid, &ttxpage);
//printf("**** Old channelinfo: %llx %d\n", chan, apid);
            audio_map[chan].apid = apid;
            audio_map[chan].subpid = subpid;
            audio_map[chan].mode = mode;
            audio_map[chan].volume = volume;
	    audio_map[chan].ttxpid = ttxpid;
	    audio_map[chan].ttxpage = ttxpage;
          }
          fclose(audio_config_file);
        }
}

void loadZapitSettings()
{
	if (!config.loadConfig(CONFIGFILE))
		WARN("%s not found", CONFIGFILE);

	live_channel_id = config.getInt64("lastChannel", 0);
	lastChannelRadio = config.getInt32("lastChannelRadio", 0);
	lastChannelTV    = config.getInt32("lastChannelTV", 0);
	rezapTimeout = config.getInt32("rezapTimeout", 1);
	feTimeout        = config.getInt32("feTimeout", 40);
	fastZap = config.getBool("fastZap", 1);
	sortNames = config.getBool("sortNames", 0);
	sortlist = sortNames;
	scan_pids = config.getBool("scanPids", 0);

	highVoltage = config.getBool("highVoltage", 0);
	voltageOff = config.getBool("voltageOff", 0);

	useGotoXX = config.getInt32("useGotoXX", 0);
	gotoXXLatitude = strtod(config.getString("gotoXXLatitude", "0.0").c_str(), NULL);
	gotoXXLongitude = strtod(config.getString("gotoXXLongitude", "0.0").c_str(), NULL);
	gotoXXLaDirection = config.getInt32("gotoXXLaDirection", 1);
	gotoXXLoDirection = config.getInt32("gotoXXLoDirection", 0);
	repeatUsals = config.getInt32("repeatUsals", 0);

	scanSDT = config.getInt32("scanSDT", 0);

	uni_scr = config.getInt32("uni_scr", -1);
	uni_qrg = config.getInt32("uni_qrg", 0);

	cam_ci = config.getInt32("cam_ci", 2);

	diseqcType = (diseqc_t)config.getInt32("diseqcType", NO_DISEQC);
	motorRotationSpeed = config.getInt32("motorRotationSpeed", 18); // default: 1.8 degrees per second

	frontend->setDiseqcRepeats(config.getInt32("diseqcRepeats", 0));
	frontend->setCurrentSatellitePosition(config.getInt32("lastSatellitePosition", 0));
	frontend->setDiseqcType(diseqcType);

	printf("[zapit.cpp] diseqc type = %d\n", diseqcType);
	load_audio_map();
}

CZapitClient::responseGetLastChannel load_settings(void)
{
	CZapitClient::responseGetLastChannel lastchannel;

	//if (config.getInt32("lastChannelMode", 0))
	if (currentMode & RADIO_MODE)
		lastchannel.mode = 'r';
	else
		lastchannel.mode = 't';

	lastchannel.channelNumber = (currentMode & RADIO_MODE) ? lastChannelRadio : lastChannelTV;
	//lastchannel.channelNumber = config.getInt32((currentMode & RADIO_MODE) ? "lastChannelRadio" : "lastChannelTV", 0);
//printf("GET LAST CHAN:: %d !!!!!!!\n\n\n", lastchannel.channelNumber);
	return lastchannel;
}

/* currently, recording always starts after zap to channel.
 * if we not in record mode, we just send new pmt over cam0,
 * if mode is recording, we zapping from or back to recording channel.
 * if we zap from, we start cam1  for new live and update cam0 with camask for rec.
 * if to recording channel, we must stop cam1 and update cam0 with live+rec camask.
 */

void start_camd(bool forupdate = false)
{
	if(!g_current_channel)
		return;

	if(currentMode & RECORD_MODE) {
		if(rec_channel_id != live_channel_id) {
			/* zap from rec. channel */
			cam1->setCaPmt(g_current_channel->getCaPmt(), DEMUX_SOURCE_0, DEMUX_DECODE_0 /*1*/); // demux 0
		} else if(forupdate) { //FIXME broken!
#if 0 // moved to stream2file.cpp
			/* forupdate means pmt update  for live channel, using old camask. */
			cam0->setCaPmt(g_current_channel->getCaPmt(), DEMUX_SOURCE_0, cam0->getCaMask() | DEMUX_DECODE_0 | DEMUX_DECODE_2, true);// update
#endif
		} else {
			/* zap back to rec. channel */
			cam0->setCaPmt(g_current_channel->getCaPmt(), DEMUX_SOURCE_0, cam0->getCaMask() | DEMUX_DECODE_0 | DEMUX_DECODE_2 /*5*/, true); // update
			cam1->sendMessage(0,0); // stop/close
		}
	} else {
		cam0->setCaPmt(g_current_channel->getCaPmt(), DEMUX_SOURCE_0, cam0->getCaMask() | DEMUX_DECODE_0 /*1*/, forupdate);
	}
	int len;
	unsigned char * pmt = g_current_channel->getRawPmt(len);
	ca->SendPMT(DEMUX_SOURCE_0, pmt, len);
}

static int pmt_update_fd = -1;
static bool update_pmt = true;

int zapit(const t_channel_id channel_id, bool in_nvod, bool forupdate = 0, bool startplayback = true)
{
	bool transponder_change = false;
	tallchans_iterator cit;
	bool failed = false;

DBG("[zapit] zapto channel id %llx diseqcType %d nvod %d\n", channel_id, diseqcType, in_nvod);

	sig_delay = 2;
        if (!firstzap && g_current_channel) {
                //DBG("*** Remembering apid = %d for channel = %llx\n",  channel->getAudioPid(), channel->getChannelID());
printf("[zapit] saving channel, apid %x sub pid %x mode %d volume %d\n", g_current_channel->getAudioPid(), dvbsub_getpid(), audio_mode, volume_right);
                audio_map[g_current_channel->getChannelID()].apid = g_current_channel->getAudioPid();
                audio_map[g_current_channel->getChannelID()].mode = audio_mode;
                audio_map[g_current_channel->getChannelID()].volume = audioDecoder->getVolume();
                audio_map[g_current_channel->getChannelID()].subpid = dvbsub_getpid();
		tuxtx_subtitle_running(&audio_map[g_current_channel->getChannelID()].ttxpid, &audio_map[g_current_channel->getChannelID()].ttxpage, NULL);
        }
	/* firstzap right now does nothing but control saving the audio channel */
	firstzap = false;

	if (in_nvod) {
		current_is_nvod = true;

		cit = nvodchannels.find(channel_id);

		if (cit == nvodchannels.end()) {
			DBG("channel_id " PRINTF_CHANNEL_ID_TYPE " not found", channel_id);
			return -1;
		}
	} else {
		current_is_nvod = false;

		cit = allchans.find(channel_id);

		if (currentMode & RADIO_MODE) {
			if ((cit == allchans.end()) || (cit->second.getServiceType() != ST_DIGITAL_RADIO_SOUND_SERVICE)) {
				DBG("channel_id " PRINTF_CHANNEL_ID_TYPE " not found", channel_id);
				return -1;
			}
		} else {
			if (cit == allchans.end() || (cit->second.getServiceType() == ST_DIGITAL_RADIO_SOUND_SERVICE)) {
				DBG("channel_id " PRINTF_CHANNEL_ID_TYPE " not found", channel_id);
				//return -1;
				// in case neutrino zap without nvod=true
				cit = nvodchannels.find(channel_id);
				if (cit == nvodchannels.end()) {
					DBG("channel_id " PRINTF_CHANNEL_ID_TYPE " AS NVOD not found", channel_id);
					return -1;
				}
				current_is_nvod = true;
			}
		}
	}

	pmt_stop_update_filter(&pmt_update_fd);

	ca->SendPMT(0, (unsigned char*) "", 0, CA_SLOT_TYPE_CI);
	stopPlayBack(!forupdate);

	if(!forupdate && g_current_channel && g_current_channel->getCaPmt()) {
		g_current_channel->resetPids();
		g_current_channel->setCaPmt(NULL);
		g_current_channel->setRawPmt(NULL);
	}

	/* store the new channel */
	if ((!g_current_channel) || (channel_id != g_current_channel->getChannelID()))
		g_current_channel = &(cit->second);

	live_channel_id = g_current_channel->getChannelID();
	saveZapitSettings(false, false);

	printf("[zapit] zap to %s(%llx)\n", g_current_channel->getName().c_str(), live_channel_id);

        /* have motor move satellite dish to satellite's position if necessary */
	if (!(currentMode & RECORD_MODE)) {
		transponder_change = frontend->setInput(g_current_channel, current_is_nvod);
		if(transponder_change && !current_is_nvod) {
			waitForMotor = frontend->driveToSatellitePosition(g_current_channel->getSatellitePosition());
			if(waitForMotor > 0) {
				printf("[zapit] waiting %d seconds for motor to turn satellite dish.\n", waitForMotor);
				eventServer->sendEvent(CZapitClient::EVT_ZAP_MOTOR, CEventServer::INITID_ZAPIT, &waitForMotor, sizeof(waitForMotor));
				for(int i = 0; i < waitForMotor; i++) {
					sleep(1);
					if(abort_zapit) {
						abort_zapit = 0;
						return -1;
					}
				}
			}
		}

		/* if channel's transponder does not match frontend's tuned transponder ... */
		if (transponder_change || current_is_nvod) {
			if (frontend->tuneChannel(g_current_channel, current_is_nvod) == false) {
				return -1;
			}
		}
	}

	if (g_current_channel->getServiceType() == ST_NVOD_REFERENCE_SERVICE) {
		current_is_nvod = true;
		//saveZapitSettings(false);
		return 0;
	}
	//bool we_playing = scan_pids && channel->getPidsFlag();//FIXME: this starts playback before pmt
	bool we_playing = 0;
	//bool we_playing = channel->getPidsFlag();

	if (we_playing) {
		printf("[zapit] channel have pids: vpid %X apid %X pcr %X\n", g_current_channel->getVideoPid(), g_current_channel->getPreAudioPid(), g_current_channel->getPcrPid());fflush(stdout);
		if((g_current_channel->getAudioChannelCount() <= 0) && g_current_channel->getPreAudioPid() > 0) {
			std::string desc = "Preset";
			g_current_channel->addAudioChannel(g_current_channel->getPreAudioPid(), CZapitAudioChannel::MPEG, desc, 0xFF);
		}
		startPlayBack(g_current_channel);
	}

	DBG("looking up pids for channel_id " PRINTF_CHANNEL_ID_TYPE "\n", g_current_channel->getChannelID());

	/* get program map table pid from program association table */
	if (g_current_channel->getPmtPid() == 0) {
		printf("[zapit] no pmt pid, going to parse pat\n");
		if (parse_pat(g_current_channel) < 0) {
			printf("[zapit] pat parsing failed\n");
			failed = true;
		}
	}

	/* parse program map table and store pids */
	if ((!failed) && (parse_pmt(g_current_channel) < 0)) {
		printf("[zapit] pmt parsing failed\n");
		if (parse_pat(g_current_channel) < 0) {
			printf("pat parsing failed\n");
			failed = true;
		}
		else if (parse_pmt(g_current_channel) < 0) {
			printf("[zapit] pmt parsing failed\n");
			failed = true;
		}
	}

	if ((!failed) && (g_current_channel->getAudioPid() == 0) && (g_current_channel->getVideoPid() == 0)) {
		printf("[zapit] neither audio nor video pid found\n");
		failed = true;
	}

	/* start sdt scan even if the service was not found in pat or pmt
	 * if the frontend did not tune, we don't get here, so this is fine */
	if (transponder_change)
		sdt_wakeup = true;

	if (failed)
		return -1;

	if (transponder_change == true) {
		g_current_channel->getCaPmt()->ca_pmt_list_management = 0x03;
	} else {
		g_current_channel->getCaPmt()->ca_pmt_list_management = 0x04;
	}

        audio_map_it = audio_map.find(live_channel_id);
        if((audio_map_it != audio_map.end()) ) {
		printf("[zapit] channel found, audio pid %x, subtitle pid %x mode %d volume %d\n",
			audio_map_it->second.apid, audio_map_it->second.subpid, audio_map_it->second.mode, audio_map_it->second.volume);
                if(g_current_channel->getAudioChannelCount() > 1) {
                        for (int i = 0; i < g_current_channel->getAudioChannelCount(); i++) {
                          if (g_current_channel->getAudioChannel(i)->pid == audio_map_it->second.apid ) {
                            DBG("***** Setting audio!\n");
                            g_current_channel->setAudioChannel(i);
                            if(we_playing && (g_current_channel->getAudioChannel(i)->audioChannelType != CZapitAudioChannel::MPEG)) change_audio_pid(i);
                          }
                        }
                }
#if 0
                if(firstzap) {
                        if(audioDecoder)
                                audioDecoder->setVolume(audio_map_it->second.volume, audio_map_it->second.volume);
                }
#endif
                volume_left = volume_right = audio_map_it->second.volume;
                audio_mode = audio_map_it->second.mode;
#if 0
		if(audio_map_it->second.subpid > 0)
			dvbsub_start(audio_map_it->second.subpid);
#endif
		dvbsub_setpid(audio_map_it->second.subpid);

		std::string tmplang;
		for (int i = 0 ; i < (int)g_current_channel->getSubtitleCount() ; ++i) {
			CZapitAbsSub* s = g_current_channel->getChannelSub(i);
			if(s->pId == audio_map_it->second.ttxpid) {
				tmplang = s->ISO639_language_code;
				break;
			}
		}
		if(tmplang.empty())
			tuxtx_set_pid(audio_map_it->second.ttxpid, audio_map_it->second.ttxpage, (char *) g_current_channel->getTeletextLang());
		else
			tuxtx_set_pid(audio_map_it->second.ttxpid, audio_map_it->second.ttxpage, (char *) tmplang.c_str());
        } else {
                volume_left = volume_right = def_volume_left;
                audio_mode = def_audio_mode;
		tuxtx_set_pid(0, 0, (char *) g_current_channel->getTeletextLang());
        }
        if(audioDecoder) {
                //audioDecoder->setVolume(volume_left, volume_right);
                audioDecoder->setChannel(audio_mode);
        }

	if (!we_playing && startplayback)
		startPlayBack(g_current_channel);

	printf("[zapit] sending capmt....\n");

	if(forupdate) {
		if(event_mode) 
			eventServer->sendEvent(CZapitClient::EVT_PMT_CHANGED, CEventServer::INITID_ZAPIT, &channel_id, sizeof(channel_id));
	}
	start_camd(forupdate);
//play:
	send_ca_id(1);

        if (update_pmt)
                pmt_set_update_filter(g_current_channel, &pmt_update_fd);

	return 0;
}

int change_audio_pid(uint8_t index)
{
	if ((!audioDemux) || (!audioDecoder) || (!g_current_channel))
		return -1;

	/* stop demux filter */
	if (audioDemux->Stop() < 0)
		return -1;

	/* stop audio playback */
	if (audioDecoder->Stop() < 0)
		return -1;

	/* update current channel */
	g_current_channel->setAudioChannel(index);

	/* set bypass mode */
	CZapitAudioChannel *currentAudioChannel = g_current_channel->getAudioChannel();

	if (!currentAudioChannel) {
		WARN("No current audio channel");
		return -1;
	}

	switch (currentAudioChannel->audioChannelType) {
		case CZapitAudioChannel::AC3:
			audioDecoder->SetStreamType(AUDIO_FMT_DOLBY_DIGITAL);
			break;
		case CZapitAudioChannel::MPEG:
			audioDecoder->SetStreamType(AUDIO_FMT_MPEG);
			break;
		case CZapitAudioChannel::AAC:
			audioDecoder->SetStreamType(AUDIO_FMT_AAC);
			break;
		case CZapitAudioChannel::AACPLUS:
			audioDecoder->SetStreamType(AUDIO_FMT_AAC_PLUS);
			break;
		case CZapitAudioChannel::DTS:
			audioDecoder->SetStreamType(AUDIO_FMT_DTS);
			break;
		default:
			printf("[zapit] unknown audio channel type 0x%x\n", currentAudioChannel->audioChannelType);
			break;
	}

	printf("[zapit] change apid to 0x%x\n", g_current_channel->getAudioPid());
	/* set demux filter */
	if (audioDemux->pesFilter(g_current_channel->getAudioPid()) < 0)
		return -1;

	/* start demux filter */
	if (audioDemux->Start() < 0)
		return -1;

	/* start audio playback */
	if (audioDecoder->Start() < 0)
		return -1;

	return 0;
}

void setRadioMode(void)
{
	currentMode |= RADIO_MODE;
	currentMode &= ~TV_MODE;
	//stopPlayBack(true);//FIXME why this needed ? to test!
}

void setTVMode(void)
{
	currentMode |= TV_MODE;
	currentMode &= ~RADIO_MODE;
}

int getMode(void)
{
	int mode = currentMode & (~RECORD_MODE);
	return mode;

	if (currentMode & TV_MODE)
		return CZapitClient::MODE_TV;
	if (currentMode & RADIO_MODE)
		return CZapitClient::MODE_RADIO;
	return 0;
}

void setRecordMode(void)
{
	if(currentMode & RECORD_MODE) return;
	currentMode |= RECORD_MODE;
	if(event_mode) eventServer->sendEvent(CZapitClient::EVT_RECORDMODE_ACTIVATED, CEventServer::INITID_ZAPIT );
	rec_channel_id = live_channel_id;
#if 0 // this is done in stream2file.cpp
	if(g_current_channel)
		cam0->setCaPmt(g_current_channel->getCaPmt(), DEMUX_SOURCE_0, 5, true); // demux 0 + 2, update
#endif
}

void unsetRecordMode(void)
{
	if(!(currentMode & RECORD_MODE)) return;
	currentMode &= ~RECORD_MODE;
	if(event_mode) eventServer->sendEvent(CZapitClient::EVT_RECORDMODE_DEACTIVATED, CEventServer::INITID_ZAPIT );

	/* if we on rec. channel, just update pmt with new camask,
	 * else we must stop cam1 and start cam0 for current live channel
	 * in standby should be no cam1 running.
	 */
	if(standby)
		cam0->sendMessage(0,0); // stop
	else if(live_channel_id == rec_channel_id) {
		cam0->setCaPmt(g_current_channel->getCaPmt(), DEMUX_SOURCE_0, (cam0->getCaMask() | DEMUX_DECODE_0) & ~DEMUX_DECODE_2 /*1*/, true); // demux 0, update
	} else {
		cam1->sendMessage(0,0); // stop
		cam0->setCaPmt(g_current_channel->getCaPmt(), DEMUX_SOURCE_0, cam0->getCaMask() & ~DEMUX_DECODE_2 /*1*/); // start
	}
	ca->SendPMT(DEMUX_SOURCE_2, (unsigned char*) "", 0, CA_SLOT_TYPE_SMARTCARD);
	rec_channel_id = 0;
}

int prepare_channels(fe_type_t frontendType, diseqc_t pdiseqcType)
{
	g_current_channel = 0;
	transponders.clear();
	g_bouquetManager->clearAll();
	allchans.clear();  // <- this invalidates all bouquets, too!
        if(scanInputParser) {
                delete scanInputParser;
                scanInputParser = NULL;
        }

	if (LoadServices(frontendType, pdiseqcType, false) < 0)
		return -1;

	INFO("LoadServices: success");
	g_bouquetManager->loadBouquets();
	// 2004.08.02 g_bouquetManager->storeBouquets();
	return 0;
}

void parseScanInputXml(void)
{
	switch (frontend->getInfo()->type) {
	case FE_QPSK:
		scanInputParser = parseXmlFile(SATELLITES_XML);
		break;

	case FE_QAM:
		scanInputParser = parseXmlFile(CABLES_XML);
		break;

	default:
		WARN("Unknown type %d", frontend->getInfo()->type);
		return;
	}
}

int start_scan(int scan_mode)
{
	if (!scanInputParser) {
		parseScanInputXml();
		if (!scanInputParser) {
			WARN("scan not configured");
			return -1;
		}
	}

	scan_runs = 1;
	stopPlayBack(true);
        pmt_stop_update_filter(&pmt_update_fd);

	found_transponders = 0;
	found_channels = 0;

	if (pthread_create(&scan_thread, 0, start_scanthread,  (void*)scan_mode)) {
		ERROR("pthread_create");
		scan_runs = 0;
		return -1;
	}
	return 0;
}

int start_fast_scan(int scan_mode, int opid)
{
	fast_scan_type_t scant;

	scant.type = scan_mode;
	scant.op = (fs_operator_t) opid;

	scan_runs = 1;
	stopPlayBack(true);
	pmt_stop_update_filter(&pmt_update_fd);

	found_transponders = 0;
	found_channels = 0;
        if (pthread_create(&scan_thread, 0, start_fast_scan,  (void*)&scant)) {
                ERROR("pthread_create");
                scan_runs = 0;
                return -1;
        }
	return 0;
}

bool zapit_parse_command(CBasicMessage::Header &rmsg, int connfd)
{
	CZapitMessages::responseCmd response;
	DBG("cmd %d (version %d) received\n", rmsg.cmd, rmsg.version);

	if ((standby) && ((rmsg.cmd != CZapitMessages::CMD_SET_VOLUME)
		&& (rmsg.cmd != CZapitMessages::CMD_MUTE)
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
		zapTo(msgZapto.bouquet, msgZapto.channel);
		break;
	}

	case CZapitMessages::CMD_ZAPTO_CHANNELNR: {
		CZapitMessages::commandZaptoChannelNr msgZaptoChannelNr;
		CBasicServer::receive_data(connfd, &msgZaptoChannelNr, sizeof(msgZaptoChannelNr)); // bouquet & channel number are already starting at 0!
		zapTo(msgZaptoChannelNr.channel);
		break;
	}

	case CZapitMessages::CMD_ZAPTO_SERVICEID:
	case CZapitMessages::CMD_ZAPTO_SUBSERVICEID: {
		CZapitMessages::commandZaptoServiceID msgZaptoServiceID;
		CZapitMessages::responseZapComplete msgResponseZapComplete;
		CBasicServer::receive_data(connfd, &msgZaptoServiceID, sizeof(msgZaptoServiceID));
		msgResponseZapComplete.zapStatus = zapTo_ChannelID(msgZaptoServiceID.channel_id, (rmsg.cmd == CZapitMessages::CMD_ZAPTO_SUBSERVICEID));
		CBasicServer::send_data(connfd, &msgResponseZapComplete, sizeof(msgResponseZapComplete));
		break;
	}

	case CZapitMessages::CMD_ZAPTO_SERVICEID_NOWAIT:
	case CZapitMessages::CMD_ZAPTO_SUBSERVICEID_NOWAIT: {
		CZapitMessages::commandZaptoServiceID msgZaptoServiceID;
		CBasicServer::receive_data(connfd, &msgZaptoServiceID, sizeof(msgZaptoServiceID));
		zapTo_ChannelID(msgZaptoServiceID.channel_id, (rmsg.cmd == CZapitMessages::CMD_ZAPTO_SUBSERVICEID_NOWAIT));
		break;
	}

	case CZapitMessages::CMD_GET_LAST_CHANNEL: {
		CZapitClient::responseGetLastChannel responseGetLastChannel;
		responseGetLastChannel = load_settings();
		CBasicServer::send_data(connfd, &responseGetLastChannel, sizeof(responseGetLastChannel)); // bouquet & channel number are already starting at 0!
		break;
	}

	case CZapitMessages::CMD_GET_CURRENT_SATELLITE_POSITION: {
		int32_t currentSatellitePosition = g_current_channel ? g_current_channel->getSatellitePosition() : frontend->getCurrentSatellitePosition();
		CBasicServer::send_data(connfd, &currentSatellitePosition, sizeof(currentSatellitePosition));
		break;
	}

	case CZapitMessages::CMD_SET_AUDIOCHAN: {
		CZapitMessages::commandSetAudioChannel msgSetAudioChannel;
		CBasicServer::receive_data(connfd, &msgSetAudioChannel, sizeof(msgSetAudioChannel));
		change_audio_pid(msgSetAudioChannel.channel);
		break;
	}

	case CZapitMessages::CMD_SET_MODE: {
		CZapitMessages::commandSetMode msgSetMode;
		CBasicServer::receive_data(connfd, &msgSetMode, sizeof(msgSetMode));
		if (msgSetMode.mode == CZapitClient::MODE_TV)
			setTVMode();
		else if (msgSetMode.mode == CZapitClient::MODE_RADIO)
			setRadioMode();
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
		msgCurrentSID.channel_id = (g_current_channel != 0) ? g_current_channel->getChannelID() : 0;
		CBasicServer::send_data(connfd, &msgCurrentSID, sizeof(msgCurrentSID));
		break;
	}

	case CZapitMessages::CMD_GET_CURRENT_SERVICEINFO: {
		CZapitClient::CCurrentServiceInfo msgCurrentServiceInfo;
		memset(&msgCurrentServiceInfo, 0, sizeof(CZapitClient::CCurrentServiceInfo));
		if(g_current_channel) {
			msgCurrentServiceInfo.onid = g_current_channel->getOriginalNetworkId();
			msgCurrentServiceInfo.sid = g_current_channel->getServiceId();
			msgCurrentServiceInfo.tsid = g_current_channel->getTransportStreamId();
			msgCurrentServiceInfo.vpid = g_current_channel->getVideoPid();
			msgCurrentServiceInfo.apid = g_current_channel->getAudioPid();
			msgCurrentServiceInfo.vtxtpid = g_current_channel->getTeletextPid();
			msgCurrentServiceInfo.pmtpid = g_current_channel->getPmtPid();
			msgCurrentServiceInfo.pcrpid = g_current_channel->getPcrPid();
			msgCurrentServiceInfo.tsfrequency = frontend->getFrequency();
			msgCurrentServiceInfo.rate = frontend->getRate();
			msgCurrentServiceInfo.fec = frontend->getCFEC();
			msgCurrentServiceInfo.vtype = g_current_channel->type;
			//msgCurrentServiceInfo.diseqc = g_current_channel->getDiSEqC();
		}
		if(!msgCurrentServiceInfo.fec)
			msgCurrentServiceInfo.fec = (fe_code_rate)3;
		if (frontend->getInfo()->type == FE_QPSK)
			msgCurrentServiceInfo.polarisation = frontend->getPolarization();
		else
			msgCurrentServiceInfo.polarisation = 2;
		CBasicServer::send_data(connfd, &msgCurrentServiceInfo, sizeof(msgCurrentServiceInfo));
		break;
	}

	case CZapitMessages::CMD_GET_DELIVERY_SYSTEM: {
		CZapitMessages::responseDeliverySystem _response;
		switch (frontend->getInfo()->type) {
		case FE_QAM:
			_response.system = DVB_C;
			break;
		case FE_QPSK:
			_response.system = DVB_S;
			break;
		case FE_OFDM:
			_response.system = DVB_T;
			break;
		default:
			WARN("Unknown type %d", frontend->getInfo()->type);
			return false;

		}
		CBasicServer::send_data(connfd, &_response, sizeof(_response));
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
                CZapitMessages::responseGetChannelName _response;
                CBasicServer::receive_data(connfd, &requested_channel_id, sizeof(requested_channel_id));
		if(requested_channel_id == 0) {
			if(g_current_channel) {
				strncpy(_response.name, g_current_channel->getName().c_str(), CHANNEL_NAME_SIZE);
				_response.name[CHANNEL_NAME_SIZE-1] = 0;
			} else
				_response.name[0] = 0;
		} else {
                tallchans_iterator it = allchans.find(requested_channel_id);
                if (it == allchans.end())
                        _response.name[0] = 0;
                else
                        strncpy(_response.name, it->second.getName().c_str(), CHANNEL_NAME_SIZE);
			_response.name[CHANNEL_NAME_SIZE-1] = 0;
		}
                CBasicServer::send_data(connfd, &_response, sizeof(_response));
                break;
        }

       case CZapitMessages::CMD_IS_TV_CHANNEL: {
               t_channel_id                             requested_channel_id;
               CZapitMessages::responseGeneralTrueFalse _response;
               CBasicServer::receive_data(connfd, &requested_channel_id, sizeof(requested_channel_id));
               tallchans_iterator it = allchans.find(requested_channel_id);
               if (it == allchans.end()) {
		       it = nvodchannels.find(requested_channel_id);
                       /* if in doubt (i.e. unknown channel) answer yes */
               	       if (it == nvodchannels.end())
                              _response.status = true;
                       else
                       /* FIXME: the following check is no even remotely accurate */
                              _response.status = (it->second.getServiceType() != ST_DIGITAL_RADIO_SOUND_SERVICE);
		} else
                /* FIXME: the following check is no even remotely accurate */
                      _response.status = (it->second.getServiceType() != ST_DIGITAL_RADIO_SOUND_SERVICE);

               CBasicServer::send_data(connfd, &_response, sizeof(_response));
               break;
       }

	case CZapitMessages::CMD_BQ_RESTORE: {
		//2004.08.02 g_bouquetManager->restoreBouquets();
		if(g_list_changed) {
			prepare_channels(frontend->getInfo()->type, diseqcType);
			g_list_changed = 0;
		} else {
			g_bouquetManager->clearAll();
			g_bouquetManager->loadBouquets();
		}
		response.cmd = CZapitMessages::CMD_READY;
		CBasicServer::send_data(connfd, &response, sizeof(response));
		break;
	}

	case CZapitMessages::CMD_REINIT_CHANNELS: {
		// Houdini: save actual channel to restore it later, old version's channel was set to scans.conf initial channel
  		t_channel_id cid= g_current_channel ? g_current_channel->getChannelID() : 0;

   		prepare_channels(frontend->getInfo()->type, diseqcType);

 		tallchans_iterator cit = allchans.find(cid);
  		if (cit != allchans.end())
  			g_current_channel = &(cit->second);

		response.cmd = CZapitMessages::CMD_READY;
		CBasicServer::send_data(connfd, &response, sizeof(response));
		eventServer->sendEvent(CZapitClient::EVT_SERVICES_CHANGED, CEventServer::INITID_ZAPIT);
		break;
	}

	case CZapitMessages::CMD_RELOAD_CURRENTSERVICES: {
  	        response.cmd = CZapitMessages::CMD_READY;
  	        CBasicServer::send_data(connfd, &response, sizeof(response));
DBG("[zapit] sending EVT_SERVICES_CHANGED\n");
		frontend->setTsidOnid(0);
		zapit(live_channel_id, current_is_nvod);
  	        eventServer->sendEvent(CZapitClient::EVT_SERVICES_CHANGED, CEventServer::INITID_ZAPIT);
		//eventServer->sendEvent(CZapitClient::EVT_BOUQUETS_CHANGED, CEventServer::INITID_ZAPIT);
  	        break;
  	}
	case CZapitMessages::CMD_SCANSTART: {
		int scan_mode;
		CBasicServer::receive_data(connfd, &scan_mode, sizeof(scan_mode));

		if (start_scan(scan_mode) == -1)
			eventServer->sendEvent(CZapitClient::EVT_SCAN_FAILED, CEventServer::INITID_ZAPIT);
		break;
	}
	case CZapitMessages::CMD_SCANSTOP: {
		if(scan_runs) {
			//pthread_cancel(scan_thread);
			abort_scan = 1;
			pthread_join(scan_thread, NULL);
			abort_scan = 0;
			scan_runs = 0;
		}
		break;
	}

	case CZapitMessages::CMD_SETCONFIG:
		Zapit_config Cfg;
                CBasicServer::receive_data(connfd, &Cfg, sizeof(Cfg));
		setZapitConfig(&Cfg);
		break;
	case CZapitMessages::CMD_GETCONFIG:
		sendConfig(connfd);
		break;
	case CZapitMessages::CMD_REZAP:
		if (currentMode & RECORD_MODE)
			break;
		if(rezapTimeout > 0)
			sleep(rezapTimeout);
		if(g_current_channel)
			zapit(g_current_channel->getChannelID(), current_is_nvod);
		break;
        case CZapitMessages::CMD_TUNE_TP: {
			CBasicServer::receive_data(connfd, &TP, sizeof(TP));
			sig_delay = 0;
			TP.feparams.inversion = INVERSION_AUTO;
			const char *name = scanProviders.size() > 0  ? scanProviders.begin()->second.c_str() : "unknown";

			switch (frontend->getInfo()->type) {
			case FE_QPSK:
			case FE_OFDM: {
				t_satellite_position satellitePosition = scanProviders.begin()->first;
				printf("[zapit] tune to sat %s freq %d rate %d fec %d pol %d\n", name, TP.feparams.frequency, TP.feparams.u.qpsk.symbol_rate, TP.feparams.u.qpsk.fec_inner, TP.polarization);
				frontend->setInput(satellitePosition, TP.feparams.frequency,  TP.polarization);
				frontend->driveToSatellitePosition(satellitePosition);
				break;
			}
			case FE_QAM:
				printf("[zapit] tune to cable %s freq %d rate %d fec %d\n", name, TP.feparams.frequency, TP.feparams.u.qam.symbol_rate, TP.feparams.u.qam.fec_inner);
				break;
			default:
				WARN("Unknown type %d", frontend->getInfo()->type);
				return false;
			}
			frontend->tuneFrequency(&TP.feparams, TP.polarization, true);
		}
		break;
        case CZapitMessages::CMD_SCAN_TP: {
                CBasicServer::receive_data(connfd, &TP, sizeof(TP));

#if 0
printf("[zapit] TP_id %d freq %d rate %d fec %d pol %d\n", TP.TP_id, TP.feparams.frequency, TP.feparams.u.qpsk.symbol_rate, TP.feparams.u.qpsk.fec_inner, TP.polarization);
#endif
		if(!(TP.feparams.frequency > 0) && g_current_channel) {
			transponder_list_t::iterator transponder = transponders.find(g_current_channel->getTransponderId());
			TP.feparams.frequency = transponder->second.feparams.frequency;
			switch (frontend->getInfo()->type) {
			case FE_QPSK:
			case FE_OFDM:
				TP.feparams.u.qpsk.symbol_rate = transponder->second.feparams.u.qpsk.symbol_rate;
				TP.feparams.u.qpsk.fec_inner = transponder->second.feparams.u.qpsk.fec_inner;
				TP.polarization = transponder->second.polarization;
				break;
			case FE_QAM:
				TP.feparams.u.qam.symbol_rate = transponder->second.feparams.u.qam.symbol_rate;
				TP.feparams.u.qam.fec_inner = transponder->second.feparams.u.qam.fec_inner;
				TP.feparams.u.qam.modulation = transponder->second.feparams.u.qam.modulation;
				break;
			default:
				WARN("Unknown type %d", frontend->getInfo()->type);
				return false;
			}

			if(scanProviders.size() > 0)
				scanProviders.clear();
#if 0
			std::map<string, t_satellite_position>::iterator spos_it;
			for (spos_it = satellitePositions.begin(); spos_it != satellitePositions.end(); spos_it++)
				if(spos_it->second == g_current_channel->getSatellitePosition())
					scanProviders[transponder->second.DiSEqC] = spos_it->first.c_str();
#endif
			//FIXME not ready
			//if(satellitePositions.find(g_current_channel->getSatellitePosition()) != satellitePositions.end())
			g_current_channel = 0;
		}
		stopPlayBack(true);
        	pmt_stop_update_filter(&pmt_update_fd);
		scan_runs = 1;
		if (pthread_create(&scan_thread, 0, scan_transponder,  (void*) &TP)) {
			ERROR("pthread_create");
			scan_runs = 0;
		}
		//else
		//	pthread_detach(scan_thread);
                break;
        }

	case CZapitMessages::CMD_SCANREADY: {
		CZapitMessages::responseIsScanReady msgResponseIsScanReady;
		msgResponseIsScanReady.satellite = curr_sat;
		msgResponseIsScanReady.transponder = found_transponders;
		msgResponseIsScanReady.processed_transponder = processed_transponders;
		msgResponseIsScanReady.services = found_channels;
		if (scan_runs > 0)
			msgResponseIsScanReady.scanReady = false;
		else
			msgResponseIsScanReady.scanReady = true;
		CBasicServer::send_data(connfd, &msgResponseIsScanReady, sizeof(msgResponseIsScanReady));
		break;
	}

	case CZapitMessages::CMD_SCANGETSATLIST: {
		uint32_t  satlength;
		CZapitClient::responseGetSatelliteList sat;
		satlength = sizeof(sat);

		sat_iterator_t sit;
		for(sit = satellitePositions.begin(); sit != satellitePositions.end(); sit++) {
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
		CZapitClient::commandSetScanSatelliteList sat;
		scanProviders.clear();
//printf("[zapit] SETSCANSATLIST\n");
		while (CBasicServer::receive_data(connfd, &sat, sizeof(sat))) {
printf("[zapit] adding to scan %s (position %d)\n", sat.satName, sat.position);
			scanProviders[sat.position] = sat.satName;
		}
		break;
	}

	case CZapitMessages::CMD_SCANSETSCANMOTORPOSLIST: {
#if 0 // absolute
		CZapitClient::commandSetScanMotorPosList pos;
		bool changed = false;
		while (CBasicServer::receive_data(connfd, &pos, sizeof(pos))) {
			//printf("adding %d (motorPos %d)\n", pos.satPosition, pos.motorPos);
			changed |= (motorPositions[pos.satPosition] != pos.motorPos);
			motorPositions[pos.satPosition] = pos.motorPos;
		}

		if (changed)
			SaveMotorPositions();
#endif
		break;
	}

	case CZapitMessages::CMD_SCANSETDISEQCTYPE: {
		//diseqcType is global
		CBasicServer::receive_data(connfd, &diseqcType, sizeof(diseqcType));
		frontend->setDiseqcType(diseqcType);
		DBG("set diseqc type %d", diseqcType);
		break;
	}

	case CZapitMessages::CMD_SCANSETDISEQCREPEAT: {
		uint32_t  repeats;
		CBasicServer::receive_data(connfd, &repeats, sizeof(repeats));
		frontend->setDiseqcRepeats(repeats);
		DBG("set diseqc repeats to %d", repeats);
		break;
	}

	case CZapitMessages::CMD_SCANSETBOUQUETMODE:
		CBasicServer::receive_data(connfd, &bouquetMode, sizeof(bouquetMode));
		break;

        case CZapitMessages::CMD_SCANSETTYPE:
                CBasicServer::receive_data(connfd, &scanType, sizeof(scanType));
                break;

	case CZapitMessages::CMD_SET_EVENT_MODE: {
		CZapitMessages::commandSetRecordMode msgSetRecordMode;
		CBasicServer::receive_data(connfd, &msgSetRecordMode, sizeof(msgSetRecordMode));
//printf("[zapit] event mode: %d\n", msgSetRecordMode.activate);fflush(stdout);
		event_mode = msgSetRecordMode.activate;
		if(event_mode)
			pipzap = 0;
		else if(!standby)
			pipzap = 1;
                break;
	}
	case CZapitMessages::CMD_SET_RECORD_MODE: {
		CZapitMessages::commandSetRecordMode msgSetRecordMode;
		CBasicServer::receive_data(connfd, &msgSetRecordMode, sizeof(msgSetRecordMode));
printf("[zapit] recording mode: %d\n", msgSetRecordMode.activate);fflush(stdout);
		if (msgSetRecordMode.activate)
			setRecordMode();
		else
			unsetRecordMode();
		break;
	}

	case CZapitMessages::CMD_GET_RECORD_MODE: {
		CZapitMessages::responseGetRecordModeState msgGetRecordModeState;
		msgGetRecordModeState.activated = (currentMode & RECORD_MODE);
		CBasicServer::send_data(connfd, &msgGetRecordModeState, sizeof(msgGetRecordModeState));
		break;
	}

	case CZapitMessages::CMD_SB_GET_PLAYBACK_ACTIVE: {
		CZapitMessages::responseGetPlaybackState msgGetPlaybackState;
                msgGetPlaybackState.activated = playing;
#if 0 //FIXME
		if (videoDecoder) {
                	if (videoDecoder->getPlayState() == VIDEO_PLAYING)
                        	msgGetPlaybackState.activated = 1;
                }
#endif
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

	case CZapitMessages::CMD_BQ_EXISTS_CHANNEL_IN_BOUQUET: {
		CZapitMessages::commandExistsChannelInBouquet msgExistsChInBq;
		CZapitMessages::responseGeneralTrueFalse responseBool;
		CBasicServer::receive_data(connfd, &msgExistsChInBq, sizeof(msgExistsChInBq)); // bouquet & channel number are already starting at 0!
		responseBool.status = g_bouquetManager->existsChannelInBouquet(msgExistsChInBq.bouquet, msgExistsChInBq.channel_id);
		CBasicServer::send_data(connfd, &responseBool, sizeof(responseBool));
		break;
	}

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
			allchans.erase(msgRemoveChannelFromBouquet.channel_id);
			g_current_channel = 0;
			g_list_changed = 1;
		}
#endif
		break;
	}

	case CZapitMessages::CMD_BQ_MOVE_CHANNEL: {
		CZapitMessages::commandMoveChannel msgMoveChannel;
		CBasicServer::receive_data(connfd, &msgMoveChannel, sizeof(msgMoveChannel)); // bouquet & channel number are already starting at 0!
		if (msgMoveChannel.bouquet < g_bouquetManager->Bouquets.size())
			g_bouquetManager->Bouquets[msgMoveChannel.bouquet]->moveService(msgMoveChannel.oldPos, msgMoveChannel.newPos,
					(((currentMode & RADIO_MODE) && msgMoveChannel.mode == CZapitClient::MODE_CURRENT)
					 || (msgMoveChannel.mode==CZapitClient::MODE_RADIO)) ? 2 : 1);
		break;
	}

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

		response.cmd = CZapitMessages::CMD_READY;
		CBasicServer::send_data(connfd, &response, sizeof(response));
#if 0
		//if (msgBoolean.truefalse)
		if(g_list_changed) {
			eventServer->sendEvent(CZapitClient::EVT_SERVICES_CHANGED, CEventServer::INITID_ZAPIT);
		} else
			eventServer->sendEvent(CZapitClient::EVT_BOUQUETS_CHANGED, CEventServer::INITID_ZAPIT);
#endif
		g_bouquetManager->saveBouquets();
		g_bouquetManager->saveUBouquets();
		g_bouquetManager->renumServices();
		//eventServer->sendEvent(CZapitClient::EVT_SERVICES_CHANGED, CEventServer::INITID_ZAPIT);
		eventServer->sendEvent(CZapitClient::EVT_BOUQUETS_CHANGED, CEventServer::INITID_ZAPIT);
		if(g_list_changed) {
			SaveServices(true);
			g_list_changed = 0;
		}
		break;
	}

        case CZapitMessages::CMD_SET_VIDEO_SYSTEM: {
		CZapitMessages::commandInt msg;
		CBasicServer::receive_data(connfd, &msg, sizeof(msg));
                setVideoSystem_t(msg.val);
                break;
        }

        case CZapitMessages::CMD_SET_NTSC: {
                setVideoSystem_t(8);
                break;
        }

	case CZapitMessages::CMD_SB_START_PLAYBACK:
		//playbackStopForced = false;
		startPlayBack(g_current_channel);
		break;

	case CZapitMessages::CMD_SB_STOP_PLAYBACK:
		stopPlayBack(false);
		response.cmd = CZapitMessages::CMD_READY;
		CBasicServer::send_data(connfd, &response, sizeof(response));
		break;

	case CZapitMessages::CMD_SB_LOCK_PLAYBACK:
		/* hack. if standby true, dont blank video */
		standby = true;
		stopPlayBack(true);
		standby = false;
		playbackStopForced = true;
		response.cmd = CZapitMessages::CMD_READY;
		CBasicServer::send_data(connfd, &response, sizeof(response));
		break;
	case CZapitMessages::CMD_SB_UNLOCK_PLAYBACK:
		playbackStopForced = false;
		startPlayBack(g_current_channel);
		start_camd();
		response.cmd = CZapitMessages::CMD_READY;
		CBasicServer::send_data(connfd, &response, sizeof(response));
		break;
	case CZapitMessages::CMD_SET_DISPLAY_FORMAT: {
		CZapitMessages::commandInt msg;
		CBasicServer::receive_data(connfd, &msg, sizeof(msg));
#if 0 //FIXME
		if(videoDecoder)
			videoDecoder->setCroppingMode((video_displayformat_t) msg.val);
#endif
		break;
	}

	case CZapitMessages::CMD_SET_AUDIO_MODE: {
		CZapitMessages::commandInt msg;
		CBasicServer::receive_data(connfd, &msg, sizeof(msg));
		if(audioDecoder) audioDecoder->setChannel((int) msg.val);
		audio_mode = msg.val;
		break;
	}

	case CZapitMessages::CMD_GET_AUDIO_MODE: {
		CZapitMessages::commandInt msg;
		msg.val = (int) audio_mode;
		CBasicServer::send_data(connfd, &msg, sizeof(msg));
		break;
	}

	case CZapitMessages::CMD_SET_ASPECTRATIO: {
		CZapitMessages::commandInt msg;
		CBasicServer::receive_data(connfd, &msg, sizeof(msg));
		aspectratio=(int) msg.val;
		if(videoDecoder) videoDecoder->setAspectRatio(aspectratio, -1);
		break;
	}

	case CZapitMessages::CMD_GET_ASPECTRATIO: {
		CZapitMessages::commandInt msg;
		if(videoDecoder) aspectratio=videoDecoder->getAspectRatio();
		msg.val = aspectratio;
		CBasicServer::send_data(connfd, &msg, sizeof(msg));
		break;
	}

	case CZapitMessages::CMD_SET_MODE43: {
		CZapitMessages::commandInt msg;
		CBasicServer::receive_data(connfd, &msg, sizeof(msg));
		mode43=(int) msg.val;
		if(videoDecoder) videoDecoder->setAspectRatio(-1, mode43);
		break;
	}

#if 0 
	//FIXME howto read aspect mode back?
	case CZapitMessages::CMD_GET_MODE43: {
		CZapitMessages::commandInt msg;
		if(videoDecoder) mode43=videoDecoder->getCroppingMode();
		msg.val = mode43;
		CBasicServer::send_data(connfd, &msg, sizeof(msg));
		break;
	}
#endif

	case CZapitMessages::CMD_GETPIDS: {
		if (g_current_channel) {
			CZapitClient::responseGetOtherPIDs responseGetOtherPIDs;
			responseGetOtherPIDs.vpid = g_current_channel->getVideoPid();
			responseGetOtherPIDs.ecmpid = 0; // TODO: remove
			responseGetOtherPIDs.vtxtpid = g_current_channel->getTeletextPid();
			responseGetOtherPIDs.pmtpid = g_current_channel->getPmtPid();
			responseGetOtherPIDs.pcrpid = g_current_channel->getPcrPid();
			responseGetOtherPIDs.selected_apid = g_current_channel->getAudioChannelIndex();
			responseGetOtherPIDs.privatepid = g_current_channel->getPrivatePid();
			CBasicServer::send_data(connfd, &responseGetOtherPIDs, sizeof(responseGetOtherPIDs));
			sendAPIDs(connfd);
		}
		break;
	}

	case CZapitMessages::CMD_GET_FE_SIGNAL: {
		CZapitClient::responseFESignal response_FEsig;

		response_FEsig.sig = frontend->getSignalStrength();
		response_FEsig.snr = frontend->getSignalNoiseRatio();
		response_FEsig.ber = frontend->getBitErrorRate();

		CBasicServer::send_data(connfd, &response_FEsig, sizeof(CZapitClient::responseFESignal));
		//sendAPIDs(connfd);
		break;
	}

	case CZapitMessages::CMD_SETSUBSERVICES: {
		CZapitClient::commandAddSubServices msgAddSubService;

		while (CBasicServer::receive_data(connfd, &msgAddSubService, sizeof(msgAddSubService))) {
			t_original_network_id original_network_id = msgAddSubService.original_network_id;
			t_service_id          service_id          = msgAddSubService.service_id;
DBG("NVOD insert %llx\n", CREATE_CHANNEL_ID_FROM_SERVICE_ORIGINALNETWORK_TRANSPORTSTREAM_ID(msgAddSubService.service_id, msgAddSubService.original_network_id, msgAddSubService.transport_stream_id));
			nvodchannels.insert (
			    std::pair <t_channel_id, CZapitChannel> (
				CREATE_CHANNEL_ID_FROM_SERVICE_ORIGINALNETWORK_TRANSPORTSTREAM_ID(msgAddSubService.service_id, msgAddSubService.original_network_id, msgAddSubService.transport_stream_id),
				CZapitChannel (
				    "NVOD",
				    service_id,
				    msgAddSubService.transport_stream_id,
				    original_network_id,
				    1,
				    g_current_channel ? g_current_channel->getSatellitePosition() : 0,
				    0
				)
			    )
			);
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
		//if(!audioDecoder) audioDecoder = new CAudio();
		if(!audioDecoder) break;
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
		//if(!audioDecoder) audioDecoder = new CAudio();
		if(audioDecoder)
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
		if(audioDecoder)
			msgBoolean.truefalse = audioDecoder->getMuteStatus();
		CBasicServer::send_data(connfd, &msgBoolean, sizeof(msgBoolean));
		break;
	}
	case CZapitMessages::CMD_SET_STANDBY: {
		CZapitMessages::commandBoolean msgBoolean;
		CBasicServer::receive_data(connfd, &msgBoolean, sizeof(msgBoolean));
		if (msgBoolean.truefalse) {
			// if(videoDecoder && (currentMode & RECORD_MODE)) videoDecoder->freeze();
			enterStandby();
			response.cmd = CZapitMessages::CMD_READY;
			CBasicServer::send_data(connfd, &response, sizeof(response));
		} else
			leaveStandby();
		break;
	}

	case CZapitMessages::CMD_NVOD_SUBSERVICE_NUM: {
		CZapitMessages::commandInt msg;
		CBasicServer::receive_data(connfd, &msg, sizeof(msg));
	}

	case CZapitMessages::CMD_SEND_MOTOR_COMMAND: {
		CZapitMessages::commandMotor msgMotor;
		CBasicServer::receive_data(connfd, &msgMotor, sizeof(msgMotor));
		printf("[zapit] received motor command: %x %x %x %x %x %x\n", msgMotor.cmdtype, msgMotor.address, msgMotor.cmd, msgMotor.num_parameters, msgMotor.param1, msgMotor.param2);
		if(msgMotor.cmdtype > 0x20)
		frontend->sendMotorCommand(msgMotor.cmdtype, msgMotor.address, msgMotor.cmd, msgMotor.num_parameters, msgMotor.param1, msgMotor.param2);
		// TODO !!
		//else if(g_current_channel)
		//  frontend->satFind(msgMotor.cmdtype, g_current_channel);
		break;
	}

	default:
		WARN("unknown command %d (version %d)", rmsg.cmd, CZapitMessages::ACTVERSION);
		break;
	}

	DBG("cmd %d processed\n", rmsg.cmd);

	return true;
}

/****************************************************************/
/*  functions for new command handling via CZapitClient		*/
/*  these functions should be encapsulated in a class CZapit	*/
/****************************************************************/

void addChannelToBouquet(const unsigned int bouquet, const t_channel_id channel_id)
{
	//DBG("addChannelToBouquet(%d, %d)\n", bouquet, channel_id);

	CZapitChannel* chan = g_bouquetManager->findChannelByChannelID(channel_id);

	if (chan != NULL)
		if (bouquet < g_bouquetManager->Bouquets.size())
			g_bouquetManager->Bouquets[bouquet]->addService(chan);
		else
			WARN("bouquet not found");
	else
		WARN("channel_id not found in channellist");
}

bool send_data_count(int connfd, int data_count)
{
	CZapitMessages::responseGeneralInteger responseInteger;
	responseInteger.number = data_count;
	if (CBasicServer::send_data(connfd, &responseInteger, sizeof(responseInteger)) == false) {
		ERROR("could not send any return");
		return false;
	}
	return true;
}

void sendAPIDs(int connfd)
{
	if (!send_data_count(connfd, g_current_channel->getAudioChannelCount()))
		return;
	for (uint32_t  i = 0; i < g_current_channel->getAudioChannelCount(); i++) {
		CZapitClient::responseGetAPIDs response;
		response.pid = g_current_channel->getAudioPid(i);
		strncpy(response.desc, g_current_channel->getAudioChannel(i)->description.c_str(), 25);
		response.is_ac3 = response.is_aac = 0;
		if (g_current_channel->getAudioChannel(i)->audioChannelType == CZapitAudioChannel::AC3) {
			response.is_ac3 = 1;
		} else if (g_current_channel->getAudioChannel(i)->audioChannelType == CZapitAudioChannel::AAC) {
			response.is_aac = 1;
		}
		response.component_tag = g_current_channel->getAudioChannel(i)->componentTag;

		if (CBasicServer::send_data(connfd, &response, sizeof(response)) == false) {
			ERROR("could not send any return");
			return;
		}
	}
}

void internalSendChannels(int connfd, ZapitChannelList* channels, const unsigned int first_channel_nr, bool nonames)
{
	int data_count = channels->size();
#if RECORD_RESEND // old, before tv/radio resend
	if (currentMode & RECORD_MODE) {
		for (uint32_t  i = 0; i < channels->size(); i++)
			if ((*channels)[i]->getTransponderId() != channel->getTransponderId())
				data_count--;
	}
#endif
	if (!send_data_count(connfd, data_count))
		return;

	for (uint32_t  i = 0; i < channels->size();i++) {
#if RECORD_RESEND // old, before tv/radio resend
		if ((currentMode & RECORD_MODE) && ((*channels)[i]->getTransponderId() != frontend->getTsidOnid()))
			continue;
#endif

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

void sendBouquets(int connfd, const bool emptyBouquetsToo, CZapitClient::channelsMode mode)
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
                      ((curMode & TV_MODE) && !g_bouquetManager->Bouquets[i]->tvChannels.empty())))
                   )
		{
// ATTENTION: in RECORD_MODE empty bouquets are not send!
#if RECORD_RESEND // old, before tv/radio resend
			if ((!(currentMode & RECORD_MODE)) || ((frontend != NULL) &&
			     (((currentMode & RADIO_MODE) && (g_bouquetManager->Bouquets[i]->recModeRadioSize(frontend->getTsidOnid()) > 0)) ||
			      ((currentMode & TV_MODE)    && (g_bouquetManager->Bouquets[i]->recModeTVSize   (frontend->getTsidOnid()) > 0)))))
#endif
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
	}
	msgBouquet.bouquet_nr = RESPONSE_GET_BOUQUETS_END_MARKER;
	if (CBasicServer::send_data(connfd, &msgBouquet, sizeof(msgBouquet)) == false) {
		ERROR("could not send end marker");
		return;
	}
}

void sendBouquetChannels(int connfd, const unsigned int bouquet, const CZapitClient::channelsMode mode, bool nonames)
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

void sendChannels(int connfd, const CZapitClient::channelsMode mode, const CZapitClient::channelsOrder order)
{
	ZapitChannelList channels;

	if (order == CZapitClient::SORT_BOUQUET) {
		CBouquetManager::ChannelIterator cit = (((currentMode & RADIO_MODE) && (mode == CZapitClient::MODE_CURRENT)) || (mode==CZapitClient::MODE_RADIO)) ? g_bouquetManager->radioChannelsBegin() : g_bouquetManager->tvChannelsBegin();
		for (; !(cit.EndOfChannels()); cit++)
			channels.push_back(*cit);
	}
	else if (order == CZapitClient::SORT_ALPHA)   // ATTENTION: in this case response.nr does not return the actual number of the channel for zapping!
	{
		if (((currentMode & RADIO_MODE) && (mode == CZapitClient::MODE_CURRENT)) || (mode==CZapitClient::MODE_RADIO)) {
			for (tallchans_iterator it = allchans.begin(); it != allchans.end(); it++)
				if (it->second.getServiceType() == ST_DIGITAL_RADIO_SOUND_SERVICE)
					channels.push_back(&(it->second));
		} else {
			for (tallchans_iterator it = allchans.begin(); it != allchans.end(); it++)
				if (it->second.getServiceType() != ST_DIGITAL_RADIO_SOUND_SERVICE)
					channels.push_back(&(it->second));
		}
		sort(channels.begin(), channels.end(), CmpChannelByChName());
	}

	internalSendChannels(connfd, &channels, 0, false);
}

int startPlayBack(CZapitChannel *thisChannel)
{
	bool have_pcr = false;
	bool have_audio = false;
	bool have_video = false;
	bool have_teletext = false;

	if(!thisChannel)
		thisChannel = g_current_channel;
	if ((playbackStopForced == true) || (!thisChannel) || playing)
		return -1;

	printf("[zapit] vpid %X apid %X pcr %X\n", thisChannel->getVideoPid(), thisChannel->getAudioPid(), thisChannel->getPcrPid());
	if(standby) {
		frontend->Open();
		return 0;
	}

	if (thisChannel->getPcrPid() != 0)
		have_pcr = true;
	if (thisChannel->getAudioPid() != 0)
		have_audio = true;
	if ((thisChannel->getVideoPid() != 0) && (currentMode & TV_MODE))
		have_video = true;
	if (thisChannel->getTeletextPid() != 0)
		have_teletext = true;

	if ((!have_audio) && (!have_video) && (!have_teletext))
		return -1;
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
		const char *audioStr = "UNKNOWN";
		switch (thisChannel->getAudioChannel()->audioChannelType) {
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
				printf("[zapit] unknown audio channel type 0x%x\n", thisChannel->getAudioChannel()->audioChannelType);
				break;
		}

		printf("[zapit] starting %s audio\n", audioStr);
		audioDemux->Start();
		audioDecoder->Start();
	}

	/* start video */
	if (have_video) {
		videoDecoder->Start(0, thisChannel->getPcrPid(), thisChannel->getVideoPid());
		videoDemux->Start();
	}
#if 0
	if(have_teletext)
		videoDecoder->StartVBI(thisChannel->getTeletextPid());
#endif
	playing = true;

	return 0;
}

int stopPlayBack(bool stop_camd)
{
	if(stop_camd) {
		if(currentMode & RECORD_MODE) {
			/* if we recording and rec == live, only update camask on cam0,
			 * else stop cam1 */
			if(live_channel_id == rec_channel_id) {
				cam0->setCaPmt(g_current_channel->getCaPmt(), DEMUX_SOURCE_0, DEMUX_DECODE_2 /*4*/, true); // demux 2, update
			} else
				cam1->sendMessage(0,0);
		} else {
			cam0->sendMessage(0,0);
			unlink("/tmp/pmt.tmp");
		}
		ca->SendPMT(0, (unsigned char*) "", 0, CA_SLOT_TYPE_SMARTCARD);
	}

	printf("stopPlayBack: standby %d forced %d\n", standby, playbackStopForced);

	//if(standby || !playing)
	if (!playing)
		return 0;

	if (playbackStopForced)
		return -1;

	if (videoDemux)
		videoDemux->Stop();
	if (audioDemux)
		audioDemux->Stop();
	if (pcrDemux)
		pcrDemux->Stop();
	if (audioDecoder) {
		audioDecoder->Stop();
	}

	if (videoDecoder) {
		/* hack. if standby, dont blank video -> for paused timeshift */
		videoDecoder->Stop(standby ? false : true);
		videoDecoder->StopVBI();
	}

	playing = false;

	tuxtx_stop_subtitle();
	if(standby)
		dvbsub_pause();
	else
		dvbsub_stop();

	return 0;
}

void setVideoSystem_t(int video_system)
{
	if(videoDecoder)
		videoDecoder->SetVideoSystem(video_system);
}

void stopPlaying(void)
{
	saveZapitSettings(true, true);
	stopPlayBack(true);
}

void enterStandby(void)
{
	if (standby)
		return;

	standby = true;

	saveZapitSettings(true, true);
	stopPlayBack(true);

	if(!(currentMode & RECORD_MODE)) {
		frontend->Close();
		rename("/tmp/pmt.tmp", "/tmp/pmt.tmp.off");
	}
}

void leaveStandby(void)
{
	if(!standby) return;

	printf("[zapit] diseqc type = %d\n", diseqcType);

	if (!frontend) {
		frontend = new CFrontend();
		frontend->setDiseqcRepeats(config.getInt32("diseqcRepeats", 0));
		frontend->setCurrentSatellitePosition(config.getInt32("lastSatellitePosition", 0));
		frontend->setDiseqcType(diseqcType);
	}
	if(!(currentMode & RECORD_MODE)) {
		frontend->Open();
		frontend->setTsidOnid(0);
		frontend->setDiseqcType(diseqcType);
		if (!cam0) {
			cam0 = new CCam();
		}
		rename("/tmp/pmt.tmp.off", "/tmp/pmt.tmp");
	}
	if (!cam1) {
		cam1 = new CCam();
	}

	standby = false;
	if (g_current_channel)
		/* tune channel, with stopped playback to not bypass the parental PIN check */
		zapit(live_channel_id, current_is_nvod, false, false);
}

unsigned zapTo(const unsigned int bouquet, const unsigned int pchannel)
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

	return zapTo_ChannelID((*channels)[pchannel]->getChannelID(), false);
}

unsigned int zapTo_ChannelID(t_channel_id channel_id, bool isSubService)
{
	unsigned int result = 0;

	if (zapit(channel_id, isSubService) < 0) {
DBG("[zapit] zapit failed, chid %llx\n", channel_id);
		if(event_mode) eventServer->sendEvent((isSubService ? CZapitClient::EVT_ZAP_SUB_FAILED : CZapitClient::EVT_ZAP_FAILED), CEventServer::INITID_ZAPIT, &channel_id, sizeof(channel_id));
		return result;
	}

	result |= CZapitClient::ZAP_OK;

DBG("[zapit] zapit OK, chid %llx\n", channel_id);
	if (isSubService) {
DBG("[zapit] isSubService chid %llx\n", channel_id);
		if(event_mode) eventServer->sendEvent(CZapitClient::EVT_ZAP_SUB_COMPLETE, CEventServer::INITID_ZAPIT, &channel_id, sizeof(channel_id));
	}
	else if (current_is_nvod) {
DBG("[zapit] NVOD chid %llx\n", channel_id);
		if(event_mode) eventServer->sendEvent(CZapitClient::EVT_ZAP_COMPLETE_IS_NVOD, CEventServer::INITID_ZAPIT, &channel_id, sizeof(channel_id));
		result |= CZapitClient::ZAP_IS_NVOD;
	}
	else
		if(event_mode) eventServer->sendEvent(CZapitClient::EVT_ZAP_COMPLETE, CEventServer::INITID_ZAPIT, &channel_id, sizeof(channel_id));

	return result;
}

unsigned zapTo(const unsigned int pchannel)
{
	CBouquetManager::ChannelIterator cit = ((currentMode & RADIO_MODE) ? g_bouquetManager->radioChannelsBegin() : g_bouquetManager->tvChannelsBegin()).FindChannelNr(pchannel);
	if (!(cit.EndOfChannels()))
		return zapTo_ChannelID((*cit)->getChannelID(), false);
	else
		return 0;
}

void signal_handler(int signum)
{
	signal (signum, SIG_IGN);
	printf("signal %d received\n", signum); fflush(stdout);
	switch (signum) {
	case SIGUSR1:
		zapit_debug = !zapit_debug;
		break;
	default:
                CZapitClient zapit;
                zapit.shutdown();
                break;
	}
}

int zapit_main_thread(void *data)
{
	Z_start_arg *ZapStart_arg = (Z_start_arg *) data;
	int	video_mode = ZapStart_arg->video_mode;

	time_t stime;
	printf("[zapit] starting... tid %ld\n", syscall(__NR_gettid));
	abort_zapit = 0;

	pcrDemux = new cDemux();
	videoDemux = new cDemux();
	videoDemux->Open(DMX_VIDEO_CHANNEL);

	pcrDemux->Open(DMX_PCR_ONLY_CHANNEL, videoDemux->getBuffer());

#if 0
	videoDecoder = new cVideo(VIDEO_STD_PAL, videoDemux->getChannel(), videoDemux->getBuffer());//PAL
	videoDecoder->Standby(false);
	videoDecoder->SetVideoSystem(video_mode);
#else
	videoDecoder = new cVideo(video_mode, videoDemux->getChannel(), videoDemux->getBuffer());//PAL
	videoDecoder->Standby(false);
#endif
	//videoDecoder = new cVideo(video_mode, videoDemux->getChannel(), videoDemux->getBuffer());//PAL

	audioDemux = new cDemux();
	audioDemux->Open(DMX_AUDIO_CHANNEL);

	audioDecoder = new cAudio(audioDemux->getBuffer(), videoDecoder->GetTVEnc(), videoDecoder->GetTVEncSD());
	videoDecoder->SetAudioHandle(audioDecoder->GetHandle());

	videoDecoder->OpenVBI(1);

	ca = cCA::GetInstance();

	scan_runs = 0;
	found_channels = 0;
	curr_sat = 0;

	frontend = new CFrontend();

	/* load configuration or set defaults if no configuration file exists */
	loadZapitSettings();

	/* CA_INIT_CI or CA_INIT_SC or CA_INIT_BOTH */
	switch(cam_ci){
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
	ca->Start();

	/* create bouquet manager */
	g_bouquetManager = new CBouquetManager();

	if (config.getInt32("lastChannelMode", 0)){
		setRadioMode();
	}
	else{
		setTVMode();
	}
	if(ZapStart_arg->uselastchannel == 0){
		live_channel_id = (currentMode & RADIO_MODE) ? ZapStart_arg->startchannelradio_id : ZapStart_arg->startchanneltv_id ;
		lastChannelRadio = ZapStart_arg->startchannelradio_nr;
		lastChannelTV    = ZapStart_arg->startchanneltv_nr;
	}

	if (prepare_channels(frontend->getInfo()->type, diseqcType) < 0)
		WARN("error parsing services");
	else
		INFO("channels have been loaded succesfully");

	CBasicServer zapit_server;

	if (!zapit_server.prepare(ZAPIT_UDS_NAME))
		return -1;

	eventServer = new CEventServer;

	tallchans_iterator cit;
	cit = allchans.find(live_channel_id);
	if(cit != allchans.end())
		g_current_channel = &(cit->second);

	zapit_ready = 1;
	// some older? hw needs this sleep. e.g. my hd-1c.
	// if sleep is not set -> blackscreen after boot.
	// sleep(1) is ok here. (striper)
	sleep(1);
	leaveStandby();
	//firstzap = false;
	stime = time(0);
	//time_t curtime;
	pthread_create (&tsdt, NULL, sdt_thread, (void *) NULL);
	while (zapit_server.run(zapit_parse_command, CZapitMessages::ACTVERSION, true)) {
		if (pmt_update_fd != -1) {
			unsigned char buf[4096];
			int ret = pmtDemux->Read(buf, 4095, 10);
				if (ret > 0) {
#if HAVE_TRIPLEDRAGON
					pmt_stop_update_filter(&pmt_update_fd);
#endif
					printf("[zapit] pmt updated, sid 0x%x new version 0x%x\n", (buf[3] << 8) + buf[4], (buf[5] >> 1) & 0x1F);
					if(g_current_channel) {
						int vpid = g_current_channel->getVideoPid();
						parse_pmt(g_current_channel);
						if(vpid != g_current_channel->getVideoPid()) {
							zapit(g_current_channel->getChannelID(), current_is_nvod, true);
						} else {
							if(event_mode)  {
								t_channel_id channel_id = g_current_channel->getChannelID();
								eventServer->sendEvent(CZapitClient::EVT_PMT_CHANGED, CEventServer::INITID_ZAPIT, &channel_id, sizeof(channel_id));
							}
							start_camd(true);
							pmt_set_update_filter(g_current_channel, &pmt_update_fd);
						}
					}
				}
			}
		/* yuck, don't waste that much cpu time :) */
		usleep(0);
#if 0
		if(!standby && !scan_runs &&g_current_channel) {
			curtime = time(0);
			if(sig_delay && (curtime - stime) > sig_delay) {
				stime = curtime;
				uint16_t sig  = frontend->getSignalStrength();
				//if(sig < 8000)
				if(sig < 28000) {
					printf("[monitor] signal %d, trying to re-tune...\n", sig);
					frontend->retuneChannel();
				}
			}
		}
#endif
	}

	if (g_current_channel) {
		audio_map[g_current_channel->getChannelID()].apid = g_current_channel->getAudioPid();
		audio_map[g_current_channel->getChannelID()].mode = audio_mode;
		audio_map[g_current_channel->getChannelID()].volume = volume_right;
	}

	stopPlaying();
//	enterStandby();

	SaveMotorPositions();

	zapit_ready = 0;
	pthread_join (tsdt, NULL);
	INFO("shutdown started");

	videoDecoder->CloseVBI();

	if (pcrDemux)
		delete pcrDemux;
	if (pmtDemux)
		delete pmtDemux;
	if (audioDecoder)
		delete audioDecoder;
	if (videoDecoder)
		delete videoDecoder;
	if (videoDemux)
		delete videoDemux;
	if (audioDemux)
		delete audioDemux;

	INFO("demuxes/decoders deleted");

	if (frontend) {
		frontend->Close();
		delete frontend;
	}
	INFO("frontend deleted");
	if (ca) {
		INFO("stopping CA");
		ca->Stop();
		delete ca;
	}
	INFO("shutdown complete");
	return 0;
}

void send_ca_id(int caid)
{
  if(event_mode) eventServer->sendEvent(CZapitClient::EVT_ZAP_CA_ID, CEventServer::INITID_ZAPIT, &caid, sizeof(int));
}

void setZapitConfig(Zapit_config * Cfg)
{
	printf("[zapit] %s...\n", __FUNCTION__);
	motorRotationSpeed = Cfg->motorRotationSpeed;
	config.setInt32("motorRotationSpeed", motorRotationSpeed);
	config.setBool("writeChannelsNames", Cfg->writeChannelsNames);
	config.setBool("makeRemainingChannelsBouquet", Cfg->makeRemainingChannelsBouquet);
	config.setBool("saveLastChannel", Cfg->saveLastChannel);
	fastZap = Cfg->fastZap;
	sortNames = Cfg->sortNames;
	sortlist = sortNames;
	highVoltage = Cfg->highVoltage;
	scan_pids = Cfg->scanPids;
	rezapTimeout = Cfg->rezapTimeout;
	feTimeout = Cfg->feTimeout;
	useGotoXX = Cfg->useGotoXX;
	gotoXXLaDirection = Cfg->gotoXXLaDirection;
	gotoXXLoDirection = Cfg->gotoXXLoDirection;
	gotoXXLatitude = Cfg->gotoXXLatitude;
	gotoXXLongitude = Cfg->gotoXXLongitude;
	repeatUsals = Cfg->repeatUsals;

	scanSDT = Cfg->scanSDT;
	saveZapitSettings(true, false);
}

void sendConfig(int connfd)
{
	Zapit_config Cfg;
	printf("[zapit] %s...\n", __FUNCTION__);
	Cfg.motorRotationSpeed = motorRotationSpeed;
	Cfg.writeChannelsNames = config.getBool("writeChannelsNames", true);
	Cfg.makeRemainingChannelsBouquet = config.getBool("makeRemainingChannelsBouquet", true);
	Cfg.saveLastChannel = config.getBool("saveLastChannel", true);
	Cfg.fastZap = fastZap;
	Cfg.sortNames = sortNames;
	Cfg.highVoltage = highVoltage;
	Cfg.scanPids = scan_pids;
	Cfg.rezapTimeout = rezapTimeout;
	Cfg.feTimeout = feTimeout;
	Cfg.scanSDT = scanSDT;
	Cfg.useGotoXX = useGotoXX;
	Cfg.gotoXXLaDirection = gotoXXLaDirection;
	Cfg.gotoXXLoDirection = gotoXXLoDirection;
	Cfg.gotoXXLatitude = gotoXXLatitude;
	Cfg.gotoXXLongitude = gotoXXLongitude;
	Cfg.repeatUsals = repeatUsals;

	CBasicServer::send_data(connfd, &Cfg, sizeof(Cfg));
}

void getZapitConfig(Zapit_config *Cfg)
{
	printf("[zapit] %s...\n", __FUNCTION__);
        Cfg->motorRotationSpeed = motorRotationSpeed;
        Cfg->writeChannelsNames = config.getBool("writeChannelsNames", true);
        Cfg->makeRemainingChannelsBouquet = config.getBool("makeRemainingChannelsBouquet", true);
        Cfg->saveLastChannel = config.getBool("saveLastChannel", true);
        Cfg->fastZap = fastZap;
        Cfg->sortNames = sortNames;
        Cfg->highVoltage = highVoltage;
        Cfg->scanPids = scan_pids;
        Cfg->rezapTimeout = rezapTimeout;
        Cfg->feTimeout = feTimeout;
        Cfg->scanSDT = scanSDT;
        Cfg->useGotoXX = useGotoXX;
        Cfg->gotoXXLaDirection = gotoXXLaDirection;
        Cfg->gotoXXLoDirection = gotoXXLoDirection;
        Cfg->gotoXXLatitude = gotoXXLatitude;
        Cfg->gotoXXLongitude = gotoXXLongitude;
	Cfg->repeatUsals = repeatUsals;
}

sdt_tp_t sdt_tp;
void * sdt_thread(void * /*arg*/)
{
	time_t tstart, tcur, wtime = 0;
	int ret;
	t_transport_stream_id           transport_stream_id = 0;
	t_original_network_id           original_network_id = 0;
	t_satellite_position            satellitePosition = 0;
	freq_id_t                       freq = 0;
	transponder_id_t 		tpid = 0;
	FILE * fd = 0;
	FILE * fd1 = 0;
	bool updated = 0;

	tcur = time(0);
	tstart = time(0);
	sdt_tp.clear();
printf("[zapit] sdt monitor started\n");
	while(zapit_ready) {
		sleep(1);
		if(sdt_wakeup) {
			sdt_wakeup = 0;
			if(g_current_channel) {
				wtime = time(0);
				transport_stream_id = g_current_channel->getTransportStreamId();
				original_network_id = g_current_channel->getOriginalNetworkId();
				satellitePosition = g_current_channel->getSatellitePosition();
				freq = g_current_channel->getFreqId();
				tpid = g_current_channel->getTransponderId();
			}
		}
		if(!scanSDT)
			continue;

		tcur = time(0);
		if(wtime && ((tcur - wtime) > 2) && !sdt_wakeup) {
printf("[sdt monitor] wakeup...\n");
			wtime = 0;

			if(scan_runs || (currentMode & RECORD_MODE))
				continue;

			updated = 0;
			tallchans_iterator ccI;
			tallchans_iterator dI;
			transponder_list_t::iterator tI;
			sdt_tp_t::iterator stI;
			char tpstr[256];
			char satstr[256];
			bool tpdone = 0;
			bool satfound = 0;

			tI = transponders.find(tpid);
			if(tI == transponders.end()) {
				printf("[sdt monitor] tp not found ?!\n");
				continue;
			}
			stI = sdt_tp.find(tpid);

			if ((stI != sdt_tp.end()) && ((time_monotonic() - stI->second) < 3600)) {
				printf("[sdt monitor] TP already updated less than an hour ago.\n");
				continue;
			}
			if(!curchans.empty())
				curchans.clear();

			ret = parse_current_sdt(transport_stream_id, original_network_id, satellitePosition, freq);
			if(ret){
				if(ret == -1)
					printf("[sdt monitor] scanSDT broken ?\n");
				continue;
			}
			sdt_tp.insert(std::pair <transponder_id_t, time_t> (tpid, time_monotonic()));

			char buffer[256];
			fd = fopen(CURRENTSERVICES_TMP, "w");
			if(!fd) {
				printf("[sdt monitor] " CURRENTSERVICES_TMP ": cant open!\n");
				continue;
			}
#if 0
                	std::map<string, t_satellite_position>::iterator spos_it;
                	for (spos_it = satellitePositions.begin(); spos_it != satellitePositions.end(); spos_it++) {
				if(spos_it->second == satellitePosition)
					break;
			}
#endif
			sat_iterator_t spos_it = satellitePositions.find(satellitePosition);
			if(spos_it == satellitePositions.end())
				continue;

                        switch (frontend->getInfo()->type) {
                                case FE_QPSK: /* satellite */
					sprintf(satstr, "\t<%s name=\"%s\" position=\"%hd\">\n", "sat", spos_it->second.name.c_str(), satellitePosition);
                                        sprintf(tpstr, "\t\t<TS id=\"%04x\" on=\"%04x\" frq=\"%u\" inv=\"%hu\" sr=\"%u\" fec=\"%hu\" pol=\"%hu\">\n",
                                        tI->second.transport_stream_id, tI->second.original_network_id,
                                        tI->second.feparams.frequency, tI->second.feparams.inversion,
                                        tI->second.feparams.u.qpsk.symbol_rate, tI->second.feparams.u.qpsk.fec_inner,
                                        tI->second.polarization);
                                        break;
                                case FE_QAM: /* cable */
					sprintf(satstr, "\t<%s name=\"%s\"\n", "cable", spos_it->second.name.c_str());
                                        sprintf(tpstr, "\t\t<TS id=\"%04x\" on=\"%04x\" frq=\"%u\" inv=\"%hu\" sr=\"%u\" fec=\"%hu\" mod=\"%hu\">\n",
                                        tI->second.transport_stream_id, tI->second.original_network_id,
                                        tI->second.feparams.frequency, tI->second.feparams.inversion,
                                        tI->second.feparams.u.qam.symbol_rate, tI->second.feparams.u.qam.fec_inner,
                                        tI->second.feparams.u.qam.modulation);
                                        break;
                                case FE_OFDM:
                                default:
                                        break;
                        }
			fd1 = fopen(CURRENTSERVICES_XML, "r");
			if(!fd1) {
				fprintf(fd, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<zapit>\n");
			} else {
				fgets(buffer, 255, fd1);
				while(!feof(fd1) && !strstr(buffer, satfound ? "</sat>" : "</zapit>")) {
					if(!satfound && !strcmp(buffer, satstr))
						satfound = 1;
					fputs(buffer, fd);
					fgets(buffer, 255, fd1);
				}
				//fclose(fd1);
			}
			for (tallchans_iterator cI = curchans.begin(); cI != curchans.end(); cI++) {
				ccI = allchans.find(cI->second.getChannelID());
				if(ccI == allchans.end()) {
					if(!tpdone) {
						if(!satfound)
							fprintf(fd, "%s", satstr);
						fprintf(fd, "%s", tpstr);
						tpdone = 1;
					}
					updated = 1;
#if 0
					fprintf(fd, "\t\t\t<S action=\"add\" i=\"%04x\" n=\"%s [%d%c]\" t=\"%x\"/>\n",
                                        	cI->second.getServiceId(), convert_UTF8_To_UTF8_XML(cI->second.getName().c_str()).c_str(),
                                       		abs(cI->second.getSatellitePosition())/10, cI->second.getSatellitePosition() > 0 ? 'E' : 'W',
                                        	cI->second.getServiceType());
#endif
					fprintf(fd, "\t\t\t<S action=\"add\" i=\"%04x\" n=\"%s\" t=\"%x\" s=\"%d\"/>\n",
                                        	cI->second.getServiceId(), convert_UTF8_To_UTF8_XML(cI->second.getName().c_str()).c_str(),
                                        	cI->second.getServiceType(), cI->second.scrambled);
				} else {
					if(strcmp(cI->second.getName().c_str(), ccI->second.getName().c_str()) || cI->second.scrambled != ccI->second.scrambled) {
					   if(!tpdone) {
						if(!satfound)
							fprintf(fd, "%s", satstr);
						fprintf(fd, "%s", tpstr);
						tpdone = 1;
					   }
					   updated = 1;
					   fprintf(fd, "\t\t\t<S action=\"replace\" i=\"%04x\" n=\"%s\" t=\"%x\" s=\"%d\"/>\n",
                                        	cI->second.getServiceId(), convert_UTF8_To_UTF8_XML(cI->second.getName().c_str()).c_str(),
                                        	cI->second.getServiceType(), cI->second.scrambled);
					}
#if 0
					char newname[128];
					sprintf(newname, "%s [%d%c]", cI->second.getName().c_str(), abs(cI->second.getSatellitePosition())/10, cI->second.getSatellitePosition() > 0 ? 'E' : 'W');
					if(strcmp(newname, ccI->second.getName().c_str())) {
					   if(!tpdone) {
						if(!satfound)
							fprintf(fd, "%s", satstr);
						fprintf(fd, "%s", tpstr);
						tpdone = 1;
					   }
					   updated = 1;
					   fprintf(fd, "\t\t\t<S action=\"replace\" i=\"%04x\" n=\"%s [%d%c]\" t=\"%x\"/>\n",
                                        	cI->second.getServiceId(), convert_UTF8_To_UTF8_XML(cI->second.getName().c_str()).c_str(),
                                        	abs(cI->second.getSatellitePosition())/10, cI->second.getSatellitePosition() > 0 ? 'E' : 'W',
                                        	cI->second.getServiceType());
					}
#endif
				}
			}
			for (ccI = allchans.begin(); ccI != allchans.end(); ccI++) {
				if(ccI->second.getTransponderId() == tpid) {
					dI = curchans.find(ccI->second.getChannelID());
					if(dI == curchans.end()) {
					   if(!tpdone) {
						if(!satfound)
							fprintf(fd, "%s", satstr);
						fprintf(fd, "%s", tpstr);
						tpdone = 1;
					   }
					   updated = 1;
					   fprintf(fd, "\t\t\t<S action=\"remove\" i=\"%04x\" n=\"%s\" t=\"%x\"/>\n",
                                        	ccI->second.getServiceId(), convert_UTF8_To_UTF8_XML(ccI->second.getName().c_str()).c_str(),
                                        	ccI->second.getServiceType());
					}
				}
			}
			if(tpdone) {
				fprintf(fd, "\t\t</TS>\n");
				fprintf(fd, "\t</sat>\n");
			} else if(satfound)
				fprintf(fd, "\t</sat>\n");
			if(fd1) {
				fgets(buffer, 255, fd1);
				while(!feof(fd1)) {
					fputs(buffer, fd);
					fgets(buffer, 255, fd1);
				}
				if(!satfound) fprintf(fd, "</zapit>\n");
				fclose(fd1);
			} else
				fprintf(fd, "</zapit>\n");
			fclose(fd);
			rename(CURRENTSERVICES_TMP, CURRENTSERVICES_XML);
			if(updated && event_mode && (scanSDT == 1))
			  eventServer->sendEvent(CZapitClient::EVT_SDT_CHANGED, CEventServer::INITID_ZAPIT);
			if(!updated)
				printf("[sdt monitor] no changes.\n");
			else
				printf("[sdt monitor] found changes.\n");
		}
	}
	return 0;
}
