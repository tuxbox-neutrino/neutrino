/*
 * Copyright (C) 2011 CoolStream International Ltd
 *
 * License: GPLv2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation;
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

/* zapit */
#include <zapit/debug.h>
#include <zapit/scansdt.h>
#include <zapit/scanpmt.h>
#include <zapit/pat.h>

#include <zapit/types.h>
#include <zapit/zapit.h>
#include <zapit/scan.h>
#include <dmx.h>

#include <dvbsi++/descriptor_tag.h>
#include <dvbsi++/ca_identifier_descriptor.h>
#include <eitd/edvbstring.h>
#include <zapit/dvbstring.h>

#define DEBUG_SDT
//#define DEBUG_SDT_UNUSED
//#define DEBUG_SDT_SERVICE

CSdt::CSdt(t_satellite_position spos, freq_id_t frq, bool curr, int dnum)
{
	satellitePosition = spos;
	freq_id = frq;
	dmxnum = dnum;
	current = curr;
	transport_stream_id = 0;
	original_network_id = 0;
	//FIXME sdt update ??
	cable = CFEManager::getInstance()->getLiveFE()->getCurrentDeliverySystem() == DVB_C;
}

CSdt::~CSdt()
{
	ServiceDescriptionSectionIterator sit;
	for (sit = sections.begin(); sit != sections.end(); ++sit) {
		delete *(sit);
	}
}

bool CSdt::PMTPing(unsigned short pid, unsigned short sid)
{
	bool ret = false;
	unsigned char filter[DMX_FILTER_SIZE];
	unsigned char mask[DMX_FILTER_SIZE];
	unsigned char buffer[PMT_SECTION_SIZE];

	cDemux * dmx = new cDemux(dmxnum);
	dmx->Open(DMX_PSI_CHANNEL);

	memset(filter, 0x00, DMX_FILTER_SIZE);
	memset(mask, 0x00, DMX_FILTER_SIZE);

	filter[0] = 0x02;	/* table_id */
	filter[1] = sid >> 8;
	filter[2] = sid;
	filter[3] = 0x01;	/* current_next_indicator */
	filter[4] = 0x00;	/* section_number */
	mask[0] = 0xFF;
	mask[1] = 0xFF;
	mask[2] = 0xFF;
	mask[3] = 0x01;
	mask[4] = 0xFF;
	if (!dmx->sectionFilter(pid, filter, mask, 1)) {
		ret = false;
	}else{
		if(dmx->Read(buffer, PMT_SECTION_SIZE) > 0){
			ProgramMapSection pmt(buffer);
			if(0x1fff==pmt.getPcrPid()){
				ret = false;
			}else{
				ret = true;
			}
		}
	}
	delete dmx;
#ifdef DEBUG_SDT
	if(!ret)
			printf("Ping: PMT-pid 0%x failed\n", pid);
#endif
	return ret;
}

