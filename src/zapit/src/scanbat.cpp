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
#include <zapit/scanbat.h>
#include <zapit/scan.h>
#include <dvbsi++/descriptor_tag.h>
#include <dvbsi++/bouquet_name_descriptor.h>
#include <dvbsi++/linkage_descriptor.h>
#include <math.h>
#include <eitd/edvbstring.h>

#define DEBUG_BAT
#define DEBUG_BAT_UNUSED

CBat::CBat(t_satellite_position spos, freq_id_t frq, int dnum)
{
	satellitePosition = spos;
	freq_id = frq;
	dmxnum = dnum;
	cable = (CServiceScan::getInstance()->GetFrontend()->getInfo()->type == FE_QAM);
}

CBat::~CBat()
{
	BouquetAssociationSectionConstIterator sit;
	for (sit = sections.begin(); sit != sections.end(); ++sit) {
		delete *(sit);
	}
}

bool CBat::Start()
{
	int ret = start();
	return (ret == 0);
}

bool CBat::Stop()
{
	int ret = join();
	return (ret == 0);
}

void CBat::run()
{
	if(Parse())
		printf("[scan] BAT finished.\n");
	else
		printf("[scan] BAT failed !\n");
}

bool CBat::Read()
{
	bool ret = true;
	int secdone[255];
	int sectotal = -1;

	memset(secdone, 0, 255);

	cDemux * dmx = new cDemux(dmxnum);
	dmx->Open(DMX_PSI_CHANNEL);

	unsigned char buffer[BAT_SECTION_SIZE];

	unsigned char filter[DMX_FILTER_SIZE];
	unsigned char mask[DMX_FILTER_SIZE];

	memset(filter, 0x00, DMX_FILTER_SIZE);
	memset(mask, 0x00, DMX_FILTER_SIZE);

	int flen = 1;
	filter[0] = 0x4A;
	mask[0] = 0xFF;

	if (dmx->sectionFilter(0x11, filter, mask, flen) < 0) {
		delete dmx;
		return false;
	}

	do {
		if (dmx->Read(buffer, BAT_SECTION_SIZE) < 0) {
			delete dmx;
			return false;
		}

		if (CServiceScan::getInstance()->Aborted()) {
			ret = false;
			goto _return;
		}

		if (buffer[0] != 0x4A) {
			printf("[BAT] ******************************************* Bogus section received: 0x%x\n", buffer[0]);
			ret = false;
			goto _return;
		}

		unsigned char secnum = buffer[6];
#ifdef DEBUG_BAT
		printf("[BAT] section %X last %X -> %s\n", secnum, buffer[7], secdone[secnum] ? "skip" : "use");
#endif
		if(secdone[secnum])
			continue;
		secdone[secnum] = 1;
		sectotal++;

		BouquetAssociationSection * bat = new BouquetAssociationSection(buffer);
		sections.push_back(bat);

	} while(sectotal < buffer[7]);
_return:
	dmx->Stop();
	delete dmx;
	return ret;
}

