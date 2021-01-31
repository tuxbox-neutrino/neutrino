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

#ifndef __zapit_scan_pmt_h__
#define __zapit_scan_pmt_h__

#include "channel.h"
#include <dvbsi++/program_map_section.h>

int pmt_set_update_filter(CZapitChannel * const channel, int * fd);
int pmt_stop_update_filter(int * fd);

#define PMT_SECTION_SIZE 1024
class CPmt
{
	private:
		int dmxnum;
		unsigned char buffer[PMT_SECTION_SIZE];

		bool Read(unsigned short pid, unsigned short sid);
		void MakeCAMap(casys_map_t &camap);
		bool ParseEsInfo(ElementaryStreamInfo *esinfo, CZapitChannel * const channel);
	public:
		CPmt(int dnum = 0);
		~CPmt();

		bool Parse(CZapitChannel * const channel);
		bool haveCaSys(int pmtpid, int service_id);
};

/* registration_descriptor format IDs */
#define DRF_ID_HDMV       0x48444d56
#define DRF_ID_VC1        0x56432D31   /* defined in RP227 */
#define DRF_ID_DTS1       0x44545331
#define DRF_ID_DTS2       0x44545332
#define DRF_ID_DTS3       0x44545333
#define DRF_ID_S302M      0x42535344
#define DRF_ID_TSHV       0x54534856
#define DRF_ID_AC3        0x41432d33
#define DRF_ID_GA94       0x47413934
#define DRF_ID_CUEI       0x43554549
#define DRF_ID_ETV1       0x45545631
#define DRF_ID_HEVC       0x48455643
#define DRF_ID_KLVA       0x4b4c5641   /* defined in RP217 */
#define DRF_ID_OPUS       0x4f707573
#define DRF_ID_EAC3       0x45414333   /* defined in A/52 Annex G */
#define DRF_ID_AC4        0x41432D34   /* defined in ETSI TS 103 190-2 Annex D */

#define STREAM_TYPE_VIDEO_MPEG1     0x01
#define STREAM_TYPE_VIDEO_MPEG2     0x02
#define STREAM_TYPE_AUDIO_MPEG1     0x03
#define STREAM_TYPE_AUDIO_MPEG2     0x04
#define STREAM_TYPE_PRIVATE_SECTION 0x05
#define STREAM_TYPE_PRIVATE_DATA    0x06
#define STREAM_TYPE_AUDIO_AAC       0x0f
#define STREAM_TYPE_AUDIO_AAC_LATM  0x11
#define STREAM_TYPE_VIDEO_MPEG4     0x10
#define STREAM_TYPE_METADATA        0x15
#define STREAM_TYPE_VIDEO_H264      0x1b
#define STREAM_TYPE_VIDEO_HEVC      0x24
#define STREAM_TYPE_VIDEO_SHVC      0x27
#define STREAM_TYPE_VIDEO_CAVS      0x42
#define STREAM_TYPE_VIDEO_VC1       0xea
#define STREAM_TYPE_VIDEO_DIRAC     0xd1

#define STREAM_TYPE_AUDIO_AC3       0x81
#define STREAM_TYPE_AUDIO_DTS       0x82
#define STREAM_TYPE_AUDIO_LPCM      0x83
#define STREAM_TYPE_AUDIO_DTSHD     0x86
#define STREAM_TYPE_AUDIO_EAC3      0x87

#endif /* __zapit_scan_pmt_h__ */
