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
#define ES_TYPE_HEVC		0x24
#define ES_TYPE_MPA		0x03
#define ES_TYPE_EAC3		0x7a
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

CGenPsi::CGenPsi()
{
	nba = 0;
	vpid = 0;
	vtype = 0;

	pcrpid=0;
	vtxtpid = 0;
	memset(apid, 0, sizeof(apid));
	memset(atypes, 0, sizeof(atypes));
	nsub = 0;
	memset(dvbsubpid, 0, sizeof(dvbsubpid));
	neac3 = 0;
	memset(eac3_pid, 0, sizeof(eac3_pid));
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
		case EN_TYPE_HEVC:
			pcrpid=vpid=pid;
			vtype = ES_TYPE_HEVC;
			break;
		case EN_TYPE_PCR:
			pcrpid=pid;
			break;
		case EN_TYPE_AUDIO:
			apid[nba]=pid;
			atypes[nba]=isAC3;
			if(data != NULL){
				apid_lang[nba][0] = data[0];
				apid_lang[nba][1] = data[1];
				apid_lang[nba][2] = data[2];
			}else{
				apid_lang[nba][0] = 'u';
				apid_lang[nba][1] = 'n';
				apid_lang[nba][2] = 'k';
			}
			nba++;
			break;
		case EN_TYPE_TELTEX:
			vtxtpid = pid;
			if(data != NULL){
				vtxtlang[0] = data[0];
				vtxtlang[1] = data[1];
				vtxtlang[2] = data[2];
			}else{
				vtxtlang[0] = 'u';
				vtxtlang[1] = 'n';
				vtxtlang[2] = 'k';
			}
			break;
		case EN_TYPE_DVBSUB:
			dvbsubpid[nsub] = pid;
			if(data != NULL){
				dvbsublang[nsub][0] = data[0];
				dvbsublang[nsub][1] = data[1];
				dvbsublang[nsub][2] = data[2];
			}else{
				dvbsublang[nsub][0] = 'u';
				dvbsublang[nsub][1] = 'n';
				dvbsublang[nsub][2] = 'k';
			}
			nsub++;
			break;
		case EN_TYPE_AUDIO_EAC3:
			eac3_pid[neac3] = pid;
			if(data != NULL){
				eac3_lang[neac3][0] = data[0];
				eac3_lang[neac3][1] = data[1];
				eac3_lang[neac3][2] = data[2];
			}else{
				eac3_lang[neac3][0] = 'u';
				eac3_lang[neac3][1] = 'n';
				eac3_lang[neac3][2] = 'k';
			}
			neac3++;
			break;
		default:
			break;
	}
}

void CGenPsi::build_pat(uint8_t* buffer)
{	
	buffer[0x00] = 0x47;
	buffer[0x01] = 0x40;
	buffer[0x02] = 0x00; // PID = 0x0000
	buffer[0x03] = 0x10 ;

	buffer[0x04] = 0x00; // CRC calculation begins here
	buffer[0x05] = 0x00; // 0x00: Program association section
	buffer[0x06] = 0xb0;
	buffer[0x07] = 0x11; // section_length
	buffer[0x08] = 0x00;
	buffer[0x09] = 0xbb; // TS id = 0x00b0
	buffer[0x0a] = 0xc1;
	// section # and last section #
	buffer[0x0b] = buffer[0x0c] = 0x00;
	// Network PID (useless)
	buffer[0x0d] = buffer[0x0e] = 0x00;
	buffer[0x0f] = 0xe0;
	buffer[0x10] = 0x10;
	
	// Program Map PID	
	buffer[0x11] = 0x03;
	buffer[0x12] = 0xe8;
	buffer[0x13] = 0xe0;
	buffer[0x14] = pmt_pid;
	
	// Put CRC in buffer[0x15...0x18]
	calc_crc32psi(&buffer[0x15], &buffer[OFS_HDR_2], 0x15-OFS_HDR_2 );

	// needed stuffing bytes
	for (int i=0x19; i < 188; i++)
	{
		buffer[i]=0xff;
	}
}

