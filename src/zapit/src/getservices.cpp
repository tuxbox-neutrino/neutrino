/*
 * $Id: getservices.cpp,v 1.84 2003/12/09 21:12:28 thegoodguy Exp $
 *
 * (C) 2002, 2003 by Andreas Oberritter <obi@tuxbox.org>
 *
 * (C) 2007-2013 Stefan Seyfried
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

#include <zapit/debug.h>
#include <zapit/zapit.h>
#include <zapit/femanager.h>
#include <zapit/getservices.h>
#include <zapit/settings.h>
#include <zapit/satconfig.h>
#include <xmlinterface.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>

//#define SAVE_DEBUG

extern transponder_list_t transponders;

CServiceManager * CServiceManager::manager = NULL;

CServiceManager::CServiceManager()
{
	scanInputParser = NULL;
	service_count = 0;
	services_changed = false;
	keep_numbers = false;
}

CServiceManager::~CServiceManager()
{
	delete scanInputParser;
	transponders.clear();
}

CServiceManager * CServiceManager::getInstance()
{
	if(manager == NULL)
		manager = new CServiceManager();
	return manager;
}

bool CServiceManager::ParseScanXml(fe_type_t delsys)
{
	if(scanInputParser) {
		delete scanInputParser;
		scanInputParser = NULL;
	}
	//frontendType = CFEManager::getInstance()->getLiveFE()->getInfo()->type;
	switch (delsys) {
		case FE_QPSK:
			scanInputParser = parseXmlFile(SATELLITES_XML);
			break;
		case FE_QAM:
			scanInputParser = parseXmlFile(CABLES_XML);
			break;
		case FE_OFDM:
			scanInputParser = parseXmlFile(TERRESTRIAL_XML);
			break;
		default:
			WARN("Unknown type %d", frontendType);
			return false;
	}
	return (scanInputParser != NULL);
}
#if 0 
//never used
xmlDocPtr CServiceManager::ScanXml()
{
	if(!scanInputParser)
		ParseScanXml();

	return scanInputParser;
}
#endif
bool CServiceManager::AddChannel(CZapitChannel * &channel)
{
	channel_insert_res_t ret = allchans.insert (
		channel_pair_t (channel->getChannelID(), *channel));
	delete channel;
	channel = &ret.first->second;
	if(ret.second)
		services_changed = true;
	return ret.second;
}

bool CServiceManager::AddCurrentChannel(CZapitChannel * &channel)
{
	channel_insert_res_t ret = curchans.insert (
			channel_pair_t (channel->getChannelID(), *channel));
	delete channel;
	channel = &ret.first->second;
	return ret.second;
}

bool CServiceManager::AddNVODChannel(CZapitChannel * &channel)
{
	t_service_id          service_id = channel->getServiceId();
	t_original_network_id original_network_id = channel->getOriginalNetworkId();
	t_transport_stream_id transport_stream_id = channel->getTransportStreamId();

	t_satellite_position satellitePosition = channel->getSatellitePosition();
	//FIXME define CREATE_NVOD_CHANNEL_ID
	t_channel_id sub_channel_id =
		((uint64_t) ( satellitePosition >= 0 ? satellitePosition : (uint64_t)(0xF000+ abs(satellitePosition))) << 48) |
		(uint64_t) CREATE_CHANNEL_ID(service_id, original_network_id, transport_stream_id);

	channel_insert_res_t ret = nvodchannels.insert (
			channel_pair_t (sub_channel_id, *channel));
	delete channel;
	channel = &ret.first->second;
	return ret.second;
}

void CServiceManager::ResetChannelNumbers(bool bouquets, bool numbers)
{
	for (channel_map_iterator_t it = allchans.begin(); it != allchans.end(); ++it) {
#if 0 /* force to get free numbers if there are any */
		if(have_numbers) {
			if(!it->second.number) {
				it->second.number = GetFreeNumber(it->second.getServiceType() == ST_DIGITAL_RADIO_SOUND_SERVICE);
			}
		} else {
			it->second.number = 0;
		}
#endif
		if(!keep_numbers || numbers)
			it->second.number = 0;
		if(bouquets)
			it->second.has_bouquet = 0;
	}
	if(numbers) {
		tv_numbers.clear();
		radio_numbers.clear();
	}
}

void CServiceManager::RemoveChannel(const t_channel_id channel_id)
{
	allchans.erase(channel_id);
	services_changed = true;
}

void CServiceManager::RemoveAllChannels()
{
	allchans.clear();
}
#if 0 
//never used
void CServiceManager::RemoveNVODChannels()
{
	nvodchannels.clear();
}
#endif
void CServiceManager::RemoveCurrentChannels()
{
	curchans.clear();
}

CZapitChannel * CServiceManager::FindChannel(const t_channel_id channel_id, bool * current_is_nvod)
{
	if(current_is_nvod)
		*current_is_nvod = false;

	channel_map_iterator_t cit = nvodchannels.find(channel_id);
	if (cit == nvodchannels.end()) {
		cit = allchans.find(channel_id);
		if (cit == allchans.end()) {
			//printf("%s: channel %llx not found\n", __FUNCTION__, channel_id);
			return NULL;
		}
	} else if(current_is_nvod)
		*current_is_nvod = true;

	return &cit->second;
}

CZapitChannel * CServiceManager::FindChannelByName(std::string name)
{
	for (channel_map_iterator_t it = allchans.begin(); it != allchans.end(); ++it) {
		if(it->second.getName().length() == name.length() &&
			!strcasecmp(it->second.getName().c_str(), name.c_str())) {
			return &it->second;
		}
	}
	return NULL;
}

CZapitChannel * CServiceManager::FindCurrentChannel(const t_channel_id channel_id)
{
	channel_map_iterator_t cit = curchans.find(channel_id);
	if(cit != curchans.end())
		return &cit->second;

	return NULL;
}

