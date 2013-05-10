/*
 * $Id: scan.cpp,v 1.134 2004/02/10 21:01:14 metallica Exp $
 *
 * (C) 2002-2003 Andreas Oberritter <obi@tuxbox.org>
 *
 * (C) 2009, 2001-2012 Stefan Seyfried
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

#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>

#include <zapit/client/zapitclient.h>
#include <zapit/debug.h>
#include <zapit/scan.h>
#include <zapit/settings.h>
#include <zapit/satconfig.h>
#include <zapit/zapit.h>
#include <xmlinterface.h>
#include <zapit/scansdt.h>
#include <zapit/scannit.h>
#include <zapit/scanbat.h>

//#define USE_BAT

extern CBouquetManager *g_bouquetManager;
extern transponder_list_t transponders; //  defined in zapit.cpp
extern CZapitClient::bouquetMode bouquetMode;
extern int motorRotationSpeed;

CBouquetManager* scanBouquetManager;

CServiceScan * CServiceScan::scan = NULL;

CServiceScan * CServiceScan::getInstance()
{
	if(scan == NULL)
		scan = new CServiceScan();
	return scan;
}

CServiceScan::CServiceScan()
{
	started = false;
	running = false;

	cable_nid = 0;

	frontend = CFEManager::getInstance()->getFE(0);
}

CServiceScan::~CServiceScan()
{
}

bool CServiceScan::Start(scan_type_t mode, void * arg)
{
	if(started)
		return false;

	scan_mode = mode;
	scan_arg = arg;
	abort_scan = false;

        started = true;
        int ret = start();
        return (ret == 0);
}

bool CServiceScan::Stop()
{
	if(!started)
		return false;

	started = false;
	int ret = join();
	return (ret == 0);
}

void CServiceScan::run()
{
	running = true;

	CleanAllMaps();

	found_transponders = 0;
	processed_transponders = 0;
	found_channels = 0;
	found_tv_chans = 0;
	found_radio_chans = 0;
	found_data_chans = 0;
	failed_transponders = 0;
	fake_tid = fake_nid = 0;

	switch(scan_mode) {
		case SCAN_PROVIDER:
			ScanProviders();
			break;
		case SCAN_TRANSPONDER:
			ScanTransponder();
			break;
#if 0
		case SCAN_FAST:
			ScanFast();
			break;
#endif
		default:
			break;
	}
}

void CServiceScan::CleanAllMaps()
{
	scantransponders.clear();
	scanedtransponders.clear();
	failedtransponders.clear();
	nittransponders.clear();
	service_types.clear();
}

bool CServiceScan::tuneFrequency(FrontendParameters *feparams, uint8_t polarization, t_satellite_position satellitePosition)
{
	frontend->setInput(satellitePosition, feparams->dvb_feparams.frequency, polarization);
	int ret = frontend->driveToSatellitePosition(satellitePosition, false); //true);
	if(ret > 0) {
		printf("[scan] waiting %d seconds for motor to turn satellite dish.\n", ret);
		CZapit::getInstance()->SendEvent(CZapitClient::EVT_SCAN_PROVIDER, (void *) "moving rotor", 13);
		for(int i = 0; i < ret; i++) {
			sleep(1);
			if(abort_scan)
				return false;
		}
	}
	/* for unicable, retry tuning two times before assuming it failed */
	int retry = (frontend->getDiseqcType() == DISEQC_UNICABLE) * 2 + 1;
	do {
		ret = frontend->tuneFrequency(feparams, polarization, false);
		if (ret)
			return true;
		if (abort_scan)
			break;
		retry--;
		if (retry) {
			int rand_us = (rand() * 1000000LL / RAND_MAX);
			printf("[scan] SCR retrying tune, retry=%d after %dms\n", retry, rand_us/1000);
			usleep(rand_us);
		}
	} while (retry > 0);
	return false;
}

