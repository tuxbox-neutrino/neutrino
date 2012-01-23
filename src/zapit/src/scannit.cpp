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
#include <dmx.h>
#include <zapit/scannit.h>
#include <zapit/scan.h>
#include <dvbsi++/descriptor_tag.h>
#include <math.h>

#define DEBUG_NIT
//#define DEBUG_NIT_UNUSED

CNit::CNit(t_satellite_position spos, freq_id_t frq, unsigned short pnid, int dnum)
{
	satellitePosition = spos;
	freq_id = frq;
	nid = pnid;
	dmxnum = dnum;
	cable = (CServiceScan::getInstance()->GetFrontend()->getInfo()->type == FE_QAM);
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

	if (dmx->sectionFilter(0x10, filter, mask, flen) < 0) {
		delete dmx;
		return false;
	}

	do {
		if (dmx->Read(buffer, NIT_SECTION_SIZE) < 0) {
			delete dmx;
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
		if(secdone[nit_index][secnum]) // mark sec XX done
			continue;
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
#ifdef DEBUG_NIT
	printf("NIT: %d sections\n", sections.size());
#endif
	for (sit = sections.begin(); sit != sections.end(); ++sit) {
		NetworkInformationSection * nit = *sit;
		const TransportStreamInfoList *tslist = nit->getTsInfo();
#ifdef DEBUG_NIT
		printf("NIT: %d TransportStreamInfos\n", tslist->size());
#endif
		if (CServiceScan::getInstance()->Aborted())
			return false;

		for(TransportStreamInfoConstIterator tit = tslist->begin(); tit != tslist->end(); ++tit) {
			TransportStreamInfo * tsinfo = *tit;
			const DescriptorList * dlist = tsinfo->getDescriptors();
#if 0 //ifdef DEBUG_NIT
			printf("NIT: tsid %04x onid %04x %d descriptors\n", tsinfo->getTransportStreamId(),
					tsinfo->getOriginalNetworkId(), dlist->size());
#endif
			DescriptorConstIterator dit;
			for (dit = dlist->begin(); dit != dlist->end(); ++dit) {
				Descriptor * d = *dit;
				switch(d->getTag()) {
				case SATELLITE_DELIVERY_SYSTEM_DESCRIPTOR:
					ParseSatelliteDescriptor((SatelliteDeliverySystemDescriptor *)d, tsinfo);
					break;

				case CABLE_DELIVERY_SYSTEM_DESCRIPTOR:
					ParseCableDescriptor((CableDeliverySystemDescriptor *)d, tsinfo);
					break;

				case SERVICE_LIST_DESCRIPTOR:
					ParseServiceList((ServiceListDescriptor *) d, tsinfo);
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
	if (cable)
		return false;

	t_satellite_position newSat;
	newSat  = ((sd->getOrbitalPosition() >> 12) & 0xF) * 1000;
	newSat += ((sd->getOrbitalPosition() >> 8) & 0xF) * 100;
	newSat += ((sd->getOrbitalPosition() >> 4) & 0xF) * 10;
	newSat += ((sd->getOrbitalPosition()) & 0xF);
	if (newSat && (!sd->getWestEastFlag()))
		newSat = 3600 - newSat;

	if(satellitePosition != newSat) {
		printf("NIT: different satellite position: our %d nit %d\n",
				satellitePosition, sd->getOrbitalPosition());
		return false;
	}

	uint8_t polarization = sd->getPolarization();

	FrontendParameters feparams;

	feparams.inversion = INVERSION_AUTO;
	feparams.frequency = sd->getFrequency() * 10;
	feparams.u.qpsk.symbol_rate = sd->getSymbolRate() * 100;

	int fec_inner = CFrontend::getCodeRate(sd->getFecInner(), sd->getModulationSystem());
	if(sd->getModulation() == 2)
		fec_inner += 9;

	feparams.u.qpsk.fec_inner = (fe_code_rate_t) fec_inner;
	feparams.frequency = (int) 1000 * (int) round ((double) feparams.frequency / (double) 1000);

	freq_id_t freq = feparams.frequency / 1000;
	transponder_id_t TsidOnid = CREATE_TRANSPONDER_ID_FROM_SATELLITEPOSITION_ORIGINALNETWORK_TRANSPORTSTREAM_ID(
			freq, satellitePosition, tsinfo->getTransportStreamId(), tsinfo->getOriginalNetworkId());

	CServiceScan::getInstance()->AddTransponder(TsidOnid, &feparams, polarization, true);

	return true;
}

bool CNit::ParseCableDescriptor(CableDeliverySystemDescriptor * sd, TransportStreamInfo * tsinfo)
{
	if (!cable)
		return false;

	FrontendParameters feparams;

	feparams.inversion = INVERSION_AUTO;
	feparams.frequency = sd->getFrequency() * 100;
	feparams.u.qam.symbol_rate = sd->getSymbolRate() * 100;
	feparams.u.qam.fec_inner = CFrontend::getCodeRate(sd->getFecInner());
	feparams.u.qam.modulation = CFrontend::getModulation(sd->getModulation());

	if(feparams.frequency > 1000*1000)
		feparams.frequency /= 1000;

	freq_id_t freq = feparams.frequency / 1000;
	transponder_id_t TsidOnid = CREATE_TRANSPONDER_ID_FROM_SATELLITEPOSITION_ORIGINALNETWORK_TRANSPORTSTREAM_ID(
		freq, satellitePosition, tsinfo->getTransportStreamId(), tsinfo->getOriginalNetworkId());

	CServiceScan::getInstance()->AddTransponder(TsidOnid, &feparams, 0, true);
	return true;
}

bool CNit::ParseServiceList(ServiceListDescriptor * sd, TransportStreamInfo * tsinfo)
{
	const ServiceListItemList * slist = sd->getServiceList();
	ServiceListItemConstIterator it;
	for (it = slist->begin(); it != slist->end(); ++it) {
		ServiceListItem * s = *it;

		t_channel_id channel_id = CZapitChannel::makeChannelId(satellitePosition,
				freq_id, tsinfo->getTransportStreamId(), tsinfo->getOriginalNetworkId(), s->getServiceId());
		CServiceScan::getInstance()->AddServiceType(channel_id, s->getServiceType());
	}
	return true;
}