CZapitChannel * CServiceManager::FindChannel48(const t_channel_id channel_id)
{
	for (channel_map_iterator_t it = allchans.begin(); it != allchans.end(); ++it) {
		if((it->second.getChannelID() & 0xFFFFFFFFFFFFULL) == (channel_id & 0xFFFFFFFFFFFFULL))
			return &it->second;
	}
	return NULL;
}

/* TODO: those FindChannel* functions are naive at best. By using a different construction
 * scheme for the channel_id,they could easily be made much more efficient. Side effects would
 * need to be checked first, though */
CZapitChannel* CServiceManager::FindChannelFuzzy(const t_channel_id channel_id,
						 const t_satellite_position pos, const freq_id_t freq)
{
	for (channel_map_iterator_t it = allchans.begin(); it != allchans.end(); ++it) {
		CZapitChannel *ret = &it->second;
		if ((ret->getChannelID() & 0xFFFFFFFFFFFFULL) != (channel_id & 0xFFFFFFFFFFFFULL))
			continue;
		/* use position only on SAT boxes.
		 * Cable/terr does not need thix: There usually is no second cable provider
		 * to choose from and people are wondering why their ubouquets are no longer
		 * working => because they had the wrong 's="x"' attribute.
		 * TODO: think about mixed-mode (sat/cable/terrestrial) operation */
		if (ret->deltype == FE_QPSK && pos != ret->getSatellitePosition())
			continue;
		/* match +-2MHz to make old ubouquets work with updated satellites.xml */
		if (abs((int)ret->getFreqId() - (int)freq) < 3)
			return ret;
	}
	return NULL;
}

bool CServiceManager::GetAllRadioChannels(ZapitChannelList &list, int flags)
{
	list.clear();
	for (channel_map_iterator_t it = allchans.begin(); it != allchans.end(); ++it) {
		if (it->second.getServiceType() == ST_DIGITAL_RADIO_SOUND_SERVICE &&
				(it->second.flags & flags))
			list.push_back(&(it->second));
	}
	return (!list.empty());
}

bool CServiceManager::GetAllTvChannels(ZapitChannelList &list, int flags)
{
	list.clear();
	for (channel_map_iterator_t it = allchans.begin(); it != allchans.end(); ++it) {
		if (it->second.getServiceType() != ST_DIGITAL_RADIO_SOUND_SERVICE &&
				(it->second.flags & flags))
			list.push_back(&(it->second));
	}
	return (!list.empty());
}

bool CServiceManager::GetAllHDChannels(ZapitChannelList &list, int flags)
{
	list.clear();
	for (channel_map_iterator_t it = allchans.begin(); it != allchans.end(); ++it) {
		if ((it->second.flags & flags) && it->second.isHD())
			list.push_back(&(it->second));
	}
	return (!list.empty());
}

bool CServiceManager::GetAllUnusedChannels(ZapitChannelList &list, int flags)
{
	list.clear();
	for (channel_map_iterator_t it = allchans.begin(); it != allchans.end(); ++it) {
		if ((it->second.flags & flags) && it->second.has_bouquet == false)
			list.push_back(&(it->second));
	}
	return (!list.empty());
}

bool CServiceManager::GetAllSatelliteChannels(ZapitChannelList &list, t_satellite_position position, int flags)
{
	list.clear();
	for (channel_map_iterator_t it = allchans.begin(); it != allchans.end(); ++it) {
		if((it->second.flags & flags) && it->second.getSatellitePosition() == position)
			list.push_back(&(it->second));
	}
	return (!list.empty());
}

bool CServiceManager::GetAllTransponderChannels(ZapitChannelList &list, transponder_id_t tpid, int flags)
{
	list.clear();
	for (channel_map_iterator_t it = allchans.begin(); it != allchans.end(); ++it) {
		if((it->second.flags & flags) && it->second.getTransponderId() == tpid)
			list.push_back(&(it->second));
	}
	return (!list.empty());
}

std::string CServiceManager::GetServiceName(t_channel_id channel_id)
{
        channel_map_iterator_t it = allchans.find(channel_id);
        if (it != allchans.end())
                return it->second.getName();
        else
                return "";
}

void CServiceManager::ParseTransponders(xmlNodePtr node, t_satellite_position satellitePosition, fe_type_t delsys)
{
	uint8_t polarization = 0;

	/* read all transponders */
	while ((node = xmlGetNextOccurence(node, "TS")) != NULL) {
		FrontendParameters feparams;

		t_transport_stream_id transport_stream_id = xmlGetNumericAttribute(node, "id", 16);
		t_original_network_id original_network_id = xmlGetNumericAttribute(node, "on", 16);
		feparams.dvb_feparams.frequency = xmlGetNumericAttribute(node, "frq", 0);
		feparams.dvb_feparams.inversion = (fe_spectral_inversion) xmlGetNumericAttribute(node, "inv", 0);

		switch (delsys) {
		case FE_OFDM:
		case FE_QAM:
			feparams.dvb_feparams.u.qam.symbol_rate = xmlGetNumericAttribute(node, "sr", 0);
			feparams.dvb_feparams.u.qam.fec_inner = (fe_code_rate_t) xmlGetNumericAttribute(node, "fec", 0);
			feparams.dvb_feparams.u.qam.modulation = (fe_modulation_t) xmlGetNumericAttribute(node, "mod", 0);

			if (feparams.dvb_feparams.frequency > 1000*1000)
				feparams.dvb_feparams.frequency = feparams.dvb_feparams.frequency/1000; //transponderlist was read from tuxbox
			break;
		default:
			feparams.dvb_feparams.u.qpsk.fec_inner = (fe_code_rate_t) xmlGetNumericAttribute(node, "fec", 0);
			feparams.dvb_feparams.u.qpsk.symbol_rate = xmlGetNumericAttribute(node, "sr", 0);

			polarization = xmlGetNumericAttribute(node, "pol", 0);

			if(feparams.dvb_feparams.u.qpsk.symbol_rate < 50000)
				feparams.dvb_feparams.u.qpsk.symbol_rate = feparams.dvb_feparams.u.qpsk.symbol_rate * 1000;

			if(feparams.dvb_feparams.frequency < 20000)
				feparams.dvb_feparams.frequency = feparams.dvb_feparams.frequency*1000;
			else
				feparams.dvb_feparams.frequency = (int) 1000 * (int) round ((double) feparams.dvb_feparams.frequency / (double) 1000);
		}
		freq_id_t freq = CREATE_FREQ_ID(feparams.dvb_feparams.frequency, delsys != FE_QPSK);

		transponder_id_t tid = CREATE_TRANSPONDER_ID64(freq, satellitePosition,original_network_id,transport_stream_id);
		transponder t(delsys, tid, feparams, polarization);
		pair<map<transponder_id_t, transponder>::iterator,bool> ret;

		ret = transponders.insert(transponder_pair_t(tid, t));
		if (ret.second == false)
			t.dump("[zapit] duplicate in all transponders:");

		/* read channels that belong to the current transponder */
		ParseChannels(node->xmlChildrenNode, transport_stream_id, original_network_id, satellitePosition, freq, polarization, delsys);

		/* hop to next transponder */
		node = node->xmlNextNode;
	}
	UpdateSatTransponders(satellitePosition);
	return;
}