bool CServiceScan::AddTransponder(transponder_id_t TsidOnid, FrontendParameters *feparams, uint8_t polarity, bool fromnit)
{
	stiterator tI;
	transponder t(frontendType, TsidOnid, *feparams, polarity);
	if (fromnit) {
		for (tI = nittransponders.begin(); tI != nittransponders.end(); ++tI) {
			if(t.compare(tI->second))
				return false;
		}
#if 1 // debug
		if((tI = nittransponders.find(TsidOnid)) != nittransponders.end()) {
			tI->second.dump("[scan] same tid, different params: old");
			t.dump("[scan] same tid, different params: new");
		}
#endif
		t.dump("[scan] add tp from nit:");
		nittransponders.insert(transponder_pair_t(TsidOnid, t));
	} else {
		for (tI = scantransponders.begin(); tI != scantransponders.end(); ++tI) {
			if(t.compare(tI->second))
				return false;
		}
		if (tI == scantransponders.end()) {
			t.dump("[scan] add tp from settings:");
			scantransponders.insert(transponder_pair_t(TsidOnid, t));
		}
	}
	return true;
}

bool CServiceScan::AddFromNit()
{
	scantransponders.clear();
	stiterator tI;
	for (stiterator ntI = nittransponders.begin(); ntI != nittransponders.end(); ++ntI) {
		/* full compare with failed, ignore same transponders */
		for (tI = failedtransponders.begin(); tI != failedtransponders.end(); ++tI) {
			if (ntI->second.compare(tI->second))
				break;
		}
		if (tI != failedtransponders.end()) {
			ntI->second.dump("[scan] not use tp from nit:");
			continue;
		}
		/* partial compare with scaned, ignore same transponders */
		for (tI = scanedtransponders.begin(); tI != scanedtransponders.end(); ++tI) {
			if (ntI->second == tI->second)
				break;
		}
		if (tI == scanedtransponders.end()) {
			ntI->second.dump("[scan] use tp from nit:");
			scantransponders.insert(transponder_pair_t(ntI->first, ntI->second));
		}
		/* common satellites.xml have V/H, update to L/R if found */
		stiterator stI = transponders.find(ntI->first);
		if(stI != transponders.end() && stI->second == ntI->second) {
			stI->second.polarization = ntI->second.polarization;
		}
	}
	nittransponders.clear();
	printf("\n\n[scan] found %d additional transponders from nit\n", (int)scantransponders.size());
	return !scantransponders.empty();
}

