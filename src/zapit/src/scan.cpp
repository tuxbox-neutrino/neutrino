/*
 * $Id: scan.cpp,v 1.134 2004/02/10 21:01:14 metallica Exp $
 *
 * (C) 2002-2003 Andreas Oberritter <obi@tuxbox.org>
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
#include <zapit/getservices.h>
#include <zapit/nit.h>
#include <zapit/scan.h>
#include <zapit/sdt.h>
#include <zapit/settings.h>
#include <zapit/satconfig.h>
#include <zapit/frontend_c.h>
#include <zapit/zapit.h>
#include <xmlinterface.h>

#define NIT_THREAD

extern CBouquetManager *g_bouquetManager;
extern transponder_list_t transponders; //  defined in zapit.cpp
extern CZapitClient::bouquetMode bouquetMode;
extern int motorRotationSpeed;

void *nit_thread(void * data);

//int scan_mode = 0; /* 0 = NIT, 1 = fast */

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
	scan_nit = false;
	running = false;
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

	scantransponders.clear();
	scanedtransponders.clear();
	nittransponders.clear();
	service_types.clear();

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
		case SCAN_FAST:
			ScanFast();
			break;
		default:
			break;
	}
}

bool CServiceScan::tuneFrequency(FrontendParameters *feparams, uint8_t polarization, t_satellite_position satellitePosition)
{
	CFrontend::getInstance()->setInput(satellitePosition, feparams->frequency, polarization);
	int ret = CFrontend::getInstance()->driveToSatellitePosition(satellitePosition, false); //true);
	if(ret > 0) {
		printf("[scan] waiting %d seconds for motor to turn satellite dish.\n", ret);
		CZapit::getInstance()->SendEvent(CZapitClient::EVT_SCAN_PROVIDER, (void *) "moving rotor", 13);
		for(int i = 0; i < ret; i++) {
			sleep(1);
			if(abort_scan)
				return false;
		}
	}
	return CFrontend::getInstance()->tuneFrequency(feparams, polarization, false);
}

bool CServiceScan::AddTransponder(transponder_id_t TsidOnid, FrontendParameters *feparams, uint8_t polarity, bool fromnit)
{
	DBG("AddTransponder: freq %d pol %d tpid %llx\n", feparams->frequency, polarity, TsidOnid);
//if(fromnit) printf("AddTransponder: freq %d pol %d tpid %llx\n", feparams->frequency, polarity, TsidOnid);
	freq_id_t freq;
	if(cable)
		freq = feparams->frequency/100;
	else
		freq = feparams->frequency/1000;

	uint8_t poltmp1 = polarity & 1;
	uint8_t poltmp2;

	stiterator tI;

	tI = scanedtransponders.find(TsidOnid);

	if (tI != scanedtransponders.end()) {
		poltmp2 = tI->second.polarization & 1;

		if(poltmp2 != poltmp1) {
			t_transport_stream_id transport_stream_id = tI->second.transport_stream_id;
			t_original_network_id original_network_id = tI->second.original_network_id;
			uint16_t freq1 = GET_FREQ_FROM_TPID(tI->first);
			//FIXME simplify/change GET_SATELLITEPOSITION_FROM_TRANSPONDER_ID
			t_satellite_position satellitePosition = GET_SATELLITEPOSITION_FROM_TRANSPONDER_ID(tI->first) & 0xFFF;
			if(GET_SATELLITEPOSITION_FROM_TRANSPONDER_ID(tI->first) & 0xF000)
				satellitePosition = -satellitePosition;

			freq1++;
			TsidOnid = CREATE_TRANSPONDER_ID_FROM_SATELLITEPOSITION_ORIGINALNETWORK_TRANSPORTSTREAM_ID(
				freq1, satellitePosition, original_network_id, transport_stream_id);
				printf("[scan] AddTransponder: SAME freq %d pol1 %d pol2 %d tpid %llx\n", feparams->frequency, poltmp1, poltmp2, TsidOnid);
			feparams->frequency = feparams->frequency+1000;
			tI = scanedtransponders.find(TsidOnid);
		}
	}
        else for (tI = scanedtransponders.begin(); tI != scanedtransponders.end(); tI++) {
		poltmp2 = tI->second.polarization & 1;
                if((abs(GET_FREQ_FROM_TPID(tI->first) - freq) <= 3))
			if(poltmp2 == poltmp1)
                        	break;
        }
	if(tI == scanedtransponders.end()) {
		DBG("[scan] insert tp-id %llx freq %d rate %d\n", TsidOnid, feparams->frequency, cable? feparams->u.qam.symbol_rate : feparams->u.qpsk.symbol_rate );
		if(fromnit) {
			if(nittransponders.find(TsidOnid) == nittransponders.end()) {
				printf("[scan] insert tp-id %llx freq %d pol %d rate %d\n", TsidOnid, feparams->frequency, polarity, cable? feparams->u.qam.symbol_rate : feparams->u.qpsk.symbol_rate );
				nittransponders.insert (
						std::pair <transponder_id_t, transponder> (
							TsidOnid,
							transponder ( (TsidOnid >> 16) &0xFFFF,
								TsidOnid &0xFFFF, *feparams, polarity)));
			}
		}
		else {
			found_transponders++;
			scantransponders.insert (
					std::pair <transponder_id_t, transponder> (
						TsidOnid,
						transponder ( (TsidOnid >> 16) &0xFFFF, TsidOnid &0xFFFF, *feparams, polarity)));
			scanedtransponders.insert (
					std::pair <transponder_id_t, transponder> (
						TsidOnid,
						transponder ( (TsidOnid >> 16) &0xFFFF, TsidOnid &0xFFFF, *feparams, polarity)));
		}
		return true;
	}
	else
		tI->second.feparams.u.qpsk.fec_inner = feparams->u.qpsk.fec_inner;

	return false;
}

