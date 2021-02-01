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

#include <zapit/debug.h>
#include <zapit/getservices.h>
#include <hardware/dmx.h>
#include <zapit/scannit.h>
#include <zapit/scan.h>
#include <dvbsi++/descriptor_tag.h>
#include <dvbsi++/network_name_descriptor.h>
#include <dvbsi++/linkage_descriptor.h>
#include <dvbsi++/private_data_specifier_descriptor.h>
#include <math.h>
#include <eitd/edvbstring.h>
#include <system/set_threadname.h>

//#define DEBUG_NIT
//#define DEBUG_NIT_UNUSED
//#define DEBUG_LCN

CNit::CNit(t_satellite_position spos, freq_id_t frq, unsigned short pnid, int dnum)
{
	satellitePosition = spos;
	freq_id = frq;
	nid = pnid;
	dmxnum = dnum;
	orbitalPosition = 0;
}

CNit::~CNit()
{
	NetworkInformationSectionIterator sit;
	for (sit = sections.begin(); sit != sections.end(); ++sit) {
		delete *(sit);
	}
}

bool CNit::Start()
{
	int ret = start();
	return (ret == 0);
}

bool CNit::Stop()
{
	int ret = join();
	return (ret == 0);
}

void CNit::run()
{
	set_threadname("zap:nit");
	if(Parse())
		printf("[scan] NIT finished.\n");
	else
		printf("[scan] NIT failed !\n");
}

bool CNit::Read()
{
	bool ret = true;
	int secdone[2][255];
	int sectotal[2] = { -1, -1 };
	int nit_index = 0;

	for(int i = 0; i < 2; i++) {
		for (int j = 0; j < 255; j++)
		secdone[i][j] = 0;
	}

	cDemux * dmx = new cDemux(dmxnum);
	dmx->Open(DMX_PSI_CHANNEL);

	unsigned char buffer[NIT_SECTION_SIZE];

	unsigned char filter[DMX_FILTER_SIZE];
	unsigned char mask[DMX_FILTER_SIZE];

	memset(filter, 0x00, DMX_FILTER_SIZE);
	memset(mask, 0x00, DMX_FILTER_SIZE);

	int flen = 1;
	filter[0] = 0x40;
	mask[0] = (nid != 0) ? 0xFE : 0xFF; // in case we have network ID we also want the 'other' tables

	if (nid) { // filter for the network ID
		filter[1] = (nid >> 8) & 0xff;
		filter[2] = (nid >> 0) & 0xff;
		mask[1] = 0xff;
		mask[2] = 0xff;
		flen = 3;
	}

	if (!dmx->sectionFilter(0x10, filter, mask, flen)) {
		delete dmx;
#ifdef DEBUG_NIT
		printf("[NIT] filter failed\n");
#endif
		return false;
	}

	do {
		if (dmx->Read(buffer, NIT_SECTION_SIZE) < 0) {
			delete dmx;
#ifdef DEBUG_NIT
			printf("[NIT] read failed\n");
#endif
			return false;
		}

		if (CServiceScan::getInstance()->Aborted()) {
			ret = false;
			goto _return;
		}

		if ((buffer[0] != 0x40) && ((nid > 0) && (buffer[0] != 0x41))) {
			// NIT actual or NIT other 
			printf("[NIT] ******************************************* Bogus section received: 0x%x\n", buffer[0]);
			ret = false;
			goto _return;
		}

		nit_index = buffer[0] & 1; 

		unsigned char secnum = buffer[6];
#ifdef DEBUG_NIT
		printf("[NIT] section %X last %X -> %s\n", secnum, buffer[7], secdone[nit_index][secnum] ? "skip" : "use");
#endif
		if(secdone[nit_index][secnum]) { // mark sec XX done
			secdone[nit_index][secnum]++;
			if(secdone[nit_index][secnum] >= 5)
				break;
			continue;
		}
		secdone[nit_index][secnum] = 1;
		sectotal[nit_index]++;

		NetworkInformationSection * nit = new NetworkInformationSection(buffer);
		sections.push_back(nit);

	} while(sectotal[nit_index] < buffer[7]);
_return:
	dmx->Stop();
	delete dmx;
	return ret;
}