/* read all sdt sections */
bool CSdt::Read()
{
	int secdone[255];
	int sectotal = -1;
	bool cable_hack_done = false;

	memset(secdone, 0, sizeof(secdone));
	cDemux * dmx = new cDemux(dmxnum);
	dmx->Open(DMX_PSI_CHANNEL);

	unsigned char buffer[SDT_SECTION_SIZE];
	unsigned char filter[DMX_FILTER_SIZE];
	unsigned char mask[DMX_FILTER_SIZE];

	int flen = 1;
	memset(filter, 0x00, DMX_FILTER_SIZE);
	memset(mask, 0x00, DMX_FILTER_SIZE);

	filter[0] = 0x42;
	mask[0] = 0xFF;

	if (transport_stream_id && original_network_id) {
		flen = 8;
		filter[1] = (transport_stream_id >> 8) & 0xff;
		filter[2] = transport_stream_id & 0xff;
		filter[6] = (original_network_id >> 8) & 0xff;
		filter[7] = original_network_id & 0xff;

		mask[1] = 0xFF;
		mask[2] = 0xFF;
		mask[6] = 0xFF;
		mask[7] = 0xFF;
	}
	if (!dmx->sectionFilter(0x11, filter, mask, flen)) {
		delete dmx;
		return false;
	}
	do {
_repeat:
		if(current && current_tp_id != CFEManager::getInstance()->getLiveFE()->getTsidOnid())
			break;

		if (dmx->Read(buffer, SDT_SECTION_SIZE) < 0) {
			delete dmx;
			return false;
		}
		unsigned char secnum = buffer[6];
		if(cable_hack_done) {
			if( (transport_stream_id == ((buffer[3] << 8) | buffer[4])) &&
					(original_network_id == ((buffer[8] << 8) | buffer[9])))
				break;
		} else {
			transport_stream_id = (buffer[3] << 8) | buffer[4];
			original_network_id = (buffer[8] << 8) | buffer[9];
		}
#ifdef DEBUG_SDT
		printf("[SDT] section %X last %X tsid 0x%x onid 0x%x -> %s\n", buffer[6], buffer[7], transport_stream_id, original_network_id, secdone[secnum] ? "skip" : "use");
#endif
		if(secdone[secnum]) {
			secdone[secnum]++;
			if(secdone[secnum] >= 5)
				break;
			continue;
		}
		secdone[secnum] = 1;
		sectotal++;

		ServiceDescriptionSection * sdt = new ServiceDescriptionSection(buffer);
		sections.push_back(sdt);

	} while(sectotal < buffer[7]);

	if(cable && !cable_hack_done && sectotal == 0) {
		cable_hack_done = true;
		secdone[0] = 0;
		goto _repeat;
	}
	delete dmx;
	if(current && current_tp_id != CFEManager::getInstance()->getLiveFE()->getTsidOnid())
		return false;
	return true;
}

