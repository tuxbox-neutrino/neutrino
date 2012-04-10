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

#define PAL	0
#define NTSC	1
#define AUDIO_CONFIG_FILE "/var/tuxbox/config/zapit/audio.conf"

typedef std::map<t_channel_id, audio_map_set_t> audio_map_t;
typedef audio_map_t::iterator audio_map_iterator_t;
typedef std::map<transponder_id_t, time_t> sdt_tp_map_t;

/* complete zapit start thread-parameters in a struct */
typedef struct ZAPIT_start_arg
{
        t_channel_id startchanneltv_id;
        t_channel_id startchannelradio_id;
        int startchanneltv_nr;
        int startchannelradio_nr;
        int uselastchannel;
        int video_mode;
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
        int uni_scr;
        int uni_qrg;
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

		bool started;
		bool event_mode;
		bool firstzap;
		bool playing;
		bool list_changed;

		int audio_mode;
		int def_audio_mode;
		int aspectratio;
		int mode43;
		unsigned int volume_left;
		unsigned int volume_right;
		unsigned int def_volume_left;
		unsigned int def_volume_right;

		int currentMode;
		bool playbackStopForced;
		diseqc_t diseqcType;

		int video_mode;
		Zapit_config config;
		CConfigFile configfile;
	
		CEventServer *eventServer;
		CBasicServer zapit_server;

		CZapitChannel * current_channel;
		t_channel_id live_channel_id;
		TP_params TP;

		CFrontend * live_fe;

		audio_map_t audio_map;
		bool current_is_nvod;
		bool standby;
		t_channel_id  lastChannelRadio;
		t_channel_id  lastChannelTV;
		int abort_zapit;
		int pmt_update_fd;

		void LoadAudioMap();
		void SaveAudioMap();
		void SaveSettings(bool write);
		void SaveChannelPids(CZapitChannel* channel);
		void RestoreChannelPids(CZapitChannel* channel);
		void ConfigFrontend();

		bool TuneChannel(CFrontend *frontend, CZapitChannel * channel, bool &transponder_change);
		bool ParsePatPmt(CZapitChannel * channel);

		bool send_data_count(int connfd, int data_count);
		void sendAPIDs(int connfd);
		void internalSendChannels(int connfd, ZapitChannelList* channels, const unsigned int first_channel_nr, bool nonames);
		void sendBouquets(int connfd, const bool emptyBouquetsToo, CZapitClient::channelsMode mode);
		void sendBouquetChannels(int connfd, const unsigned int bouquet, const CZapitClient::channelsMode mode, bool nonames);
		void sendChannels(int connfd, const CZapitClient::channelsMode mode, const CZapitClient::channelsOrder order);
		void SendConfig(int connfd);

		bool StartPlayBack(CZapitChannel *thisChannel);
		bool StopPlayBack(bool send_pmt);
		void SendPMT(bool forupdate = false);
		void SetAudioStreamType(CZapitAudioChannel::ZapitAudioChannelType audioChannelType);

		void enterStandby();
		void leaveStandby();
		unsigned int ZapTo(const unsigned int bouquet, const unsigned int pchannel);
		unsigned int ZapTo(t_channel_id channel_id, bool isSubService = false);
		unsigned int ZapTo(const unsigned int pchannel);
		void PrepareScan();

		CZapitSdtMonitor SdtMonitor;

		void run();
	protected:
		static CZapit * zapit;
		CZapit();
	public:
		~CZapit();
		static CZapit * getInstance();

		void LoadSettings();
		bool Start(Z_start_arg* ZapStart_arg);
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
		bool StartFastScan(int scan_mode, int opid);

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

		CZapitChannel * GetCurrentChannel() { return current_channel; };
		t_channel_id GetCurrentChannelID() { return live_channel_id; };
		void SetCurrentChannelID(const t_channel_id channel_id) { live_channel_id = channel_id; };
		void SetLiveFrontend(CFrontend * fe) { if(fe) live_fe = fe; }
		CFrontend * GetLiveFrontend() { return live_fe; };
};
#endif /* __zapit_h__ */
