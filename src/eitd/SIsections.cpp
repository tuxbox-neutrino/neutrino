//
// $Id: SIsections.cpp,v 1.61 2009/09/04 18:37:00 dbt Exp $
//
// classes for SI sections (dbox-II-project)
//
//    Homepage: http://dbox2.elxsi.de
//
//    Copyright (C) 2001 fnbrd (fnbrd@gmx.de)
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
//

#include <config.h>
#include <stdio.h>
#include <unistd.h>

#include "SIutils.hpp"
#include "SIservices.hpp"
#include "SIevents.hpp"
#include "SIsections.hpp"
#include <edvbstring.h>

#include <dvbsi++/descriptor_tag.h>
#include <dvbsi++/nvod_reference_descriptor.h>
#include <dvbsi++/service_descriptor.h>

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
		evts.insert(e);
	}
	parsed = 1;
}

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