void CServiceManager::ParseChannels(xmlNodePtr node, const t_transport_stream_id transport_stream_id, const t_original_network_id original_network_id, t_satellite_position satellitePosition, freq_id_t freq, uint8_t polarization, fe_type_t delsys)
{
	int dummy = 0;
	int * have_ptr = &dummy;

	sat_iterator_t sit = satellitePositions.find(satellitePosition);
	if(sit != satellitePositions.end())
		have_ptr = &sit->second.have_channels;

	while ((node = xmlGetNextOccurence(node, "S")) != NULL) {
		*have_ptr = 1;
		t_service_id service_id = xmlGetNumericAttribute(node, "i", 16);
		std::string name = xmlGetAttribute(node, "n");
		uint8_t service_type = xmlGetNumericAttribute(node, "t", 16);
		uint16_t vpid = xmlGetNumericAttribute(node, "v", 16);
		uint16_t apid = xmlGetNumericAttribute(node, "a", 16);
		uint16_t pcrpid = xmlGetNumericAttribute(node, "p", 16);
		uint16_t pmtpid = xmlGetNumericAttribute(node, "pmt", 16);
		uint16_t txpid = xmlGetNumericAttribute(node, "tx", 16);
		uint16_t vtype = xmlGetNumericAttribute(node, "vt", 16);
		uint16_t scrambled = xmlGetNumericAttribute(node, "s", 16);
		int number = xmlGetNumericAttribute(node, "num", 10);
		int flags = xmlGetNumericAttribute(node, "f", 10);
		/* default if no flags present */
		if (flags == 0)
			flags = CZapitChannel::UPDATED;

		t_channel_id chid = CREATE_CHANNEL_ID64;
		char *ptr = xmlGetAttribute(node, "action");
		bool remove = ptr ? (!strcmp(ptr, "remove") || !strcmp(ptr, "replace")) : false;
		bool add    = ptr ? (!strcmp(ptr, "add")    || !strcmp(ptr, "replace")) : true;
		if (remove) {
			int result = allchans.erase(chid);
			printf("[getservices]: %s '%s' (sid=0x%x): %s", add ? "replacing" : "removing",
					name.c_str(), service_id, result ? "succeded.\n" : "FAILED!\n");
		}
		if(!add) {
			node = node->xmlNextNode;
			continue;
		}
		audio_map_set_t * pidmap = CZapit::getInstance()->GetSavedPids(chid);
		if(pidmap)
			apid = pidmap->apid;

		CZapitChannel * channel = new CZapitChannel(name, chid, service_type,
				satellitePosition, freq);
		channel->deltype = delsys;

		service_number_map_t * channel_numbers = (service_type == ST_DIGITAL_RADIO_SOUND_SERVICE) ? &radio_numbers : &tv_numbers;

		if(!keep_numbers)
			number = 0;

		if(number) {
			have_numbers = true;
			service_number_map_t::iterator it = channel_numbers->find(number);
			if(it != channel_numbers->end()) {
				printf("[zapit] duplicate channel number %d: %s id %" PRIx64 " freq %d\n", number,
						name.c_str(), chid, freq);
				number = 0;
				dup_numbers = true; // force save after loading
			} else
				channel_numbers->insert(number);
		}

		bool ret = AddChannel(channel);
		//printf("INS CHANNEL %s %x\n", name.c_str(), (int) &ret.first->second);
		if(ret == false) {
			printf("[zapit] duplicate channel %s id %" PRIx64 " freq %d (old %s at %d)\n",
					name.c_str(), chid, freq, channel->getName().c_str(), channel->getFreqId());
		} else {
			service_count++;
			channel->number = number;
			channel->flags = flags;
			channel->scrambled = scrambled;
			channel->polarization = polarization;
			service_type = channel->getServiceType();
			if(pmtpid != 0 && (((channel->getServiceType() == ST_DIGITAL_RADIO_SOUND_SERVICE) && (apid > 0))
						|| ( (channel->getServiceType() == ST_DIGITAL_TELEVISION_SERVICE)  && (vpid > 0) && (apid > 0))) ) {
				DBG("[getserv] preset chan %s vpid %X sid %X tpid %X onid %X\n", name.c_str(), vpid, service_id, transport_stream_id, transport_stream_id);
				channel->setVideoPid(vpid);
				channel->setAudioPid(apid);
				channel->setPcrPid(pcrpid);
				channel->setPmtPid(pmtpid);
				channel->setTeletextPid(txpid);
				channel->setPidsFlag();
				channel->type = vtype;
			}
		}
		node = node->xmlNextNode;
	}
	return;
}