bool CNit::Parse()
{
	printf("[scan] trying to parse NIT\n");

	if(!Read())
		return false;

	NetworkInformationSectionIterator sit;
	for (sit = sections.begin(); sit != sections.end(); ++sit) {
		NetworkInformationSection * nit = *sit;

		if (CServiceScan::getInstance()->Aborted())
			return false;

		const DescriptorList * dlist = nit->getDescriptors();
		DescriptorConstIterator dit;
#ifdef DEBUG_NIT
		printf("NIT: section %d, %d descriptors\n", nit->getSectionNumber(), dlist->size());
#endif
		unsigned int pdsd = 0;
		for (dit = dlist->begin(); dit != dlist->end(); ++dit) {
			Descriptor * d = *dit;
			switch(d->getTag()) {
				case NETWORK_NAME_DESCRIPTOR:
					{
						NetworkNameDescriptor * nd = (NetworkNameDescriptor *) d;
						networkName = stringDVBUTF8(nd->getNetworkName());
#ifdef DEBUG_NIT
						printf("NIT: network name [%s]\n", networkName.c_str());
#endif
					}
					break;
				case LINKAGE_DESCRIPTOR:
					{
#ifdef DEBUG_NIT
						LinkageDescriptor * ld = (LinkageDescriptor*) d;
						printf("NIT: linkage, tsid %04x onid %04x sid %04x type %02x\n", ld->getTransportStreamId(),
								ld->getOriginalNetworkId(), ld->getServiceId(), ld->getLinkageType());
#endif
					}
					break;
				case PRIVATE_DATA_SPECIFIER_DESCRIPTOR:
					{
						PrivateDataSpecifierDescriptor * pd = (PrivateDataSpecifierDescriptor *)d;
						pdsd = pd->getPrivateDataSpecifier();
#ifdef DEBUG_NIT
						printf("NIT: private data specifier %08x\n", pdsd);
#endif
					}
					break;
				default:
					{
#ifdef DEBUG_NIT_UNUSED
						printf("NIT: descriptor %02x len %d: ", d->getTag(), d->getLength());
#if 0
						uint8_t len = 2+d->getLength();
						uint8_t buf[len];
						d->writeToBuffer(buf);
						for(uint8_t i = 0; i < len; i++)
							printf("%02x ", buf[i]);
#endif
						printf("\n");
#endif
					}
					break;
			}
		}
		const TransportStreamInfoList *tslist = nit->getTsInfo();
		for(TransportStreamInfoConstIterator tit = tslist->begin(); tit != tslist->end(); ++tit) {
			TransportStreamInfo * tsinfo = *tit;
			dlist = tsinfo->getDescriptors();
#ifdef DEBUG_NIT
			printf("NIT: tsid %04x onid %04x %d descriptors\n", tsinfo->getTransportStreamId(),
					tsinfo->getOriginalNetworkId(), dlist->size());
#endif
			//DescriptorConstIterator dit;
			for (dit = dlist->begin(); dit != dlist->end(); ++dit) {
				Descriptor * d = *dit;
				switch(d->getTag()) {
					case SATELLITE_DELIVERY_SYSTEM_DESCRIPTOR:
						ParseSatelliteDescriptor((SatelliteDeliverySystemDescriptor *)d, tsinfo);
						break;

					case CABLE_DELIVERY_SYSTEM_DESCRIPTOR:
						ParseCableDescriptor((CableDeliverySystemDescriptor *)d, tsinfo);
						break;

					case TERRESTRIAL_DELIVERY_SYSTEM_DESCRIPTOR:
						ParseTerrestrialDescriptor((TerrestrialDeliverySystemDescriptor *)d, tsinfo);
						break;

					case EXTENSION_DESCRIPTOR:
						ParseTerrestrial2Descriptor((T2DeliverySystemDescriptor *)d, tsinfo);
						break;

					case SERVICE_LIST_DESCRIPTOR:
						ParseServiceList((ServiceListDescriptor *) d, tsinfo);
						break;

                                        case PRIVATE_DATA_SPECIFIER_DESCRIPTOR:
                                                {
                                                        PrivateDataSpecifierDescriptor * pd = (PrivateDataSpecifierDescriptor *)d;
                                                        pdsd = pd->getPrivateDataSpecifier();
#ifdef DEBUG_NIT
                                                        printf("NIT: private data specifier %08x\n", pdsd);
#endif
                                                }
                                                break;

					case LOGICAL_CHANNEL_DESCRIPTOR:
						if(pdsd == 0x00000028)
							ParseLogicalChannels((LogicalChannelDescriptor *) d, tsinfo);
						break;
					case HD_SIMULCAST_LOGICAL_CHANNEL_DESCRIPTOR:
						if(pdsd == 0x00000028)
							ParseLogicalChannels((LogicalChannelDescriptor *) d, tsinfo, true);
						break;
					default:
						{
#ifdef DEBUG_NIT_UNUSED
							printf("NIT: descriptor %02x: ", d->getTag());
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
		}
	}
	return true;
}

bool CNit::ParseSatelliteDescriptor(SatelliteDeliverySystemDescriptor * sd, TransportStreamInfo * tsinfo)
{
	if (!CServiceScan::getInstance()->GetFrontend()->hasSat())
		return false;

	t_satellite_position newSat;
	newSat  = ((sd->getOrbitalPosition() >> 12) & 0xF) * 1000;
	newSat += ((sd->getOrbitalPosition() >> 8) & 0xF) * 100;
	newSat += ((sd->getOrbitalPosition() >> 4) & 0xF) * 10;
	newSat += ((sd->getOrbitalPosition()) & 0xF);

	if (newSat > 1800)
		newSat = 3600 - newSat;
	if (!sd->getWestEastFlag())
		newSat = -newSat;
	if (abs(newSat - satellitePosition) < 5)
		newSat = satellitePosition;

	if (!orbitalPosition)
		orbitalPosition = newSat;

	if(satellitePosition != newSat) {
		printf("NIT: different satellite position: our %d nit %d (%X)\n",
				satellitePosition, newSat, sd->getOrbitalPosition());
		return false;
	}

	uint8_t modulation_system = sd->getModulationSystem();
	uint8_t modulation = sd->getModulation();

	FrontendParameters feparams;
	memset(&feparams, 0, sizeof(feparams));
	feparams.polarization = sd->getPolarization();
	feparams.pilot = ZPILOT_AUTO;
	feparams.plp_id = NO_STREAM_ID_FILTER;
	feparams.pls_code = PLS_Default_Gold_Code;
	feparams.pls_mode = PLS_Gold;

	switch (modulation_system) {
	case 0: // DVB-S
		feparams.delsys = DVB_S;
		// Hack for APSTAR 138E, 8PSK signalled but delsys set to DVB-S
		if (modulation == 2)
			feparams.delsys = DVB_S2;
		break;
	case 1: // DVB-S2
		feparams.delsys = DVB_S2;
		break;
	default:
#ifdef DEBUG_NIT
		printf("NIT: undefined modulation system %08x\n", modulation_system);
#endif
		feparams.delsys = UNKNOWN_DS;
		break;
	}
	switch (modulation) {
	case 0: // AUTO
		feparams.modulation = QAM_AUTO;
		break;
	case 1: // QPSK
		feparams.modulation = QPSK;
		break;
	case 2: // 8PSK
		feparams.modulation = PSK_8;
		break;
	case 3: // QAM_16
		feparams.modulation = QAM_16;
		break;
	}

	feparams.inversion = INVERSION_AUTO;
	feparams.frequency = sd->getFrequency() * 10;
	feparams.symbol_rate = sd->getSymbolRate() * 100;

	feparams.fec_inner = CFrontend::getCodeRate(sd->getFecInner(), feparams.delsys);
	if (feparams.delsys == DVB_S2)
		feparams.rolloff = CFrontend::getRolloff(sd->getRollOff());
	else
		feparams.rolloff = ROLLOFF_35;

	feparams.frequency = (int) 1000 * (int) round ((double) feparams.frequency / (double) 1000);

	freq_id_t freq = CREATE_FREQ_ID(feparams.frequency, false);
	transponder_id_t TsidOnid = CREATE_TRANSPONDER_ID64(
			freq, satellitePosition, tsinfo->getOriginalNetworkId(), tsinfo->getTransportStreamId());

	CServiceScan::getInstance()->AddTransponder(TsidOnid, &feparams, true);

	return true;
}

bool CNit::ParseCableDescriptor(CableDeliverySystemDescriptor * sd, TransportStreamInfo * tsinfo)
{
	if (!CServiceScan::getInstance()->GetFrontend()->hasCable())
		return false;

	FrontendParameters feparams;

	memset(&feparams, 0, sizeof(feparams));

	// FIXME: how is Annex decided in DVB-C ? Should this be done based on the receiving
	//        system set in the tuner ? For now we hardcode this to Annex-A.
	feparams.delsys		= DVB_C;
	feparams.inversion	= INVERSION_AUTO;
	feparams.frequency	= sd->getFrequency() * 100;
	feparams.symbol_rate	= sd->getSymbolRate() * 100;
	feparams.fec_inner	= CFrontend::getCodeRate(sd->getFecInner(), DVB_C);
	feparams.modulation	= CFrontend::getModulation(sd->getModulation());

	if(feparams.frequency > 1000*1000)
		feparams.frequency /= 1000;

	freq_id_t freq = CREATE_FREQ_ID(feparams.frequency, true);
	transponder_id_t TsidOnid = CREATE_TRANSPONDER_ID64(
			freq, satellitePosition, tsinfo->getOriginalNetworkId(), tsinfo->getTransportStreamId());

	CServiceScan::getInstance()->AddTransponder(TsidOnid, &feparams, true);
	return true;
}

bool CNit::ParseTerrestrialDescriptor(TerrestrialDeliverySystemDescriptor * sd, TransportStreamInfo * tsinfo)
{
	if (!CServiceScan::getInstance()->GetFrontend()->hasTerr())
		return false;

	FrontendParameters feparams;

	memset(&feparams, 0, sizeof(feparams));

	feparams.delsys			= DVB_T;
	feparams.inversion		= INVERSION_AUTO;
	feparams.frequency		= sd->getCentreFrequency() * 10;
	feparams.code_rate_HP		= CFrontend::getCodeRate(sd->getCodeRateHpStream(), DVB_T);
	feparams.code_rate_LP		= CFrontend::getCodeRate(sd->getCodeRateLpStream(), DVB_T);
	feparams.modulation		= CFrontend::getConstellation(sd->getConstellation());
	feparams.bandwidth		= CFrontend::getBandwidth(sd->getBandwidth());
	feparams.hierarchy		= CFrontend::getHierarchy(sd->getHierarchyInformation());
	feparams.transmission_mode	= CFrontend::getTransmissionMode(sd->getTransmissionMode());
	if(feparams.frequency > 1000*1000)
		feparams.frequency /= 1000;

	freq_id_t freq = (freq_id_t) (feparams.frequency/(1000*1000));
	transponder_id_t TsidOnid = CREATE_TRANSPONDER_ID64(
			freq, satellitePosition, tsinfo->getOriginalNetworkId(), tsinfo->getTransportStreamId());

	CServiceScan::getInstance()->AddTransponder(TsidOnid, &feparams, true);
	return true;
}
bool CNit::ParseTerrestrial2Descriptor(T2DeliverySystemDescriptor * sd, TransportStreamInfo * tsinfo)
{
	if (!CServiceScan::getInstance()->GetFrontend()->hasTerr())
		return false;

	FrontendParameters feparams;

	memset(&feparams, 0, sizeof(feparams));

	feparams.delsys			= DVB_T2;
	feparams.inversion		= INVERSION_AUTO;
	feparams.plp_id 		= (unsigned int)sd->getPlpId();
	if (feparams.plp_id == 0)
		feparams.plp_id = NO_STREAM_ID_FILTER;
	feparams.code_rate_HP		= CFrontend::getCodeRate(FEC_AUTO, DVB_T2);
	feparams.code_rate_LP		= CFrontend::getCodeRate(FEC_AUTO, DVB_T2);
	feparams.modulation		= CFrontend::getConstellation(QAM_AUTO);
	feparams.bandwidth		= CFrontend::getBandwidth(sd->getBandwidth());
	feparams.hierarchy		= CFrontend::getHierarchy(HIERARCHY_AUTO);
	feparams.transmission_mode	= CFrontend::getTransmissionMode(sd->getTransmissionMode());

	for (T2CellConstIterator cell = sd->getCells()->begin(); cell != sd->getCells()->end(); ++cell)
	{
		for (T2FrequencyConstIterator T2freq = (*cell)->getCentreFrequencies()->begin(); T2freq != (*cell)->getCentreFrequencies()->end(); ++T2freq)
		{
			feparams.frequency = (*T2freq) * 10;
			freq_id_t freq = (freq_id_t) (feparams.frequency/(1000*1000));
			transponder_id_t TsidOnid = CREATE_TRANSPONDER_ID64(
				freq, satellitePosition, tsinfo->getOriginalNetworkId(), tsinfo->getTransportStreamId());

			CServiceScan::getInstance()->AddTransponder(TsidOnid, &feparams, true);
		}
	}
	return true;
}

bool CNit::ParseServiceList(ServiceListDescriptor * sd, TransportStreamInfo * tsinfo)
{
	const ServiceListItemList * slist = sd->getServiceList();
	ServiceListItemConstIterator it;
	for (it = slist->begin(); it != slist->end(); ++it) {
		ServiceListItem * s = *it;
		t_channel_id channel_id = CZapitChannel::makeChannelId(0, 0,
				tsinfo->getTransportStreamId(), tsinfo->getOriginalNetworkId(), s->getServiceId());
		CServiceScan::getInstance()->AddServiceType(channel_id, s->getServiceType());
	}
	return true;
}

bool CNit::ParseLogicalChannels(LogicalChannelDescriptor * ld, TransportStreamInfo * tsinfo, bool hd)
{
	t_transport_stream_id transport_stream_id = tsinfo->getTransportStreamId();
	t_original_network_id original_network_id = tsinfo->getOriginalNetworkId();

	const LogicalChannelList &clist = *ld->getChannelList();
	LogicalChannelListConstIterator it;
	for (it = clist.begin(); it != clist.end(); ++it) { 
		t_service_id service_id = (*it)->getServiceId();
		int lcn = (*it)->getLogicalChannelNumber();
		t_channel_id channel_id = CZapitChannel::makeChannelId(0, 0,
				transport_stream_id, original_network_id, service_id);
		int visible = (*it)->getVisibleServiceFlag();
#ifdef DEBUG_LCN
		printf("NIT: logical channel tsid %04x onid %04x %012llx -> %d (%s, %d)\n", transport_stream_id, original_network_id, channel_id, lcn, hd ? "hd" : "sd", visible);
#endif
		if (visible && lcn) {
			if(hd)
				hd_logical_map[channel_id] = lcn;
			else
				logical_map[channel_id] = lcn;
		}
	}
	return true;
}
