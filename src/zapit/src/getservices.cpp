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

#include <zapit/bouquets.h>
#include <zapit/channel.h>
#include <zapit/debug.h>
#include <zapit/frontend_c.h>
#include <zapit/getservices.h>
#include <zapit/settings.h>
#include <zapit/satconfig.h>
#include <xmlinterface.h>
#include <math.h>
#include <sys/time.h>

extern xmlDocPtr scanInputParser;
extern transponder_list_t transponders;
extern tallchans allchans;
extern int scanSDT;
static int newfound;
extern CFrontend *frontend;

satellite_map_t satellitePositions;
std::map<transponder_id_t, transponder> select_transponders;

int newtpid;
int tcnt = 0;
int scnt = 0;
extern int zapit_debug;
extern map<t_channel_id, audio_map_set_t> audio_map;
void save_services(bool tocopy);

#define TIMER_START()                                                                   \
        static struct timeval tv, tv2;                                                         \
        static unsigned int msec;                                                              \
        gettimeofday(&tv, NULL)

#define TIMER_STOP(label)                                                               \
        gettimeofday(&tv2, NULL);                                                       \
        msec = ((tv2.tv_sec - tv.tv_sec) * 1000) + ((tv2.tv_usec - tv.tv_usec) / 1000); \
        printf("%s: %u msec\n", label, msec)

