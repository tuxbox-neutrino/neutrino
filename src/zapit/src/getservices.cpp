/*
 * $Id: getservices.cpp,v 1.84 2003/12/09 21:12:28 thegoodguy Exp $
 *
 * (C) 2002, 2003 by Andreas Oberritter <obi@tuxbox.org>
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
#include <zapit/frontend_c.h>
#include <zapit/getservices.h>
#include <zapit/settings.h>
#include <zapit/satconfig.h>
#include <xmlinterface.h>
#include <math.h>
#include <sys/time.h>

//#define SAVE_DEBUG

extern transponder_list_t transponders;

satellite_map_t satellitePositions;
std::map<transponder_id_t, transponder> select_transponders;

extern int zapit_debug;

CServiceManager * CServiceManager::manager = NULL;

CServiceManager::CServiceManager()
{
	scanInputParser = NULL;
	service_count = 0;
}

CServiceManager::~CServiceManager()
{
	delete scanInputParser;
}

CServiceManager * CServiceManager::getInstance()
{
	if(manager == NULL)
		manager = new CServiceManager();
	return manager;
}

bool CServiceManager::ParseScanXml(void)
{
	if(scanInputParser) {
		delete scanInputParser;
		scanInputParser = NULL;
	}
	switch (CFrontend::getInstance()->getInfo()->type) {
		case FE_QPSK:
			scanInputParser = parseXmlFile(SATELLITES_XML);
			break;

		case FE_QAM:
			scanInputParser = parseXmlFile(CABLES_XML);
			break;

		default:
			WARN("Unknown type %d", CFrontend::getInstance()->getInfo()->type);
			return false;
	}
	return (scanInputParser != NULL);
}

xmlDocPtr CServiceManager::ScanXml()
{
	if(!scanInputParser)
		ParseScanXml();

	return scanInputParser;
}

bool CServiceManager::AddChannel(CZapitChannel * &channel)
{
	channel_insert_res_t ret = allchans.insert (
		channel_pair_t (channel->getChannelID(), *channel));
	delete channel;
	channel = &ret.first->second;
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
	t_channel_id sub_channel_id =
		((uint64_t) ( satellitePosition >= 0 ? satellitePosition : (uint64_t)(0xF000+ abs(satellitePosition))) << 48) |
		(uint64_t) CREATE_CHANNEL_ID_FROM_SERVICE_ORIGINALNETWORK_TRANSPORTSTREAM_ID(service_id, original_network_id, transport_stream_id);

	channel_insert_res_t ret = nvodchannels.insert (
			channel_pair_t (sub_channel_id, *channel));
	delete channel;
	channel = &ret.first->second;
	return ret.second;
}

void CServiceManager::ResetChannelNumbers()
{
	for (channel_map_iterator_t it = allchans.begin(); it != allchans.end(); it++) {
		it->second.number = 0;
		it->second.has_bouquet = 0;
	}
}

void CServiceManager::RemoveChannel(const t_channel_id channel_id)
{
	allchans.erase(channel_id);
}

void CServiceManager::RemoveAllChannels()
{
	allchans.clear();
}

void CServiceManager::RemoveNVODChannels()
{
	nvodchannels.clear();
}

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
	for (channel_map_iterator_t it = allchans.begin(); it != allchans.end(); it++) {
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

bool CServiceManager::GetAllRadioChannels(ZapitChannelList &list)
{
	list.clear();
	for (channel_map_iterator_t it = allchans.begin(); it != allchans.end(); it++) {
		if (it->second.getServiceType() == ST_DIGITAL_RADIO_SOUND_SERVICE)
			list.push_back(&(it->second));
	}
	return (list.size() != 0);
}

bool CServiceManager::GetAllTvChannels(ZapitChannelList &list)
{
	list.clear();
	for (channel_map_iterator_t it = allchans.begin(); it != allchans.end(); it++) {
		if (it->second.getServiceType() != ST_DIGITAL_RADIO_SOUND_SERVICE)
			list.push_back(&(it->second));
	}
	return (list.size() != 0);
}

bool CServiceManager::GetAllHDChannels(ZapitChannelList &list)
{
	list.clear();
	for (channel_map_iterator_t it = allchans.begin(); it != allchans.end(); it++) {
		if (it->second.isHD())
			list.push_back(&(it->second));
	}
	return (list.size() != 0);
}

bool CServiceManager::GetAllUnusedChannels(ZapitChannelList &list)
{
	list.clear();
	for (channel_map_iterator_t it = allchans.begin(); it != allchans.end(); it++) {
		if (it->second.has_bouquet == false)
			list.push_back(&(it->second));
	}
	return (list.size() != 0);
}

bool CServiceManager::GetAllSatelliteChannels(ZapitChannelList &list, t_satellite_position position)
{
	list.clear();
	for (channel_map_iterator_t it = allchans.begin(); it != allchans.end(); it++) {
		if(it->second.getSatellitePosition() == position)
			list.push_back(&(it->second));
	}
	return (list.size() != 0);
}

std::string CServiceManager::GetServiceName(t_channel_id channel_id)
{
        channel_map_iterator_t it = allchans.find(channel_id);
        if (it != allchans.end())
                return it->second.getName();
        else
                return "";
}

void CServiceManager::ParseTransponders(xmlNodePtr node, t_satellite_position satellitePosition, bool cable)
{
	t_transport_stream_id transport_stream_id;
	t_original_network_id original_network_id;
	FrontendParameters feparams;
	uint8_t polarization = 0;
	uint16_t freq;
	tp_count = 0;

	memset(&feparams, 0, sizeof(FrontendParameters));

	/* read all transponders */
	while ((node = xmlGetNextOccurence(node, "TS")) != NULL) {
		transport_stream_id = xmlGetNumericAttribute(node, "id", 16);
		original_network_id = xmlGetNumericAttribute(node, "on", 16);
		feparams.frequency = xmlGetNumericAttribute(node, "frq", 0);
		feparams.inversion = (fe_spectral_inversion) xmlGetNumericAttribute(node, "inv", 0);

		if(cable) {
			feparams.u.qam.symbol_rate = xmlGetNumericAttribute(node, "sr", 0);
			feparams.u.qam.fec_inner = (fe_code_rate_t) xmlGetNumericAttribute(node, "fec", 0);
			feparams.u.qam.modulation = (fe_modulation_t) xmlGetNumericAttribute(node, "mod", 0);

			if (feparams.frequency > 1000*1000)
				feparams.frequency = feparams.frequency/1000; //transponderlist was read from tuxbox

			//feparams.frequency = (int) 1000 * (int) round ((double) feparams.frequency / (double) 1000);
		} else {
			feparams.u.qpsk.fec_inner = (fe_code_rate_t) xmlGetNumericAttribute(node, "fec", 0);
			feparams.u.qpsk.symbol_rate = xmlGetNumericAttribute(node, "sr", 0);

			polarization = xmlGetNumericAttribute(node, "pol", 0);

			if(feparams.u.qpsk.symbol_rate < 50000) feparams.u.qpsk.symbol_rate = feparams.u.qpsk.symbol_rate * 1000;

			if(feparams.frequency < 20000) 
				feparams.frequency = feparams.frequency*1000;
			else
				feparams.frequency = (int) 1000 * (int) round ((double) feparams.frequency / (double) 1000);
		}
		if(cable)
			freq = feparams.frequency/100;
		else
			freq = feparams.frequency/1000;

		transponder_id_t tid = CREATE_TRANSPONDER_ID_FROM_SATELLITEPOSITION_ORIGINALNETWORK_TRANSPORTSTREAM_ID(freq, satellitePosition,original_network_id,transport_stream_id);
		pair<map<transponder_id_t, transponder>::iterator,bool> ret;

		ret = transponders.insert (
				std::pair <transponder_id_t, transponder> ( tid,
					transponder (transport_stream_id, feparams,
						polarization, original_network_id)
					)
				);
		if (ret.second == false)
			printf("[zapit] duplicate transponder id %llx freq %d\n", tid, feparams.frequency);

		/* read channels that belong to the current transponder */
		ParseChannels(node->xmlChildrenNode, transport_stream_id, original_network_id, satellitePosition, freq);

		/* hop to next transponder */
		node = node->xmlNextNode;
	}
	return;
}