bool CServiceScan::ReadNitSdt(t_satellite_position satellitePosition)
{
	stiterator tI;
	printf("[scan] scanning tp from sat/service\n");
#ifdef USE_BAT
	bouquet_map_t bouquet_map;
	channel_number_map_t logical_map;
#endif
	std::string networkName;
	channel_number_map_t nit_logical_map;
	channel_number_map_t nit_hd_logical_map;
_repeat:
	found_transponders += scantransponders.size();
	CZapit::getInstance()->SendEvent ( CZapitClient::EVT_SCAN_NUM_TRANSPONDERS,
			&found_transponders, sizeof(found_transponders));

	for (tI = scantransponders.begin(); tI != scantransponders.end(); ++tI) {
		if(abort_scan)
			return false;

		processed_transponders++;
		/* partial compare with already scanned, skip duplicate */
		bool skip = false;
		for (stiterator ntI = scanedtransponders.begin(); ntI != scanedtransponders.end(); ++ntI) {
			if (ntI->second == tI->second) {
				skip = true;
				break;
			}
		}
		if(skip) {
			tI->second.dump("[scan] skip");
			continue;
		}

		transponder t(frontendType, tI->first, tI->second.feparams,  tI->second.polarization);
		t.dump("[scan] scanning:");
		SendTransponderInfo(tI->second);

		if (!tuneFrequency(&(tI->second.feparams), tI->second.polarization, satellitePosition)) {
			failed_transponders++;
			failedtransponders.insert(transponder_pair_t(t.transponder_id, t));
			continue;
		}
		scanedtransponders.insert(transponder_pair_t(t.transponder_id, t));

		if(abort_scan)
			return false;
		/* partial compare with existent transponders, update params if found */
		for (stiterator ttI = transponders.begin(); ttI != transponders.end(); ++ttI) {
			if(t == ttI->second) {
				ttI->second.dump("[scan] similar tp, old");
				t.dump("[scan] similar tp, new");
				ttI->second.feparams = t.feparams;
				ttI->second.polarization = t.polarization;
				break;
			}
		}

		freq_id_t freq = CREATE_FREQ_ID(tI->second.feparams.dvb_feparams.frequency, frontendType != FE_QPSK);

		CNit nit(satellitePosition, freq, cable_nid);
		if(flags & SCAN_NIT)
			nit.Start();

#ifdef USE_BAT
		CBat bat(satellitePosition, freq);
		if(flags & SCAN_BAT)
			bat.Start();
#endif
		CSdt sdt(satellitePosition, freq);
		bool sdt_parsed = sdt.Parse(tI->second.transport_stream_id, tI->second.original_network_id);

		if(flags & SCAN_NIT) {
			nit.Stop();
			networkName = nit.GetNetworkName();
			channel_number_map_t &lcn = nit.getLogicalMap();
			nit_logical_map.insert(lcn.begin(), lcn.end());
			channel_number_map_t &hdlcn = nit.getHDLogicalMap();
			nit_hd_logical_map.insert(hdlcn.begin(), hdlcn.end());
		}

#ifdef USE_BAT
		if(flags & SCAN_BAT) {
			bat.Stop();
			bouquet_map_t & tmp = bat.getBouquets();
			for(bouquet_map_t::iterator it = tmp.begin(); it != tmp.end(); ++it) {
				bouquet_map[it->first].insert(it->second.begin(), it->second.end());
				printf("########### CServiceScan::ReadNitSdt: bouquet_map [%s] size %d ###########\n", it->first.c_str(), (int)bouquet_map[it->first].size());
			}
			channel_number_map_t &lcn = bat.getLogicalMap();
			logical_map.insert(lcn.begin(), lcn.end());
		}
#endif
		if(!sdt_parsed) {
			printf("[scan] SDT failed !\n");
			continue;
		}

		transponder_id_t TsidOnid = CREATE_TRANSPONDER_ID64(freq, satellitePosition,
				tI->second.original_network_id, tI->second.transport_stream_id);

		stiterator stI = transponders.find(TsidOnid);
		if(stI == transponders.end()) {
			transponder t2(frontendType, TsidOnid, tI->second.feparams, tI->second.polarization);
			transponders.insert(transponder_pair_t(TsidOnid, t2));
		}
		printf("[scan] tpid ready: %" PRIx64 "\n", TsidOnid);
	}
	if((flags & SCAN_NIT) && AddFromNit())
		goto _repeat;

	if ((flags & SCAN_LOGICAL_NUMBERS /*(SCAN_NIT|SCAN_LOGICAL_NUMBERS)*/) && !nit_logical_map.empty()) {
		std::string pname = networkName;
		INFO("network [%s] %d logical channels (%d hd)\n", pname.c_str(), (int)nit_logical_map.size(), (int)nit_hd_logical_map.size());
		g_bouquetManager->loadBouquets(true);
		CServiceManager::getInstance()->ResetChannelNumbers(true, true);
		CZapitBouquet* bouquet;
		int bouquetId = g_bouquetManager->existsUBouquet(pname.c_str());
		if (bouquetId == -1)
			bouquet = g_bouquetManager->addBouquet(pname, true);
		else
			bouquet = g_bouquetManager->Bouquets[bouquetId];

		if (flags & SCAN_LOGICAL_HD) {
			for(channel_number_map_t::iterator cit = nit_hd_logical_map.begin(); cit != nit_hd_logical_map.end(); ++cit) {
				//nit_logical_map.erase(cit->first);
				CZapitChannel * channel = CServiceManager::getInstance()->FindChannel48(cit->first);
				if (channel) {
					channel->number = cit->second;
					if (!bouquet->getChannelByChannelID(channel->getChannelID()))
						bouquet->addService(channel);
				}
			}
		}
		for(channel_number_map_t::iterator cit = nit_logical_map.begin(); cit != nit_logical_map.end(); ++cit) {
			CZapitChannel * channel = CServiceManager::getInstance()->FindChannel48(cit->first);
			if (channel && !channel->number) {
				channel->number = cit->second;
				if (!bouquet->getChannelByChannelID(channel->getChannelID()))
					bouquet->addService(channel);
			}
		}
		bouquet->sortBouquetByNumber();
		g_bouquetManager->saveUBouquets();
	}

#ifdef USE_BAT
	if(flags & SCAN_BAT) {
		bool have_lcn = false;
		if((flags & SCAN_LOGICAL_NUMBERS) && !logical_map.empty()) {
			CServiceManager::getInstance()->ResetChannelNumbers(true, true);
			for(channel_number_map_t::iterator lit = logical_map.begin(); lit != logical_map.end(); ++lit) {
				CZapitChannel * channel = CServiceManager::getInstance()->FindChannel48(lit->first);
				if(channel) {
					channel->number = lit->second;
					CServiceManager::getInstance()->UseNumber(channel->number, channel->getServiceType() == ST_DIGITAL_RADIO_SOUND_SERVICE);
					have_lcn = true;
				}
			}
		}
#if 1
		for(bouquet_map_t::iterator it = bouquet_map.begin(); it != bouquet_map.end(); ++it) {
			CZapitBouquet* bouquet;
			std::string pname = it->first;
			int bouquetId = g_bouquetManager->existsUBouquet(pname.c_str());
			printf("########### CServiceScan::ReadNitSdt: bouquet [%s] size %d id %d ###########\n", it->first.c_str(), (int)bouquet_map[it->first].size(), bouquetId);
			if (bouquetId == -1)
				bouquet = g_bouquetManager->addBouquet(pname, true);
			else
				bouquet = g_bouquetManager->Bouquets[bouquetId];

			for(std::set<t_channel_id>::iterator cit = it->second.begin(); cit != it->second.end(); ++cit) {
				CZapitChannel * channel = CServiceManager::getInstance()->FindChannel48(*cit);
				//if (channel->getAudioPid() == 0) && (channel->getVideoPid() == 0)
				if(channel && !bouquet->getChannelByChannelID(channel->getChannelID())) {
					bouquet->addService(channel);
				}
#if 0
				else
					printf("CServiceScan::ReadNitSdt: channel id %016llx not found\n", *cit);
#endif
			}
			if(have_lcn)
				bouquet->sortBouquetByNumber();
#endif
		}
	}
#endif
	return true;
}

