/*
 * SIsections.cpp, classes for SI sections (dbox-II-project)
 *
 * (C) 2001 by fnbrd (fnbrd@gmx.de),
 *
 * Copyright (C) 2011-2012 CoolStream International Ltd
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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <unistd.h>

#include <dvbsi++/descriptor_tag.h>
#include <dvbsi++/nvod_reference_descriptor.h>
#include <dvbsi++/service_descriptor.h>
#include <dvbsi++/time_offset_section.h>
#include <dvbsi++/time_date_section.h>
#include <dvbsi++/local_time_offset_descriptor.h>

#include "SIutils.hpp"
#include "SIservices.hpp"
#include "SIevents.hpp"
#include "SIsections.hpp"
#include "debug.h"
#include "edvbstring.h"

inline unsigned min(unsigned a, unsigned b)
{
	return b < a ? b : a;
}

#ifdef USE_DVBSI_EVENTS
void SIsectionEIT::parse(void)
{
	const EventList &elist = *getEvents();

	if(elist.empty())
		return;

        t_service_id		sid = getTableIdExtension();
        t_original_network_id	onid = getOriginalNetworkId();
        t_transport_stream_id	tsid = getTransportStreamId();
	unsigned char		tid = getTableId();
	unsigned char		version = getVersionNumber();

	for (EventConstIterator eit = elist.begin(); eit != elist.end(); ++eit) {
		Event &event = (**eit);

		SIevent e(onid, tsid, sid, event.getEventId());
		e.table_id = tid;
		e.version = version;

		e.parse(event);
		//evts.insert(e);
		evts.push_back(e);
	}
	parsed = 1;
}
#else
void SIsectionEIT::parse(void)
{
	const uint8_t *actPos;
	const uint8_t *bufEnd;
	struct eit_event *evt;
	unsigned short descriptors_loop_length;

	if (!buffer || parsed)
		return;

	struct SI_section_header *siheader = (SI_section_header*) buffer;
	unsigned short bufferLength = siheader->section_length_hi << 8 | siheader->section_length_lo;

	if (bufferLength < sizeof(SI_section_EIT_header) + sizeof(struct eit_event)) {
		bufferLength=0;
		return;
	}

	unsigned char table_id = header()->table_id;
	unsigned char version_number = header()->version_number;
	actPos = buffer + sizeof(SI_section_EIT_header);
	bufEnd = buffer + bufferLength;

	while (actPos < bufEnd - sizeof(struct eit_event)) {
		evt = (struct eit_event *) actPos;
		SIevent e(evt);
		e.service_id = service_id();
		e.original_network_id = original_network_id();
		e.transport_stream_id = transport_stream_id();
		e.table_id = table_id;
		e.version = version_number;
		descriptors_loop_length = sizeof(struct eit_event) + ((evt->descriptors_loop_length_hi << 8) | evt->descriptors_loop_length_lo);
		e.parseDescriptors(actPos, min((unsigned)(bufEnd - actPos), descriptors_loop_length));
		//evts.insert(e);
		evts.push_back(e);
		actPos += descriptors_loop_length;
	}
	parsed = 1;
}
#endif

void SIsectionSDT::parse(void)
{
	const ServiceDescriptionList &slist = *getDescriptions();

	if(slist.empty())
		return;

	t_transport_stream_id transport_stream_id = getTransportStreamId();
	t_original_network_id original_network_id = getOriginalNetworkId();
	for (ServiceDescriptionConstIterator sit = slist.begin(); sit != slist.end(); ++sit) {
		ServiceDescription * service = *sit;

		SIservice s(service->getServiceId(), original_network_id, transport_stream_id);

		s.flags.EIT_schedule_flag = service->getEitScheduleFlag();
		s.flags.EIT_present_following_flag = service->getEitPresentFollowingFlag();
		s.flags.running_status = service->getRunningStatus();
		s.flags.free_CA_mode = service->getFreeCaMode();

		DescriptorConstIterator dit;
		for (dit = service->getDescriptors()->begin(); dit != service->getDescriptors()->end(); ++dit) {
			switch ((*dit)->getTag()) {
				case SERVICE_DESCRIPTOR:
					{
						ServiceDescriptor * d = (ServiceDescriptor *) *dit;
						s.serviceTyp = d->getServiceType();
					}
					break;
				case NVOD_REFERENCE_DESCRIPTOR:
					{
						NvodReferenceDescriptor * d = (NvodReferenceDescriptor *) *dit;
						NvodReferenceConstIterator it;
						const NvodReferenceList* nlist = d->getNvodReferences();
						for (it = nlist->begin(); it != nlist->end(); ++it) { 
							SInvodReference nvod((*it)->getTransportStreamId(), (*it)->getOriginalNetworkId(), (*it)->getServiceId());
							s.nvods.insert(nvod);
						}
					}
					break;
				default:
					break;
			}
		}
		svs.insert(s);
	}
	parsed = 1;
}

void SIsectionTIME::parse(uint8_t *buf)
{
	if(buf[0] != 0x70 && buf[0] != 0x73)
		return;

	bool TDT = (buf[0] == 0x70);
	if(TDT) {
		TimeAndDateSection tdt(buf);
		if(tdt.getSectionLength() < 5)
			return;
		dvbtime = parseDVBtime(tdt.getUtcTimeMjd(), tdt.getUtcTimeBcd());
		char *ct = ctime(&dvbtime);
		/* ctime() appends a useless \n... */
		ct[strlen(ct) - 1] = 0;
		/* ...and debug_colored adds another \n... */
		debug_colored(DEBUG_ERROR, "SIsectionTIME::parse: TDT time: %s", ct);
		parsed = true;
	} else {
		TimeOffsetSection tot(buf);
		dvbtime = parseDVBtime(tot.getUtcTimeMjd(), tot.getUtcTimeBcd());
		char *ct = ctime(&dvbtime);
		ct[strlen(ct) - 1] = 0;
		debug_colored(DEBUG_ERROR, "SIsectionTIME::parse: TOT time: %s", ct);
		const DescriptorList &dlist = *tot.getDescriptors();
		for (DescriptorConstIterator dit = dlist.begin(); dit != dlist.end(); ++dit) {
			uint8_t dtype = (*dit)->getTag();
			if(dtype == LOCAL_TIME_OFFSET_DESCRIPTOR) {
				/* TOT without descriptors seems to be not better than a plain TDT, such TOT's are */
				/* found on transponders which also have wrong time in TDT etc, so don't trust it. */
				parsed = true;
#ifdef DEBUG_TOT
				const LocalTimeOffsetDescriptor * d = (LocalTimeOffsetDescriptor*) *dit;
				const LocalTimeOffsetList * oflist = d->getLocalTimeOffsets();
				for (LocalTimeOffsetConstIterator it = oflist->begin(); it != oflist->end(); ++it) {
					const LocalTimeOffset * of = (LocalTimeOffset *) *it;
					time_t change_time = parseDVBtime(of->getTimeOfChangeMjd(), of->getTimeOfChangeBcd(), false);
					debug(DEBUG_ERROR, "TOT: cc=%s reg_id=%d pol=%d offs=%04x new=%04x when=%s",
							of->getCountryCode().c_str(), of->getCountryRegionId(), of->getLocalTimeOffsetPolarity(),
							of->getLocalTimeOffset(), of->getNextTimeOffset(), ctime(&change_time));
				}
#endif
			} else {
				debug(DEBUG_ERROR, "SIsectionTIME::parse: unhandled descriptor %02x", dtype);
			}
		}
	}
}