void CGenPsi::build_pmt(uint8_t* buffer)
{
	int off=0;
	
	buffer[0x00] = 0x47;
	buffer[0x01] = 0x40;
	buffer[0x02] = pmt_pid;
	buffer[0x03] = 0x10;
	buffer[0x04] = 0x00; // CRC calculation begins here
	buffer[0x05] = 0x02; // 0x02: Program map section
	buffer[0x06] = 0xb0;
	buffer[0x07] = 0x20; // section_length
	buffer[0x08] = 0x03;
	buffer[0x09] = 0xe8; // prog number
	buffer[0x0a] = 0xc1;
	// section # and last section #
	buffer[0x0b] = buffer[0x0c] = 0x00;
	// Program Clock Reference (PCR) PID
	buffer[0x0d] = pcrpid>>8;
	buffer[0x0e] = pcrpid&0xff;
	// program_info_length == 0
	buffer[0x0f] = 0xf0;
	buffer[0x10] = 0x00;
	// Video PID
	buffer[0x11] = vtype; // video stream type
	buffer[0x12] = vpid>>8;
	buffer[0x13] = vpid&0xff;
	buffer[0x14] = 0xf0;
	buffer[0x15] = 0x09; // es info length
	// useless info
	buffer[0x16] = 0x07;
	buffer[0x17] = 0x04;
	buffer[0x18] = 0x08;
	buffer[0x19] = 0x80;
	buffer[0x1a] = 0x24;
	buffer[0x1b] = 0x02;
	buffer[0x1c] = 0x11;
	buffer[0x1d] = 0x01;
	buffer[0x1e] = 0xfe;
	off = 0x1e;

	// Audio streams
	for (int index = 0; index < nba && index<10; index++)
	{
		buffer[++off] = (atypes[index]==1)? ES_TYPE_AC3 : ES_TYPE_MPA;
		buffer[++off] = apid[index]>>8;
		buffer[++off] = apid[index]&0xff;

		if (atypes[index] == 1)//ES_TYPE_AC3
		{
			buffer[++off] = 0xf0;
			buffer[++off] = 0x0c; // es info length
			buffer[++off] = 0x05;
			buffer[++off] = 0x04;
			buffer[++off] = 0x41;
			buffer[++off] = 0x43;
			buffer[++off] = 0x2d;
			buffer[++off] = 0x33;
		}
		else
		{
			buffer[++off] = 0xf0;
			buffer[++off] = 0x06; // es info length
		}
		buffer[++off] = 0x0a; // iso639 descriptor tag
		buffer[++off] = 0x04; // descriptor length
		buffer[++off] = apid_lang[index][0];
		buffer[++off] = apid_lang[index][1];
		buffer[++off] = apid_lang[index][2];
		buffer[++off] = 0x00; // audio type
	}
	// eac3 audio
	for (int index=0; index<neac3 && index<10; index++)
	{
		buffer[++off] = 0x06;//pes private type;
		buffer[++off] = 0xE0 | eac3_pid[index]>>8;
		buffer[++off] = eac3_pid[index] & 0xFF;
		buffer[++off] = 0xF0;
		buffer[++off] = 0x0d;		// es info length
		buffer[++off] = 0x52;
		buffer[++off] = 0x01;
		buffer[++off] = 0x5d;
		buffer[++off] = 0x0a;		// iso639 descriptor tag
		buffer[++off] = 0x04;		// descriptor length
		buffer[++off] = eac3_lang[index][0];	//language code[0]
		buffer[++off] = eac3_lang[index][1];	//language code[1]
		buffer[++off] = eac3_lang[index][2];	//language code[2]
		buffer[++off] = 0x01;
		buffer[++off] = 0x7a;		//enhanced_AC-3_descriptor
		buffer[++off] = 0x02;
		buffer[++off] = 0x80;
		buffer[++off] = 0xc5;
	}

	// Subtitle streams
	for (int index = 0; index < nsub && index<10; index++)
	{
		buffer[++off] = 0x06;//pes private type;
		buffer[++off] = dvbsubpid[index]>>8;
		buffer[++off] = dvbsubpid[index]&0xff;
		buffer[++off] = 0xf0;
		buffer[++off] = 0x0a; // es info length
		buffer[++off] = 0x59; // DVB sub tag
		buffer[++off] = 0x08; // descriptor length
		buffer[++off] = dvbsublang[index][0];
		buffer[++off] = dvbsublang[index][1];
		buffer[++off] = dvbsublang[index][2];
		buffer[++off] = 0x20;		//subtitle_stream.subtitling_type
		buffer[++off] = 0x01>>8;		//composition_page_id
		buffer[++off] = 0x01&0xff; 	//composition_page_id
		buffer[++off] = 0x01>>8; 		//ancillary_page_id
		buffer[++off] = 0x01&0xff;	//ancillary_page_id
	}
	
	// TeleText streams
	if(vtxtpid){
		buffer[++off] = 0x06;		//teletext stream type;
		buffer[++off] = 0xE0 | vtxtpid>>8;
		buffer[++off] = vtxtpid&0xff;
		buffer[++off] = 0xf0;
		buffer[++off] = 0x0A;		// ES_info_length
		buffer[++off] = 0x52;		//DVB-DescriptorTag: 82 (0x52)  [= stream_identifier_descriptor]
		buffer[++off] = 0x01;		// descriptor_length
		buffer[++off] = 0x03;		//component_tag
		buffer[++off] = 0x56;		// DVB teletext tag
		buffer[++off] = 0x05;		// descriptor length
		buffer[++off] = vtxtlang[0];	//language code[0]
		buffer[++off] = vtxtlang[1];	//language code[1]
		buffer[++off] = vtxtlang[2];	//language code[2]
		buffer[++off] = (/*descriptor_magazine_number*/ 0x01 & 0x06) | ((/*descriptor_type*/ 0x01 << 3) & 0xF8);
		buffer[++off] = 0x00 ;		//Teletext_page_number
	}
	buffer[0x07] = off-3; // update section_length

	// Put CRC in ts[0x29...0x2c]
	calc_crc32psi(&buffer[off+1], &buffer[OFS_HDR_2], off+1-OFS_HDR_2 );

	// needed stuffing bytes
	for (int i=off+5 ; i < 188 ; i++)
	{
		buffer[i] = 0xff;
	}
}

int CGenPsi::genpsi(int fd)
{
	uint8_t   buffer[SIZE_TS_PKT];

	build_pat(buffer);
	write(fd, buffer, SIZE_TS_PKT);

	build_pmt(buffer);
	write(fd, buffer, SIZE_TS_PKT);

	//-- finish --
	vpid=0;
	pcrpid=0;
	nba=0;
	nsub = 0;
	vtxtpid = 0;
	neac3 = 0;
	fdatasync(fd);
	return 1;

}

