/*
 * $Id: zapit.h,v 1.68 2004/10/27 16:08:40 lucgas Exp $
 */

#ifndef __zapit_h__
#define __zapit_h__

#include <OpenThreads/Thread>
#include <configfile.h>
#include <eventserver.h>
#include <connection/basicserver.h>
#include <xmltree/xmlinterface.h>
#include <ca_cs.h>

#include "client/zapitclient.h"

#include <zapit/channel.h>
#include <zapit/bouquets.h>
#include <zapit/femanager.h>
#include <zapit/fastscan.h>

#define PAL	0
#define NTSC	1
#define AUDIO_CONFIG_FILE	CONFIGDIR "/zapit/audio.conf"
#define VOLUME_CONFIG_FILE	CONFIGDIR "/zapit/volume.conf"

typedef std::map<t_channel_id, audio_map_set_t> audio_map_t;
typedef audio_map_t::iterator audio_map_iterator_t;
typedef std::map<transponder_id_t, time_t> sdt_tp_map_t;

typedef std::pair<int, int> pid_pair_t;
typedef std::pair<t_channel_id, pid_pair_t> volume_pair_t;
typedef std::multimap<t_channel_id, pid_pair_t> volume_map_t;
typedef volume_map_t::iterator volume_map_iterator_t;
typedef std::pair<volume_map_iterator_t,volume_map_iterator_t> volume_map_range_t;

#define VOLUME_PERCENT_AC3 100
#define VOLUME_PERCENT_PCM 100

/* complete zapit start thread-parameters in a struct */
typedef struct ZAPIT_start_arg
{
        t_channel_id startchanneltv_id;
        t_channel_id startchannelradio_id;
        int uselastchannel;
        int video_mode;
	int volume;
        int ci_clock;
} Z_start_arg;

typedef struct Zapit_config {
        int writeChannelsNames;
        int makeRemainingChannelsBouquet;
        int saveLastChannel;
        int rezapTimeout;
        int fastZap;
        int sortNames;
        int scanPids;
        int scanSDT;
        int cam_ci;
        int useGotoXX;
        /* FE common */
        int feTimeout;
        int gotoXXLaDirection;
        int gotoXXLoDirection;
        double gotoXXLatitude;
        double gotoXXLongitude;
        int repeatUsals;
        /* FE specific */
        int highVoltage;
        int motorRotationSpeed;
} t_zapit_config;


class CZapitSdtMonitor : public OpenThreads::Thread
{
	private:
		bool started;
		bool sdt_wakeup;

		sdt_tp_map_t sdt_tp;

		void run();

	public:
		CZapitSdtMonitor();
		~CZapitSdtMonitor();
		bool Start();
		bool Stop();
		void Wakeup();
};

class CZapit : public OpenThreads::Thread
{
	private:
		enum {
			TV_MODE = 0x01,
			RADIO_MODE = 0x02,
			RECORD_MODE = 0x04
		};

		OpenThreads::Mutex	mutex;
		bool started;
		bool event_mode;
		bool firstzap;
		bool playing;
		bool list_changed;

		int audio_mode;
		int def_audio_mode;
		int aspectratio;
		int mode43;
#if 0
		unsigned int volume_left;
		unsigned int volume_right;
#endif
		int current_volume;
		int volume_percent;

		int currentMode;
		bool playbackStopForced;
		//diseqc_t diseqcType;

		//int video_mode;
		//Zapit_config config;
		//CConfigFile configfile;
		CEventServer *eventServer;
		CBasicServer zapit_server;

		CZapitChannel * current_channel;
		t_channel_id live_channel_id;
		t_channel_id pip_channel_id;
		/* scan params */
		TP_params TP;
		fast_scan_type_t scant;

		CFrontend * live_fe;
		CFrontend * pip_fe;

		audio_map_t audio_map;
		volume_map_t vol_map;
		//bool current_is_nvod;
		//bool standby;
		t_channel_id  lastChannelRadio;
		t_channel_id  lastChannelTV;
		int abort_zapit;
		int pmt_update_fd;

		//void LoadAudioMap();
		void SaveAudioMap();
		void SaveVolumeMap();
		void SaveSettings(bool write_conf);
		//void SaveChannelPids(CZapitChannel* channel);
		void RestoreChannelPids(CZapitChannel* channel);
		//void ConfigFrontend();

		bool TuneChannel(CFrontend *frontend, CZapitChannel * channel, bool &transponder_change);
		bool ParsePatPmt(CZapitChannel * channel);

