/*
  Client-Interface f�r zapit  -   DBoxII-Project

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

#ifndef __zapitclient__
#define __zapitclient__


#include <string>
#include <vector>
#include <OpenThreads/Mutex>

/* zapit */
#include "zapittypes.h"
#include <zapit/frontend_c.h>
#include <connection/basicclient.h>

#define CHANNEL_NAME_SIZE 40

class CZapitClient:public CBasicClient
{
 private:
	virtual unsigned char   getVersion   () const;
	virtual const    char * getSocketName() const;

	OpenThreads::Mutex mutex;

 public:
	enum events
		{
			FIRST_EVENT_MARKER, // <- no actual event, needed by pzapit
			EVT_ZAP_COMPLETE = FIRST_EVENT_MARKER,
			EVT_ZAP_COMPLETE_IS_NVOD,
			EVT_ZAP_FAILED,
			EVT_ZAP_SUB_COMPLETE,
			EVT_ZAP_SUB_FAILED,
			EVT_ZAP_MOTOR,
			EVT_RECORDMODE_ACTIVATED,
			EVT_RECORDMODE_DEACTIVATED,
			EVT_SCAN_COMPLETE,
			EVT_SCAN_FAILED,
			EVT_SCAN_NUM_TRANSPONDERS,
			EVT_SCAN_REPORT_NUM_SCANNED_TRANSPONDERS,
			EVT_SCAN_REPORT_FREQUENCYP,
			EVT_SCAN_SERVICENAME,
			EVT_SCAN_FOUND_A_CHAN,
			EVT_SCAN_FOUND_TV_CHAN,
			EVT_SCAN_FOUND_RADIO_CHAN,
			EVT_SCAN_FOUND_DATA_CHAN,
			EVT_SCAN_SATELLITE,
			EVT_SCAN_NUM_CHANNELS,
			EVT_SCAN_PROVIDER,
			EVT_BOUQUETS_CHANGED,
			EVT_ZAP_CA_CLEAR,
			EVT_ZAP_CA_LOCK,
			EVT_ZAP_CA_FTA,
			EVT_ZAP_CA_ID,
			EVT_SDT_CHANGED,
			EVT_SERVICES_CHANGED,
			EVT_PMT_CHANGED,
			EVT_TUNE_COMPLETE,
			EVT_BACK_ZAP_COMPLETE,
			EVT_WEBTV_ZAP_COMPLETE,
			LAST_EVENT_MARKER // <- no actual event, needed by pzapit
		};

	enum zapStatus
		{
			ZAP_OK = 0x01,
			ZAP_IS_NVOD = 0x02,
			ZAP_INVALID_PARAM = 0x04
		};

	enum bouquetMode
		{
			BM_CREATEBOUQUETS,
			BM_DELETEBOUQUETS,
			BM_DONTTOUCHBOUQUETS,
			BM_UPDATEBOUQUETS,
			BM_CREATESATELLITEBOUQUET
		};

	enum scanType
		{
			ST_TVRADIO,
			ST_TV,
			ST_RADIO,
			ST_ALL
		};

	typedef enum channelsMode_
		{
			MODE_CURRENT,
			MODE_TV,
			MODE_RADIO,
			MODE_ALL
		} channelsMode;

	typedef enum channelsOrder_
		{
			SORT_ALPHA,
			SORT_BOUQUET
		} channelsOrder;


	struct commandAddSubServices
	{
		t_original_network_id original_network_id;
		t_service_id          service_id;
		t_transport_stream_id transport_stream_id;
	};


	struct commandSetScanSatelliteList
	{
		char satName[50];
		int  position;
	};
	typedef std::vector<commandSetScanSatelliteList> ScanSatelliteList;

	struct commandSetScanMotorPosList
	{
		t_satellite_position satPosition;
		int motorPos;
	};
	typedef std::vector<commandSetScanMotorPosList> ScanMotorPosList;

	typedef std::vector<commandAddSubServices> subServiceList;

	struct responseGetLastChannel
	{
		t_channel_id	channel_id;
		int		mode;
	};