void CServiceScan::FixServiceTypes()
{
	std::map <t_channel_id, uint8_t>::iterator stI;
	for (stI = service_types.begin(); stI != service_types.end(); ++stI) {
		CZapitChannel * channel = CServiceManager::getInstance()->FindChannel(stI->first);
		if(!channel)
			continue;
		if (channel->getServiceType() != stI->second) {
			switch (channel->getServiceType()) {
				case ST_DIGITAL_TELEVISION_SERVICE:
				case ST_DIGITAL_RADIO_SOUND_SERVICE:
				case ST_NVOD_REFERENCE_SERVICE:
				case ST_NVOD_TIME_SHIFTED_SERVICE:
					break;
				default:
					INFO("setting service_type of channel_id " PRINTF_CHANNEL_ID_TYPE " %s from %02x to %02x", stI->first, channel->getName().c_str(), channel->getServiceType(), stI->second);
					channel->setServiceType(stI->second);
					break;
			}
		}
	}
}

bool CServiceScan::ScanProvider(t_satellite_position satellitePosition)
{
	found_transponders = 0;
	processed_transponders = 0;
	transponder_list_t::iterator tI;

	TIMER_START();
	std::string satname = CServiceManager::getInstance()->GetSatelliteName(satellitePosition);

	CZapit::getInstance()->SendEvent(CZapitClient::EVT_SCAN_REPORT_NUM_SCANNED_TRANSPONDERS, &processed_transponders, sizeof(processed_transponders));
	CZapit::getInstance()->SendEvent(CZapitClient::EVT_SCAN_SATELLITE, satname.c_str(), satname.size() + 1);

	transponder_list_t &select_transponders = CServiceManager::getInstance()->GetSatelliteTransponders(satellitePosition);
	for (tI = select_transponders.begin(); tI != select_transponders.end(); ++tI) {
		AddTransponder(tI->first, &tI->second.feparams, tI->second.polarization);
	}

	ReadNitSdt(satellitePosition);

	/* channels from PAT do not have service_type set.
	 * some channels set the service_type in the BAT or the NIT.
	 * should the NIT be parsed on every transponder? */
	FixServiceTypes();
	CServiceManager::getInstance()->UpdateSatTransponders(satellitePosition);

	TIMER_STOP_SEC("[scan] scanning took");
	return true;
}