void CServiceManager::FindTransponder(xmlNodePtr search)
{
	while (search) {
		fe_type_t delsys = FE_QPSK;

		t_satellite_position satellitePosition = xmlGetSignedNumericAttribute(search, "position", 10);
		if (!(strcmp(xmlGetName(search), "cable"))) {
			delsys = FE_QAM;
			char * name = xmlGetAttribute(search, "name");
			satellitePosition = GetSatellitePosition(name);
		}
		else if (!strcmp(xmlGetName(search), "terrestrial")) {
			delsys = FE_OFDM;
			char * name = xmlGetAttribute(search, "name");
			satellitePosition = GetSatellitePosition(name);
		}
		else if ((strcmp(xmlGetName(search), "sat"))) {
			search = search->xmlNextNode;
			continue;
		}
#if 0
		//t_satellite_position satellitePosition = xmlGetSignedNumericAttribute(search, "position", 10);
		char * name = xmlGetAttribute(search, "name");
		t_satellite_position satellitePosition = GetSatellitePosition(name);
#endif
		DBG("going to parse dvb-%c provider %s\n", xmlGetName(search)[0], xmlGetAttribute(search, "name"));
		ParseTransponders(search->xmlChildrenNode, satellitePosition, delsys);
		newfound++;
		search = search->xmlNextNode;
	}
}

void CServiceManager::ParseSatTransponders(fe_type_t fType, xmlNodePtr search, t_satellite_position satellitePosition)
{
	uint8_t polarization = 0;
	FrontendParameters feparams;

	fake_tid = fake_nid = 0;
	satelliteTransponders[satellitePosition].clear();

	xmlNodePtr tps = search->xmlChildrenNode;

	while ((tps = xmlGetNextOccurence(tps, "transponder")) != NULL) {
		memset(&feparams, 0x00, sizeof(FrontendParameters));

		feparams.dvb_feparams.frequency = xmlGetNumericAttribute(tps, "frequency", 0);
		feparams.dvb_feparams.inversion = INVERSION_AUTO;

		if (fType == FE_QAM) {
			feparams.dvb_feparams.u.qam.symbol_rate = xmlGetNumericAttribute(tps, "symbol_rate", 0);
			feparams.dvb_feparams.u.qam.fec_inner = (fe_code_rate_t) xmlGetNumericAttribute(tps, "fec_inner", 0);
			feparams.dvb_feparams.u.qam.modulation = (fe_modulation_t) xmlGetNumericAttribute(tps, "modulation", 0);
			if (feparams.dvb_feparams.frequency > 1000*1000)
				feparams.dvb_feparams.frequency=feparams.dvb_feparams.frequency/1000; //transponderlist was read from tuxbox
		}
		else if (fType == FE_QPSK) {
			feparams.dvb_feparams.u.qpsk.symbol_rate = xmlGetNumericAttribute(tps, "symbol_rate", 0);
			polarization = xmlGetNumericAttribute(tps, "polarization", 0);
			uint8_t system = xmlGetNumericAttribute(tps, "system", 0);
			uint8_t modulation = xmlGetNumericAttribute(tps, "modulation", 0);
			int xml_fec = xmlGetNumericAttribute(tps, "fec_inner", 0);
			xml_fec = CFrontend::getCodeRate(xml_fec, system);
			if(modulation == 2 && ((fe_code_rate_t) xml_fec != FEC_AUTO))
				xml_fec += 9;
			feparams.dvb_feparams.u.qpsk.fec_inner = (fe_code_rate_t) xml_fec;
			feparams.dvb_feparams.frequency = (int) 1000 * (int) round ((double) feparams.dvb_feparams.frequency / (double) 1000);
		}
		else if (fType == FE_OFDM) {
			feparams.dvb_feparams.u.ofdm.bandwidth = (fe_bandwidth_t)
							xmlGetNumericAttribute(tps, "bandwidth", 0);
			feparams.dvb_feparams.u.ofdm.constellation = (fe_modulation_t)
							xmlGetNumericAttribute(tps, "constellation", 0);
			feparams.dvb_feparams.u.ofdm.transmission_mode = (fe_transmit_mode_t)
							xmlGetNumericAttribute(tps, "transmission_mode", 0);
			feparams.dvb_feparams.u.ofdm.code_rate_HP = (fe_code_rate_t)
							xmlGetNumericAttribute(tps, "code_rate_HP", 0);
			feparams.dvb_feparams.u.ofdm.code_rate_LP = (fe_code_rate_t)
							xmlGetNumericAttribute(tps, "code_rate_LP", 0);
			feparams.dvb_feparams.u.ofdm.guard_interval = (fe_guard_interval_t)
							xmlGetNumericAttribute(tps, "guard_interval", 0);
			feparams.dvb_feparams.u.ofdm.hierarchy_information = (fe_hierarchy_t)
							xmlGetNumericAttribute(tps, "hierarchy", 0);
			if (feparams.dvb_feparams.frequency > 1000*1000)
				feparams.dvb_feparams.frequency /= 1000; // old transponder list
		}
		else	/* we'll probably crash sooner or later, so write to STDERR... */
			fprintf(stderr, "[getservices] %s: unknown frontend type %d!\n", __func__, fType);

		freq_id_t freq = CREATE_FREQ_ID(feparams.dvb_feparams.frequency, fType != FE_QPSK);
		polarization &= 7;

		transponder_id_t tid = CREATE_TRANSPONDER_ID64(freq, satellitePosition, fake_nid, fake_tid);
		transponder t(fType, tid, feparams, polarization);
		satelliteTransponders[satellitePosition].insert(transponder_pair_t(tid, t));

		fake_nid ++; fake_tid ++;

		tps = tps->xmlNextNode;
	}
}