	struct responseGetBouquets
	{
		unsigned int bouquet_nr;
		char	name[30];
		bool	locked;
		bool	hidden;
	};

	typedef std::vector<responseGetBouquets> BouquetList;

	struct responseChannels
	{
		unsigned int nr;
		t_channel_id channel_id;
		char name[CHANNEL_NAME_SIZE];
		t_satellite_position satellitePosition;
		unsigned char service_type;
	};

	struct responseGetBouquetChannels : public responseChannels
	{};

	typedef std::vector<responseGetBouquetChannels> BouquetChannelList;

	struct responseNChannels
	{
		unsigned int nr;
	};

	struct responseGetBouquetNChannels : public responseNChannels
	{};

	typedef std::vector<responseGetBouquetNChannels> BouquetNChannelList;
#define DESC_MAX_LEN 38 //component descriptor name length + " (AC3)"
	struct responseGetAPIDs
	{
		uint32_t pid;
		char desc[DESC_MAX_LEN];
		int  is_ac3;
		int  is_aac;
		int  is_aache;
		int  is_eac3;
		int  is_dts;
		int  is_dtshd;
		int  is_lpcm;
		int  component_tag;
	};

	typedef std::vector<responseGetAPIDs> APIDList;

	struct responseGetOtherPIDs
	{
		uint		vpid;
		uint		ecmpid;
		uint		vtxtpid;
		uint		pcrpid;
		uint		selected_apid;
		uint		pmtpid;
		uint		privatepid;
	};

	class CCurrentServiceInfo
		{
		public:
			t_original_network_id onid;
			t_service_id           sid;
			t_transport_stream_id tsid;
			unsigned short vpid;
			unsigned short vtype;
			unsigned short apid;
			unsigned short pcrpid;
			unsigned short vtxtpid;
			unsigned int   tsfrequency;
			unsigned char  polarisation;
			unsigned char  diseqc;
			unsigned short pmtpid;
			unsigned short pmt_version;
			uint32_t	rate;
			fe_code_rate	fec;
		};

	struct responseGetPIDs
	{
		responseGetOtherPIDs	PIDs;
		APIDList		APIDs;
	};

	struct responseGetSatelliteList
	{
		char satName[50];
		t_satellite_position satPosition;
		uint8_t motorPosition;
		int satDiseqc;
	};
	typedef std::vector<responseGetSatelliteList> SatelliteList;


	struct responseFESignal
	{
		unsigned int  sig;
		unsigned int  snr;
		unsigned long ber;
		// maybe later...
		// int   has_lock;
		// int   has_signal;
		// int   has_sync;
		// int   has_carrier;
	};


 public:
	/*****************************/
	/*                           */
	/* shutdown the zapit daemon */
	/*                           */
	/*****************************/
	void shutdown();


	/****************************************/
	/*					*/
	/* general functions for zapping	*/
	/*					*/
	/****************************************/

	/* zaps to channel of specified bouquet */
	/* bouquets are numbered starting at 0 */
	void zapTo(const unsigned int bouquet, const unsigned int channel);

	/* zaps to channel  */
	void zapTo(const unsigned int channel);

	/* What zapit answers a stream that asks for a frontend. verdict is
	   0 when granted, 1 when no frontend may be had without changing what
	   the box shows, 2 for a channel that needs none or does not exist, 3
	   when zapit stops, the time ran out or the frontend did not tune.
	   hold_id is the number the stream adopts its hold under, fenumber
	   the frontend. */
	struct responseStreamHold
	{
		int32_t  verdict;
		uint32_t hold_id;
		int32_t  fenumber;
		/* Asked for, on a refusal: 1 when switching the picture to the
		   channel would free a frontend for it. */
		int32_t  switch_frees;
		responseStreamHold() : verdict(3), hold_id(0), fenumber(-1), switch_frees(0) {}
	};