/* parse sdt sections */
bool CSdt::Parse(t_transport_stream_id &tsid, t_original_network_id &onid)
{
	ServiceDescriptionSectionIterator it;
	sidpmt_map_t sidpmt;
	t_transport_stream_id pat_tsid = 0;
	if(current) {
		transport_stream_id = tsid;
		original_network_id = onid;
	}
	current_tp_id = CFEManager::getInstance()->getLiveFE()->getTsidOnid();
	bool sdt_read = Read();
	if(!sdt_read)
		pat.Reset();
	pat.Parse();
	pat_tsid = pat.GetPatTransportStreamId();

	if(!sdt_read && pat_tsid==0){
		return false;
	}

	sidpmt = pat.getSids();
	//Update form PAT if SDT is empty
	if(!sdt_read && !sidpmt.empty() && (pat_tsid == transport_stream_id || (transport_stream_id == 0 && pat_tsid > 1 ))){
		bool  ret = false;
		for (std::map<int,int>::iterator patit=sidpmt.begin(); patit!=sidpmt.end(); ++patit){
			if(patit->first != 0 && patit->second != 0){
				if(!PMTPing(patit->second,patit->first)){
					patit->second=0;
				}else{
					transport_stream_id = pat_tsid;
					original_network_id = onid;
					tsid = transport_stream_id;
					onid = original_network_id;
#ifdef DEBUG_SDT
					printf("UPDATE without SDT: SID 0x%02x PMT 0x%02x ONID 0x%02x\n",patit->first,patit->second,original_network_id);
#endif
					ParseServiceDescriptor(NULL, NULL, patit->first);
					ret = true;
				}
			}
		}
		return ret;
	}

	if(!sdt_read)
		return false;

	bool updated = false;
	for (it = sections.begin(); it != sections.end(); ++it) {
		ServiceDescriptionSection * sdt = *it;

		transport_stream_id = sdt->getTransportStreamId();
		original_network_id = sdt->getOriginalNetworkId();
		if(!current && !updated) {
			updated = true;
			transponder_id_t tpid = CREATE_TRANSPONDER_ID64(
					freq_id, satellitePosition, original_network_id, transport_stream_id);
			ZapitChannelList satChannelList;
			CServiceManager::getInstance()->GetAllTransponderChannels(satChannelList, tpid);
			for (zapit_list_it_t oldI = satChannelList.begin(); oldI != satChannelList.end(); ++oldI) {
				(*oldI)->flags &= ~CZapitChannel::UPDATED;
				(*oldI)->flags |= CZapitChannel::REMOVED;
			}
		}

#ifdef DEBUG_SDT
		printf("SDT: tid %02x onid %02x\n", sdt->getTransportStreamId(), sdt->getOriginalNetworkId());
#endif
		const ServiceDescriptionList &slist = *sdt->getDescriptions();
		for (ServiceDescriptionConstIterator sit = slist.begin(); sit != slist.end(); ++sit) {
			ServiceDescription * service = *sit;
			//printf("SDT: sid %x \n", service->getServiceId());
			DescriptorConstIterator dit;
			for (dit = service->getDescriptors()->begin(); dit != service->getDescriptors()->end(); ++dit) {
				Descriptor * d = *dit;

				if(pat_tsid == transport_stream_id){
					sidpmt_map_iterator_t sid_it = sidpmt.find(service->getServiceId());
					if(sid_it != sidpmt.end())
						sid_it->second = 0;
				}

				switch (d->getTag()) {
				case SERVICE_DESCRIPTOR:
					{
						ServiceDescriptor * sd = (ServiceDescriptor *) d;
						ParseServiceDescriptor(service, sd);
					}
					break;
				case CA_IDENTIFIER_DESCRIPTOR:
					{
#if 0 //ifdef DEBUG_SDT
						CaIdentifierDescriptor * cad = (CaIdentifierDescriptor *) d;
						const CaSystemIdList * calist = cad->getCaSystemIds();
						printf("CASYS: ");
						for(CaSystemIdConstIterator cit = calist->begin(); cit != calist->end(); ++cit)
							printf("%02x ", *cit);
						printf("\n");
#endif
					}
					break;
				default:
					{
#ifdef DEBUG_SDT_UNUSED
						printf("SDT: sid %x descriptor %02x: ", service->getServiceId(), d->getTag());
						uint8_t len = 2+d->getLength();
						uint8_t buf[len];
						d->writeToBuffer(buf);
						for(uint8_t i = 0; i < len; i++)
							printf("%02x ", buf[i]);
						printf("\n");
#endif
					}
					break;
				}
			}
			if(current && current_tp_id != CFEManager::getInstance()->getLiveFE()->getTsidOnid())
				break;
		}
	}
	if(pat_tsid == transport_stream_id){
		for (std::map<int,int>::iterator patit=sidpmt.begin(); patit!=sidpmt.end(); ++patit){
			if(current && current_tp_id != CFEManager::getInstance()->getLiveFE()->getTsidOnid())
				break;
			if(patit->first != 0 && patit->second != 0){
				if(PMTPing(patit->second,patit->first)){
					ParseServiceDescriptor(NULL, NULL, patit->first);
				}else
					printf("SKIP SID 0x%02x PMT 0x%02x\n",patit->first,patit->second);
			}
		}
	}

	tsid = transport_stream_id;
	onid = original_network_id;
	if(current && current_tp_id != CFEManager::getInstance()->getLiveFE()->getTsidOnid())
		return false;

	return true;
}

uint8_t CSdt::FixServiceType(uint8_t type)
{
	if((type == 0x9A) || (type == 0x86) || (type==0xc3) ||
			(type==0xc5) || (type==0xc6)  || (type == 0x11) ||
			(type == 0x16) || (type == 0x19) || (type == 0x1f) ||
			(type == 0x82) || (type == 0x87) || (type == 0xd3))
		return 1;
	return type;
}