void CServiceScan::Cleanup(const bool success)
{
	running = false;
	/* notify client about end of scan */
	CZapit::getInstance()->SendEvent(success ? CZapitClient::EVT_SCAN_COMPLETE : CZapitClient::EVT_SCAN_FAILED);
	if (scanBouquetManager) {
		scanBouquetManager->clearAll();
		delete scanBouquetManager;
		scanBouquetManager = NULL;
	}
	CleanAllMaps();
}

bool CServiceScan::SetFrontend(t_satellite_position satellitePosition)
{
	CFrontend * fe = CFEManager::getInstance()->getScanFrontend(satellitePosition);
	if(fe == NULL) {
		INFO("No frontend found for satellite  %d\n", satellitePosition);
		return false;
	}

	frontend = fe;
	CFEManager::getInstance()->setLiveFE(frontend);
	frontendType = frontend->getInfo()->type;
	return true;
}

void CServiceScan::CheckSatelliteChannels(t_satellite_position satellitePosition)
{
	satHaveChannels = false;

	ZapitChannelList zapitList;
	if(CServiceManager::getInstance()->GetAllSatelliteChannels(zapitList, satellitePosition))
		satHaveChannels = true;
}

void CServiceScan::SaveServices()
{
#if 0 // old
	CServiceManager::getInstance()->SaveServices(true);
	printf("[scan] save services done\n"); fflush(stdout);
	g_bouquetManager->saveBouquets();
	g_bouquetManager->loadBouquets();
	printf("[scan] save bouquets done\n");
#endif
#ifdef USE_BAT
	if(flags & SCAN_BAT) {
		g_bouquetManager->saveUBouquets();
	}
#endif
	if(flags & SCAN_RESET_NUMBERS)
		CServiceManager::getInstance()->ResetChannelNumbers(true, true);
	/* first save bouquets, next load to re-number */
	g_bouquetManager->saveBouquets();
	printf("[scan] save bouquets done\n");
	/* load and renumber */
	g_bouquetManager->loadBouquets();
	/* save with numbers */
	CServiceManager::getInstance()->SaveServices(true);
	printf("[scan] save services done\n"); fflush(stdout);
}