	/* Has zapit hold a frontend for a stream of channel_id, giving it
	   budget_ms from now to have it tuned. False when no whole answer
	   came; reply is then a failure without a hold. A Deadline on the
	   calling thread bounds the exchange itself. */
	bool holdForStream(const t_channel_id channel_id, unsigned budget_ms, responseStreamHold &reply,
			   bool ask_switch = false);
	/* Whether a stream of channel_id would get a frontend now, without
	   holding one. */
	bool probeStream(const t_channel_id channel_id, responseStreamHold &reply, bool ask_switch = false);

	/* What zapit answers the GUI asking it to switch the picture for a
	   stream: 1 when the picture shows the channel, 2 when a frontend is free
	   for the stream as things are and nothing was switched, 0 otherwise. */
	struct responseStreamSwitch
	{
		int32_t switched;
		responseStreamSwitch() : switched(0) {}
	};

	/* Has zapit switch the picture to channel_id for a stream that found no
	   frontend, if the prediction of the hold holds again; it waits for the
	   zap. False when no whole answer came, and outcome is then 0. */
	bool switchForStream(const t_channel_id channel_id, int &outcome);

	/* What zapit answers a recording that asks it to end the streams in
	   its way. stage is 1 when a frontend is to be had as the frontends
	   are, 2 when it is once the streams in fe_mask end, 3 when it is once
	   the picture's frontend is given up as well, 0 when none is, and -1
	   when zapit did nothing: it stops, or too little time was left. Bit n
	   of fe_mask: zapit took back the stream holds on frontend number n;
	   the streams on it are still the caller's to stop. */
	struct responseStreamYield
	{
		int32_t  stage;
		uint32_t fe_mask;
		responseStreamYield() : stage(-1), fe_mask(0) {}
	};

	/* Has zapit end the stream holds a recording of channel_id needs,
	   giving it budget_ms from now; channel_id 0 only has it give back
	   the holds dropped so far. False when no whole answer came; reply is
	   then one that ended nothing. A Deadline on the calling thread bounds
	   the exchange itself. */
	bool yieldStreams(const t_channel_id channel_id, unsigned budget_ms, responseStreamYield &reply);
 private:
	bool streamHoldAnswer(bool sent, responseStreamHold &reply);
 public:

	/* zaps to channel, returns the "zap-status" */
	unsigned int zapTo_serviceID(const t_channel_id channel_id);
	unsigned int zapTo_record(const t_channel_id channel_id);
	unsigned int zapTo_pip(const t_channel_id channel_id, int pip = 0);
	unsigned int zapTo_epg(const t_channel_id channel_id, bool standby = false);

	/* zaps to subservice, returns the "zap-status" */
	unsigned int zapTo_subServiceID(const t_channel_id channel_id);

	/* zaps to channel, does NOT wait for completion (uses event) */
	void zapTo_serviceID_NOWAIT(const t_channel_id channel_id);

	/* zaps to subservice, does NOT wait for completion (uses event) */
	void zapTo_subServiceID_NOWAIT(const t_channel_id channel_id);

	/* return the current (tuned) ServiceID */
	t_channel_id getCurrentServiceID();

	/* return the current satellite position */
	//int32_t getCurrentSatellitePosition();

	/* get last channel-information */
	//void getLastChannel(t_channel_id &channel_id, int &mode);

	/* audiochan set */
	void setAudioChannel(const unsigned int channel);

	/* gets all bouquets */
	/* bouquets are numbered starting at 0 */
	void getBouquets( BouquetList& bouquets, const bool emptyBouquetsToo = false, const bool utf_encoded = false, channelsMode mode = MODE_CURRENT);

 private:
	bool receive_channel_list(BouquetChannelList& channels, const bool utf_encoded);
	bool receive_nchannel_list(BouquetNChannelList& channels);

 public:
	/* gets all channels that are in specified bouquet */
	/* bouquets are numbered starting at 0 */
	bool getBouquetChannels(const unsigned int bouquet, BouquetChannelList& channels, const channelsMode mode = MODE_CURRENT, const bool utf_encoded = false);
	//bool getBouquetNChannels(const unsigned int bouquet, BouquetNChannelList& channels, const channelsMode mode = MODE_CURRENT, const bool utf_encoded = false);

