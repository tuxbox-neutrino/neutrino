/*
 * zapit's types - d-box2 linux project
 * these types are used by the clientlib and zapit itself
 *
 * (C) 2002 by thegoodguy <thegoodguy@berlios.de>
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

#ifndef __zapit__types_h__
#define __zapit__types_h__

#include <stdint.h>
#include <inttypes.h>

typedef uint16_t freq_id_t;
typedef uint16_t t_service_id;
#define SCANF_SERVICE_ID_TYPE "%hx"

typedef uint16_t t_original_network_id;
#define SCANF_ORIGINAL_NETWORK_ID_TYPE "%hx"

typedef uint16_t t_transport_stream_id;
#define SCANF_TRANSPORT_STREAM_ID_TYPE "%hx"

typedef int16_t t_satellite_position;
#define SCANF_SATELLITE_POSITION_TYPE "%hd"

typedef uint16_t t_bouquet_id;

/* unique channel identification */
typedef uint64_t t_channel_id;

#define CREATE_CHANNEL_ID(service_id,original_network_id,transport_stream_id) ((((t_channel_id)transport_stream_id) << 32) | (((t_channel_id)original_network_id) << 16) | (t_channel_id)service_id)
#define CREATE_CHANNEL_ID64 (((uint64_t)(satellitePosition+freq*4) << 48) | ((uint64_t) transport_stream_id << 32) | ((uint64_t)original_network_id << 16) | (uint64_t)service_id)

#ifndef PRIx64
#define PRIx64 "llx"
#define PRId64 "lld"
#define SCNx64 "llx"
#define SCNd64 "lld"
#endif

#define PRINTF_CHANNEL_ID_TYPE "%16" PRIx64
#define PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS "%" PRIx64
#define SCANF_CHANNEL_ID_TYPE "%" SCNx64

typedef uint64_t transponder_id_t;
#if 0
#define CREATE_TRANSPONDER_ID_FROM_SATELLITEPOSITION_ORIGINALNETWORK_TRANSPORTSTREAM_ID(freq, satellitePosition,original_network_id,transport_stream_id) \
 ( ((uint64_t)freq << 48) |  ((uint64_t) ( satellitePosition >= 0 ? satellitePosition : (uint64_t)(0xF000+ abs(satellitePosition))) << 32) | ((uint64_t)transport_stream_id << 16) | (uint64_t)original_network_id)
#endif
#define CREATE_TRANSPONDER_ID64(freq, satellitePosition,original_network_id,transport_stream_id) \
 ( ((uint64_t)freq << 48) |  ((uint64_t) ( satellitePosition >= 0 ? satellitePosition : (uint64_t)(0xF000+ abs(satellitePosition))) << 32) | ((uint64_t)transport_stream_id << 16) | (uint64_t)original_network_id)

#define GET_TRANSPORT_STREAM_ID_FROM_CHANNEL_ID(channel_id) ((t_transport_stream_id)((channel_id) >> 32))
#define GET_ORIGINAL_NETWORK_ID_FROM_CHANNEL_ID(channel_id) ((t_original_network_id)((channel_id) >> 16))
#define GET_SERVICE_ID_FROM_CHANNEL_ID(channel_id) ((t_service_id)(channel_id))

#define SAME_TRANSPONDER(id1, id2) ((id1 >> 16) == (id2 >> 16))

#define PRINTF_TRANSPONDER_ID_TYPE "%12" PRIx64
#define TRANSPONDER_ID_NOT_TUNED 0
#define GET_ORIGINAL_NETWORK_ID_FROM_TRANSPONDER_ID(transponder_id) ((t_original_network_id)(transponder_id      ))
#define GET_TRANSPORT_STREAM_ID_FROM_TRANSPONDER_ID(transponder_id) ((t_transport_stream_id)(transponder_id >> 16))
#define GET_SATELLITEPOSITION_FROM_TRANSPONDER_ID(transponder_id)   ((t_satellite_position )(transponder_id >> 32))
#define GET_SAT_FROM_TPID(transponder_id)   ((t_satellite_position )(transponder_id >> 32) & 0xFFFF)
#define GET_FREQ_FROM_TPID(transponder_id) ((freq_id_t)(transponder_id >> 48))
#define CREATE_FREQ_ID(frequency, cable)  (freq_id_t)(cable ? frequency/100 : frequency/1000)

#endif /* __zapit__types_h__ */