bool CServiceScan::ScanProviders()
{
	flags = *((int *)scan_arg);
	scanBouquetManager = new CBouquetManager();

	printf("[scan] NIT %s, fta only: %s, satellites %s\n", flags & SCAN_NIT ? "yes" : "no",
			flags & SCAN_FTA ? "yes" : "no", scanProviders.size() == 1 ? "single" : "multi");

	for (scan_list_iterator_t spI = scanProviders.begin(); spI != scanProviders.end(); ++spI) {
		t_satellite_position position = spI->first;
		if(!SetFrontend(position))
			break;

		CleanAllMaps();

		/* set satHaveChannels flag so SDT can mark channels as NEW only if
		 * this is not first satellite scan */
		CheckSatelliteChannels(position);
		printf("[scan] scanning %s at %d bouquetMode %d\n", spI->second.c_str(), position, bouquetMode);
		ScanProvider(position);
		if(abort_scan) {
			found_channels = 0;
			break;
		}

		if( !scanBouquetManager->Bouquets.empty() ) {
			scanBouquetManager->saveBouquets(bouquetMode, spI->second.c_str());
		}
		scanBouquetManager->clearAll();
	}

	/* report status */
	printf("[scan] found %d transponders (%d failed) and %d channels\n", found_transponders, failed_transponders, found_channels);
	if (found_channels) {
		SaveServices();
		Cleanup(true);
		CZapitClient myZapitClient;
		myZapitClient.reloadCurrentServices();
	} else {
		Cleanup(false);
		frontend->setTsidOnid(0);
#if 0
		t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
		CZapit::getInstance()->ZapIt(live_channel_id, false);
#endif
	}

	return (found_channels != 0);
}

bool CServiceScan::ScanTransponder()
{
	TP_params *TP = (TP_params *) scan_arg;

	if(scanProviders.empty() || !SetFrontend(scanProviders.begin()->first)) {
		Cleanup(false);
		return false;
	}

	t_satellite_position satellitePosition = scanProviders.begin()->first;
	std::string providerName = scanProviders.begin()->second.c_str();

	CheckSatelliteChannels(satellitePosition);

	scanBouquetManager = new CBouquetManager();

	printf("[scan] scanning sat %s position %d\n", providerName.c_str(), satellitePosition);
	CZapit::getInstance()->SendEvent(CZapitClient::EVT_SCAN_SATELLITE, providerName.c_str(), providerName.size()+1);
	
	TP->feparams.dvb_feparams.inversion = INVERSION_AUTO;

	flags = TP->scan_mode;
	printf("[scan] NIT %s, fta only: %s, satellites %s\n", flags & SCAN_NIT ? "yes" : "no",
			flags & SCAN_FTA ? "yes" : "no", scanProviders.size() == 1 ? "single" : "multi");

	freq_id_t freq = CREATE_FREQ_ID(TP->feparams.dvb_feparams.frequency, frontendType != FE_QPSK);

	fake_tid++; fake_nid++;

	transponder_id_t tid = CREATE_TRANSPONDER_ID64(freq, satellitePosition, fake_nid, fake_tid);
	transponder t(frontendType, tid, TP->feparams,  TP->polarization);
	t.dump("[scan]");

	AddTransponder(tid, &TP->feparams, TP->polarization);

	/* read network information table */
	ReadNitSdt(satellitePosition);
	FixServiceTypes();
	CServiceManager::getInstance()->UpdateSatTransponders(satellitePosition);
#if 0
	if (found_channels)
		ReplaceTransponderParams(freq, satellitePosition, &TP->feparams, TP->polarization);
#endif
	printf("[scan] found %d transponders (%d failed) and %d channels\n", found_transponders, failed_transponders, found_channels);
	if(abort_scan)
		found_channels = 0;

	if(found_channels) {
		scanBouquetManager->saveBouquets(bouquetMode, providerName.c_str());
		SaveServices();
		Cleanup(true);
		CZapitClient myZapitClient;
		myZapitClient.reloadCurrentServices();
	} else {
		Cleanup(false);
		frontend->setTsidOnid(0);
#if 0
		t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
		CZapit::getInstance()->ZapIt(live_channel_id, false);
#endif
	}

	return (found_channels != 0);
}
#if 0 
//never used
bool CServiceScan::ReplaceTransponderParams(freq_id_t freq, t_satellite_position satellitePosition, struct dvb_frontend_parameters * feparams, uint8_t polarization)
{
	bool ret = false;
	for (transponder_list_t::iterator tI = transponders.begin(); tI != transponders.end(); ++tI) {
		if (tI->second.satellitePosition == satellitePosition) {
			freq_id_t newfreq = CREATE_FREQ_ID(tI->second.feparams.dvb_feparams.frequency, frontendType != FE_QPSK);
			if (freq == newfreq) {
				memcpy(&tI->second.feparams, feparams, sizeof(struct dvb_frontend_parameters));
				tI->second.polarization = polarization;
				printf("[scan] replacing transponder parameters\n");
				ret = true;
				break;
			}
		}
	}
	return ret;
}
#endif
void CServiceScan::SendTransponderInfo(transponder &t)
{
	uint32_t  actual_freq = t.feparams.dvb_feparams.frequency;
	if (frontendType == FE_QAM)
		actual_freq /= 1000;
	CZapit::getInstance()->SendEvent(CZapitClient::EVT_SCAN_REPORT_FREQUENCY, &actual_freq,sizeof(actual_freq));

	CZapit::getInstance()->SendEvent(CZapitClient::EVT_SCAN_REPORT_NUM_SCANNED_TRANSPONDERS, &processed_transponders, sizeof(processed_transponders));
#if 0
	CZapit::getInstance()->SendEvent(CZapitClient::EVT_SCAN_PROVIDER, (void *) " ", 2);
	CZapit::getInstance()->SendEvent(CZapitClient::EVT_SCAN_SERVICENAME, (void *) " ", 2);
#endif

	if (frontendType == FE_QPSK) {
		uint32_t actual_polarisation = ((t.feparams.dvb_feparams.u.qpsk.symbol_rate/1000) << 16) | (t.feparams.dvb_feparams.u.qpsk.fec_inner << 8) | (uint)t.polarization;
		CZapit::getInstance()->SendEvent(CZapitClient::EVT_SCAN_REPORT_FREQUENCYP, &actual_polarisation,sizeof(actual_polarisation));
	}
}