bool CServiceScan::ReadNitSdt(t_satellite_position satellitePosition)
{
	uint32_t  actual_freq;
	uint32_t actual_polarisation;
	transponder_id_t TsidOnid = 0;
	stiterator tI;
	stiterator stI;
	std::map <transponder_id_t, transponder>::iterator sT;

	printf("[scan] scanning tp from sat/service\n");
_repeat:
	for (tI = scantransponders.begin(); tI != scantransponders.end(); tI++) {
		if(abort_scan)
			return false;

		printf("[scan] scanning: %llx\n", tI->first);

		if(cable)
			actual_freq = tI->second.feparams.frequency;
		else
			actual_freq = tI->second.feparams.frequency/1000;

		processed_transponders++;
		CZapit::getInstance()->SendEvent(CZapitClient::EVT_SCAN_REPORT_NUM_SCANNED_TRANSPONDERS, &processed_transponders, sizeof(processed_transponders));
		CZapit::getInstance()->SendEvent(CZapitClient::EVT_SCAN_PROVIDER, (void *) " ", 2);
		CZapit::getInstance()->SendEvent(CZapitClient::EVT_SCAN_SERVICENAME, (void *) " ", 2);
		CZapit::getInstance()->SendEvent(CZapitClient::EVT_SCAN_REPORT_FREQUENCY, &actual_freq,sizeof(actual_freq));

		if (!cable) {
			actual_polarisation = ((tI->second.feparams.u.qpsk.symbol_rate/1000) << 16) | (tI->second.feparams.u.qpsk.fec_inner << 8) | (uint)tI->second.polarization;
			CZapit::getInstance()->SendEvent(CZapitClient::EVT_SCAN_REPORT_FREQUENCYP, &actual_polarisation,sizeof(actual_polarisation));
		}

		if (!tuneFrequency(&(tI->second.feparams), tI->second.polarization, satellitePosition)) {
			failed_transponders++;
			continue;
		}
		if(abort_scan)
			return false;

#ifdef NIT_THREAD
		pthread_t nthread;
		if(scan_nit)
			if(pthread_create(&nthread, 0, nit_thread,  (void*)satellitePosition)) {
				ERROR("pthread_create");
				nthread = 0;
			}
#endif
		freq_id_t freq;
		if(cable)
			freq = tI->second.feparams.frequency/100;
		else
			freq = tI->second.feparams.frequency/1000;
		//INFO("parsing SDT (tsid:onid %04x:%04x)", tI->second.transport_stream_id, tI->second.original_network_id);
		int status = parse_sdt(&tI->second.transport_stream_id, &tI->second.original_network_id, satellitePosition, freq /*tI->second.feparams.frequency/1000*/);
		if(status < 0) {
			printf("[scan] SDT failed !\n");
			continue;
		}

		TsidOnid = CREATE_TRANSPONDER_ID_FROM_SATELLITEPOSITION_ORIGINALNETWORK_TRANSPORTSTREAM_ID(
				freq /*tI->second.feparams.frequency/1000*/, satellitePosition, tI->second.original_network_id,
				tI->second.transport_stream_id);

		//scanedtransponders.insert(std::pair <transponder_id_t,bool> (TsidOnid, true));

		stI = transponders.find(TsidOnid);
		if(stI == transponders.end())
			transponders.insert (
					std::pair <transponder_id_t, transponder> (
						TsidOnid,
						transponder (
							tI->second.transport_stream_id,
							tI->second.feparams,
							tI->second.polarization,
							tI->second.original_network_id
							)
						)
					);
		else
			stI->second.feparams.u.qpsk.fec_inner = tI->second.feparams.u.qpsk.fec_inner;

#ifdef NIT_THREAD
		if(scan_nit && nthread) {
			if(pthread_join(nthread, NULL))
				perror("pthread_join !!!!!!!!!");
		}
#else
		if(scan_nit) {
			printf("[scan] trying to parse NIT\n");
			int status = parse_nit(satellitePosition, freq /*tI->second.feparams.frequency/1000*/);
			if(status < 0)
				printf("[scan] NIT failed !\n");
		}
#endif
		printf("[scan] tpid ready: %llx\n", TsidOnid);
	}
	if(scan_nit) {
		printf("[scan] found %d transponders (%d failed) and %d channels\n", found_transponders, failed_transponders, found_channels);
		scantransponders.clear();
		for (tI = nittransponders.begin(); tI != nittransponders.end(); tI++) {
//int AddTransponder(transponder_id_t TsidOnid, FrontendParameters *feparams, uint8_t polarity, bool fromnit = 0)
			AddTransponder(tI->first, &tI->second.feparams, tI->second.polarization, false);
		}
		nittransponders.clear();
		printf("\n\n[scan] found %d additional transponders from nit\n", scantransponders.size());
		if(scantransponders.size()) {
			CZapit::getInstance()->SendEvent ( CZapitClient::EVT_SCAN_NUM_TRANSPONDERS,
				&found_transponders, sizeof(found_transponders));
			goto _repeat;
		}
	}
	return true;
}