int CServiceManager::LoadMotorPositions(void)
{
	FILE *fd = NULL;
	char buffer[256] = "";
	t_satellite_position satellitePosition;
	int spos = 0, mpos = 0, diseqc = 0, uncom = 0, com = 0, usals = 0, inuse, input = 0;
	int offH = 10600, offL = 9750, sw = 11700;

	printf("[getservices] loading motor positions...\n");

	/* this is only read and never written, so it only serves for
	 * upgrading from old pre-multituner capable neutrino */
	if ((fd = fopen(SATCONFIG, "r"))) {
		fgets(buffer, 255, fd);
		while(!feof(fd)) {
			sscanf(buffer, "%d %d %d %d %d %d %d %d %d %d %d", &spos, &mpos, &diseqc, &com, &uncom, &offL, &offH, &sw, &inuse, &usals, &input);

			int configured = 0;
			if (diseqc != -1 || com != -1 || uncom != -1 || usals != 0 || mpos != 0)
				configured = 1;
			satellitePosition = spos;
			sat_iterator_t sit = satellitePositions.find(satellitePosition);
			if(sit != satellitePositions.end()) {
				sit->second.diseqc = diseqc;
				sit->second.commited = com;
				sit->second.uncommited = uncom;
				sit->second.motor_position = mpos;
				sit->second.lnbOffsetLow = offL;
				sit->second.lnbOffsetHigh = offH;
				sit->second.lnbSwitch = sw;
				sit->second.use_in_scan = inuse;
				sit->second.use_usals = usals;
				sit->second.input = input;
				sit->second.position = satellitePosition;
				sit->second.configured = configured;
			}
			fgets(buffer, 255, fd);
		}
		fclose(fd);
	}
	else
		printf("[getservices] %s not found.\n", SATCONFIG);

	return 0;
}
#if 0 
//never used
void CServiceManager::SaveMotorPositions()
{
	FILE * fd;
	sat_iterator_t sit;
	printf("[getservices] saving motor positions...\n");

	fd = fopen(SATCONFIG, "w");
	if(fd == NULL) {
		printf("[zapit] cannot open %s\n", SATCONFIG);
		return;
	}
	fprintf(fd, "# sat position, stored rotor, diseqc, commited, uncommited, low, high, switch, use in full scan, use usals, input\n");
	for(sit = satellitePositions.begin(); sit != satellitePositions.end(); ++sit) {
		fprintf(fd, "%d %d %d %d %d %d %d %d %d %d %d\n", sit->first, sit->second.motor_position,
				sit->second.diseqc, sit->second.commited, sit->second.uncommited, sit->second.lnbOffsetLow,
				sit->second.lnbOffsetHigh, sit->second.lnbSwitch, sit->second.use_in_scan, sit->second.use_usals, sit->second.input);
	}
	fdatasync(fileno(fd));
	fclose(fd);
}
#endif
bool CServiceManager::InitSatPosition(t_satellite_position position, char * name, bool force, int deltype)
{
	if(force || (satellitePositions.find(position) == satellitePositions.end())) {
		satellitePositions[position].position = position;
		satellitePositions[position].diseqc = -1;
		satellitePositions[position].commited = -1;
		satellitePositions[position].uncommited = -1;
		satellitePositions[position].motor_position = 0;
		satellitePositions[position].diseqc_order = 0;
		satellitePositions[position].lnbOffsetLow = 9750;
		satellitePositions[position].lnbOffsetHigh = 10600;
		satellitePositions[position].lnbSwitch = 11700;
		satellitePositions[position].use_in_scan = 0;
		satellitePositions[position].use_usals = 0;
		satellitePositions[position].input = 0;
		satellitePositions[position].configured = 0;
		satellitePositions[position].cable_nid = 0;
		satellitePositions[position].deltype = deltype;
		if(name)
			satellitePositions[position].name = name;
		return true;
	}
	return false;
}

bool CServiceManager::LoadScanXml(fe_type_t delsys)
{
	if (ParseScanXml(delsys)) {
		/* fake position for non-satellite */
		t_satellite_position position = 0;

		xmlNodePtr search = xmlDocGetRootElement(scanInputParser)->xmlChildrenNode;
		while (search) {
			if (!(strcmp(xmlGetName(search), "sat"))) {
				position = xmlGetSignedNumericAttribute(search, "position", 10);
				char * name = xmlGetAttribute(search, "name");
				InitSatPosition(position, name, false, delsys);
			} else if (!(strcmp(xmlGetName(search), "terrestrial"))) {
				char * name = xmlGetAttribute(search, "name");
				position = fake_pos++;
				position &= 0x0EFF;
				InitSatPosition(position, name, false, delsys);
			} else if(!(strcmp(xmlGetName(search), "cable"))) {
				char * name = xmlGetAttribute(search, "name");
				position = fake_pos++;
				InitSatPosition(position, name, false, delsys);
				satellitePositions[position].cable_nid = xmlGetNumericAttribute(search, "nid", 0);
			}
			ParseSatTransponders(delsys, search, position);
			search = search->xmlNextNode;
		}
		delete scanInputParser;
		scanInputParser = NULL;
		return true;
	}
	return false;
}