	/* gets all channels */
	bool getChannels(BouquetChannelList& channels, const channelsMode mode = MODE_CURRENT, const channelsOrder order = SORT_BOUQUET, const bool utf_encoded = false);


	/* request information about a particular channel_id */

	/* channel name */
	std::string getChannelName(const t_channel_id channel_id);

	/* is channel a TV channel ? */
	//bool isChannelTVChannel(const t_channel_id channel_id);


	/* get the current_TP */
	//bool get_current_TP(TP_params* TP);

	/* restore bouquets so as if they where just loaded*/
	void restoreBouquets();

	/* reloads channels and services; whether the daemon said it had */
	bool reinitChannels();

	/* whether the daemon answered once it was through with every command sent
	   before this one */
	bool awaitCommands();

	/* called when sectionsd updates currentservices.xml */
	void reloadCurrentServices();

	/* get current APID-List */
	void getPIDS( responseGetPIDs& pids );

	/* get info about the current serivice */
	CZapitClient::CCurrentServiceInfo getCurrentServiceInfo();

	/* transfer SubService-List to zapit */
	void setSubServices( subServiceList& subServices );

	/* set Mode */
	void setMode(const channelsMode mode);

	/* set Mode */
	int getMode();

	/* set RecordMode*/
	void setRecordMode(const bool activate);
	void setEventMode(const bool activate);

	/* get RecordMode*/
	bool isRecordModeActive();

	/* mute audio */
	void muteAudio(const bool mute);
	bool getMuteStatus();

	/* set audio volume */
	void setVolume(const unsigned int left, const unsigned int right);
	void getVolume(unsigned int *left, unsigned int *right);

	/* get dvb transmission type */
	delivery_system_t getDeliverySystem(void);

	/* Lock remote control */
	void lockRc(const bool mute);

	void zaptoNvodSubService(const int num);

	/* send diseqc 1.2 motor command */
	void sendMotorCommand(uint8_t cmdtype, uint8_t address, uint8_t cmd, uint8_t num_parameters, uint8_t param1, uint8_t param2);

	//CFrontend GetLiveFrontend() { return live_fe; };

	/****************************************/
	/*					*/
	/* Scanning stuff			*/
	/*					*/
	/****************************************/
	/* start TS-Scan */
#if 0
	bool setConfig(Zapit_config Cfg);
	void getConfig(Zapit_config * Cfg);
#endif
	bool Rezap();
	bool startScan(int scan_mode);
	bool stopScan();
	/* start manual scan */
	bool scan_TP(TP_params TP);

	/* query if ts-scan is ready - response gives status */
	bool isScanReady(unsigned int &satellite, unsigned int &processed_transponder, unsigned int &transponder, unsigned int &services );

	/* query possible satellits*/
	void getScanSatelliteList( SatelliteList& satelliteList );

	/* tell zapit which satellites to scan*/
	void setScanSatelliteList( ScanSatelliteList& satelliteList );

	/* tell zapit stored satellite positions in diseqc 1.2 motor */
	//void setScanMotorPosList( ScanMotorPosList& motorPosList );

	/* set diseqcType*/
	void setDiseqcType(const diseqc_t diseqc);

	/* set diseqcRepeat*/
	void setDiseqcRepeat(const uint32_t repeat);

	/* set diseqcRepeat*/
	void setScanBouquetMode(const bouquetMode mode);

	/* set Scan-Type for channel search */
	//void setScanType(const scanType mode);

	/* get FrontEnd Signal Params */ 
	//void getFESignal (struct responseFESignal& f);

	/****************************************/
	/*					*/
	/* Bouquet editing functions		*/
	/*					*/
	/****************************************/

	/* Each of the ten below hands one command to the channel daemon and says
	   whether it got there. A command that was never written is not a change,
	   and while these answered nothing at all a caller could not tell the two
	   apart: with the daemon's socket gone the box goes on running and a web
	   request was answered as done while the bouquet list stood still.

	   True says the daemon was reached, not that it did what was asked. Only
	   the save at the end of the list is acknowledged, so every caller reads
	   its work back either way. Nothing changes for a caller that ignores the
	   answer, which is every caller these had before. */

