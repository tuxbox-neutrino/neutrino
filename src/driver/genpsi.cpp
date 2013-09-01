/*
 Copyright (c) 2004 gmo18t, Germany. All rights reserved.
 Copyright (C) 2012 CoolStream International Ltd
 Copyright (C) 2013 Jacek Jendrzej

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published
 by the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful, but
 WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software Foundation,
 Inc., 675 Mass Ave, Cambridge MA 02139, USA.

 Mit diesem Programm koennen Neutrino TS Streams für das Abspielen unter Enigma gepatched werden
 */

#include <string.h>
#include <unistd.h>
#include <driver/genpsi.h>

#define SIZE_TS_PKT			188
#define OFS_HDR_2			5
#define OFS_PMT_DATA		13
#define OFS_STREAM_TAB		17
#define SIZE_STREAM_TAB_ROW	5
#define OFS_ENIGMA_TAB		31
#define SIZE_ENIGMA_TAB_ROW   4

#define ES_TYPE_MPEG12		0x02
#define ES_TYPE_AVC		0x1b
#define ES_TYPE_MPA		0x03
#define ES_TYPE_AC3		0x81

static const uint32_t crc_table[256] = {
  0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
  0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
  0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
  0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
  0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
  0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
  0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
  0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
  0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
  0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
  0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
  0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
  0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
  0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
  0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
  0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
  0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
  0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
  0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
  0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
  0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
  0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
  0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
  0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
  0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
  0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
  0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
  0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
  0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
  0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
  0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
  0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
  0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
  0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
  0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
  0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
  0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
  0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
  0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
  0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
  0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
  0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
  0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

//-- special enigma stream description packet for  --
//-- at least 1 video, 1 audo and 1 PCR-Pid stream --
//------------------------------------------------------------------------------------
static uint8_t pkt_enigma[] =
{
	0x47, 0x40, 0x1F, 0x10, 0x00,
	0x7F, 0x80, 0x24,
	0x00, 0x00, 0x01, 0x00, 0x00,
	0x00, 0x00, 0x6D, 0x66, 0x30, 0x19,
	0x80, 0x13, 'N','E','U','T','R','I','N','O','N','G',	// tag(8), len(8), text(10) -> NG hihi ;)
	0x00, 0x02, 0x00, 0x6e,				// cVPID(8), len(8), PID(16)
	0x01, 0x03, 0x00, 0x78, 0x00,			// cAPID(8), len(8), PID(16), ac3flag(8)
// 0x02, 0x02, 0x00, 0x82,// cTPID(8), len(8), ...
	0x03, 0x02, 0x00, 0x6e				// cPCRPID(8), ...
};
//-- PAT packet for at least 1 PMT --
//----------------------------------------------------------
static uint8_t pkt_pat[] =
{
	0x47, 0x40, 0x00, 0x10, 0x00,			// HEADER-1
	0x00, 0xB0, 0x0D,					// HEADER-2
	0x04, 0x37, 0xE9, 0x00, 0x00,			// HEADER-3 sid
	0x6D, 0x66, 0xEF, 0xFF,				// PAT-DATA - PMT (PID=0xFFF) entry
};

//-- PMT packet for at least 1 video and 1 audio stream --
//--------------------------------------------------------
static uint8_t pkt_pmt[] =
{
	0x47, 0x4F, 0xFF, 0x10, 0x00,		// HEADER-1
	0x02, 0xB0, 0x17,				// HEADER-2
	0x6D, 0x66, 0xE9, 0x00, 0x00,		// HEADER-3
	0xE0, 0x00, 0xF0, 0x00,			// PMT-DATA
	0x02, 0xE0, 0x00, 0xF0, 0x00,		//   (video stream 1)
	0x03, 0xE0, 0x00, 0xF0, 0x00		//   (audio stream 1)
};

CGenPsi::CGenPsi()
{
	nba = 0;
	vpid = 0;
	vtype = 0;

	pcrpid=0;
	vtxtpid = 0;
	vtxtlang[0] = 'g';
	vtxtlang[1] = 'e';
	vtxtlang[2] = 'r';
	memset(apid, 0, sizeof(apid));
	memset(atypes, 0, sizeof(atypes));
	nsub = 0;
	memset(dvbsubpid, 0, sizeof(dvbsubpid));
}

uint32_t CGenPsi::calc_crc32psi(uint8_t *dst, const uint8_t *src, uint32_t len)
{
	uint32_t i;
	uint32_t crc = 0xffffffff;

	for (i=0; i<len; i++)
		crc = (crc << 8) ^ crc_table[((crc >> 24) ^ *src++) & 0xff];

	if (dst)
	{
		dst[0] = (crc >> 24) & 0xff;
		dst[1] = (crc >> 16) & 0xff;
		dst[2] = (crc >> 8) & 0xff;
		dst[3] = (crc) & 0xff;
	}

	return crc;
}

void CGenPsi::addPid(uint16_t pid, uint16_t pidtype, short isAC3, const char *data)
{
	switch(pidtype)
	{
		case EN_TYPE_VIDEO:
			pcrpid=vpid=pid;
			vtype = ES_TYPE_MPEG12;
			break;
		case EN_TYPE_AVC:
			pcrpid=vpid=pid;
			vtype = ES_TYPE_AVC;
			break;
		case EN_TYPE_PCR:
			pcrpid=pid;
			break;
		case EN_TYPE_AUDIO:
			apid[nba]=pid;
			atypes[nba]=isAC3;
			nba++;
			break;
		case EN_TYPE_TELTEX:
			vtxtpid = pid;
			if(data != NULL){
				vtxtlang[0] = data[0];
				vtxtlang[1] = data[1];
				vtxtlang[2] = data[2];
			}
			break;
		case EN_TYPE_DVBSUB:
			dvbsubpid[nsub] = pid;
			if(data != NULL){
				dvbsublang[nsub][0] = data[0];
				dvbsublang[nsub][1] = data[1];
				dvbsublang[nsub][2] = data[2];
			}
			nsub++;
			break;
		default:
			break;
	}
}

//== setup a new TS packet with format ==
//== predefined with a template        ==
//=======================================
#define COPY_TEMPLATE(dst, src) copy_template(dst, src, sizeof(src))

int CGenPsi::copy_template(uint8_t *dst, uint8_t *src, int len)
{
//-- reset buffer --
	memset(dst, 0xFF, SIZE_TS_PKT);
//-- copy PMT template --
	memmove(dst, src, len);
	
	return len;
}

int CGenPsi::genpsi(int fd)
{
	uint8_t   pkt[SIZE_TS_PKT];
	int       i, data_len, patch_len, ofs;

	//-- copy "Enigma"-template --
	data_len = COPY_TEMPLATE(pkt, pkt_enigma);

	//-- adjust len dependent to number of audio streams --
	data_len += ((SIZE_ENIGMA_TAB_ROW+1) * (nba-1));

	patch_len = data_len - OFS_HDR_2 + 1;
	pkt[OFS_HDR_2+1] |= (patch_len>>8);
	pkt[OFS_HDR_2+2]  = (patch_len & 0xFF);
	//-- write row with desc. for video stream --
	ofs = OFS_ENIGMA_TAB;
	pkt[ofs]   = EN_TYPE_VIDEO;
	pkt[ofs+1] = 0x02;
	pkt[ofs+2] = (vpid>>8);
	pkt[ofs+3] = (vpid & 0xFF);
	//-- for each audio stream, write row with desc. --
	ofs += SIZE_ENIGMA_TAB_ROW;
	for (i=0; i<nba; i++)
	{
		pkt[ofs]   = EN_TYPE_AUDIO;
		pkt[ofs+1] = 0x03;
		pkt[ofs+2] = (apid[i]>>8);
		pkt[ofs+3] = (apid[i] & 0xFF);
		pkt[ofs+4] = (atypes[i]==1)? 0x01 : 0x00;

		ofs += (SIZE_ENIGMA_TAB_ROW + 1);
	}
	//-- write row with desc. for pcr stream (eq. video) --
	pkt[ofs]   = EN_TYPE_PCR;
	pkt[ofs+1] = 0x02;
	pkt[ofs+2] = (pcrpid>>8);
	pkt[ofs+3] = (pcrpid & 0xFF);

	//-- calculate CRC --
	calc_crc32psi(&pkt[data_len], &pkt[OFS_HDR_2], data_len-OFS_HDR_2 );
	//-- write TS packet --
	write(fd, pkt, SIZE_TS_PKT);

	//-- (II) build PAT --
	data_len = COPY_TEMPLATE(pkt, pkt_pat);
// 	pkt[0xf]= 0xE0 | (pmtpid>>8);
// 	pkt[0x10] = pmtpid & 0xFF;
	//-- calculate CRC --
	calc_crc32psi(&pkt[data_len], &pkt[OFS_HDR_2], data_len-OFS_HDR_2 );
	//-- write TS packet --
	write(fd, pkt, SIZE_TS_PKT);

	//-- (III) build PMT --
	data_len = COPY_TEMPLATE(pkt, pkt_pmt);
	//-- adjust len dependent to count of audio streams --
	data_len += (SIZE_STREAM_TAB_ROW * (nba-1));
	if(vtxtpid){
		data_len += (SIZE_STREAM_TAB_ROW * (1))+10;//add teletext row length
	}
	if(nsub){
		data_len += ((SIZE_STREAM_TAB_ROW+10) * nsub);//add dvbsub row length
	}
	patch_len = data_len - OFS_HDR_2 + 1;
	pkt[OFS_HDR_2+1] |= (patch_len>>8);
	pkt[OFS_HDR_2+2]  = (patch_len & 0xFF);
	//-- patch pcr PID --
	ofs = OFS_PMT_DATA;
	pkt[ofs]  |= (pcrpid>>8);
	pkt[ofs+1] = (pcrpid & 0xFF);
	//-- write row with desc. for ES video stream --
	ofs = OFS_STREAM_TAB;
	pkt[ofs]   = vtype;
	pkt[ofs+1] = 0xE0 | (vpid>>8);
	pkt[ofs+2] = (vpid & 0xFF);
	pkt[ofs+3] = 0xF0;
	pkt[ofs+4] = 0x00;

	//-- for each ES audio stream, write row with desc. --
	for (i=0; i<nba; i++)
	{
		ofs += SIZE_STREAM_TAB_ROW;
		pkt[ofs]   = (atypes[i]==1)? ES_TYPE_AC3 : ES_TYPE_MPA;
		pkt[ofs+1] = 0xE0 | (apid[i]>>8);
		pkt[ofs+2] = (apid[i] & 0xFF);
		pkt[ofs+3] = 0xF0;
		pkt[ofs+4] = 0x00;
	}

	//teletext
	if(vtxtpid){
		ofs += SIZE_STREAM_TAB_ROW;
		pkt[ofs] = 0x06;		//teletext stream type;
		pkt[ofs+1] = 0xE0 | vtxtpid>>8;
		pkt[ofs+2] = vtxtpid&0xff;
		pkt[ofs+3] = 0xf0;
		pkt[ofs+4] = 0x0A;		// ES_info_length
		pkt[ofs+5] = 0x52;		//DVB-DescriptorTag: 82 (0x52)  [= stream_identifier_descriptor]
		pkt[ofs+6] = 0x01;		// descriptor_length
		pkt[ofs+7] = 0x03;		//component_tag
		pkt[ofs+8] = 0x56;		// DVB teletext tag
		pkt[ofs+9] = 0x05;		// descriptor length
		pkt[ofs+10] = vtxtlang[0];	//language code[0]
		pkt[ofs+11] = vtxtlang[1];	//language code[1]
		pkt[ofs+12] = vtxtlang[2];	//language code[2]
		pkt[ofs+13] = (/*descriptor_magazine_number*/ 0x01 & 0x06) | ((/*descriptor_type*/ 0x01 << 3) & 0xF8);
		pkt[ofs+14] = 0x00 ;		//Teletext_page_number
	}

	//dvbsub
	for (i=0; i<nsub; i++)
	{
		ofs += SIZE_STREAM_TAB_ROW;
		if(i > 0 || vtxtpid)
			ofs += 10;

		pkt[ofs]   = 0x06;//subtitle stream type;
		pkt[ofs+1] = 0xE0 | dvbsubpid[i]>>8;
		pkt[ofs+2] = dvbsubpid[i] & 0xFF;
		pkt[ofs+3] = 0xF0;
		pkt[ofs+4] = 0x0A;		// es info length
		pkt[ofs+5] = 0x59;		// DVB sub tag
		pkt[ofs+6] = 0x08;		// descriptor length
		pkt[ofs+7] = dvbsublang[i][0];	//language code[0]
		pkt[ofs+8] = dvbsublang[i][1];	//language code[1]
		pkt[ofs+9] = dvbsublang[i][2];	//language code[2]
		pkt[ofs+10] = 0x20;		//subtitle_stream.subtitling_type
		pkt[ofs+11] = 0x01>>8;		//composition_page_id
		pkt[ofs+12] = 0x01&0xff; 	//composition_page_id
		pkt[ofs+13] = 0x01>>8; 		//ancillary_page_id
		pkt[ofs+14] = 0x01&0xff;	//ancillary_page_id
	}

	//-- calculate CRC --
	calc_crc32psi(&pkt[data_len], &pkt[OFS_HDR_2], data_len-OFS_HDR_2 );
	//-- write TS packet --
	write(fd, pkt, SIZE_TS_PKT);

	//-- finish --
	vpid=0;
	pcrpid=0;
	nba=0;
	nsub = 0;
	vtxtpid = 0;
	fdatasync(fd);
	return 1;
}