void CServiceScan::ChannelFound(uint8_t service_type, std::string providerName, std::string serviceName)
{
	CZapit::getInstance()->SendEvent(CZapitClient::EVT_SCAN_PROVIDER, (void *) providerName.c_str(), providerName.length() + 1);

	found_channels++;
	CZapit::getInstance()->SendEvent( CZapitClient::EVT_SCAN_NUM_CHANNELS, &found_channels, sizeof(found_channels));

	switch (service_type) {
		case ST_DIGITAL_TELEVISION_SERVICE:
			found_tv_chans++;
			CZapit::getInstance()->SendEvent(CZapitClient::EVT_SCAN_FOUND_TV_CHAN, &found_tv_chans, sizeof(found_tv_chans));
			CZapit::getInstance()->SendEvent(CZapitClient::EVT_SCAN_SERVICENAME, (void *) serviceName.c_str(), serviceName.length() + 1);
			break;
		case ST_DIGITAL_RADIO_SOUND_SERVICE:
			found_radio_chans++;
			CZapit::getInstance()->SendEvent(CZapitClient::EVT_SCAN_FOUND_RADIO_CHAN, &found_radio_chans, sizeof(found_radio_chans));
			CZapit::getInstance()->SendEvent(CZapitClient::EVT_SCAN_SERVICENAME, (void *) serviceName.c_str(), serviceName.length() + 1);
			break;
		case ST_NVOD_REFERENCE_SERVICE:
		case ST_NVOD_TIME_SHIFTED_SERVICE:
			CZapit::getInstance()->SendEvent(CZapitClient::EVT_SCAN_SERVICENAME, (void *) serviceName.c_str(), serviceName.length() + 1);
		case ST_DATA_BROADCAST_SERVICE:
		case ST_RCS_MAP:
		case ST_RCS_FLS:
		default:
			found_data_chans++;
			CZapit::getInstance()->SendEvent(CZapitClient::EVT_SCAN_FOUND_DATA_CHAN, &found_data_chans, sizeof(found_data_chans));
			break;
	}
}

void CServiceScan::AddServiceType(t_channel_id channel_id, uint8_t service_type)
{
	service_types[channel_id] = service_type;
}