void CServiceManager::ParseChannels(xmlNodePtr node, const t_transport_stream_id transport_stream_id, const t_original_network_id original_network_id, t_satellite_position satellitePosition, freq_id_t freq)
{
	t_service_id service_id;
	std::string  name;
	uint8_t      service_type;
	unsigned short vpid, apid, pcrpid, pmtpid, txpid, vtype, scrambled;
	std::string desc = "";
	desc += "Preset";
	t_channel_id chid;
	int dummy;
	int * have_ptr = &dummy;

	sat_iterator_t sit = satellitePositions.find(satellitePosition);
	if(sit != satellitePositions.end())
		have_ptr = &sit->second.have_channels;

	while ((node = xmlGetNextOccurence(node, "S")) != NULL) {
		*have_ptr = 1;
		service_id = xmlGetNumericAttribute(node, "i", 16);
		name = xmlGetAttribute(node, "n");
		service_type = xmlGetNumericAttribute(node, "t", 16);
		vpid = xmlGetNumericAttribute(node, "v", 16);
		apid = xmlGetNumericAttribute(node, "a", 16);
		pcrpid = xmlGetNumericAttribute(node, "p", 16);
		pmtpid = xmlGetNumericAttribute(node, "pmt", 16);
		txpid = xmlGetNumericAttribute(node, "tx", 16);
		vtype = xmlGetNumericAttribute(node, "vt", 16);
		scrambled = xmlGetNumericAttribute(node, "s", 16);

		chid = CREATE_CHANNEL_ID64;
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

		CZapitChannel * channel = new CZapitChannel ( name, service_id,
				transport_stream_id,
				original_network_id,
				service_type,
				satellitePosition,
				freq);

		bool ret = AddChannel(channel);

		//printf("INS CHANNEL %s %x\n", name.c_str(), (int) &ret.first->second);
		if(ret == false) {
			printf("[zapit] duplicate channel %s id %llx freq %d (old %s at %d)\n",
					name.c_str(), chid, freq, channel->getName().c_str(), channel->getFreqId());
		} else {
			service_count++;
			channel->scrambled = scrambled;
			service_type = channel->getServiceType();
			if(pmtpid != 0 && (((service_type == 2) && (apid > 0)) || ( (service_type == 1)  && (vpid > 0) && (apid > 0))) ) {
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
	t_satellite_position satellitePosition = 0;
	while (search) {
		bool cable = false;

		if (!(strcmp(xmlGetName(search), "cable")))
			cable = true;
		else if ((strcmp(xmlGetName(search), "sat"))) {
			search = search->xmlNextNode;
			continue;
		}

		satellitePosition = xmlGetSignedNumericAttribute(search, "position", 10);
		DBG("going to parse dvb-%c provider %s\n", xmlGetName(search)[0], xmlGetAttribute(search, "name"));
		ParseTransponders(search->xmlChildrenNode, satellitePosition, cable);
		newfound++;
		search = search->xmlNextNode;
	}
}

void CServiceManager::ParseSatTransponders(fe_type_t frontendType, xmlNodePtr search, t_satellite_position satellitePosition)
{
	uint8_t polarization = 0;
	uint8_t system = 0, modulation = 1;
	int xml_fec;
	FrontendParameters feparams;

	fake_tid = fake_nid = 0;

	xmlNodePtr tps = search->xmlChildrenNode;

	while ((tps = xmlGetNextOccurence(tps, "transponder")) != NULL) {
		memset(&feparams, 0x00, sizeof(FrontendParameters));

		feparams.frequency = xmlGetNumericAttribute(tps, "frequency", 0);

		freq_id_t freq;
		if (frontendType == FE_QAM) {
			if (feparams.frequency > 1000*1000)
				feparams.frequency=feparams.frequency/1000; //transponderlist was read from tuxbox
			//feparams.frequency = (int) 1000 * (int) round ((double) feparams.frequency / (double) 1000);
			freq = feparams.frequency/100;
		}
		else {
			feparams.frequency = (int) 1000 * (int) round ((double) feparams.frequency / (double) 1000);
			freq = feparams.frequency/1000;
		}

		feparams.inversion = INVERSION_AUTO;

		if (frontendType == FE_QAM) {
			feparams.u.qam.symbol_rate = xmlGetNumericAttribute(tps, "symbol_rate", 0);
			feparams.u.qam.fec_inner = (fe_code_rate_t) xmlGetNumericAttribute(tps, "fec_inner", 0);
			feparams.u.qam.modulation = (fe_modulation_t) xmlGetNumericAttribute(tps, "modulation", 0);
		}
		else if (frontendType == FE_QPSK) {
			feparams.u.qpsk.symbol_rate = xmlGetNumericAttribute(tps, "symbol_rate", 0);
			polarization = xmlGetNumericAttribute(tps, "polarization", 0);
			system = xmlGetNumericAttribute(tps, "system", 0);
			modulation = xmlGetNumericAttribute(tps, "modulation", 0);
			xml_fec = xmlGetNumericAttribute(tps, "fec_inner", 0);
			xml_fec = CFrontend::getCodeRate(xml_fec, system);
			if(modulation == 2)
				xml_fec += 9;
			feparams.u.qpsk.fec_inner = (fe_code_rate_t) xml_fec;
		}
		
		transponder_id_t tid =
			CREATE_TRANSPONDER_ID_FROM_SATELLITEPOSITION_ORIGINALNETWORK_TRANSPORTSTREAM_ID(
					freq /*feparams.frequency/1000*/, satellitePosition, fake_nid, fake_tid);

		polarization &= 1;
		select_transponders.insert (
				std::pair <transponder_id_t, transponder> (tid,
					transponder (fake_tid, feparams,
						polarization, fake_nid))
				);
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

	if ((fd = fopen(SATCONFIG, "r"))) {
		fgets(buffer, 255, fd);
		while(!feof(fd)) {
			sscanf(buffer, "%d %d %d %d %d %d %d %d %d %d %d", &spos, &mpos, &diseqc, &com, &uncom, &offL, &offH, &sw, &inuse, &usals, &input);

			satellitePosition = spos;
			sat_iterator_t sit = satellitePositions.find(satellitePosition);
			if(sit != satellitePositions.end()) {
				sit->second.motor_position = mpos;
				sit->second.diseqc = diseqc;
				sit->second.commited = com;
				sit->second.uncommited = uncom;
				sit->second.lnbOffsetLow = offL;
				sit->second.lnbOffsetHigh = offH;
				sit->second.lnbSwitch = sw;
				sit->second.use_in_scan = inuse;
				sit->second.use_usals = usals;
				sit->second.input = input;
				sit->second.position = satellitePosition;
			}
			fgets(buffer, 255, fd);
		}
		fclose(fd);
	}
	else
		printf("[getservices] %s not found.\n", SATCONFIG);

	return 0;
}

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
	for(sit = satellitePositions.begin(); sit != satellitePositions.end(); sit++) {
		fprintf(fd, "%d %d %d %d %d %d %d %d %d %d %d\n", sit->first, sit->second.motor_position,
				sit->second.diseqc, sit->second.commited, sit->second.uncommited, sit->second.lnbOffsetLow,
				sit->second.lnbOffsetHigh, sit->second.lnbSwitch, sit->second.use_in_scan, sit->second.use_usals, sit->second.input);
	}
	fdatasync(fileno(fd));
	fclose(fd);
}

void CServiceManager::InitSatPosition(t_satellite_position position)
{
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
}

bool CServiceManager::LoadServices(bool only_current)
{
	xmlDocPtr parser;
	static bool satcleared = 0;//clear only once, because menu is static
	service_count = 0;
	printf("[zapit] Loading services, channel size %d ..\n", sizeof(CZapitChannel));
	fe_type_t frontendType = CFrontend::getInstance()->getInfo()->type;

	if(only_current)
		goto do_current;


	TIMER_START();
	allchans.clear();
	transponders.clear();
	select_transponders.clear();
	fake_tid = fake_nid = 0;

	if (ParseScanXml()) {
		t_satellite_position position = 0;
		if(!satcleared)
			satellitePositions.clear();
		satcleared = 1;

		xmlNodePtr search = xmlDocGetRootElement(scanInputParser)->xmlChildrenNode;
		while (search) {
			if (!(strcmp(xmlGetName(search), "sat"))) {
				position = xmlGetSignedNumericAttribute(search, "position", 10);
				char * name = xmlGetAttribute(search, "name");
				/* FIXME reuse this */
				if(satellitePositions.find(position) == satellitePositions.end()) {
					InitSatPosition(position);
				}
				satellitePositions[position].name = name;
			} else if(!(strcmp(xmlGetName(search), "cable"))) {
				char * name = xmlGetAttribute(search, "name");
				if(satellitePositions.find(position) == satellitePositions.end()) {
					InitSatPosition(position);
				}
				satellitePositions[position].name = name;
			}
			ParseSatTransponders(frontendType, search, position);
			position++;
			search = search->xmlNextNode;
		}
	}

	parser = parseXmlFile(SERVICES_XML);
	if (parser != NULL) {
		xmlNodePtr search = xmlDocGetRootElement(parser)->xmlChildrenNode;
		while (search) {
			if (!(strcmp(xmlGetName(search), "sat"))) {
				t_satellite_position position = xmlGetSignedNumericAttribute(search, "position", 10);
				char * name = xmlGetAttribute(search, "name");
				if(satellitePositions.find(position) == satellitePositions.end()) {
					InitSatPosition(position);
					satellitePositions[position].name = name;
				}
			}

			search = search->xmlNextNode;
		}
		FindTransponder(xmlDocGetRootElement(parser)->xmlChildrenNode);
		xmlFreeDoc(parser);
	}

	if(frontendType == FE_QPSK) {
		LoadMotorPositions();
	}

	printf("[zapit] %d services loaded (%d)...\n", service_count, allchans.size());
	TIMER_STOP("[zapit] service loading took");

	if(0) { //zapit_debug) {//FIXME
		sat_iterator_t sit;
		for(sit = satellitePositions.begin(); sit != satellitePositions.end(); sit++)
			printf("satelliteName = %s (%d), satellitePosition = %d motor position = %d usals %d\n", sit->second.name.c_str(), sit->second.name.size(), sit->first, sit->second.motor_position, sit->second.use_usals);
	}
do_current:
	DBG("Loading current..\n");
	if (CZapit::getInstance()->scanSDT() && (parser = parseXmlFile(CURRENTSERVICES_XML))) {
		newfound = 0;
		printf("[getservices] " CURRENTSERVICES_XML "  found.\n");
		FindTransponder(xmlDocGetRootElement(parser)->xmlChildrenNode);
		xmlFreeDoc(parser);
		unlink(CURRENTSERVICES_XML);
		if(newfound)
			SaveServices(true);
	}

	if(!only_current) {
		parser = parseXmlFile(MYSERVICES_XML);
		if (parser != NULL) {
			FindTransponder(xmlDocGetRootElement(parser)->xmlChildrenNode);
			xmlFreeDoc(parser);
		}
	}

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
	switch (CFrontend::getInstance()->getInfo()->type) {
		case FE_QPSK: /* satellite */
			fprintf(fd, "\t<sat name=\"%s\" position=\"%hd\" diseqc=\"%hd\" uncommited=\"%hd\">\n",
					config.name.c_str(), config.position, config.diseqc, config.uncommited);
			break;
		case FE_QAM: /* cable */
			fprintf(fd, "\t<cable name=\"%s\" position=\"%hd\">\n", config.name.c_str(), config.position);
			break;
		case FE_OFDM:
		default:
			break;
	}
}

void CServiceManager::WriteTransponderHeader(FILE * fd, struct transponder &tp)
{
	switch (CFrontend::getInstance()->getInfo()->type) {
		case FE_QPSK: /* satellite */
			fprintf(fd, "\t\t<TS id=\"%04x\" on=\"%04x\" frq=\"%u\" inv=\"%hu\" sr=\"%u\" fec=\"%hu\" pol=\"%hu\">\n",
					tp.transport_stream_id, tp.original_network_id,
					tp.feparams.frequency, tp.feparams.inversion,
					tp.feparams.u.qpsk.symbol_rate, tp.feparams.u.qpsk.fec_inner,
					tp.polarization);
			break;
		case FE_QAM: /* cable */
			fprintf(fd, "\t\t<TS id=\"%04x\" on=\"%04x\" frq=\"%u\" inv=\"%hu\" sr=\"%u\" fec=\"%hu\" mod=\"%hu\">\n",
					tp.transport_stream_id, tp.original_network_id,
					tp.feparams.frequency, tp.feparams.inversion,
					tp.feparams.u.qam.symbol_rate, tp.feparams.u.qam.fec_inner,
					tp.feparams.u.qam.modulation);
			break;
		case FE_OFDM:
		default:
			break;
	}
}

void CServiceManager::SaveServices(bool tocopy)
{
	transponder_id_t 		tpid = 0;
	FILE * fd = 0;
	bool updated = 0;

	channel_map_iterator_t ccI;
	channel_map_iterator_t dI;
	transponder_list_t::iterator tI;
	bool tpdone = 0;
	bool satdone = 0;
	int processed = 0;
	sat_iterator_t spos_it;
	updated = 0;
#ifdef SAVE_DEBUG
	set<t_channel_id> chans_processed;
	DBG("\nChannel size: %d\n", sizeof(CZapitChannel));
#endif
	printf("total channels: %d\n", allchans.size());
	fd = fopen(SERVICES_TMP, "w");
	if(!fd) {
		perror(SERVICES_TMP);
		return;
	}
	fprintf(fd, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<zapit api=\"3\">\n");
	for (spos_it = satellitePositions.begin(); spos_it != satellitePositions.end(); spos_it++) {
		satdone = 0;
#ifdef SAVE_DEBUG
		printf("Process sat: %s\n", spos_it->second.name.c_str());
		printf("processed channels: %d\n", chans_processed.size());
		printf("tp count: %d\n", transponders.size());
#endif
		for(tI = transponders.begin(); tI != transponders.end(); tI++) {
			t_satellite_position satpos = GET_SATELLITEPOSITION_FROM_TRANSPONDER_ID(tI->first) & 0xFFF;
			tpdone = 0;
			if(GET_SATELLITEPOSITION_FROM_TRANSPONDER_ID(tI->first) & 0xF000)
				satpos = -satpos;

			if(satpos != spos_it->first) {
#ifdef SAVE_DEBUG
				printf("Sat position %d not found !!\n", satpos);
#endif
				continue;
			}
			tpid = tI->first;
			for (ccI = allchans.begin(); ccI != allchans.end(); ccI++) {
				if(ccI->second.getTransponderId() == tpid) {
					if(!satdone) {
						WriteSatHeader(fd, spos_it->second);
						satdone = 1;
					}
					if(!tpdone) {
						WriteTransponderHeader(fd, tI->second);
						tpdone = 1;
					}

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
			switch (CFrontend::getInstance()->getInfo()->type) {
				case FE_QPSK:
					fprintf(fd, "\t</sat>\n");
					break;
				case FE_QAM:
					fprintf(fd, "\t</cable>\n");
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
	for (channel_map_iterator_t it = allchans.begin(); it != allchans.end(); it++)
		if (chans_processed.find(it->first) == chans_processed.end())
			printf("unused channel %d sat %d freq %d sid %04X: %s\n", ++i, it->second.getSatellitePosition(), it->second.getFreqId(), it->second.getServiceId(), it->second.getName().c_str());
	chans_processed.clear();
#endif
	printf("processed channels: %d\n", processed);
}

/* helper for reused code */
void CServiceManager::WriteCurrentService(FILE * fd, bool &satfound, bool &tpdone,
		bool &updated, char * satstr, struct transponder &tp, CZapitChannel &channel, const char * action)
{
	if(!tpdone) {
		if(!satfound)
			fprintf(fd, "%s", satstr);
		WriteTransponderHeader(fd, tp);
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

	t_satellite_position satellitePosition = GET_SATELLITEPOSITION_FROM_TRANSPONDER_ID(tpid) & 0xFFF;
	if(GET_SATELLITEPOSITION_FROM_TRANSPONDER_ID(tpid) & 0xF000)
		satellitePosition = -satellitePosition;


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

	switch (CFrontend::getInstance()->getInfo()->type) {
		case FE_QPSK: /* satellite */
			sprintf(satstr, "\t<%s name=\"%s\" position=\"%hd\">\n", "sat", spos_it->second.name.c_str(), satellitePosition);
			break;
		case FE_QAM: /* cable */
			sprintf(satstr, "\t<%s name=\"%s\"\n", "cable", spos_it->second.name.c_str());
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
	}
	for (channel_map_iterator_t cI = curchans.begin(); cI != curchans.end(); cI++) {
		ccI = allchans.find(cI->second.getChannelID());
		if(ccI == allchans.end()) {
			WriteCurrentService(fd, satfound, tpdone, updated, satstr, tI->second, cI->second, "add");
		} else {
			if(strcmp(cI->second.getName().c_str(), ccI->second.getName().c_str()) || cI->second.scrambled != ccI->second.scrambled) {
				WriteCurrentService(fd, satfound, tpdone, updated, satstr, tI->second, cI->second, "replace");
			}
		}
	}
	for (ccI = allchans.begin(); ccI != allchans.end(); ccI++) {
		if(ccI->second.getTransponderId() == tpid) {
			dI = curchans.find(ccI->second.getChannelID());
			if(dI == curchans.end())
				WriteCurrentService(fd, satfound, tpdone, updated, satstr, tI->second, ccI->second, "remove");
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

	return updated;
}
