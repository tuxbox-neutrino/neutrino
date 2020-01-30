/*
 * classes SIservices and SIservices (dbox-II-project)
 *
 * (C) 2001 by fnbrd (fnbrd@gmx.de),
 *     2003 by thegoodguy <thegoodguy@berlios.de>
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

#ifndef SISERVICES_HPP
#define SISERVICES_HPP

#include <set>
#include <algorithm>
#include <string>
#include <cstring> // memset

#include <sectionsdclient/sectionsdMsg.h>

class SInvodReference
{
public:
	t_service_id          service_id;
	t_original_network_id original_network_id;
	t_transport_stream_id transport_stream_id;

	SInvodReference(const t_transport_stream_id new_transport_stream_id, const t_original_network_id new_original_network_id, const t_service_id new_service_id):
					service_id(new_service_id),
					original_network_id(new_original_network_id),
					transport_stream_id(new_transport_stream_id)
		{
		}

	bool operator < (const SInvodReference& ref) const
		{
			return uniqueKey() < ref.uniqueKey();
		}
#if 0
//unused
	void toStream(char * &p) const
		{
			*(t_service_id          *)p = service_id;			p += sizeof(t_service_id);
			*(t_original_network_id *)p = original_network_id;		p += sizeof(t_original_network_id);
			*(t_transport_stream_id *)p = transport_stream_id;		p += sizeof(t_transport_stream_id);
		}

	t_service_id getServiceID() const
		{
			return service_id;
		}
#endif
	t_channel_id uniqueKey(void) const {
		return CREATE_CHANNEL_ID(service_id, original_network_id, transport_stream_id); // cf. zapittypes.h
	}
	void dump(void) const
		{
			printf("NVOD Ref. Service-ID: %hu\n", service_id);
			printf("NVOD Ref. Original-Network-ID: %hu\n", original_network_id);
			printf("NVOD Ref. Transport-Stream-ID: %hu\n", transport_stream_id);
		}

};

// Fuer for_each
struct printSInvodReference : public std::unary_function<class SInvodReference, void>
{
	void operator() (const SInvodReference &ref) { ref.dump();}
};

typedef std::set <SInvodReference, std::less<SInvodReference> > SInvodReferences;

class SIservice 
{
//protected:
public:
	struct {
		unsigned char EIT_schedule_flag : 1;
		unsigned char EIT_present_following_flag : 1;
		unsigned char running_status : 3;
		unsigned char free_CA_mode : 1;
	} flags;
	t_service_id          service_id;
	t_original_network_id original_network_id;
	t_transport_stream_id transport_stream_id;
	unsigned char serviceTyp;
	int is_actual;
	SInvodReferences nvods;
#if 0 // unused
	std::string serviceName; // Name aus dem Service-Descriptor
	std::string providerName; // Name aus dem Service-Descriptor
#endif

	SIservice(const t_service_id _service_id, const t_original_network_id _original_network_id, const t_transport_stream_id _transport_stream_id):
		service_id(_service_id),
		original_network_id(_original_network_id),
		transport_stream_id(_transport_stream_id)
	{
		serviceTyp=0;
		memset(&flags, 0, sizeof(flags));
		is_actual = 0;
	}

#if 0 
// unused
	int eitScheduleFlag(void)	{ return (int)flags.EIT_schedule_flag; }
	int eitPresentFollowingFlag(void) { return (int)flags.EIT_present_following_flag; }
#endif
	int runningStatus(void)	const	{ return (int)flags.running_status; }
#if 0 
// unused
	int freeCAmode(void)		{ return (int)flags.free_CA_mode; }
#endif
	bool operator < (const SIservice& s) const {
		return uniqueKey() < s.uniqueKey();
	}

	t_channel_id uniqueKey(void) const {
		//notice that tsid & onid were changed for compatibility sake - order should be onid tsid when being sorted
		return CREATE_CHANNEL_ID(service_id, transport_stream_id, original_network_id);
	}

	void dump(void) const {
		printf("Original-Network-ID: %hu\n", original_network_id);
		printf("Service-ID: %hu\n", service_id);
		printf("Service-Typ: %hhu\n", serviceTyp);
#if 0 // unused
		if(!providerName.empty())
			printf("Provider-Name: %s\n", providerName.c_str());
		if(!serviceName.empty())
			printf("Service-Name: %s\n", serviceName.c_str());
#endif
		for_each(nvods.begin(), nvods.end(), printSInvodReference());
		printf("\n");
	}
};

// Fuer for_each
struct printSIservice : public std::unary_function<SIservice, void>
{
	void operator() (const SIservice &s) { s.dump();}
};

// Als Klasse, da ich nicht weiss, wie man eine Forward-Referenz auf ein typedef macht
class SIservices : public std::set <SIservice, std::less<SIservice> >
{
};

#endif // SISERVICES_HPP