void ParseTransponders(xmlNodePtr node, t_satellite_position satellitePosition, bool cable)
{
	t_transport_stream_id transport_stream_id;
	t_original_network_id original_network_id;
	FrontendParameters feparams;
	uint8_t polarization = 0;
	uint16_t freq;
	tcnt =0;

	memset(&feparams, 0, sizeof(FrontendParameters));

	/* read all transponders */
	while ((node = xmlGetNextOccurence(node, "TS")) != NULL) {
		transport_stream_id = xmlGetNumericAttribute(node, (char *) "id", 16);
		original_network_id = xmlGetNumericAttribute(node, (char *) "on", 16);
		feparams.frequency = xmlGetNumericAttribute(node, (char *) "frq", 0);
		feparams.inversion = (fe_spectral_inversion) xmlGetNumericAttribute(node, (char *) "inv", 0);

		if(cable) {
			feparams.u.qam.symbol_rate = xmlGetNumericAttribute(node, (char *) "sr", 0);
			feparams.u.qam.fec_inner = (fe_code_rate_t) xmlGetNumericAttribute(node, (char *) "fec", 0);
			feparams.u.qam.modulation = (fe_modulation_t) xmlGetNumericAttribute(node, (char *) "mod", 0);

			if (feparams.frequency > 1000*1000)
				feparams.frequency=feparams.frequency/1000; //transponderlist was read from tuxbox
		} else {
			feparams.u.qpsk.fec_inner = (fe_code_rate_t) xmlGetNumericAttribute(node, (char *) "fec", 0);
			feparams.u.qpsk.symbol_rate = xmlGetNumericAttribute(node, (char *) "sr", 0);

			polarization = xmlGetNumericAttribute(node, (char *) "pol", 0);

			if(feparams.u.qpsk.symbol_rate < 50000) feparams.u.qpsk.symbol_rate = feparams.u.qpsk.symbol_rate * 1000;

			if(feparams.frequency < 20000) feparams.frequency = feparams.frequency*1000;
		}
		feparams.frequency = (int) 1000 * (int) round ((double) feparams.frequency / (double) 1000);
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

void ParseChannels(xmlNodePtr node, const t_transport_stream_id transport_stream_id, const t_original_network_id original_network_id, t_satellite_position satellitePosition, freq_id_t freq)
{
	t_service_id service_id;
	std::string  name;
	uint8_t      service_type;
	unsigned short vpid, apid, pcrpid, pmtpid, txpid, vtype, scrambled;
	tallchans_iterator cit;
	std::string desc = "";
	desc += "Preset";
	t_channel_id chid;

	while ((node = xmlGetNextOccurence(node, "S")) != NULL) {
		service_id = xmlGetNumericAttribute(node, (char *) "i", 16);
		name = xmlGetAttribute(node, (char *) "n");
		service_type = xmlGetNumericAttribute(node, (char *) "t", 16);
		vpid = xmlGetNumericAttribute(node, (char *) "v", 16);
		apid = xmlGetNumericAttribute(node, (char *) "a", 16);
		pcrpid = xmlGetNumericAttribute(node, (char *) "p", 16);
		pmtpid = xmlGetNumericAttribute(node, (char *) "pmt", 16);
		txpid = xmlGetNumericAttribute(node, (char *) "tx", 16);
		vtype = xmlGetNumericAttribute(node, (char *) "vt", 16);
		scrambled = xmlGetNumericAttribute(node, (char *) "s", 16);

		chid = CREATE_CHANNEL_ID64;
		char *ptr = xmlGetAttribute(node, (char *) "action");
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
#if 0
		switch (service_type) {
			case ST_DIGITAL_TELEVISION_SERVICE:
			case ST_NVOD_REFERENCE_SERVICE:
			case ST_NVOD_TIME_SHIFTED_SERVICE:
			case ST_DIGITAL_RADIO_SOUND_SERVICE:
#endif
				{
					map<t_channel_id, audio_map_set_t>::iterator audio_map_it;
					audio_map_it = audio_map.find(chid);
					if((audio_map_it != audio_map.end()) && (audio_map_it->second.apid != 0)) {
						apid = audio_map_it->second.apid;
					}

					pair<map<t_channel_id, CZapitChannel>::iterator,bool> ret;
					ret = allchans.insert (
						std::pair <t_channel_id, CZapitChannel> (chid,
							CZapitChannel ( name, service_id,
									transport_stream_id,
									original_network_id,
									service_type,
									satellitePosition,
									freq
									)
								)
							);
//printf("INS CHANNEL %s %x\n", name.c_str(), &ret.first->second);
					if(ret.second == false) {
						printf("[zapit] duplicate channel %s id %llx freq %d (old %s at %d)\n",
							name.c_str(), chid, freq, ret.first->second.getName().c_str(), ret.first->second.getFreqId());
					} else {
						scnt++;
						tallchans_iterator cit = ret.first;
						cit->second.scrambled = scrambled;
						service_type = cit->second.getServiceType();
						if(pmtpid != 0 && (((service_type == 2) && (apid > 0)) || ( (service_type == 1)  && (vpid > 0) && (apid > 0))) ) {
							DBG("[getserv] preset chan %s vpid %X sid %X tpid %X onid %X\n", name.c_str(), vpid, service_id, transport_stream_id, transport_stream_id);
							cit->second.setVideoPid(vpid);
							cit->second.setAudioPid(apid);
							cit->second.setPcrPid(pcrpid);
							cit->second.setPmtPid(pmtpid);
							cit->second.setTeletextPid(txpid);
							cit->second.setPidsFlag();
							cit->second.type = vtype;
						}
					}
				}
#if 0
				break;

			default:
				break;
		}
#endif
		node = node->xmlNextNode;
	}
	return;
}

void FindTransponder(xmlNodePtr search)
{
	t_satellite_position satellitePosition = 0;
	newtpid = 0xC000;
	while (search) {
		bool cable = false;

		if (!(strcmp(xmlGetName(search), "cable")))
			cable = true;
		else if ((strcmp(xmlGetName(search), "sat"))) {	
			search = search->xmlNextNode;
			continue;
		}

		satellitePosition = xmlGetSignedNumericAttribute(search, (char *) "position", 10);
		DBG("going to parse dvb-%c provider %s\n", xmlGetName(search)[0], xmlGetAttribute(search, (char *) "name"));
		ParseTransponders(search->xmlChildrenNode, satellitePosition, cable);
		newfound++;
		search = search->xmlNextNode;
	}
}

static uint32_t fake_tid, fake_nid;
void ParseSatTransponders(fe_type_t frontendType, xmlNodePtr search, t_satellite_position satellitePosition)
{
	uint8_t polarization = 0;
	uint8_t system = 0, modulation = 1;
	int xml_fec;
	FrontendParameters feparams;

	fake_tid = fake_nid = 0;

	xmlNodePtr tps = search->xmlChildrenNode;

	while ((tps = xmlGetNextOccurence(tps, "transponder")) != NULL) {
		memset(&feparams, 0x00, sizeof(FrontendParameters));

		feparams.frequency = xmlGetNumericAttribute(tps, (char *) "frequency", 0);
		if (frontendType == FE_QAM) {
			if (feparams.frequency > 1000*1000)
				feparams.frequency=feparams.frequency/1000; //transponderlist was read from tuxbox
			feparams.frequency = (int) 1000 * (int) round ((double) feparams.frequency / (double) 1000);
		}
		else feparams.frequency = (int) 1000 * (int) round ((double) feparams.frequency / (double) 1000);

		feparams.inversion = INVERSION_AUTO;

		if (frontendType == FE_QAM) {
			feparams.u.qam.symbol_rate = xmlGetNumericAttribute(tps, (char *) "symbol_rate", 0);
			feparams.u.qam.fec_inner = (fe_code_rate_t) xmlGetNumericAttribute(tps, (char *) "fec_inner", 0);
			feparams.u.qam.modulation = (fe_modulation_t) xmlGetNumericAttribute(tps, (char *) "modulation", 0);
		}
		else if (frontendType == FE_QPSK) {
			feparams.u.qpsk.symbol_rate = xmlGetNumericAttribute(tps, (char *) "symbol_rate", 0);
			polarization = xmlGetNumericAttribute(tps, (char *) "polarization", 0);
			system = xmlGetNumericAttribute(tps, (char *) "system", 0);
			modulation = xmlGetNumericAttribute(tps, (char *) "modulation", 0);
			xml_fec = xmlGetNumericAttribute(tps, (char *) "fec_inner", 0);
			xml_fec = CFrontend::getCodeRate(xml_fec, system);
			if(modulation == 2)
				xml_fec += 9;
			feparams.u.qpsk.fec_inner = (fe_code_rate_t) xml_fec;
		}
		transponder_id_t tid = 
			CREATE_TRANSPONDER_ID_FROM_SATELLITEPOSITION_ORIGINALNETWORK_TRANSPORTSTREAM_ID(
					feparams.frequency/1000, satellitePosition, fake_nid, fake_tid);

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

int LoadMotorPositions(void)
{
	FILE *fd = NULL;
	char buffer[256] = "";
	t_satellite_position satellitePosition;
	int spos = 0, mpos = 0, diseqc = 0, uncom = 0, com = 0, usals = 0, inuse;
	int offH = 10600, offL = 9750, sw = 11700;
	
	printf("[getservices] loading motor positions...\n");
	
	if ((fd = fopen(SATCONFIG, "r"))) {
		fgets(buffer, 255, fd);
		while(!feof(fd)) {
			sscanf(buffer, "%d %d %d %d %d %d %d %d %d %d", &spos, &mpos, &diseqc, &com, &uncom, &offL, &offH, &sw, &inuse, &usals);

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
			}
			fgets(buffer, 255, fd);	
		}
		fclose(fd);
	}
	else
		printf("[getservices] %s not found.\n", SATCONFIG);
	
	return 0;
}

void SaveMotorPositions()
{
	FILE * fd;
	sat_iterator_t sit;
	printf("[getservices] saving motor positions...\n");

	fd = fopen(SATCONFIG, "w");
	if(fd == NULL) {
		printf("[zapit] cannot open %s\n", SATCONFIG);
		return;
	}
	fprintf(fd, "# sat position, stored rotor, diseqc, commited, uncommited, low, high, switch, use in full scan, use usals\n");
	for(sit = satellitePositions.begin(); sit != satellitePositions.end(); sit++) {
		fprintf(fd, "%d %d %d %d %d %d %d %d %d %d\n", sit->first, sit->second.motor_position, 
				sit->second.diseqc, sit->second.commited, sit->second.uncommited, sit->second.lnbOffsetLow,
				sit->second.lnbOffsetHigh, sit->second.lnbSwitch, sit->second.use_in_scan, sit->second.use_usals);
	}
	fdatasync(fileno(fd));
	fclose(fd);
}

static void init_sat(t_satellite_position position)
{
	satellitePositions[position].position = 0;
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
}

int LoadServices(fe_type_t frontendType, diseqc_t diseqcType, bool only_current)
{
	xmlDocPtr parser;
	bool satcleared = 0;
	scnt = 0;
	printf("[zapit] Loading services, channel size %d ..\n", sizeof(CZapitChannel));

	if(only_current)
		goto do_current;

	TIMER_START();
	select_transponders.clear();
	fake_tid = fake_nid = 0;

	if(!scanInputParser)  {
		if(frontendType == FE_QPSK) {
			scanInputParser = parseXmlFile(SATELLITES_XML);
		} else if(frontendType == FE_QAM) {
			scanInputParser = parseXmlFile(CABLES_XML);
		}
	}

	if (scanInputParser != NULL) {
		t_satellite_position position = 0;
		if(!satcleared) 
			satellitePositions.clear();
		satcleared = 1;

		xmlNodePtr search = xmlDocGetRootElement(scanInputParser)->xmlChildrenNode;
		while (search) {
			if (!(strcmp(xmlGetName(search), "sat"))) {
				position = xmlGetSignedNumericAttribute(search, (char *) "position", 10);
				char * name = xmlGetAttribute(search,(char *)  "name");

				if(satellitePositions.find(position) == satellitePositions.end()) {
					init_sat(position);
				}
				satellitePositions[position].name = name;
			} else if(!(strcmp(xmlGetName(search), "cable"))) {
				char * name = xmlGetAttribute(search,(char *)  "name");
				if(satellitePositions.find(position) == satellitePositions.end()) {
					init_sat(position);
				}
				satellitePositions[position].name = name;
				position++;
			}
			ParseSatTransponders(frontendType, search, position);
			search = search->xmlNextNode;
		}
	}

	parser = parseXmlFile(SERVICES_XML);
	if (parser != NULL) {
		xmlNodePtr search = xmlDocGetRootElement(parser)->xmlChildrenNode;
		while (search) {
			if (!(strcmp(xmlGetName(search), "sat"))) {
				t_satellite_position position = xmlGetSignedNumericAttribute(search, (char *) "position", 10);
				char * name = xmlGetAttribute(search,(char *)  "name");
				if(satellitePositions.find(position) == satellitePositions.end()) {
					init_sat(position);
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

	printf("[zapit] %d services loaded (%d)...\n", scnt, allchans.size());
	TIMER_STOP("[zapit] service loading took");

	if(1) { //zapit_debug) {//FIXME
		sat_iterator_t sit;
		for(sit = satellitePositions.begin(); sit != satellitePositions.end(); sit++)
			printf("satelliteName = %s (%d), satellitePosition = %d motor position = %d usals %d\n", sit->second.name.c_str(), sit->second.name.size(), sit->first, sit->second.motor_position, sit->second.use_usals);
	}
do_current:
	DBG("Loading current..\n");
	if (scanSDT && (parser = parseXmlFile(CURRENTSERVICES_XML))) {
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

	return 0;
}

//#define SAVE_DEBUG
void zapit_cp(char * from, char * to)
{
        char cmd[256] = "cp -f ";
        strcat(cmd, from);
        strcat(cmd, " ");
        strcat(cmd, to);
        system(cmd);
	sync();
}

void SaveServices(bool tocopy) 
{
	transponder_id_t 		tpid = 0;
	FILE * fd = 0;
	bool updated = 0;

	tallchans_iterator ccI;
	tallchans_iterator dI;
	transponder_list_t::iterator tI;
	char tpstr[256];
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
			switch (frontend->getInfo()->type) {
				case FE_QPSK: /* satellite */
					sprintf(tpstr, "\t\t<TS id=\"%04x\" on=\"%04x\" frq=\"%u\" inv=\"%hu\" sr=\"%u\" fec=\"%hu\" pol=\"%hu\">\n",
							tI->second.transport_stream_id, tI->second.original_network_id,
							tI->second.feparams.frequency, tI->second.feparams.inversion,
							tI->second.feparams.u.qpsk.symbol_rate, tI->second.feparams.u.qpsk.fec_inner,
							tI->second.polarization);
					break;
				case FE_QAM: /* cable */
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

			tpid = tI->first;
			for (ccI = allchans.begin(); ccI != allchans.end(); ccI++) {
				if(ccI->second.getTransponderId() == tpid) {
					if(!satdone) {
						switch (frontend->getInfo()->type) {
							case FE_QPSK: /* satellite */
								fprintf(fd, "\t<sat name=\"%s\" position=\"%hd\" diseqc=\"%hd\" uncommited=\"%hd\">\n",
										spos_it->second.name.c_str(), spos_it->first, spos_it->second.diseqc, spos_it->second.uncommited);
								break;
							case FE_QAM: /* cable */
								fprintf(fd, "\t<cable name=\"%s\">\n", spos_it->second.name.c_str());
								break;
							case FE_OFDM:
							default:
								break;
						}

						satdone = 1;
					}
					if(!tpdone) {
						fprintf(fd, "%s", tpstr);
						tpdone = 1;
					}
					if(ccI->second.getPidsFlag()) {
						fprintf(fd, "\t\t\t<S i=\"%04x\" n=\"%s\" v=\"%x\" a=\"%x\" p=\"%x\" pmt=\"%x\" tx=\"%x\" t=\"%x\" vt=\"%d\" s=\"%d\"/>\n",
								ccI->second.getServiceId(), convert_UTF8_To_UTF8_XML(ccI->second.getName().c_str()).c_str(),
								ccI->second.getVideoPid(), ccI->second.getPreAudioPid(),
								ccI->second.getPcrPid(), ccI->second.getPmtPid(), ccI->second.getTeletextPid(),
								ccI->second.getServiceType(true), ccI->second.type, ccI->second.scrambled);
					} else {
						fprintf(fd, "\t\t\t<S i=\"%04x\" n=\"%s\" t=\"%x\" s=\"%d\"/>\n",
								ccI->second.getServiceId(), convert_UTF8_To_UTF8_XML(ccI->second.getName().c_str()).c_str(),
								ccI->second.getServiceType(true), ccI->second.scrambled);
					}
					processed++;
#ifdef SAVE_DEBUG
					chans_processed.insert(ccI->second.getChannelID());
#endif
				}
			}
			if(tpdone) fprintf(fd, "\t\t</TS>\n");
		}
		if(satdone) {
			switch (frontend->getInfo()->type) {
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
		zapit_cp((char *) SERVICES_TMP, (char *) SERVICES_XML);
		unlink(SERVICES_TMP);
	}
#ifdef SAVE_DEBUG
	printf("processed channels: %d\n", chans_processed.size());
	int i = 0;
	for (tallchans::iterator it = allchans.begin(); it != allchans.end(); it++)
		if (chans_processed.find(it->first) == chans_processed.end())
			printf("unsed channel %d sat %d freq %d sid %04X: %s\n", ++i, it->second.getSatellitePosition(), it->second.getFreqId(), it->second.getServiceId(), it->second.getName().c_str());
	chans_processed.clear();
#endif
	printf("processed channels: %d\n", processed);
}