bool CSdt::ParseServiceDescriptor(ServiceDescription * service, ServiceDescriptor * sd, t_service_id SID)
{
#ifdef DEBUG_SDT
	if(SID)
		printf("=================== PAT: sid %04x ===================\n",SID);
#endif
	uint8_t service_type = SID ? 1 : FixServiceType(sd->getServiceType());
	uint8_t real_type = SID ? 1 : sd->getServiceType();
	t_service_id service_id = SID ? SID : service->getServiceId();
	bool free_ca = SID ? 1 : service->getFreeCaMode();

	int tsidonid = (transport_stream_id << 16) | original_network_id;
	std::string providerName = SID ? "" :  stringDVBUTF8(sd->getServiceProviderName(), 0, tsidonid);
	std::string serviceName = SID ? "" : stringDVBUTF8(sd->getServiceName(), 0, tsidonid);

#ifdef DEBUG_SDT_SERVICE
	printf("SDT: sid %04x type %x provider [%s] service [%s] scrambled %d\n", service_id, sd->getServiceType(),
			providerName.c_str(), serviceName.c_str(), free_ca);
#endif
	if(!current && free_ca && CServiceScan::getInstance()->isFtaOnly())
		return false;

	if (!CheckScanType(service_type))
		return false;

	if (serviceName.empty() || serviceName == "."){
		char buf_tmp[64];
		const char *n = SID ? "UNK (0x%04X_0x%04X)":"unknown (0x%04X_0x%04X)";
		snprintf(buf_tmp, sizeof(buf_tmp), n, transport_stream_id, service_id);
		serviceName = buf_tmp;
	} else {
		FixWhiteSpaces(serviceName);
	}

	t_channel_id channel_id = CZapitChannel::makeChannelId(satellitePosition,
			freq_id, transport_stream_id, original_network_id, service_id);
	if(current) {
		if(CServiceManager::getInstance()->FindCurrentChannel(channel_id))
			return false;

		CZapitChannel * channel = new CZapitChannel(serviceName, channel_id,
				real_type, satellitePosition, freq_id);

		channel->delsys = CServiceScan::getInstance()->GetFrontend()->getCurrentDeliverySystem();
		CServiceManager::getInstance()->AddCurrentChannel(channel);

		channel->scrambled = free_ca;
		if(pat.Parse(channel)) {
			CPmt pmt;
			if(pmt.haveCaSys(channel->getPmtPid(), channel->getServiceId()))
				channel->scrambled = true;
			else
				channel->scrambled = false;
#ifdef DEBUG_SDT_SERVICE
			if(free_ca != channel->scrambled)
				printf("SDT: service update: [%s] free_ca %d scrambled %d\n",
						serviceName.c_str(), free_ca, channel->scrambled);
#endif
		}
#if 0 //FIXME updates scrambled flag without reloading, but prevent changes found if only scrambled different
		bool scrambled = channel->scrambled;
		channel = CServiceManager::getInstance()->FindChannel(channel_id);
		if(channel)
			channel->scrambled = scrambled;
#endif
		return true;
	}

	if (providerName.empty()) {
		if(lastProviderName.empty())
			providerName = "Unknown Provider";
		else
			providerName = lastProviderName;
	} else {
		FixWhiteSpaces(providerName);
		CServiceManager::getInstance()->ReplaceProviderName(providerName, transport_stream_id, original_network_id);
		lastProviderName = providerName;
	}

	CServiceScan::getInstance()->ChannelFound(service_type, providerName, serviceName);

	CZapitChannel *channel = CheckChannelId(service_id);
	if (channel) {
		channel->setName(serviceName);
		channel->setServiceType(real_type);
		channel->freq = freq_id;
		if (CServiceScan::getInstance()->SatHaveChannels() && (channel->flags & CZapitChannel::NOT_FOUND))
			channel->flags = CZapitChannel::NEW;
		else
			channel->flags = CZapitChannel::UPDATED;
	} else {
		channel = new CZapitChannel(serviceName, channel_id,
				real_type, satellitePosition, freq_id);
		CServiceManager::getInstance()->AddChannel(channel);

		channel->delsys = CServiceScan::getInstance()->GetFrontend()->getCurrentDeliverySystem();
		channel->flags = CZapitChannel::UPDATED;
		/* mark channel as new, if this satellite already have channels */
		if (CServiceScan::getInstance()->SatHaveChannels())
			channel->flags = CZapitChannel::NEW;
	}
	channel->scrambled = free_ca;

	AddToBouquet(providerName, channel);

	if (CZapit::getInstance()->scanPids()) {
		if(pat.Parse(channel)) {
			CPmt pmt;
			pmt.Parse(channel);
#ifdef DEBUG_SDT_SERVICE
			if(free_ca != channel->scrambled)
				printf("SDT: provider [%s] service [%s] free_ca %d scrambled %d camap.size %d\n", providerName.c_str(),
						serviceName.c_str(), free_ca, channel->scrambled, channel->camap.size());
#endif
		}
	}
	if(service_type == ST_DIGITAL_TELEVISION_SERVICE && !channel->scrambled) {
		CZapit::getInstance()->SetCurrentChannelID(channel->getChannelID());
	}

	return true;
}

