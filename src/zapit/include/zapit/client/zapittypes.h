/*
 * $Id: zapittypes.h,v 1.23 2004/02/24 23:50:57 thegoodguy Exp $
 *
 * zapit's types which are used by the clientlib - d-box2 linux project
 *
 * (C) 2002 by thegoodguy <thegoodguy@berlios.de>
 * (C) 2002, 2003 by Andreas Oberritter <obi@tuxbox.org>
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

#ifndef __zapittypes_h__
#define __zapittypes_h__


#include <zapit/types.h>
#include <inttypes.h>
#include <string>
#include <map>
#include <linux/dvb/frontend.h>

typedef uint16_t t_service_id;
#define SCANF_SERVICE_ID_TYPE "%hx"

typedef uint16_t t_original_network_id;
#define SCANF_ORIGINAL_NETWORK_ID_TYPE "%hx"

typedef uint16_t t_transport_stream_id;
#define SCANF_TRANSPORT_STREAM_ID_TYPE "%hx"

typedef int16_t t_satellite_position;
#define SCANF_SATELLITE_POSITION_TYPE "%hd"

typedef uint16_t t_network_id;
//Introduced by Nirvana 11/05. Didn't check if there are similar types
typedef uint16_t t_bouquet_id;
//Introduced by Nirvana 11/05. Didn't check if there are similar types
typedef uint32_t t_transponder_id;
#define CREATE_TRANSPONDER_ID_FROM_ORIGINALNETWORK_TRANSPORTSTREAM_ID(original_network_id,transport_stream_id) ((((t_original_network_id) original_network_id) << 16) | (t_transport_stream_id) transport_stream_id)

/* unique channel identification */
typedef uint64_t t_channel_id;
#define CREATE_CHANNEL_ID_FROM_SERVICE_ORIGINALNETWORK_TRANSPORTSTREAM_ID(service_id,original_network_id,transport_stream_id) ((((t_channel_id)transport_stream_id) << 32) | (((t_channel_id)original_network_id) << 16) | (t_channel_id)service_id)
#define CREATE_CHANNEL_ID CREATE_CHANNEL_ID_FROM_SERVICE_ORIGINALNETWORK_TRANSPORTSTREAM_ID(service_id, original_network_id, transport_stream_id)
#define GET_ORIGINAL_NETWORK_ID_FROM_CHANNEL_ID(channel_id) ((t_original_network_id)((channel_id) >> 16))
#define GET_SERVICE_ID_FROM_CHANNEL_ID(channel_id) ((t_service_id)(channel_id))
#define PRINTF_CHANNEL_ID_TYPE "%16llx"
#define PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS "%llx"
#define SCANF_CHANNEL_ID_TYPE "%llx"

#define CREATE_CHANNEL_ID64 (((uint64_t)(satellitePosition+freq*4) << 48) | ((uint64_t) transport_stream_id << 32) | ((uint64_t)original_network_id << 16) | (uint64_t)service_id)
//#define CREATE_CHANNEL_ID64 CREATE_CHANNEL_ID_FROM_64(satellitePosition, service_id, original_network_id, transport_stream_id)

/* diseqc types */
typedef enum {
	NO_DISEQC,
	MINI_DISEQC,
	SMATV_REMOTE_TUNING,
	DISEQC_1_0,
	DISEQC_1_1,
	DISEQC_1_2,
	DISEQC_ADVANCED
#if 0
	, DISEQC_2_0,
	DISEQC_2_1,
	DISEQC_2_2
#endif
} diseqc_t;

/* dvb transmission types */
typedef enum {
	DVB_C,
	DVB_S,
	DVB_T
} delivery_system_t;

/* video display formats (cf. video_displayformat_t in driver/dvb/include/linux/dvb/video.h): */
typedef enum {
	ZAPIT_VIDEO_PAN_SCAN,       /* use pan and scan format */
	ZAPIT_VIDEO_LETTER_BOX,     /* use letterbox format */
	ZAPIT_VIDEO_CENTER_CUT_OUT  /* use center cut out format */
} video_display_format_t;

typedef enum {
	ST_RESERVED,
	ST_DIGITAL_TELEVISION_SERVICE,
	ST_DIGITAL_RADIO_SOUND_SERVICE,
	ST_TELETEXT_SERVICE,
	ST_NVOD_REFERENCE_SERVICE,
	ST_NVOD_TIME_SHIFTED_SERVICE,
	ST_MOSAIC_SERVICE,
	ST_PAL_CODED_SIGNAL,
	ST_SECAM_CODED_SIGNAL,
	ST_D_D2_MAC,
	ST_FM_RADIO,
	ST_NTSC_CODED_SIGNAL,
	ST_DATA_BROADCAST_SERVICE,
	ST_COMMON_INTERFACE_RESERVED,
	ST_RCS_MAP,
	ST_RCS_FLS,
	ST_DVB_MHP_SERVICE
} service_type_t;

#define	VERTICAL 0
#define HORIZONTAL 1

/* complete transponder-parameters in a struct */
typedef struct TP_parameter
{
	uint64_t TP_id;					/* diseqc<<24 | feparams->frequency>>8 */
	uint8_t polarization;
	uint8_t diseqc;
	int scan_mode;
	struct dvb_frontend_parameters feparams;
} TP_params;

/* complete channel-parameters in a struct */
typedef struct Channel_parameter
{
	std::string name;
	t_service_id service_id;
	t_transport_stream_id transport_stream_id;
	t_original_network_id original_network_id;
	unsigned char service_type;
	uint32_t TP_id;					/* diseqc<<24 | feparams->frequency>>8 */
} CH_params;

typedef struct TP_map
{
	TP_params TP;
	TP_map(const TP_params p_TP)
	{
		TP = p_TP;
	}
} t_transponder;

#define MAX_LNB 64 
typedef struct Zapit_config {
	int motorRotationSpeed;
	int writeChannelsNames;
	int makeRemainingChannelsBouquet;
	int saveLastChannel;
	int rezapTimeout;
	int feTimeout;
	int fastZap;
	int sortNames;
	int highVoltage;
	int scanPids;
	int scanSDT;
	int useGotoXX;
	int gotoXXLaDirection;
	int gotoXXLoDirection;
	int repeatUsals;
	double gotoXXLatitude;
	double gotoXXLongitude;
} t_zapit_config;

typedef std::map <uint32_t, TP_map> TP_map_t;
typedef std::map <uint32_t, TP_map>::iterator TP_iterator;

#endif /* __zapittypes_h__ */