bool CServiceManager::LoadServices(bool only_current)
{
	if(CFEManager::getInstance()->getLiveFE() == NULL)
		return false;

	xmlDocPtr parser;
	service_count = 0;
	printf("[zapit] Loading services, channel size %d ..\n", (int)sizeof(CZapitChannel));
	//frontendType = CFEManager::getInstance()->getLiveFE()->getInfo()->type;

	if(only_current)
		goto do_current;

#if 0 // FIXME: obsolete ?
	static bool satcleared = 0;//clear only once, because menu is static
	if(!satcleared) {
		satellitePositions.clear();
		satcleared = 1;
	}
#endif

	TIMER_START();
	allchans.clear();
	transponders.clear();
	tv_numbers.clear();
	radio_numbers.clear();
	have_numbers = false;
	dup_numbers = false;

	fake_tid = fake_nid = 0;
	fake_pos = 0xF00;

	if (CFEManager::getInstance()->haveSat()) {
		INFO("Loading satellites...");
		LoadScanXml(FE_QPSK);
	}
	if (CFEManager::getInstance()->haveCable()) {
		INFO("Loading cables...");
		LoadScanXml(FE_QAM);
	}
	if (CFEManager::getInstance()->haveTerr()) {
		INFO("Loading terrestrial...");
		LoadScanXml(FE_OFDM);
	}

	parser = parseXmlFile(SERVICES_XML);
	if (parser != NULL) {
		xmlNodePtr search = xmlDocGetRootElement(parser)->xmlChildrenNode;
		while (search) {
			char * name = xmlGetAttribute(search, "name");
			t_satellite_position position;
			if (!(strcmp(xmlGetName(search), "sat"))) {
				position = xmlGetSignedNumericAttribute(search, "position", 10);
				InitSatPosition(position, name, false, FE_QPSK);
			} else if (!(strcmp(xmlGetName(search), "terrestrial"))) {
				position = GetSatellitePosition(name);
				if (!position)
					position = (0x0EFF & fake_pos++);
				InitSatPosition(position, name, false, FE_OFDM);
			} else {
				position = GetSatellitePosition(name);
				if (!position)
					position = fake_pos++;
				InitSatPosition(position, name, false, FE_QAM);
			}

			search = search->xmlNextNode;
		}
		FindTransponder(xmlDocGetRootElement(parser)->xmlChildrenNode);
		xmlFreeDoc(parser);
	}

#if 0
	if (CFEManager::getInstance()->haveSat()) {
		LoadMotorPositions();
	}
#endif

	LoadProviderMap();
	printf("[zapit] %d services loaded (%d)...\n", service_count, (int)allchans.size());
	TIMER_STOP("[zapit] service loading took");

	if(0) { //zapit_debug) {//FIXME
		sat_iterator_t sit;
		for(sit = satellitePositions.begin(); sit != satellitePositions.end(); ++sit)
			printf("satelliteName = %s (%d), satellitePosition = %d motor position = %d usals %d\n", sit->second.name.c_str(), (int)sit->second.name.size(), sit->first, sit->second.motor_position, sit->second.use_usals);
	}
	/* reset flag after loading services.xml */
	services_changed = false;
do_current:
#if 0
	DBG("Loading current..\n");
	if (CZapit::getInstance()->scanSDT() && (parser = parseXmlFile(CURRENTSERVICES_XML))) {
		newfound = 0;
		printf("[getservices] " CURRENTSERVICES_XML "  found.\n");
		FindTransponder(xmlDocGetRootElement(parser)->xmlChildrenNode);
		xmlFreeDoc(parser);
		unlink(CURRENTSERVICES_XML);
		if(newfound) {
			//SaveServices(true);
			services_changed = true;
		}
	}
#endif
	if(!only_current) {
		parser = parseXmlFile(MYSERVICES_XML);
		if (parser != NULL) {
			FindTransponder(xmlDocGetRootElement(parser)->xmlChildrenNode);
			xmlFreeDoc(parser);
		}
	}
	/* if no numbers, zapit will save after loading bouquets, with numbers */
	if(service_count && keep_numbers && (!have_numbers || dup_numbers))
		services_changed = true;

	return true;
}

void CServiceManager::CopyFile(char * from, char * to)
{
        char cmd[256] = "cp -f ";
        strcat(cmd, from);
        strcat(cmd, " ");
        strcat(cmd, to);
        system(cmd);
	sync();
}

void CServiceManager::WriteSatHeader(FILE * fd, sat_config_t &config)
{
	switch (config.deltype) {
		case FE_QPSK: /* satellite */
			fprintf(fd, "\t<sat name=\"%s\" position=\"%hd\" diseqc=\"%hd\" uncommited=\"%hd\">\n",
					config.name.c_str(), config.position, config.diseqc, config.uncommited);
			break;
		case FE_QAM: /* cable */
			fprintf(fd, "\t<cable name=\"%s\" position=\"%hd\">\n", config.name.c_str(), config.position);
			break;
		case FE_OFDM: /* terrestrial */
			fprintf(fd, "\t<terrestrial name=\"%s\" position=\"%hd\">\n", config.name.c_str(), config.position);
			break;
		default:
			break;
	}
}

void CServiceManager::SaveServices(bool tocopy, bool if_changed, bool no_deleted)
{
	int processed = 0;

	if(if_changed && !services_changed)
		return;

#ifdef SAVE_DEBUG
	set<t_channel_id> chans_processed;
#endif
	printf("CServiceManager::SaveServices: total channels: %d\n", (int)allchans.size());
	FILE * fd = fopen(SERVICES_TMP, "w");
	if(!fd) {
		perror(SERVICES_TMP);
		return;
	}
	fprintf(fd, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<zapit api=\"3\">\n");
	for (sat_iterator_t spos_it = satellitePositions.begin(); spos_it != satellitePositions.end(); ++spos_it) {
		bool satdone = 0;
#ifdef SAVE_DEBUG
		printf("Process sat: %s\n", spos_it->second.name.c_str());
		printf("processed channels: %d\n", chans_processed.size());
		printf("tp count: %d\n", transponders.size());
#endif
		for(transponder_list_t::iterator tI = transponders.begin(); tI != transponders.end(); ++tI) {
			bool tpdone = 0;
			if(tI->second.satellitePosition != spos_it->first) {
#ifdef SAVE_DEBUG
				printf("Sat position %d not found !!\n", satpos);
#endif
				continue;
			}
			for (channel_map_iterator_t ccI = allchans.begin(); ccI != allchans.end(); ++ccI) {
				if(ccI->second.flags & CZapitChannel::NOT_FOUND)
					continue;
				if(ccI->second.getTransponderId() == tI->first) {
					if(!satdone) {
						WriteSatHeader(fd, spos_it->second);
						satdone = 1;
					}
					if(!tpdone) {
						tI->second.dumpServiceXml(fd);
						tpdone = 1;
					}

					/* don't dump removed channels if no_deleted == true */
					if (!no_deleted || !(ccI->second.flags & CZapitChannel::REMOVED))
						ccI->second.dumpServiceXml(fd);
					processed++;
#ifdef SAVE_DEBUG
					chans_processed.insert(ccI->second.getChannelID());
#endif
				}
			}
			if(tpdone) fprintf(fd, "\t\t</TS>\n");
		}
		if(satdone) {
			switch (spos_it->second.deltype) {
				case FE_QPSK:
					fprintf(fd, "\t</sat>\n");
					break;
				case FE_QAM:
					fprintf(fd, "\t</cable>\n");
					break;
				case FE_OFDM:
					fprintf(fd, "\t</terrestrial>\n");
					break;
				default:
					break;
			}
		}
	}
	fprintf(fd, "</zapit>\n");
	fclose(fd);
	if(tocopy) {
		CopyFile((char *) SERVICES_TMP, (char *) SERVICES_XML);
		unlink(SERVICES_TMP);
	}
#ifdef SAVE_DEBUG
	printf("processed channels: %d\n", chans_processed.size());
	int i = 0;
	for (channel_map_iterator_t it = allchans.begin(); it != allchans.end(); ++it)
		if (chans_processed.find(it->first) == chans_processed.end())
			printf("unused channel %d sat %d freq %d sid %04X: %s\n", ++i, it->second.getSatellitePosition(), it->second.getFreqId(), it->second.getServiceId(), it->second.getName().c_str());
	chans_processed.clear();
#endif
	printf("CServiceManager::SaveServices: processed channels: %d\n", processed);
	services_changed = false;
}

