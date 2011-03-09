#ifndef SINETWORKS_HPP
#define SINETWORKS_HPP
//
// $Id: SInetworks.hpp,v 1.3 2006/02/08 21:15:50 houdini Exp $
//
// classes SIservices and SIservices (dbox-II-project)
//
//    Homepage: http://dbox2.elxsi.de
//
//    Copyright (C) 2001 fnbrd (fnbrd@gmx.de),
//                  2002 thegoodguy (thegoodguy@berlios.de)
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


#include <algorithm>
#include <endian.h>

#include <sectionsdclient/sectionsdMsg.h>

// forward references
class SIservice;
class SIevent;
class SInetwork;
/*
struct loop_len {
#if __BYTE_ORDER == __BIG_ENDIAN
	unsigned reserved_future_use		: 4;
	unsigned descriptors_loop_length_hi	: 4;
#else
	unsigned descriptors_loop_length_hi	: 4;
	unsigned reserved_future_use		: 4;
#endif
	unsigned descriptors_loop_length_lo	: 8;
} __attribute__ ((packed)) ; // 2 Bytes
*/
struct nit_transponder {

	unsigned transport_stream_id_hi		: 8;
	unsigned transport_stream_id_lo		: 8;
	unsigned original_network_id_hi		: 8;
	unsigned original_network_id_lo		: 8;
#if __BYTE_ORDER == __BIG_ENDIAN
	unsigned reserved_future_use		: 4;
	unsigned descriptors_loop_length_hi	: 4;
#else
	unsigned descriptors_loop_length_hi	: 4;
	unsigned reserved_future_use		: 4;
#endif
	unsigned descriptors_loop_length_lo	: 8;
	
} __attribute__ ((packed)) ; // 6 Bytes

struct satellite_delivery_descriptor {
	unsigned frequency_1			: 8;
	unsigned frequency_2			: 8;
	unsigned frequency_3			: 8;
	unsigned frequency_4			: 8;
	unsigned orbital_pos_hi			: 8;
	unsigned orbital_pos_lo			: 8;
	#if __BYTE_ORDER == __BIG_ENDIAN
	unsigned west_east_flag			: 1;
	unsigned polarization			: 2;
	unsigned modulation			: 5;
	#else
	unsigned modulation			: 5;
	unsigned polarization			: 2;
	unsigned west_east_flag			: 1;	
	#endif
	unsigned symbol_rate_1			: 8;
	unsigned symbol_rate_2			: 8;
	unsigned symbol_rate_3			: 8;
	#if __BYTE_ORDER == __BIG_ENDIAN
	unsigned symbol_rate_4			: 4;
	unsigned fec_inner			: 4;
	#else
	unsigned fec_inner			: 4;
	unsigned symbol_rate_4			: 4;	
	#endif
} __attribute__ ((packed)) ; // 11 Bytes

struct cable_delivery_descriptor {
	unsigned frequency_1			: 8;
	unsigned frequency_2			: 8;
	unsigned frequency_3			: 8;
	unsigned frequency_4			: 8;
	unsigned reserved_hi			: 8;
	#if __BYTE_ORDER == __BIG_ENDIAN
	unsigned reserved_lo			: 4;
	unsigned fec_outer			: 4;
	#else
	unsigned fec_outer			: 4;	
	unsigned reserved_lo			: 4;
	#endif
	unsigned modulation			: 8;
	unsigned symbol_rate_1			: 8;
	unsigned symbol_rate_2			: 8;
	unsigned symbol_rate_3			: 8;
	#if __BYTE_ORDER == __BIG_ENDIAN
	unsigned symbol_rate_4			: 4;
	unsigned fec_inner			: 4;
	#else
	unsigned fec_inner			: 4;
	unsigned symbol_rate_4			: 4;	
	#endif
} __attribute__ ((packed)) ; // 11 Bytes

class SInetwork {
public:
	SInetwork(const struct nit_transponder *s) {
		
		network_id		= 0;
		transport_stream_id	= 0;
		original_network_id	= 0;
		delivery_type		= 0;
		is_actual		= false;
	}
	
	// Um einen service zum Suchen zu erstellen
	SInetwork(const t_network_id _network_id, const t_original_network_id _original_network_id, const t_transport_stream_id _transport_stream_id)
	{
		network_id		= _network_id;
		original_network_id	= _original_network_id;
		transport_stream_id	= _transport_stream_id;
	}
	// Std-Copy
	SInetwork(const SInetwork &s) {
	
		network_id		= s.network_id;
		original_network_id	= s.original_network_id;
		transport_stream_id 	= s.transport_stream_id;
		delivery_type		= s.delivery_type;
		is_actual		= s.is_actual;
		memmove(delivery_descriptor, s.delivery_descriptor, sizeof(struct satellite_delivery_descriptor));
	}
	
	t_network_id network_id;
	t_original_network_id original_network_id;
	t_transport_stream_id transport_stream_id;
	unsigned short delivery_type;
	bool is_actual;
	char delivery_descriptor[sizeof(struct satellite_delivery_descriptor)];
	
	bool operator < (const SInetwork& s) const {
		return uniqueKey() < s.uniqueKey();
	}

	t_transponder_id uniqueKey(void) const {
		return CREATE_TRANSPONDER_ID_FROM_ORIGINALNETWORK_TRANSPORTSTREAM_ID(original_network_id, transport_stream_id);
	}

	void dump(void) const {
	
		printf("Original-Network-ID: %hu\n", original_network_id);
		printf("Transport-Stream-ID: %hu\n", transport_stream_id);
	}
};

// Fuer for_each
struct printSInetwork : public std::unary_function<SInetwork, void>
{
	void operator() (const SInetwork &s) { s.dump();}
};

// Als Klasse, da ich nicht weiss, wie man eine Forward-Referenz auf ein typedef macht
class SInetworks : public std::set <SInetwork, std::less<SInetwork> >
{
};

#endif // SINETWORKS_HPP