/* check current freq from -2 to +2 to find if channel already exist */
CZapitChannel * CSdt::CheckChannelId(t_service_id service_id)
{
	int flist[5] = { freq_id, freq_id-1, freq_id+1, freq_id-2, freq_id+2 };
	for(int i = 0; i < 5; i++) {
		freq_id_t freq = flist[i];
		t_channel_id channel_id = CREATE_CHANNEL_ID64;
		CZapitChannel * channel = CServiceManager::getInstance()->FindChannel(channel_id);
		if(channel)
			return channel;
	}
	return NULL;
}

/* strip repeated and begin/end whitespaces */
void CSdt::FixWhiteSpaces(std::string &str)
{
	size_t found;
	while((found = str.find("  ")) != std::string::npos)
		str.replace(found, 2, " ");

	if(isspace(str[0]))
		str = str.substr(1, str.length());
	if(isspace(str[str.length()-1]))
		str = str.substr(0, str.length()-1);
}

/* check if service type wanted in current scan type */
bool CSdt::CheckScanType(uint8_t service_type)
{
	int flags;
	if(current)
		flags = CServiceScan::SCAN_TVRADIO;
	else
		flags = CServiceScan::getInstance()->GetFlags();

	if ((flags & CServiceScan::SCAN_ALL) == CServiceScan::SCAN_ALL)
		return true;
	if ((service_type == 1) && (flags & CServiceScan::SCAN_TV))
		return true;
	if ((service_type == 2) && (flags & CServiceScan::SCAN_RADIO))
		return true;

	return false;
}

/* create bouquet if not exist, and add channel to bouquet */
bool CSdt::AddToBouquet(std::string &providerName, CZapitChannel *channel)
{
	switch (channel->getServiceType()) {
	case ST_DIGITAL_TELEVISION_SERVICE:
	case ST_DIGITAL_RADIO_SOUND_SERVICE:
	case ST_NVOD_REFERENCE_SERVICE:
	case ST_NVOD_TIME_SHIFTED_SERVICE:
		{
			char pname[100];
			if SAT_POSITION_CABLE(satellitePosition)
				snprintf(pname, 100, "[C%03d.%d] %s",
						abs((int)satellitePosition)/10, abs((int)satellitePosition)%10, providerName.c_str());
			else if SAT_POSITION_TERR(satellitePosition)
				snprintf(pname, 100, "[T%03d.%d] %s",
						abs((int)satellitePosition)/10, abs((int)satellitePosition)%10, providerName.c_str());
			else
				snprintf(pname, 100, "[%c%03d.%d] %s", satellitePosition > 0? 'E' : 'W',
						abs((int)satellitePosition)/10, abs((int)satellitePosition)%10, providerName.c_str());

			int bouquetId = scanBouquetManager->existsBouquet(pname);
			CZapitBouquet* bouquet;

			if (bouquetId == -1)
				bouquet = scanBouquetManager->addBouquet(std::string(pname), false);
			else
				bouquet = scanBouquetManager->Bouquets[bouquetId];

			bouquet->addService(channel);

			return true;
		}
	default:
		break;
	}
	return false;
}