bool CServiceManager::CopyCurrentServices(transponder_id_t tpid)
{
	channel_map_iterator_t aI;
	bool updated = false;

	for (channel_map_iterator_t cI = curchans.begin(); cI != curchans.end(); ++cI) {
		aI = allchans.find(cI->second.getChannelID());
		if(aI == allchans.end()) {
			channel_insert_res_t ret = allchans.insert(channel_pair_t (cI->second.getChannelID(), cI->second));
			ret.first->second.flags = CZapitChannel::NEW;
			updated = true;
			printf("CServiceManager::CopyCurrentServices: [%s] add\n", cI->second.getName().c_str());
		} else {
			if(cI->second.scrambled != aI->second.scrambled || cI->second.getName() != aI->second.getName()) {
				aI->second.setName(cI->second.getName());
				aI->second.scrambled = cI->second.scrambled;
				aI->second.flags = CZapitChannel::UPDATED;
				updated = true;
				printf("CServiceManager::CopyCurrentServices: [%s] replace\n", cI->second.getName().c_str());
			}
		}
	}
	for (aI = allchans.begin(); aI != allchans.end(); ++aI) {
		if(aI->second.getTransponderId() == tpid) {
			channel_map_iterator_t dI = curchans.find(aI->second.getChannelID());
			if(dI == curchans.end()) {
				aI->second.flags = CZapitChannel::REMOVED;
				updated = true;
				printf("CServiceManager::CopyCurrentServices: [%s] remove\n", aI->second.getName().c_str());
			} else if(aI->second.flags & CZapitChannel::REMOVED) {
				printf("CServiceManager::CopyCurrentServices: [%s] restore\n", aI->second.getName().c_str());
				aI->second.flags = CZapitChannel::UPDATED;
				updated = true;
			}
		}
	}
	if(updated)
		services_changed = true;

	return updated;
}

/* helper for reused code */
void CServiceManager::WriteCurrentService(FILE * fd, bool &satfound, bool &tpdone,
		bool &updated, char * satstr, transponder &tp, CZapitChannel &channel, const char * action)
{
	if(!tpdone) {
		if(!satfound)
			fprintf(fd, "%s", satstr);
		tp.dumpServiceXml(fd);
		tpdone = 1;
	}
	updated = 1;
	channel.dumpServiceXml(fd, action);
}

