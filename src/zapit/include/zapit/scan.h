/*
 *  $Id: scan.h,v 1.26 2003/03/14 07:31:50 obi Exp $
 */

#ifndef __scan_h__
#define __scan_h__

#include <linux/dvb/frontend.h>

#include <inttypes.h>
#include <map>
#include <string>

#include <zapit/femanager.h>
#include <zapit/getservices.h>
#include <zapit/fastscan.h>
#include "bouquets.h"
#include <OpenThreads/Thread>

extern CBouquetManager* scanBouquetManager;

class CServiceScan : public OpenThreads::Thread
{
	public:
		typedef enum scan_type {
			SCAN_PROVIDER,
			SCAN_TRANSPONDER,
			SCAN_FAST
		} scan_type_t;
		typedef enum scan_flags {
			SCAN_NIT		= 0x01,
			SCAN_BAT		= 0x02,
			SCAN_FTA		= 0x04,
			SCAN_RESET_NUMBERS	= 0x08,
			SCAN_LOGICAL_NUMBERS	= 0x10,
			SCAN_TV			= 0x20,
			SCAN_RADIO		= 0x40,
			SCAN_TVRADIO		= 0x60,
			SCAN_DATA		= 0x80,
			SCAN_ALL		= 0xE0
		} scan_flags_t;

	private:
		bool started;
		bool running;
		bool cable;
		bool abort_scan;
		scan_type_t scan_mode;
		int flags;
		void * scan_arg;

		uint32_t fake_tid, fake_nid;
		uint32_t found_transponders;
		uint32_t processed_transponders;
		uint32_t found_channels;
		uint32_t found_tv_chans;
		uint32_t found_radio_chans;
		uint32_t found_data_chans;
		uint32_t failed_transponders;
		unsigned short cable_nid;

		short curr_sat;

		std::map <transponder_id_t, transponder> scantransponders;   // TP list to scan
		std::map <transponder_id_t, transponder> scanedtransponders; // TP list for current scan
		std::map <transponder_id_t, transponder> nittransponders;
		std::map <t_channel_id, uint8_t> service_types;

		bool AddTransponder(xmlNodePtr transponder, uint8_t diseqc_pos, t_satellite_position satellitePosition);
		bool ScanProvider(xmlNodePtr search, t_satellite_position satellitePosition, uint8_t diseqc_pos);
		void Cleanup(const bool success);
		bool tuneFrequency(FrontendParameters *feparams, uint8_t polarization, t_satellite_position satellitePosition);
		bool ReadNitSdt(t_satellite_position satellitePosition);
		void FixServiceTypes();

		bool ScanTransponder();
		bool ScanProviders();
		void SaveServices();

		/* fast scan */
		std::map <t_channel_id, t_satellite_position> fast_services_sat;
		std::map <t_channel_id, freq_id_t> fast_services_freq;
		std::map <t_channel_id, int> fast_services_number;

		void InitFastscanLnb(int id);
		bool ParseFst(unsigned short pid, fast_scan_operator_t * op);
		bool ParseFnt(unsigned short pid, unsigned short operator_id);
		void process_logical_service_descriptor(const unsigned char * const buffer, const t_transport_stream_id transport_stream_id, const t_original_network_id original_network_id, t_satellite_position satellitePosition, freq_id_t freq);
		void process_service_list_descriptor(const unsigned char * const buffer, const t_transport_stream_id transport_stream_id, const t_original_network_id original_network_id, t_satellite_position satellitePosition, freq_id_t freq);
		void process_satellite_delivery_system_descriptor(const unsigned char * const buffer, FrontendParameters * feparams, uint8_t * polarization, t_satellite_position * satellitePosition);
		bool ScanFast();

		void run();

		CFrontend * frontend;
		static CServiceScan * scan;
		CServiceScan();

	public:
		~CServiceScan();
		static CServiceScan * getInstance();

		bool Start(scan_type_t mode, void * arg);
		bool Stop();

		bool AddTransponder(transponder_id_t TsidOnid, FrontendParameters *feparams, uint8_t polarity, bool fromnit = false);
		void ChannelFound(uint8_t service_type, std::string providerName, std::string serviceName);
		void AddServiceType(t_channel_id channel_id, uint8_t service_type);

		bool Scanning() { return running; };
		void Abort() { abort_scan = 1; };
		bool Aborted() { return abort_scan; };

		bool SetFrontend(t_satellite_position satellitePosition);
		CFrontend * GetFrontend() { return frontend; };

		uint32_t & FoundTransponders() { return found_transponders; };
		uint32_t & FoundChannels() { return found_channels; };
		void SetCableNID(unsigned short nid) { cable_nid = nid; }
		bool isFtaOnly() { return flags & SCAN_FTA; }
		int GetFlags() { return flags; }
};

#endif /* __scan_h__ */