bool CServiceScan::AddTransponder(xmlNodePtr transponder, uint8_t diseqc_pos, t_satellite_position satellitePosition)
{
	uint8_t polarization = 0;
	uint8_t system = 0, modulation = 1;
	int xml_fec;
	FrontendParameters feparams;
	memset(&feparams, 0x00, sizeof(FrontendParameters));

	feparams.frequency = xmlGetNumericAttribute(transponder, "frequency", 0);

	freq_id_t freq;
	if(cable) {
		if (feparams.frequency > 1000*1000)
			feparams.frequency = feparams.frequency/1000; //transponderlist was read from tuxbox
		//feparams.frequency = (int) 1000 * (int) round ((double) feparams.frequency / (double) 1000);
		freq = feparams.frequency/100;
	}
	else  {
		feparams.frequency = (int) 1000 * (int) round ((double) feparams.frequency / (double) 1000);
		freq = feparams.frequency/1000;
	}

	feparams.inversion = INVERSION_AUTO;

	if (cable) {
		feparams.u.qam.symbol_rate = xmlGetNumericAttribute(transponder, "symbol_rate", 0);
		feparams.u.qam.fec_inner = (fe_code_rate_t) xmlGetNumericAttribute(transponder, "fec_inner", 0);
		feparams.u.qam.modulation = (fe_modulation_t) xmlGetNumericAttribute(transponder, "modulation", 0);
		diseqc_pos = 0;
	}
	else if (CFrontend::getInstance()->getInfo()->type == FE_QPSK) {
		feparams.u.qpsk.symbol_rate = xmlGetNumericAttribute(transponder, "symbol_rate", 0);
		polarization = xmlGetNumericAttribute(transponder, "polarization", 0);
		system = xmlGetNumericAttribute(transponder, "system", 0);
		modulation = xmlGetNumericAttribute(transponder, "modulation", 0);
		xml_fec = xmlGetNumericAttribute(transponder, "fec_inner", 0);
		xml_fec = CFrontend::getCodeRate(xml_fec, system);
		if(modulation == 2)
			xml_fec += 9;
		feparams.u.qpsk.fec_inner = (fe_code_rate_t) xml_fec;
	}
	/* read network information table */
	fake_tid++; fake_nid++;
	AddTransponder(
			CREATE_TRANSPONDER_ID_FROM_SATELLITEPOSITION_ORIGINALNETWORK_TRANSPORTSTREAM_ID(freq /*feparams.frequency/1000*/, satellitePosition, fake_nid, fake_tid),
			&feparams, polarization);
	return true;
}