bool CServiceManager::SaveCurrentServices(transponder_id_t tpid)
{
	channel_map_iterator_t ccI;
	channel_map_iterator_t dI;
	transponder_list_t::iterator tI;
	char satstr[256];
	char buffer[256];

	FILE * fd = 0;
	FILE * fd1 = 0;

	bool updated = 0;
	bool tpdone = 0;
	bool satfound = 0;

	tI = transponders.find(tpid);
	if(tI == transponders.end()) {
		printf("[sdt monitor] tp not found ?!\n");
		return false;
	}
	t_satellite_position satellitePosition = tI->second.satellitePosition;

	fd = fopen(CURRENTSERVICES_TMP, "w");
	if(!fd) {
		printf("[sdt monitor] " CURRENTSERVICES_TMP ": cant open!\n");
		return false;
	}

	sat_iterator_t spos_it = satellitePositions.find(satellitePosition);
	if(spos_it == satellitePositions.end()){
		fclose(fd);
		return false;
	}

	const char * footer = "";
	switch (tI->second.deltype) {
		case FE_QPSK: /* satellite */
			sprintf(satstr, "\t<%s name=\"%s\" position=\"%hd\">\n", "sat", spos_it->second.name.c_str(), satellitePosition);
			footer = "</sat>";
			break;
		case FE_QAM: /* cable */
			sprintf(satstr, "\t<%s name=\"%s\"\n", "cable", spos_it->second.name.c_str());
			footer = "</cable>";
			break;
		case FE_OFDM:
			sprintf(satstr, "\t<%s name=\"%s\"\n", "terrestrial", spos_it->second.name.c_str());
			footer = "</terrestrial>";
			break;
		default:
			break;
	}
	fd1 = fopen(CURRENTSERVICES_XML, "r");
	if(!fd1) {
		fprintf(fd, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<zapit>\n");
	} else {
		fgets(buffer, 255, fd1);
		while(!feof(fd1) && !strstr(buffer, satfound ? footer : "</zapit>")) {
			if(!satfound && !strcmp(buffer, satstr))
				satfound = 1;
			fputs(buffer, fd);
			fgets(buffer, 255, fd1);
		}
	}
	for (channel_map_iterator_t cI = curchans.begin(); cI != curchans.end(); ++cI) {
		ccI = allchans.find(cI->second.getChannelID());
		if(ccI == allchans.end()) {
			WriteCurrentService(fd, satfound, tpdone, updated, satstr, tI->second, cI->second, "add");
		} else {
			if(strcmp(cI->second.getName().c_str(), ccI->second.getName().c_str()) || cI->second.scrambled != ccI->second.scrambled) {
				cI->second.number = ccI->second.number;
				WriteCurrentService(fd, satfound, tpdone, updated, satstr, tI->second, cI->second, "replace");
			}
		}
	}
	for (ccI = allchans.begin(); ccI != allchans.end(); ++ccI) {
		if(ccI->second.getTransponderId() == tpid) {
			dI = curchans.find(ccI->second.getChannelID());
			if(dI == curchans.end())
				WriteCurrentService(fd, satfound, tpdone, updated, satstr, tI->second, ccI->second, "remove");
		}
	}
	if(tpdone) {
		fprintf(fd, "\t\t</TS>\n");
		fprintf(fd, "\t%s\n", footer);
	} else if(satfound)
		fprintf(fd, "\t%s\n", footer);
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

	return updated;
}

#define PROVIDER_MAP_XML CONFIGDIR "/providermap.xml"
bool CServiceManager::LoadProviderMap()
{
	xmlDocPtr parser;

	replace_map.clear();
	parser = parseXmlFile(PROVIDER_MAP_XML);
	if (parser != NULL) {
		xmlNodePtr node = xmlDocGetRootElement(parser)->xmlChildrenNode;
		while ((node = xmlGetNextOccurence(node, "TS")) != NULL) {
			provider_replace replace;
			replace.transport_stream_id = xmlGetNumericAttribute(node, "id", 16);
			replace.original_network_id = xmlGetNumericAttribute(node, "on", 16);
			replace.frequency = xmlGetNumericAttribute(node, "frq", 0);

			char * name = xmlGetAttribute(node, "name");
			char * newname = xmlGetAttribute(node, "newname");
			if(name)
				replace.name = name;
			if(newname)
				replace.newname = newname;

			DBG("prov map: tsid %04x onid %04x freq %d name [%s] to [%s]\n",
					replace.transport_stream_id, replace.original_network_id,
					replace.frequency, replace.name.c_str(), replace.newname.c_str());
			replace_map.push_back(replace);
			node = node->xmlNextNode;
		}
		xmlFreeDoc(parser);
		return true;
	}
	return false;
}

bool CServiceManager::ReplaceProviderName(std::string &name, t_transport_stream_id tsid, t_original_network_id onid)
{
	std::string newname;

	prov_replace_map_iterator_t it;
	for (it = replace_map.begin(); it != replace_map.end(); ++it) {
		provider_replace replace = *it;
		/* if replace map has tsid and onid */
		if(replace.transport_stream_id && replace.original_network_id) {
			/* compare tsid/onid */
			if(replace.transport_stream_id == tsid && replace.original_network_id == onid) {
				/* if new name present, old name should be present */
				if(!replace.newname.empty()) {
					if (name == replace.name)
						newname = replace.newname;
				} else {
					newname = replace.name;
				}
			}
		} else {
			/* no tsid/onid, only names. if new name present, old name should be present */
			if(!replace.newname.empty()) {
				if(name == replace.name)
					newname = replace.newname;
			}
			/* no tsid/onid, no newname, only name. compare name without case */
			else if(!strcasecmp(replace.name.c_str(), name.c_str()))
				newname = replace.name;
		}
		if(!newname.empty()) {
			DBG("ReplaceProviderName: old [%s] new [%s]\n", name.c_str(), newname.c_str());
			name = newname;
			return true;
		}
	}
	return false;
}
#if 0 
//never used
int CServiceManager::GetFreeNumber(bool radio)
{
	service_number_map_t * channel_numbers = radio ? &radio_numbers : &tv_numbers;
	int i = 0;
	while(true) {
		++i;
		service_number_map_t::iterator it = channel_numbers->find(i);
		if(it == channel_numbers->end()) {
			channel_numbers->insert(i);
			return i;
		}
	}
}
#endif
int CServiceManager::GetMaxNumber(bool radio)
{
	service_number_map_t * channel_numbers = radio ? &radio_numbers : &tv_numbers;
	int i = 0;
	service_number_map_t::iterator it = channel_numbers->end();
	if(it != channel_numbers->begin()) {
		--it;
		i = *it;
	}
	return i+1;
}
#if 0 
//never used
void CServiceManager::FreeNumber(int number, bool radio)
{
	service_number_map_t * channel_numbers = radio ? &radio_numbers : &tv_numbers;
	channel_numbers->erase(number);
}

void CServiceManager::UseNumber(int number, bool radio)
{
	service_number_map_t * channel_numbers = radio ? &radio_numbers : &tv_numbers;
	channel_numbers->insert(number);
}
#endif
bool CServiceManager::GetTransponder(transponder_id_t tid, transponder &t)
{
	stiterator tI = transponders.find(tid);
	if(tI != transponders.end()) {
		t = tI->second;
		return true;
	}
	return false;
}

void CServiceManager::UpdateSatTransponders(t_satellite_position satellitePosition)
{
	pair<map<transponder_id_t, transponder>::iterator,bool> ret;
	transponder_list_t & stransponders = satelliteTransponders[satellitePosition];
	for (transponder_list_t::iterator tI = transponders.begin(); tI != transponders.end(); ++tI) {
		for (stiterator stI = stransponders.begin(); stI != stransponders.end(); ++stI) {
			if (stI->second.compare(tI->second)) {
				stransponders.erase(stI);
				break;
			}
		}
		if (tI->second.satellitePosition == satellitePosition) {
			ret = stransponders.insert(transponder_pair_t(tI->first, tI->second));
			if (ret.second == false)
				tI->second.dump("[zapit] duplicate in sat transponders:");
		}
	}
}

bool CServiceManager::IsChannelTVChannel(const t_channel_id channel_id)
{
	bool ret = true;
	CZapitChannel * channel = FindChannel(channel_id);
	if(channel)
		ret = (channel->getServiceType() != ST_DIGITAL_RADIO_SOUND_SERVICE);
	return ret;
}
