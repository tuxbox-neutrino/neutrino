/*
        Neutrino-GUI  -   DBoxII-Project

        Copyright (C) 2011 CoolStream International Ltd

        License: GPLv2

        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation.

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with this program; if not, write to the Free Software
        Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __scan_h__
#define __scan_h__

#include <linux/dvb/frontend.h>

#include <inttypes.h>
#include <map>
#include <list>
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
			SCAN_ALL		= 0xE0,
			SCAN_LOGICAL_HD		= 0x100
		} scan_flags_t;

	private:
		bool started;
		bool running;
		bool abort_scan;
		scan_type_t scan_mode;
		int flags;
		void * scan_arg;
		bool satHaveChannels;

		uint32_t fake_tid, fake_nid;
		uint32_t found_transponders;
		uint32_t processed_transponders;
		uint32_t found_channels;
		uint32_t found_tv_chans;
		uint32_t found_radio_chans;
		uint32_t found_data_chans;
		uint32_t failed_transponders;
		unsigned short cable_nid;

		transponder_list_t scantransponders;   // list to scan
		transponder_list_t scanedtransponders; // successfully scanned
		transponder_list_t failedtransponders; // failed to tune
		transponder_list_t nittransponders;    // transponders from NIT
		std::map <t_channel_id, uint8_t> service_types;

		bool ScanProvider(t_satellite_position satellitePosition);
		void Cleanup(const bool success);
		bool tuneFrequency(FrontendParameters *feparams, t_satellite_position satellitePosition);
		void SendTransponderInfo(transponder &t);
		bool ReadNitSdt(t_satellite_position satellitePosition);
		bool AddFromNit();
		void FixServiceTypes();

		void CheckSatelliteChannels(t_satellite_position satellitePosition);
		bool ScanTransponder();
		bool ScanProviders();
		void SaveServices();
		void CleanAllMaps();
		//bool ReplaceTransponderParams(freq_id_t freq, t_satellite_position satellitePosition, FrontendParameters *feparams);

		/* fast scan */
		std::map <t_channel_id, t_satellite_position> fast_services_sat;
		std::map <t_channel_id, freq_id_t> fast_services_freq;
		std::map <t_channel_id, int> fast_services_number;
		std::list<std::vector<uint8_t> > fst_sections;
		uint32_t tune_tp_index;

		unsigned char fst_version;
#if ENABLE_FASTSCAN
		bool quiet_fastscan;
		void InitFastscanLnb(int id);
		bool FastscanTune(int id);
		bool ReadFst(unsigned short pid, unsigned short operator_id, bool one_section = false);
		bool ParseFst(unsigned short pid, fast_scan_operator_t * op);
		bool ParseFnt(unsigned short pid, unsigned short operator_id);
		void process_logical_service_descriptor(const unsigned char * const buffer, const t_transport_stream_id transport_stream_id, const t_original_network_id original_network_id, t_satellite_position satellitePosition, freq_id_t freq);
		void process_service_list_descriptor(const unsigned char * const buffer, const t_transport_stream_id transport_stream_id, const t_original_network_id original_network_id, t_satellite_position satellitePosition, freq_id_t freq);
		void process_satellite_delivery_system_descriptor(const unsigned char * const buffer, FrontendParameters * feparams, t_satellite_position * satellitePosition);
		bool ScanFast();
		void ReportFastScan(FrontendParameters &feparams, t_satellite_position satellitePosition);
#endif
		void run();

		CFrontend * frontend;
		static CServiceScan * scan;
		CServiceScan();

	public:
		~CServiceScan();
		static CServiceScan * getInstance();

		bool Start(scan_type_t mode, void * arg);
		bool Stop();

		bool AddTransponder(transponder_id_t TsidOnid, FrontendParameters *feparams, bool fromnit = false);
		void ChannelFound(uint8_t service_type, std::string providerName, std::string serviceName);
		void AddServiceType(t_channel_id channel_id, uint8_t service_type);

		bool Scanning() { return running; };
		void Abort() { abort_scan = true; };
		bool Aborted() { return abort_scan; };

		bool SetFrontend(t_satellite_position satellitePosition);
		CFrontend * GetFrontend() { return frontend; };

		uint32_t & FoundTransponders() { return found_transponders; };
		uint32_t & FoundChannels() { return found_channels; };
		void SetCableNID(unsigned short nid) { cable_nid = nid; }
		bool isFtaOnly() { return flags & SCAN_FTA; }
		int GetFlags() { return flags; }
		bool SatHaveChannels() { return satHaveChannels; }
#if ENABLE_FASTSCAN
		/* fast-scan */
		bool TestDiseqcConfig(int num);
		bool ReadFstVersion(int num);
		unsigned char GetFstVersion() { return fst_version; }
		void QuietFastScan(bool enable) { quiet_fastscan = enable; }
		bool ScanFast(int num, bool reload = true);
#endif
};

#endif /* __scan_h__ */