		bool send_data_count(int connfd, int data_count);
		void sendAPIDs(int connfd);
		void internalSendChannels(int connfd, ZapitChannelList* channels, const unsigned int first_channel_nr, bool nonames);
		void sendBouquets(int connfd, const bool emptyBouquetsToo, CZapitClient::channelsMode mode);
		void sendBouquetChannels(int connfd, const unsigned int bouquet, const CZapitClient::channelsMode mode, bool nonames);
		void sendChannels(int connfd, const CZapitClient::channelsMode mode, const CZapitClient::channelsOrder order);
		void SendConfig(int connfd);
		void SendCmdReady(int connfd);

		bool StartPlayBack(CZapitChannel *thisChannel);
		//bool StopPlayBack(bool send_pmt);
		void SendPMT(bool forupdate = false);
		void SetAudioStreamType(CZapitAudioChannel::ZapitAudioChannelType audioChannelType);

		void enterStandby();
		//void leaveStandby();
		unsigned int ZapTo(const unsigned int bouquet, const unsigned int pchannel);
		unsigned int ZapTo(t_channel_id channel_id, bool isSubService = false);
		unsigned int ZapTo(const unsigned int pchannel);
		void PrepareScan();

		//CZapitSdtMonitor SdtMonitor;

		void run();
	protected:
		diseqc_t diseqcType;
		int video_mode;
		CConfigFile configfile;
		bool current_is_nvod;
		bool standby;
		Zapit_config config;
		CZapitSdtMonitor SdtMonitor;
		void LoadAudioMap();
		void LoadVolumeMap();
		void SaveChannelPids(CZapitChannel* channel);
		virtual void ConfigFrontend();
		bool StopPlayBack(bool send_pmt);
		virtual void leaveStandby();

		static CZapit * zapit;
		CZapit();
	public:
		~CZapit();
		static CZapit * getInstance();

		virtual void LoadSettings();
		virtual bool Start(Z_start_arg* ZapStart_arg);
		bool Stop();
		bool ParseCommand(CBasicMessage::Header &rmsg, int connfd);
		bool ZapIt(const t_channel_id channel_id, bool for_update = false, bool startplayback = true);
		bool ZapForRecord(const t_channel_id channel_id);
		bool ChangeAudioPid(uint8_t index);
		void SetRadioMode();
		void SetTVMode();
		void SetRecordMode(bool enable);
		int getMode();

		bool PrepareChannels();
		bool StartScan(int scan_mode);
		bool StartScanTP(TP_params * TPparams);
//		bool StartFastScan(int scan_mode, int opid);

		void addChannelToBouquet(const unsigned int bouquet, const t_channel_id channel_id);
		void SetConfig(Zapit_config * Cfg);
		void GetConfig(Zapit_config &Cfg);

		virtual void SendEvent(const unsigned int eventID, const void* eventbody = NULL, const unsigned int eventbodysize = 0);

		audio_map_set_t * GetSavedPids(const t_channel_id channel_id);

		/* inlines */
		void Abort() { abort_zapit = 1; };
		bool Recording() { return currentMode & RECORD_MODE; };
		bool makeRemainingChannelsBouquet() { return config.makeRemainingChannelsBouquet; };
		bool scanSDT() { return config.scanSDT; };
		bool scanPids() { return config.scanPids; };
		void scanPids(bool enable) { config.scanPids = enable; };

		void GetAudioMode(int &mode) { mode = audio_mode; }

		CZapitChannel * GetCurrentChannel() { return current_channel; };
		t_channel_id GetCurrentChannelID() { return live_channel_id; };
		t_channel_id GetPipChannelID() { return pip_channel_id; };
		t_channel_id GetLastTVChannel() { return lastChannelTV; }
		t_channel_id GetLastRADIOChannel() { return lastChannelRadio; }
		void SetCurrentChannelID(const t_channel_id channel_id) { live_channel_id = channel_id; };
		void SetLiveFrontend(CFrontend * fe) { if(fe) live_fe = fe; }
		CFrontend * GetLiveFrontend() { return live_fe; };

		int GetPidVolume(t_channel_id channel_id, int pid, bool ac3 = false);
		void SetPidVolume(t_channel_id channel_id, int pid, int percent);
		void SetVolume(int vol);
		int GetVolume() { return current_volume; };
		int SetVolumePercent(int percent);
		bool StartPip(const t_channel_id channel_id);
		bool StopPip();
};
#endif /* __zapit_h__ */