void CServiceScan::FixServiceTypes()
{
	std::map <t_channel_id, uint8_t>::iterator stI;
	for (stI = service_types.begin(); stI != service_types.end(); stI++) {
		CZapitChannel * channel = CServiceManager::getInstance()->FindChannel(stI->first);
		if (channel) {
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
}

bool CServiceScan::ScanProvider(xmlNodePtr search, t_satellite_position satellitePosition, uint8_t diseqc_pos)
{
	xmlNodePtr tps = NULL;
	transponder_list_t::iterator tI;
	found_transponders = 0;
	processed_transponders = 0;

	TIMER_START();
	sat_iterator_t sit = satellitePositions.find(satellitePosition);
	if(sit == satellitePositions.end()) {
		printf("[zapit] WARNING satellite position %d not found!\n", satellitePosition);
		return false;
	}

	CZapit::getInstance()->SendEvent(CZapitClient::EVT_SCAN_REPORT_NUM_SCANNED_TRANSPONDERS, &processed_transponders, sizeof(processed_transponders));
	CZapit::getInstance()->SendEvent(CZapitClient::EVT_SCAN_SATELLITE, sit->second.name.c_str(), sit->second.name.size() + 1);
	tps = search->xmlChildrenNode;
	/* TPs from current service list */
	for(tI = transponders.begin(); tI != transponders.end(); tI++) {
		if(abort_scan)
			return false;
		t_satellite_position satpos = GET_SATELLITEPOSITION_FROM_TRANSPONDER_ID(tI->first) & 0xFFF;
		if(GET_SATELLITEPOSITION_FROM_TRANSPONDER_ID(tI->first) & 0xF000)
			satpos = -satpos;
		if(satpos != satellitePosition)
			continue;
		AddTransponder(tI->first, &tI->second.feparams, tI->second.polarization);
	}
	/* read all transponders */
	while ((tps = xmlGetNextOccurence(tps, "transponder")) != NULL) {
		if(abort_scan)
			return false;
		AddTransponder(tps, diseqc_pos, satellitePosition);

		/* next transponder */
		tps = tps->xmlNextNode;
	}
	CZapit::getInstance()->SendEvent ( CZapitClient::EVT_SCAN_NUM_TRANSPONDERS,
					&found_transponders, sizeof(found_transponders));

	ReadNitSdt(satellitePosition);

	/* channels from PAT do not have service_type set.
	 * some channels set the service_type in the BAT or the NIT.
	 * should the NIT be parsed on every transponder? */
	FixServiceTypes();

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
	scantransponders.clear();
	scanedtransponders.clear();
	nittransponders.clear();
	service_types.clear();
}

bool CServiceScan::ScanProviders()
{
	scan_list_iterator_t spI;
	char providerName[80] = "";
	const char *frontendType;
	uint8_t diseqc_pos = 0;
	scanBouquetManager = new CBouquetManager();
	bool satfeed = false;
	int mode = (int) scan_arg;

	curr_sat = 0;

	cable = (CFrontend::getInstance()->getInfo()->type == FE_QAM);
	if (cable)
		frontendType = "cable";
	else
		frontendType = "sat";

	scan_nit = (mode & 0xFF) == 0; // NIT (0) or fast (1)
	int scan_sat_mode = mode & 0xFF00; // single = 0, all = 1

	printf("[zapit] scan mode %s, satellites %s\n", scan_nit ? "fast" : "NIT", scan_sat_mode ? "all" : "single");
	// CZapitClient myZapitClient;

	/* get first child */
	//xmlNodePtr search = xmlDocGetRootElement(scanInputParser)->xmlChildrenNode;
	xmlNodePtr search = xmlDocGetRootElement(CServiceManager::getInstance()->ScanXml())->xmlChildrenNode;

	/* read all sat or cable sections */
	while ((search = xmlGetNextOccurence(search, frontendType)) != NULL) {
		t_satellite_position position = xmlGetSignedNumericAttribute(search, "position", 10);

		if(cable) {
			strcpy(providerName, xmlGetAttribute(search, "name"));
			for (spI = scanProviders.begin(); spI != scanProviders.end(); spI++)
				if (!strcmp(spI->second.c_str(), providerName)) {
					position = spI->first;
					break;
				}

		} else {
			for (spI = scanProviders.begin(); spI != scanProviders.end(); spI++)
				if(spI->first == position)
					break;
		}

		/* provider is not wanted - jump to the next one */
		if (spI != scanProviders.end()) {
			/* get name of current satellite oder cable provider */
			strcpy(providerName, xmlGetAttribute(search, "name"));

			if (cable && xmlGetAttribute(search, "satfeed")) {
				if (!strcmp(xmlGetAttribute(search, "satfeed"), "true"))
					satfeed = true;
			}

			/* increase sat counter */
			curr_sat++;

			scantransponders.clear();
			scanedtransponders.clear();
			nittransponders.clear();

			printf("[scan] scanning %s at %d bouquetMode %d\n", providerName, position, bouquetMode);
			ScanProvider(search, position, diseqc_pos);
			if(abort_scan) {
				found_channels = 0;
				break;
			}

			if(scanBouquetManager->Bouquets.size() > 0) {
				scanBouquetManager->saveBouquets(bouquetMode, providerName);
			}
			scanBouquetManager->clearAll();
		}
		/* go to next satellite */
		search = search->xmlNextNode;
	}

	/* report status */
	printf("[scan] found %d transponders (%d failed) and %d channels\n", found_transponders, failed_transponders, found_channels);
	//FIXME move to Cleanup() ?
	if (found_channels) {
		CServiceManager::getInstance()->SaveServices(true);
		printf("[scan] save services done\n"); fflush(stdout);
	        g_bouquetManager->saveBouquets();
	        //g_bouquetManager->sortBouquets();
	        //g_bouquetManager->renumServices();
	        //g_bouquetManager->clearAll();
		g_bouquetManager->loadBouquets();
		printf("[scan] save bouquets done\n");
		Cleanup(true);
		/* this can hang as the thread handling the connections
		 * could already be in g_Zapit->stopScan(), waiting for
		 * *this* thread to join().
		myZapitClient.reloadCurrentServices();
		 * so let's do another ugly, layer-violating hack...
		 */
		CFrontend::getInstance()->setTsidOnid(0);
		t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
		CZapit::getInstance()->ZapIt(live_channel_id, false);
		CZapit::getInstance()->SendEvent(CZapitClient::EVT_SERVICES_CHANGED);
	} else {
		Cleanup(false);
		CFrontend::getInstance()->setTsidOnid(0);
		t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
		CZapit::getInstance()->ZapIt(live_channel_id, false);
	}

	return (found_channels != 0);
}

bool CServiceScan::ScanTransponder()
{
	TP_params *TP = (TP_params *) scan_arg;
	char providerName[32] = "";
	t_satellite_position satellitePosition = 0;

	scanBouquetManager = new CBouquetManager();

	cable = (CFrontend::getInstance()->getInfo()->type == FE_QAM);

	strcpy(providerName, scanProviders.size() > 0 ? scanProviders.begin()->second.c_str() : "unknown provider");

	satellitePosition = scanProviders.begin()->first;
	printf("[scan_transponder] scanning sat %s position %d\n", providerName, satellitePosition);
	CZapit::getInstance()->SendEvent(CZapitClient::EVT_SCAN_SATELLITE, providerName, strlen(providerName) + 1);

	//scan_nit = TP->scan_mode;
	scan_nit = (TP->scan_mode & 0xFF) == 0; // NIT (0) or fast (1)

	TP->feparams.inversion = INVERSION_AUTO;

	if (!cable) {
		printf("[scan_transponder] freq %d rate %d fec %d pol %d NIT %s\n", TP->feparams.frequency, TP->feparams.u.qpsk.symbol_rate, TP->feparams.u.qpsk.fec_inner, TP->polarization, scan_nit ? "no" : "yes");
	} else
		printf("[scan_transponder] freq %d rate %d fec %d mod %d\n", TP->feparams.frequency, TP->feparams.u.qam.symbol_rate, TP->feparams.u.qam.fec_inner, TP->feparams.u.qam.modulation);

	freq_id_t freq;
	if(cable)
		freq = TP->feparams.frequency/100;
	else
		freq = TP->feparams.frequency/1000;
	/* read network information table */
	fake_tid++; fake_nid++;
	AddTransponder(
			CREATE_TRANSPONDER_ID_FROM_SATELLITEPOSITION_ORIGINALNETWORK_TRANSPORTSTREAM_ID(freq, satellitePosition, fake_nid, fake_tid),
			&TP->feparams, TP->polarization);
	ReadNitSdt(satellitePosition);

	if(abort_scan)
		found_channels = 0;

	// CZapitClient myZapitClient;
	//FIXME move to Cleanup() ?
	if(found_channels) {
		CServiceManager::getInstance()->SaveServices(true);
		scanBouquetManager->saveBouquets(bouquetMode, providerName);
		g_bouquetManager->saveBouquets();
		//g_bouquetManager->sortBouquets();
		//g_bouquetManager->renumServices();
		//g_bouquetManager->clearAll();
		g_bouquetManager->loadBouquets();
		Cleanup(true);
		/* see the explanation in CServiceScan::ScanProviders() in why this is a bad idea
		myZapitClient.reloadCurrentServices();
		 */
		CFrontend::getInstance()->setTsidOnid(0);
		t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
		CZapit::getInstance()->ZapIt(live_channel_id, false);
		CZapit::getInstance()->SendEvent(CZapitClient::EVT_SERVICES_CHANGED);
	} else {
		Cleanup(false);
		CFrontend::getInstance()->setTsidOnid(0);
		t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
		CZapit::getInstance()->ZapIt(live_channel_id, false);
	}
	if(cable)
		DBG("[scan_transponder] done scan freq %d rate %d fec %d mod %d\n", TP->feparams.frequency, TP->feparams.u.qam.symbol_rate, TP->feparams.u.qam.fec_inner,TP->feparams.u.qam.modulation);
	else
		DBG("[scan_transponder] done scan freq %d rate %d fec %d pol %d\n", TP->feparams.frequency, TP->feparams.u.qpsk.symbol_rate, TP->feparams.u.qpsk.fec_inner, TP->polarization);

	return (found_channels != 0);
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
