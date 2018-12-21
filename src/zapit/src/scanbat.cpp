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
#include <zapit/scanbat.h>
#include <zapit/scan.h>
#include <dvbsi++/descriptor_tag.h>
#include <dvbsi++/bouquet_name_descriptor.h>
#include <dvbsi++/linkage_descriptor.h>
#include <dvbsi++/private_data_specifier_descriptor.h>
#include <math.h>
#include <eitd/edvbstring.h>
#include <system/set_threadname.h>

#define DEBUG_BAT
#define DEBUG_BAT_UNUSED
#define DEBUG_LCN

static bool compare_section_num(BouquetAssociationSection * first, BouquetAssociationSection * second)
{
	return first->getSectionNumber() < second->getSectionNumber();
}

CBat::CBat(t_satellite_position spos, freq_id_t frq, int dnum)
{
	satellitePosition = spos;
	freq_id = frq;
	dmxnum = dnum;
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
	set_threadname("zap:bat");
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

	memset(secdone, 0, sizeof(secdone));

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

	if (!dmx->sectionFilter(0x11, filter, mask, flen)) {
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
                if(secdone[secnum]) {
                        secdone[secnum]++;
                        if(secdone[secnum] >= 5)
                                break;
                        continue;
                }
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

	sections.sort(compare_section_num);
	BouquetAssociationSectionConstIterator sit;
	for (sit = sections.begin(); sit != sections.end(); ++sit) {
		BouquetAssociationSection * bat = *sit;
		uint16_t bouquet_id = bat->getTableIdExtension();

		if (CServiceScan::getInstance()->Aborted())
			return false;

		const DescriptorList * dlist = bat->getDescriptors();
		DescriptorConstIterator dit;
#ifdef DEBUG_BAT
		printf("BAT: section %d, %d descriptors\n", bat->getSectionNumber(), (int)dlist->size());
#endif
		unsigned int pdsd = 0;
		for (dit = dlist->begin(); dit != dlist->end(); ++dit) {
			Descriptor * d = *dit;
			//printf("BAT: parse descriptor %02x len %d\n", d->getTag(), d->getLength());
			switch(d->getTag()) {
				case BOUQUET_NAME_DESCRIPTOR:
					{
						BouquetNameDescriptor * nd = (BouquetNameDescriptor *) d;
						/*std::string*/ bouquetName = stringDVBUTF8(nd->getBouquetName());
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
				case PRIVATE_DATA_SPECIFIER_DESCRIPTOR:
					{
						PrivateDataSpecifierDescriptor * pd = (PrivateDataSpecifierDescriptor *)d;
						pdsd = pd->getPrivateDataSpecifier();
#ifdef DEBUG_BAT
						printf("BAT: private data specifier %08x\n", pdsd);
#endif
					}
					break;
				case COUNTRY_AVAILABILITY_DESCRIPTOR:
				default:
					{
#ifdef DEBUG_BAT_UNUSED
						printf("BAT: descriptor %02x: ", d->getTag());
						uint8_t len = 2+d->getLength();
						uint8_t *buf = new uint8_t[len];
						if(buf){
							d->writeToBuffer(buf);
							for(uint8_t i = 0; i < len; i++)
								printf("%02x ", buf[i]);
							printf("\n");
							delete []buf;
						}
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
					b->getOriginalNetworkId(), (int)dlist->size());
#endif
			for (dit = dlist->begin(); dit != dlist->end(); ++dit) {
				Descriptor * d = *dit;
				switch(d->getTag()) {
					case SERVICE_LIST_DESCRIPTOR:
						ParseServiceList((ServiceListDescriptor *) d, b);
						break;
#if 1
					case LOGICAL_CHANNEL_DESCRIPTOR:
						ParseLogicalChannels((LogicalChannelDescriptor *) d, b);
						break;
#endif
					case PRIVATE_DATA_SPECIFIER_DESCRIPTOR:
						{
							PrivateDataSpecifierDescriptor * pd = (PrivateDataSpecifierDescriptor *)d;
							pdsd = pd->getPrivateDataSpecifier();
#ifdef DEBUG_BAT
							printf("BAT: private data specifier %08x\n", pdsd);
#endif
						}
						break;
					default:
						{
#ifdef DEBUG_BAT_UNUSED
							printf("BAT: descriptor %02x: ", d->getTag());
							uint8_t len = 2+d->getLength();
							uint8_t *buf = new uint8_t[len];
							if(buf){
								d->writeToBuffer(buf);
								for(uint8_t i = 0; i < len; i++)
									printf("%02x ", buf[i]);
								printf("\n");
								delete []buf;
							}
#endif
						}
						break;
				}
				switch(pdsd) {
					case 0x00000010:
						if(d->getTag() == 0x82) {
							uint8_t *buf = new uint8_t[2 + d->getLength()];
							if(buf){
								d->writeToBuffer(buf);
								LogicalChannelDescriptor ld(buf);
								ParseLogicalChannels(&ld, b);
								delete []buf;
							}
						}
						break;
					default:
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
#if 1
		t_channel_id channel_id = CZapitChannel::makeChannelId(0 /*satellitePosition*/,
				0 /*freq_id*/, b->getTransportStreamId(), b->getOriginalNetworkId(), s->getServiceId());
		CServiceScan::getInstance()->AddServiceType(channel_id, s->getServiceType());
#endif
#ifdef DEBUG_BAT
		printf("BAT: service tsid %04x onid %04x sid %04x type %02x\n",
				b->getTransportStreamId(), b->getOriginalNetworkId(), s->getServiceId(), s->getServiceType());
#endif
		bouquet_map[bouquetName].insert(channel_id);
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
		int lcn = (*it)->getLogicalChannelNumber();
		if(lcn == 0)
			continue;

		t_service_id service_id = (*it)->getServiceId();
		t_channel_id channel_id = CZapitChannel::makeChannelId(0, 0,
				transport_stream_id, original_network_id, service_id);
		/* (*it)->getVisibleServiceFlag(); */
		logical_map[channel_id] = lcn;
#ifdef DEBUG_LCN
		std::string name;
		CZapitChannel * chan = CServiceManager::getInstance()->FindChannel48(channel_id);
		if(chan)
			name = chan->getName();
		printf("BAT: logical channel %05d: tsid %04x onid %04x %016" PRIx64 " [%s] visible %d\n", lcn, transport_stream_id, original_network_id, channel_id, name.c_str(), (*it)->getVisibleServiceFlag());
#endif
	}
	return true;
}
