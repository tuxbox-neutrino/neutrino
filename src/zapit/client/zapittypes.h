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

#include <inttypes.h>
#include <string>
#include <map>
#include <zapit/types.h>
#include <zapit/transponder.h>
#include <zapit/frontend_types.h>

/* diseqc types */
typedef enum {
	NO_DISEQC,
	MINI_DISEQC,
	SMATV_REMOTE_TUNING,
	DISEQC_1_0,
	DISEQC_1_1,
	DISEQC_1_2,
	DISEQC_ADVANCED,
	DISEQC_UNICABLE,
	DISEQC_UNICABLE2
#if 0
	, DISEQC_2_0,
	DISEQC_2_1,
	DISEQC_2_2
#endif
} diseqc_t;

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
	uint8_t diseqc;
	int scan_mode;
	FrontendParameters feparams;
} TP_params;

enum ChannelType {
       CHANNEL_MPEG2   = 0,
       CHANNEL_MPEG4   = 1,
       CHANNEL_HEVC    = 6,
       CHANNEL_CAVS    = 16
};

#endif /* __zapittypes_h__ */