bool CBat::Parse()
{
	printf("[scan] trying to parse BAT\n");

	if(!Read())
		return false;

	BouquetAssociationSectionConstIterator sit;
	for (sit = sections.begin(); sit != sections.end(); ++sit) {
		BouquetAssociationSection * bat = *sit;
		uint16_t bouquet_id = bat->getTableIdExtension();

		if (CServiceScan::getInstance()->Aborted())
			return false;

		const DescriptorList * dlist = bat->getDescriptors();
		DescriptorConstIterator dit;
#ifdef DEBUG_BAT
		printf("BAT: section %d, %d descriptors\n", bat->getSectionNumber(), dlist->size());
#endif
		for (dit = dlist->begin(); dit != dlist->end(); ++dit) {
			Descriptor * d = *dit;
			//printf("BAT: parse descriptor %02x len %d\n", d->getTag(), d->getLength());
			switch(d->getTag()) {
				case BOUQUET_NAME_DESCRIPTOR:
					{
						BouquetNameDescriptor * nd = (BouquetNameDescriptor *) d;
						std::string bouquetName = stringDVBUTF8(nd->getBouquetName());
						printf("BAT: bouquet name [%s]\n", bouquetName.c_str());
					}
					break;
				case LINKAGE_DESCRIPTOR:
					{
#ifdef DEBUG_BAT
						LinkageDescriptor * ld = (LinkageDescriptor*) d;
						printf("BAT: linkage, tsid %04x onid %04x sid %04x type %02x\n", ld->getTransportStreamId(),
								ld->getOriginalNetworkId(), ld->getServiceId(), ld->getLinkageType());
#endif
					}
					break;
				case COUNTRY_AVAILABILITY_DESCRIPTOR:
				case PRIVATE_DATA_SPECIFIER_DESCRIPTOR:
				default:
					{
#ifdef DEBUG_BAT_UNUSED
						printf("BAT: descriptor %02x: ", d->getTag());
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
		const BouquetAssociationList &blist = *bat->getBouquets();
		BouquetAssociationConstIterator it;
		for(it = blist.begin(); it != blist.end(); ++it) {
			BouquetAssociation * b = *it;
			dlist = b->getDescriptors();
#if 1
			printf("BAT: bouquet_id %04x tsid %04x onid %04x %d descriptors\n", bouquet_id, b->getTransportStreamId(),
					b->getOriginalNetworkId(), dlist->size());
#endif
			for (dit = dlist->begin(); dit != dlist->end(); ++dit) {
				Descriptor * d = *dit;
				switch(d->getTag()) {
					case SERVICE_LIST_DESCRIPTOR:
						ParseServiceList((ServiceListDescriptor *) d, b);
						break;

					case LOGICAL_CHANNEL_DESCRIPTOR:
						ParseLogicalChannels((LogicalChannelDescriptor *) d, b);
						break;
					default:
						{
#ifdef DEBUG_BAT_UNUSED
							printf("BAT: descriptor %02x: ", d->getTag());
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

bool CBat::ParseServiceList(ServiceListDescriptor * sd, BouquetAssociation * b)
{
	const ServiceListItemList * slist = sd->getServiceList();
	ServiceListItemConstIterator it;
	for (it = slist->begin(); it != slist->end(); ++it) {
		ServiceListItem * s = *it;
#if 0
		t_channel_id channel_id = CZapitChannel::makeChannelId(satellitePosition,
				freq_id, b->getTransportStreamId(), b->getOriginalNetworkId(), s->getServiceId());
		CServiceScan::getInstance()->AddServiceType(channel_id, s->getServiceType());
#endif
#ifdef DEBUG_BAT
		printf("BAT: service tsid %04x onid %04x sid %04x type %02x\n",
				b->getTransportStreamId(), b->getOriginalNetworkId(), s->getServiceId(), s->getServiceType());
#endif
	}
	return true;
}

bool CBat::ParseLogicalChannels(LogicalChannelDescriptor * ld, BouquetAssociation * b)
{
	t_transport_stream_id transport_stream_id = b->getTransportStreamId();
	t_original_network_id original_network_id = b->getOriginalNetworkId();

	const LogicalChannelList &clist = *ld->getChannelList();
	LogicalChannelListConstIterator it;
	for (it = clist.begin(); it != clist.end(); ++it) {
		t_service_id service_id = (*it)->getServiceId();
		int lcn = (*it)->getLogicalChannelNumber();
		/* FIXME dont use freq_id / satellitePosition ? */
		t_channel_id channel_id = CZapitChannel::makeChannelId(satellitePosition,
				freq_id, transport_stream_id, original_network_id, service_id);
		/* (*it)->getVisibleServiceFlag(); */
		logical_map[channel_id] = lcn;
#ifdef DEBUG_LCN
		printf("BAT: logical channel tsid %04x onid %04x %llx -> %d\n", transport_stream_id, original_network_id, channel_id, lcn);
#endif
	}
	return true;
}