	/* adds bouquet at the end of the bouquetlist*/
	bool addBouquet(const char * const name); // UTF-8 encoded

	/* moves a bouquet from one position to another */
	/* bouquets are numbered starting at 0 */
	bool moveBouquet(const unsigned int bouquet, const unsigned int newPos);
	/* deletes a bouquet with all its channels */
	/* bouquets are numbered starting at 0 */
	bool deleteBouquet(const unsigned int bouquet);

	/* assigns new name to bouquet*/
	/* bouquets are numbered starting at 0 */
	bool renameBouquet(const unsigned int bouquet, const char * const newName); // UTF-8 encoded

	/* moves a channel of a bouquet from one position to another, channel lists begin at position=1*/
	/* bouquets are numbered starting at 0 */
	//void moveChannel(const unsigned int bouquet, unsigned int oldPos, unsigned int newPos, channelsMode mode = MODE_CURRENT);

	// -- check if Bouquet-Name exists (2002-04-02 rasc)
	// -- Return bq_id or -1
	/* bouquets are numbered starting at 0 */
	signed int existsBouquet(const char * const name); // UTF-8 encoded


	// -- check if Channel already in Bouquet (2002-04-05 rasc)
	// -- Return true/false
	/* bouquets are numbered starting at 0 */
	//bool  existsChannelInBouquet(const unsigned int bouquet, const t_channel_id channel_id);


	/* adds a channel at the end of then channel list to specified bouquet */
	/* same channels can be in more than one bouquet */
	/* bouquets can contain both tv and radio channels */
	/* bouquets are numbered starting at 0 */
	bool addChannelToBouquet(const unsigned int bouquet, const t_channel_id channel_id);

	/* removes a channel from specified bouquet */
	/* bouquets are numbered starting at 0 */
	bool removeChannelFromBouquet(const unsigned int bouquet, const t_channel_id channel_id);

	/* set a bouquet's lock-state*/
	/* bouquets are numbered starting at 0 */
	bool setBouquetLock(const unsigned int bouquet, const bool lock);

	/* set a bouquet's hidden-state*/
	/* bouquets are numbered starting at 0 */
	bool setBouquetHidden(const unsigned int bouquet, const bool hidden);

	/* renums the channellist, means gives the channels new numbers */
	/* based on the bouquet order and their order within bouquets */
	/* necessarily after bouquet editing operations*/
	bool renumChannellist();

	/* saves current bouquet configuration to bouquets.xml */
	/* answers whether the daemon saved both files, not whether it was asked */
	bool saveBouquets(const bool saveall = false) __attribute__((warn_unused_result));

	/****************************************/
	/*					*/
	/* blah functions			*/
	/*					*/
	/****************************************/

	void setStandby(const bool enable);
	void startPlayBack(const bool sendpmt = false);
	void stopPlayBack(const bool sendpmt = false);
	void stopPip(int pip = 0);
	void lockPlayBack(const bool sendpmt = true);
	void unlockPlayBack(const bool sendpmt = true);
	bool tune_TP(TP_params TP);
	bool isPlayBackActive();
	//void setDisplayFormat(const video_display_format_t mode);
	void setAudioMode(int mode);
	//void getAudioMode(int * mode);
	void setVideoSystem(int video_system);
	void getOSDres(int *mosd);
	void setOSDres(int mosd);
	void getAspectRatio(int *ratio);
	void setAspectRatio(int ratio);
	void getMode43(int *m43);
	void setMode43(int m43);
	void getVideoFormat(int *vf);

	/****************************************/
	/*					*/
	/* Event functions			*/
	/*					*/
	/****************************************/

	/*
	  ein beliebiges Event anmelden
	*/
	void registerEvent(const unsigned int eventID, const unsigned int clientID, const char * const udsName);

	/*
	  ein beliebiges Event abmelden
	*/
	void unRegisterEvent(const unsigned int eventID, const unsigned int clientID);

	virtual ~CZapitClient() {};
};

#define PAL	0
#define NTSC	1

#endif
