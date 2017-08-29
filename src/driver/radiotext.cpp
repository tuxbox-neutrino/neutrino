/*
	$Id: radiotext.cpp,v 1.7 2009/10/31 10:11:02 seife Exp $
	
	Neutrino-GUI  -   DBoxII-Project

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA

	ripped from:
*/

/*
 * radioaudio.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * This is a "plugin" for the Video Disk Recorder (VDR).
 *
 * Written by:                  Lars Tegeler <email@host.dom>
 *
 * Project's homepage:          www.math.uni-paderborn.de/~tegeler/vdr
 *
 * Latest version available at: URL
 *
 * See the file COPYING for license information.
 *
 * Description:
 *
 * This Plugin display an background image while the vdr is switcht to radio channels.
 *
 
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <malloc.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <ctype.h>
#include <config.h>

#if 0
#ifdef HAVE_TRIPLEDRAGON
#include <zapit/td-demux-compat.h>
#include <tddevices.h>
#define DMXDEV	"/dev/" DEVICE_NAME_DEMUX "1"
#elif HAVE_DVB_API_VERSION < 3
#include <ost/dmx.h>
#define DMXDEV  "/dev/dvb/card0/demux0"
#define DVRDEV  "/dev/dvb/card0/dvr0"
#define dmx_pes_filter_params   dmxPesFilterParams
#define pes_type                pesType
#else
#include <linux/dvb/dmx.h>
#define DMXDEV  "/dev/dvb/adapter0/demux0"
#define DVRDEV  "/dev/dvb/adapter0/dvr0"
#endif
#endif

#include <global.h>
#include <system/settings.h>
#include <system/set_threadname.h>
#include <neutrino.h>
#include <gui/color.h>

#include "radiotext.h"
#include "radiotools.h"

rtp_classes rtp_content;

// RDS rest
bool RDS_PSShow = false;
int RDS_PSIndex = 0;
char RDS_PSText[12][9];

// plugin audiorecorder service
bool ARec_Receive = false, ARec_Record = false;

#if ENABLE_RASS
// ... Gallery (1..999)
#define RASS_GALMAX 999
bool Rass_Gallery[RASS_GALMAX+1];
int Rass_GalStart, Rass_GalEnd, Rass_GalCount, Rass_SlideFoto;
#endif

#define floor
const char *DataDir = "./";
//cRadioAudio *RadioAudio;
//cRadioTextOsd *RadioTextOsd;
//cRDSReceiver *RDSReceiver;

// RDS-Chartranslation: 0x80..0xff
unsigned char rds_addchar[128] = {
    0xe1, 0xe0, 0xe9, 0xe8, 0xed, 0xec, 0xf3, 0xf2, 
    0xfa, 0xf9, 0xd1, 0xc7, 0x8c, 0xdf, 0x8e, 0x8f, 
    0xe2, 0xe4, 0xea, 0xeb, 0xee, 0xef, 0xf4, 0xf6, 
    0xfb, 0xfc, 0xf1, 0xe7, 0x9c, 0x9d, 0x9e, 0x9f, 
    0xaa, 0xa1, 0xa9, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 
    0xa8, 0xa9, 0xa3, 0xab, 0xac, 0xad, 0xae, 0xaf, 
    0xba, 0xb9, 0xb2, 0xb3, 0xb1, 0xa1, 0xb6, 0xb7, 
    0xb5, 0xbf, 0xf7, 0xb0, 0xbc, 0xbd, 0xbe, 0xa7, 
    0xc1, 0xc0, 0xc9, 0xc8, 0xcd, 0xcc, 0xd3, 0xd2, 
    0xda, 0xd9, 0xca, 0xcb, 0xcc, 0xcd, 0xd0, 0xcf, 
    0xc2, 0xc4, 0xca, 0xcb, 0xce, 0xcf, 0xd4, 0xd6, 
    0xdb, 0xdc, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 
    0xc3, 0xc5, 0xc6, 0xe3, 0xe4, 0xdd, 0xd5, 0xd8, 
    0xde, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xf0,
    0xe3, 0xe5, 0xe6, 0xf3, 0xf4, 0xfd, 0xf5, 0xf8, 
    0xfe, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

static const char *entitystr[5]  = { "&apos;", "&amp;", "&quote;", "&gt", "&lt" };
static const char *entitychar[5] = { "'", "&", "\"", ">", "<" };

char *CRadioText::rds_entitychar(char *text)
{
	int i = 0, l, lof, lre, space;
	char *temp;

	while (i < 5) {
		if ((temp = strstr(text, entitystr[i])) != NULL) {
			if (S_Verbose >= 2)
				printf("RText-Entity: %s\n", text);
			l = strlen(entitystr[i]);
			lof = (temp-text);
			if (strlen(text) < RT_MEL) {
				lre = strlen(text) - lof - l;
				space = 1;
			}
			else {
				lre =  RT_MEL - 1 - lof - l;
				space = 0;
			}
			memmove(text+lof, entitychar[i], 1);
			memmove(text+lof+1, temp+l, lre);
			if (space != 0)
				memmove(text+lof+1+lre, "         ", l-1);
		}
		else i++;
	}
	return text;
}

char* CRadioText::ptynr2string(int nr)
{
	switch (nr) {
        // Source: http://www.ebu.ch/trev_255-beale.pdf
		case  0: return tr(const_cast<char *>("unknown program type"));
		case  1: return tr(const_cast<char *>("News")); 
		case  2: return tr(const_cast<char *>("Current affairs")); 
		case  3: return tr(const_cast<char *>("Information")); 
		case  4: return tr(const_cast<char *>("Sport")); 
		case  5: return tr(const_cast<char *>("Education")); 
		case  6: return tr(const_cast<char *>("Drama")); 
		case  7: return tr(const_cast<char *>("Culture")); 
		case  8: return tr(const_cast<char *>("Science")); 
		case  9: return tr(const_cast<char *>("Varied")); 
		case 10: return tr(const_cast<char *>("Pop music")); 
		case 11: return tr(const_cast<char *>("Rock music")); 
		case 12: return tr(const_cast<char *>("M.O.R. music")); 
		case 13: return tr(const_cast<char *>("Light classical")); 
		case 14: return tr(const_cast<char *>("Serious classical")); 
		case 15: return tr(const_cast<char *>("Other music")); 
		// 16-30 "Spares"
		case 31: return tr(const_cast<char *>("Alarm"));
		default: return const_cast<char *>("?");
	}
}

bool CRadioText::DividePes(unsigned char *data, int length, int *substart, int *subend)
{
	int i = *substart;
	int found = 0;
	while ((i<length-2) && (found < 2))
	{
		if ((found == 0) && (data[i] == 0xFF) && (data[i+1] == 0xFD)) {
			*substart = i;
			found++;
		}
		else if ((found == 1) && (data[i] == 0xFD) && (data[i+1] == 0xFF)) {
			*subend = i;
			found++;
		}
		i++;
	}
	if ((found == 1) && (data[length-1] == 0xFD)) {
		*subend = length-1;
		found++;
	}
	if (found == 2) {
		return(true);
	} else {
		return(false);
	}
}

int CRadioText::PES_Receive(unsigned char *data, int len)
{
	const int mframel = 263;  // max. 255(MSG)+4(ADD/SQC/MFL)+2(CRC)+2(Start/Stop) of RDS-data
	static unsigned char mtext[mframel+1];
	static bool rt_start = false, rt_bstuff=false;
	static int index;
	static int mec = 0;

	int offset = 0;

	while (true) 
	{
		if (len < offset + 6)
			return offset;
		int pesl = (data[offset + 4] << 8) + data[offset + 5] + 6;
		if (pesl <= 0 || offset + pesl > len)
			return offset;
		offset += pesl;

		/* try to find subpackets in the pes stream */
		int substart = 0;
		int subend = 0;
		while (DividePes(&data[0], pesl, &substart, &subend))
		{
			int inner_offset = subend + 1;
if (inner_offset < 3) fprintf(stderr, "RT %s: inner_offset < 3 (%d)\n", __FUNCTION__, inner_offset);
			int rdsl = data[subend - 1];	// RDS DataFieldLength
			// RDS DataSync = 0xfd @ end
			if (data[subend] == 0xfd && rdsl > 0) {
				// print RawData with RDS-Info
				if (S_Verbose >= 3) {
					printf("\n\nPES-Data(%d/%d): ", pesl, len);
					for (int a=inner_offset -rdsl; a<offset; a++)
						printf("%02x ", data[a]);
					printf("(End)\n\n");
				}

if (subend-2-rdsl < 0) fprintf(stderr, "RT %s: start: %d subend-2-rdsl < 0 (%d-2-%d)\n", __FUNCTION__, substart,subend,rdsl);
				for (int i = subend - 2, val; i > subend - 2 - rdsl; i--) { // <-- data reverse, from end to start
if (i < 0) { fprintf(stderr, "RT %s: i < 0 (%d)\n", __FUNCTION__, i); break; }
					val = data[i];

					if (val == 0xfe) {	// Start
						index = -1;
						rt_start = true;
						rt_bstuff = false;
						if (S_Verbose >= 2)
							printf("RDS-Start: ");
					}

					if (rt_start) {
						if (S_Verbose >= 3)
							printf("%02x ", val);
						// byte-stuffing reverse: 0xfd00->0xfd, 0xfd01->0xfe, 0xfd02->0xff
						if (rt_bstuff) {
							switch (val) {
								case 0x00: mtext[index] = 0xfd; break;
								case 0x01: mtext[index] = 0xfe; break;
								case 0x02: mtext[index] = 0xff; break;
								default: mtext[++index] = val;	// should never be
							}
							rt_bstuff = false;
							if (S_Verbose >= 3)
								printf("(Bytestuffing -> %02x) ", mtext[index]);
						}
						else
							mtext[++index] = val;
						if (val == 0xfd && index > 0)	// stuffing found
							rt_bstuff = true;
						// early check for used MEC
						if (index == 5) {
							//mec = val;
							switch (val) {
								case 0x0a:			// RT
									have_radiotext = true;
								case 0x46:			// RTplus-Tags
								case 0xda:			// RASS
								case 0x07:			// PTY
								case 0x3e:			// PTYN
								case 0x02:			// PS
									mec = val;
									break;
								default:
									rt_start = false;
									if (S_Verbose >= 2)
										printf("(RDS-MEC '%02x' not used -> End)\n", val);
							}
						}
						if (index >= mframel) {		// max. rdslength, garbage ?
							if (S_Verbose >= 1)
								printf("RDS-Error: too long, garbage ?\n");
							rt_start = false;
						}
					}

					if (rt_start && val == 0xff) {	// End
						if (S_Verbose >= 2)
							printf("(RDS-End)\n");
						rt_start = false;
						if (index < 9) {		//  min. rdslength, garbage ?
							if (S_Verbose >= 1)
								printf("RDS-Error: too short -> garbage ?\n");
						}
						else {
							// crc16-check
							unsigned short crc16 = crc16_ccitt(mtext, index-3, true);
							if (crc16 != (mtext[index-2]<<8)+mtext[index-1]) {
							    if (S_Verbose >= 1)
								printf("RDS-Error: wrong CRC # calc = %04x <> transmit = %02x%02x\n", crc16, mtext[index-2], mtext[index-1]);
							} else {

							switch (mec) {
								case 0x0a:
								case 0x46:
									if (S_Verbose >= 2)
										printf("(RDS-MEC '%02x') -> RadiotextDecode - %d\n", mec, index);
									RadiotextDecode(mtext, index);		// Radiotext, RT+
									break;
								case 0x07:  RT_PTY = mtext[8];			// PTY
									RT_MsgShow = true;
									if (S_Verbose >= 1)
										printf("RDS-PTY set to '%s'\n", ptynr2string(RT_PTY));
									break;
								case 0x3e:
									if (S_Verbose >= 2)
										printf("(RDS-MEC '%02x') -> RDS_PsPtynDecode - %d\n", mec, index);
									RDS_PsPtynDecode(true, mtext, index);	// PTYN
									break;
								case 0x02:
									if (S_Verbose >= 2)
										printf("(RDS-MEC '%02x') -> RDS_PsPtynDecode - %d\n", mec, index);
									RDS_PsPtynDecode(false, mtext, index);	// PS
									break;
								case 0xda:
#if ENABLE_RASS
									RassDecode(mtext, index);		// Rass
#endif
									break;
								}
							}
						}
					}
				}
			}
			substart = subend;
		}
	}
}


void CRadioText::RadiotextDecode(unsigned char *mtext, int len)
{
	static bool rtp_itoggle = false;
	static int rtp_idiffs = 0;
	static cTimeMs rtp_itime;
	static char plustext[RT_MEL];

	// byte 1+2 = ADD (10bit SiteAdress + 6bit EncoderAdress)
	// byte 3   = SQC (Sequence Counter 0x00 = not used)
	int leninfo = mtext[4];	// byte 4 = MFL (Message Field Length)
	if (len >= leninfo+7) {	// check complete length

		// byte 5 = MEC (Message Element Code, 0x0a for RT, 0x46 for RTplus)
		if (mtext[5] == 0x0a) {
		// byte 6+7 = DSN+PSN (DataSetNumber+ProgramServiceNumber, 
		//		       	   ignore here, always 0x00 ?)
		// byte 8   = MEL (MessageElementLength, max. 64+1 byte @ RT)
		if (mtext[8] == 0 || mtext[8] > RT_MEL || mtext[8] > leninfo-4) {
			if (S_Verbose >= 1)
			printf("RT-Error: Length = 0 or not correct !");
			return;
		}
		// byte 9 = RT-Status bitcodet (0=AB-flagcontrol, 1-4=Transmission-Number, 5+6=Buffer-Config,
		//				    ingnored, always 0x01 ?)
fprintf(stderr, "MEC=0x%02x DSN=0x%02x PSN=0x%02x MEL=%02d STATUS=0x%02x MFL=%02d\n", mtext[5], mtext[6], mtext[7], mtext[8], mtext[9], mtext[4]);
		char temptext[RT_MEL];
		memset(temptext, 0x20, RT_MEL-1);
		temptext[RT_MEL - 1] = '\0';
		for (int i = 1, ii = 0; i < mtext[8]; i++) {
			if (mtext[9+i] <= 0xfe)
			// additional rds-character, see RBDS-Standard, Annex E
				temptext[ii++] = (mtext[9+i] >= 0x80) ? rds_addchar[mtext[9+i]-0x80] : mtext[9+i];
		}
		memcpy(plustext, temptext, RT_MEL);
		rds_entitychar(temptext);
		// check repeats
		bool repeat = false;
		for (int ind = 0; ind < S_RtOsdRows; ind++) {
			if (memcmp(RT_Text[ind], temptext, RT_MEL-1) == 0) {
				repeat = true;
				if (S_Verbose >= 1)
					printf("RText-Rep[%d]: %s\n", ind, RT_Text[ind]);
			}
		}
		if (!repeat) {
			memcpy(RT_Text[RT_Index], temptext, RT_MEL);
			// +Memory
			char *temp;
			asprintf(&temp, "%s", RT_Text[RT_Index]);
		if (++rtp_content.rt_Index >= 2*MAX_RTPC)
		    rtp_content.rt_Index = 0;
		asprintf(&rtp_content.radiotext[rtp_content.rt_Index], "%s", rtrim(temp));
		free(temp);
		if (S_Verbose >= 1)
		    printf("Radiotext[%d]: %s\n", RT_Index, RT_Text[RT_Index]);
		RT_Index +=1; if (RT_Index >= S_RtOsdRows) RT_Index = 0;
		}
		RTP_TToggle = 0x03;		// Bit 0/1 = Title/Artist
		RT_MsgShow = true;
		S_RtOsd = 1;
		RT_Info = (RT_Info > 0) ? RT_Info : 1;
		RadioStatusMsg();
	}

	else if (RTP_TToggle > 0 && mtext[5] == 0x46 && S_RtFunc >= 2) {	// RTplus tags V2.0, only if RT
		if (mtext[6] > leninfo-2 || mtext[6] != 8) { // byte 6 = MEL, only 8 byte for 2 tags
			if (S_Verbose >= 1)
				printf("RTp-Error: Length not correct !");
			return;
		}

		uint rtp_typ[2], rtp_start[2], rtp_len[2];
		// byte 7+8 = ApplicationID, always 0x4bd7
		// byte 9   = Applicationgroup Typecode / PTY ?
		// bit 10#4 = Item Togglebit
		// bit 10#3 = Item Runningbit
		// Tag1: bit 10#2..11#5 = Contenttype, 11#4..12#7 = Startmarker, 12#6..12#1 = Length
		rtp_typ[0]   = (0x38 & mtext[10]<<3) | mtext[11]>>5;
		rtp_start[0] = (0x3e & mtext[11]<<1) | mtext[12]>>7;
		rtp_len[0]   = 0x3f & mtext[12]>>1;
		// Tag2: bit 12#0..13#3 = Contenttype, 13#2..14#5 = Startmarker, 14#4..14#0 = Length(5bit)
		rtp_typ[1]   = (0x20 & mtext[12]<<5) | mtext[13]>>3;
		rtp_start[1] = (0x38 & mtext[13]<<3) | mtext[14]>>5;
		rtp_len[1]   = 0x1f & mtext[14];
		if (S_Verbose >= 2)
			printf("RTplus (tag=Typ/Start/Len):  Toggle/Run = %d/%d, tag#1 = %d/%d/%d, tag#2 = %d/%d/%d\n", 
				(mtext[10]&0x10)>0, (mtext[10]&0x08)>0, rtp_typ[0], rtp_start[0], rtp_len[0], rtp_typ[1], rtp_start[1], rtp_len[1]);
		// save info
		for (int i = 0; i < 2; i++) {
			if (rtp_start[i]+rtp_len[i]+1 >= RT_MEL) {	// length-error
				if (S_Verbose >= 1)
					printf("RTp-Error (tag#%d = Typ/Start/Len): %d/%d/%d (Start+Length > 'RT-MEL' !)\n",
						i+1, rtp_typ[i], rtp_start[i], rtp_len[i]);
			} else {
				char temptext[RT_MEL];
				memset(temptext, 0x20, RT_MEL-1);
				memmove(temptext, plustext+rtp_start[i], rtp_len[i]+1);
				rds_entitychar(temptext);
				// +Memory
				memset(rtp_content.temptext, 0x20, RT_MEL-1);
				memcpy(rtp_content.temptext, temptext, RT_MEL-1);
				switch (rtp_typ[i]) {
				case 1:		// Item-Title	
					if ((mtext[10] & 0x08) > 0 && (RTP_TToggle & 0x01) == 0x01) {
					RTP_TToggle -= 0x01;
					RT_Info = 2;
					if (memcmp(RTP_Title, temptext, RT_MEL-1) != 0 || (mtext[10] & 0x10) != RTP_ItemToggle) {
						memcpy(RTP_Title, temptext, RT_MEL-1);
						if (RT_PlusShow && rtp_itime.Elapsed() > 1000)
							rtp_idiffs = (int) rtp_itime.Elapsed()/1000;
						if (!rtp_content.item_New) {
							RTP_Starttime = time(NULL);
							rtp_itime.Set(0);
							sprintf(RTP_Artist, "---");
							if (++rtp_content.item_Index >= MAX_RTPC)
							rtp_content.item_Index = 0;
							rtp_content.item_Start[rtp_content.item_Index] = time(NULL);	// todo: replay-mode
							rtp_content.item_Artist[rtp_content.item_Index] = NULL;
						}
						rtp_content.item_New = (!rtp_content.item_New) ? true : false;
						if (rtp_content.item_Index >= 0)
						    asprintf(&rtp_content.item_Title[rtp_content.item_Index], "%s", rtrim(rtp_content.temptext));
						RT_PlusShow = RT_MsgShow = rtp_itoggle = true;
						}
					}
					break;
				case 4:		// Item-Artist	
					if ((mtext[10] & 0x08) > 0 && (RTP_TToggle & 0x02) == 0x02) {
						RTP_TToggle -= 0x02;
						RT_Info = 2;
						if (memcmp(RTP_Artist, temptext, RT_MEL-1) != 0 || (mtext[10] & 0x10) != RTP_ItemToggle) {
							memcpy(RTP_Artist, temptext, RT_MEL-1);
							if (RT_PlusShow && rtp_itime.Elapsed() > 1000)
								rtp_idiffs = (int) rtp_itime.Elapsed()/1000;
							if (!rtp_content.item_New) {
								RTP_Starttime = time(NULL);
								rtp_itime.Set(0);
								sprintf(RTP_Title, "---");
								if (++rtp_content.item_Index >= MAX_RTPC)
									rtp_content.item_Index = 0;
								rtp_content.item_Start[rtp_content.item_Index] = time(NULL);	// todo: replay-mode
								rtp_content.item_Title[rtp_content.item_Index] = NULL;
							}
							rtp_content.item_New = (!rtp_content.item_New) ? true : false;
							if (rtp_content.item_Index >= 0)
								asprintf(&rtp_content.item_Artist[rtp_content.item_Index], "%s", rtrim(rtp_content.temptext));
							RT_PlusShow = RT_MsgShow = rtp_itoggle = true;
						}
					}
					break;
				case 12:	// Info_News
					asprintf(&rtp_content.info_News, "%s", rtrim(rtp_content.temptext));
					break;
				case 13:	// Info_NewsLocal
					asprintf(&rtp_content.info_NewsLocal, "%s", rtrim(rtp_content.temptext));
					break;
				case 14:	// Info_Stockmarket
					if (++rtp_content.info_StockIndex >= MAX_RTPC)
						    rtp_content.info_StockIndex = 0;
					asprintf(&rtp_content.info_Stock[rtp_content.info_StockIndex], "%s", rtrim(rtp_content.temptext));
					break;
				case 15:	// Info_Sport
					if (++rtp_content.info_SportIndex >= MAX_RTPC)
						    rtp_content.info_SportIndex = 0;
					asprintf(&rtp_content.info_Sport[rtp_content.info_SportIndex], "%s", rtrim(rtp_content.temptext));
					break;
				case 16:	// Info_Lottery
					if (++rtp_content.info_LotteryIndex >= MAX_RTPC)
					    rtp_content.info_LotteryIndex = 0;
					asprintf(&rtp_content.info_Lottery[rtp_content.info_LotteryIndex], "%s", rtrim(rtp_content.temptext));
					break;
				case 24:	// Info_DateTime
					asprintf(&rtp_content.info_DateTime, "%s", rtrim(rtp_content.temptext));
					break;
				case 25:	// Info_Weather
					if (++rtp_content.info_WeatherIndex >= MAX_RTPC)
						    rtp_content.info_WeatherIndex = 0;
					asprintf(&rtp_content.info_Weather[rtp_content.info_WeatherIndex], "%s", rtrim(rtp_content.temptext));
					break;
				case 26:	// Info_Traffic
					asprintf(&rtp_content.info_Traffic, "%s", rtrim(rtp_content.temptext));
					break;
				case 27:	// Info_Alarm
					asprintf(&rtp_content.info_Alarm, "%s", rtrim(rtp_content.temptext));
					break;
				case 28:	// Info_Advert
					asprintf(&rtp_content.info_Advert, "%s", rtrim(rtp_content.temptext));
					break;
				case 29:	// Info_Url
					asprintf(&rtp_content.info_Url, "%s", rtrim(rtp_content.temptext));
					break;
				case 30:	// Info_Other
					if (++rtp_content.info_OtherIndex >= MAX_RTPC)
						    rtp_content.info_OtherIndex = 0;
					asprintf(&rtp_content.info_Other[rtp_content.info_OtherIndex], "%s", rtrim(rtp_content.temptext));
					break;
				case 31:	// Programme_Stationname.Long
					asprintf(&rtp_content.prog_Station, "%s", rtrim(rtp_content.temptext));
					break;
				case 32:	// Programme_Now
					asprintf(&rtp_content.prog_Now, "%s", rtrim(rtp_content.temptext));
					break;
				case 33:	// Programme_Next
					asprintf(&rtp_content.prog_Next, "%s", rtrim(rtp_content.temptext));
					break;
				case 34:	// Programme_Part
					asprintf(&rtp_content.prog_Part, "%s", rtrim(rtp_content.temptext));
					break;
				case 35:	// Programme_Host
					asprintf(&rtp_content.prog_Host, "%s", rtrim(rtp_content.temptext));
					break;
				case 36:	// Programme_EditorialStaff
					asprintf(&rtp_content.prog_EditStaff, "%s", rtrim(rtp_content.temptext));
					break;
				case 38:	// Programme_Homepage
					asprintf(&rtp_content.prog_Homepage, "%s", rtrim(rtp_content.temptext));
					break;
				case 39:	// Phone_Hotline
					asprintf(&rtp_content.phone_Hotline, "%s", rtrim(rtp_content.temptext));
					break;
				case 40:	// Phone_Studio
					asprintf(&rtp_content.phone_Studio, "%s", rtrim(rtp_content.temptext));
					break;
				case 44:	// Email_Hotline
					asprintf(&rtp_content.email_Hotline, "%s", rtrim(rtp_content.temptext));
					break;
				case 45:	// Email_Studio
					asprintf(&rtp_content.email_Studio, "%s", rtrim(rtp_content.temptext));
					break;
				}
			}
		}

		// Title-end @ no Item-Running'
		if ((mtext[10] & 0x08) == 0) {
			sprintf(RTP_Title, "---");
			sprintf(RTP_Artist, "---");
			if (RT_PlusShow) {
				RT_PlusShow = false;
				rtp_itoggle = true;
				rtp_idiffs = (int) rtp_itime.Elapsed()/1000;
				RTP_Starttime = time(NULL);
			}
			RT_MsgShow = (RT_Info > 0);
			rtp_content.item_New = false;
		}

		if (rtp_itoggle) {
			if (S_Verbose >= 1) {
				struct tm tm_store;
				struct tm *ts = localtime_r(&RTP_Starttime, &tm_store);
				if (rtp_idiffs > 0)
					printf("  StartTime : %02d:%02d:%02d  (last Title elapsed = %d s)\n",
						ts->tm_hour, ts->tm_min, ts->tm_sec, rtp_idiffs);
				else
					printf("  StartTime : %02d:%02d:%02d\n", ts->tm_hour, ts->tm_min, ts->tm_sec);
				printf("  RTp-Title : %s\n  RTp-Artist: %s\n", RTP_Title, RTP_Artist);
			}
			RTP_ItemToggle = mtext[10] & 0x10;
			rtp_itoggle = false;
			rtp_idiffs = 0;
			RadioStatusMsg();
//		AudioRecorderService();
			}
			RTP_TToggle = 0;
		}
	}
	else {
		if (S_Verbose >= 1)
			printf("RDS-Error: [RTDecode] Length not correct !\n");
	}
}

void CRadioText::RDS_PsPtynDecode(bool ptyn, unsigned char *mtext, int len)
{
	if (len < 16) return;

	// decode Text
	for (int i = 8; i <= 15; i++) {
		if (mtext[i] <= 0xfe) {
			// additional rds-character, see RBDS-Standard, Annex E
			if (!ptyn)
				RDS_PSText[RDS_PSIndex][i-8] = (mtext[i] >= 0x80) ? rds_addchar[mtext[i]-0x80] : mtext[i];
			else
				RDS_PTYN[i-8] = (mtext[i] >= 0x80) ? rds_addchar[mtext[i]-0x80] : mtext[i];
		}
	}

	if (S_Verbose >= 1) {
		if (!ptyn)
			printf("RDS-PS  No= %d, Content[%d]= '%s'\n", mtext[7], RDS_PSIndex, RDS_PSText[RDS_PSIndex]);
		else
			printf("RDS-PTYN  No= %d, Content= '%s'\n", mtext[7], RDS_PTYN);
	}

	if (!ptyn) {
		RDS_PSIndex += 1; if (RDS_PSIndex >= 12) RDS_PSIndex = 0;
		RT_MsgShow = RDS_PSShow = true;
	}
}

void CRadioText::RadioStatusMsg(void)
{
	/* announce text/items for lcdproc & other */
	if (!RT_MsgShow || S_RtMsgItems <= 0)
		return;
	
	if (S_RtMsgItems >= 2) {
		char temp[100];
		int ind = (RT_Index == 0) ? S_RtOsdRows - 1 : RT_Index - 1;
		strcpy(temp, RT_Text[ind]);
		printf("RadioStatusMsg = %s\n", temp);
//		cStatus::MsgOsdTextItem(rtrim(temp), false);
	}

	if ((S_RtMsgItems == 1 || S_RtMsgItems >= 3) && ((S_RtOsdTags == 1 && RT_PlusShow) || S_RtOsdTags >= 2)) {
//		struct tm tm_store;
//		struct tm *ts = localtime_r(&RTP_Starttime, &tm_store);
//		cStatus::MsgOsdProgramme(mktime(ts), RTP_Title, RTP_Artist, 0, NULL, NULL);
		printf("RTP_Title = %s, RTP_Artist = %s\n", RTP_Title, RTP_Artist);
	}
}


#if ENABLE_RASS
// add <names> of DVB Radio Slides Specification 1.0, 20061228
void CRadioText::RassDecode(unsigned char *mtext, int len)
{
	static uint splfd = 0, spmax = 0, index = 0;
	static uint afiles, slidenumr, slideelem, filemax, fileoffp;
	static int filetype, fileoffb;
	static bool slideshow = false, slidesave = false, slidecan = false, slidedel = false, start = false;
	static uchar daten[65536];	// mpegs-stills defined <= 50kB
	FILE *fd;

	// byte 1+2 = ADD (10bit SiteAdress + 6bit EncoderAdress)
	// byte 3   = SQC (Sequence Counter 0x00 = not used)
	// byte 4   = MFL (Message Field Length), 
	if (len >= mtext[4]+7) {	// check complete length
		// byte 5   = MEC (0xda for Rass)
		// byte 6   = MEL
		if (mtext[6] == 0 || mtext[6] > mtext[4]-2) {
			if ((S_Verbose && 0x0f) >= 1)
				printf("Rass-Error: Length = 0 or not correct !\n");
			return;
		}
		// byte 7+8   = Service-ID zugehöriger Datenkanal
		// byte 9-11  = Nummer aktuelles Paket, <PNR>
		uint plfd = mtext[11] + mtext[10]*256 + mtext[9]*65536;
		// byte 12-14 = Anzahl Pakete, <NOP>
		uint pmax = mtext[14] + mtext[13]*256 + mtext[12]*65536;

		// byte 15+16 = Rass-Kennung = Header, <Rass-STA>
		if (mtext[15] == 0x40 && mtext[16] == 0xda) {		// first
			// byte 17+18 = Anzahl Dateien im Archiv, <NOI>
			afiles = mtext[18] + mtext[17]*256;
			// byte 19+20 = Slide-Nummer, <Rass-ID>
			slidenumr = mtext[20] + mtext[19]*256;
			// byte 21+22 = Element-Nummer im Slide, <INR>
			slideelem = mtext[22] + mtext[21]*256;
			// byte 23 	  = Slide-Steuerbyte, <Cntrl-Byte>: bit0 = Anzeige, bit1 = Speichern, bit2 = DarfAnzeige bei Senderwechsel, bit3 = Löschen
			slideshow = mtext[23] & 0x01;
			slidesave = mtext[23] & 0x02;
			slidecan = mtext[23] & 0x04;
			slidedel  = mtext[23] & 0x08;
			// byte 24 	  = Dateiart, <Item-Type>: 0=unbekannt/1=MPEG-Still/2=Definition
			filetype = mtext[24];
			if (filetype != 1 && filetype != 2) {
				if ((S_Verbose && 0x0f) >= 1)
					printf("Rass-Error: Filetype unknown !\n");
				return;
			}
			// byte 25-28 = Dateilänge, <Item-Length>
			filemax  = mtext[28] + mtext[27]*256 + mtext[26]*65536 + mtext[25]*65536*256;
			if (filemax >= 65536) {
				if ((S_Verbose && 0x0f) >= 1)
					printf("Rass-Error: Filesize will be too big !\n");
				return;
			}
			// byte 29-31 = Dateioffset Paketnr, <Rfu>
			fileoffp  = mtext[31] + mtext[30]*256 + mtext[29]*65536;
			// byte 32 = Dateioffset Bytenr, <Rfu>
			fileoffb  = mtext[32];
			if (S_Verbose >= 2)
				printf("Rass-Header: afiles= %d\n             slidenumr= %d, slideelem= %d\n             slideshow= %d, -save= %d, -canschow= %d, -delete= %d\n             filetype= %d, filemax= %d\n             fileoffp= %d, fileoffb= %d\n",
					afiles, slidenumr, slideelem, slideshow, slidesave, slidecan, slidedel, filetype, filemax, fileoffp, fileoffb);

			if (fileoffp == 0) {	// First
				if (S_Verbose >= 2)
					printf("Rass-Start@0 ...\n");
				start = true;
				index = 0;
				for (int i=fileoffb; i < len-2; i++) {
					if (index < filemax)
						daten[index++] = mtext[i];
					else
						start = false;
				}
			}
			splfd = plfd;
		}
		else if (plfd < pmax && plfd == splfd+1) {		// Between
			splfd = plfd;
			if (!start && fileoffp == plfd) {	// Data start, <with Rfu no more necesssary>
				if (S_Verbose >= 2)
					printf("Rass-Start@%d ...\n", fileoffp);
				start = true;
				index = 0;
			}
			else
				fileoffb = 15;
			if (start) {
				for (int i=fileoffb; i < len-2; i++) {
					if (index < filemax)
						daten[index++] = mtext[i];
					else
					start = false;
				}
			}
		}
		else if (plfd == pmax && plfd == splfd+1) {		// Last
			fileoffb = 15;
			if (start) {
				for (int i=fileoffb; i < len-4; i++) {
					if (index <= filemax)
						daten[index++] = mtext[i];
					else {
						start = false;
						return;
					}
				}
				if (S_Verbose >= 2)
					printf("... Rass-End (%d bytes)\n", index);
			}

			if (filemax > 0) {		// nothing todo, if 0 byte file
				// crc-check with bytes 'len-4/3'
				unsigned short crc16 = crc16_ccitt(daten, filemax, false);
				if (crc16 != (mtext[len-4]<<8)+mtext[len-3]) {
					if ((S_Verbose && 0x0f) >= 1)
						printf("Rass-Error: wrong CRC # calc = %04x <> transmit = %02x%02x\n", crc16, mtext[len-4], mtext[len-3]);
					start = false;
					return;
				}
			}

			// show & save file ?
			if (index == filemax) {
				if (slideshow || (slidecan && Rass_Show == -1)) {
					if (filetype == 1) {	// show only mpeg-still
						char *filepath;
						asprintf(&filepath, "%s/%s", DataDir, "Rass_show.mpg");
						if ((fd = fopen(filepath, "wb")) != NULL) {
							fwrite(daten, 1, filemax, fd);
							//fflush(fd);		// for test in replaymode
							fclose(fd);
							Rass_Show = 1;
							if (S_Verbose >= 2)
								printf("Rass-File: ready for displaying :-)\n");
						}
						else
							printf("ERROR vdr-radio: writing imagefile failed '%s'", filepath);
						free(filepath);
					}
				}
				if (slidesave || slidedel || slidenumr < RASS_GALMAX) {
					// lfd. Fotogallery 100.. ???
					if (slidenumr >= 100 && slidenumr < RASS_GALMAX) {
						(Rass_SlideFoto < RASS_GALMAX) ? Rass_SlideFoto++ : Rass_SlideFoto = 100;
						slidenumr = Rass_SlideFoto;
					}
					//
					char *filepath = NULL;
					(filetype == 2) ? asprintf(&filepath, "%s/Rass_%d.def", DataDir, slidenumr)
							: asprintf(&filepath, "%s/Rass_%d.mpg", DataDir, slidenumr);
					if ((fd = fopen(filepath, "wb")) != NULL) {
						fwrite(daten, 1, filemax, fd);
						fclose(fd);
						if (S_Verbose >= 1)
							printf("Rass-File: saving '%s'\n", filepath);
						// archivemarker mpeg-stills
						if (filetype == 1) { 
							// 0, 1000/1100/1110/1111..9000/9900/9990/9999
							if (slidenumr == 0 || slidenumr > RASS_GALMAX) {
								if (slidenumr == 0) {
									Rass_Flags[0][0] = !slidedel;
									(RT_Info > 0) ? : RT_Info = 0;	// open RadioTextOsd for ArchivTip
								}
								else {
									int islide = (int) floor(slidenumr/1000);
									for (int i = 3; i >= 0; i--) {
										if ((slidenumr % (i==3 ? 1000 : i==2 ? 100 : i==1 ? 10 : 1)) == 0) {
					        					Rass_Flags[islide][3-i] = !slidedel;	//true;
											break;
										}
									}
								}
							}
							// gallery
							else {
								Rass_Gallery[slidenumr] = !slidedel;
								if (!slidedel && (int)slidenumr > Rass_GalEnd)
									Rass_GalEnd = slidenumr;
								if (!slidedel && (Rass_GalStart == 0 || (int)slidenumr < Rass_GalStart))
									Rass_GalStart = slidenumr;
								// counter
								Rass_GalCount = 0;
								for (int i = Rass_GalStart; i <= Rass_GalEnd; i++) {
									if (Rass_Gallery[i])
										Rass_GalCount++;
								}
								Rass_Flags[10][0] = (Rass_GalCount > 0);
							}
						}
					}
					else
						printf("ERROR vdr-radio: writing image/data-file failed '%s'", filepath);
					if(filepath)
						free(filepath);
				}
			}
			start = false;
			splfd = spmax = 0;
		}
		else {
			start = false;
			splfd = spmax = 0;
		}
	}
	else {
		start = false;
		splfd = spmax = 0;
		if (S_Verbose >= 1)
			printf("RDS-Error: [Rass] Length not correct !\n");
	}
}
#endif

#if 0
void cRadioAudio::EnableRadioTextProcessing(const char *Titel, bool replay)
{
	asprintf(&RT_Titel, "%s", Titel);
	RT_Replay = replay;
	ARec_Receive = ARec_Record = false;

	first_packets = 0;
	enabled = true;
	imagedelay = 0;

	// Radiotext init
	if (S_RtFunc >= 1) {
		RT_MsgShow = RT_PlusShow = false;
		RT_ReOpen = true;
		RT_OsdTO = false;
		RT_Index = RT_PTY = RTP_TToggle = 0;
		RTP_ItemToggle = 1;
		for (int i = 0; i < 5; i++)
			memset(RT_Text[i], 0x20, RT_MEL-1);
		sprintf(RTP_Title, "---");
		sprintf(RTP_Artist, "---");
		RTP_Starttime = time(NULL);
		//
		RDS_PSShow = false;
		RDS_PSIndex = 0;
		for (int i = 0; i < 12; i++)
			memset(RDS_PSText[i], 0x20, 8);
	}

	// ...Memory
	rtp_content.start = time(NULL);
	rtp_content.item_New = false;
	rtp_content.rt_Index = -1;
	rtp_content.item_Index = -1;
	rtp_content.info_StockIndex = -1;
	rtp_content.info_SportIndex = -1;
	rtp_content.info_LotteryIndex = -1;
	rtp_content.info_WeatherIndex = -1;
	rtp_content.info_OtherIndex = -1;

	for (int i = 0; i < MAX_RTPC; i++) {
		rtp_content.radiotext[i] = NULL;
		rtp_content.radiotext[MAX_RTPC+i] = NULL;
		rtp_content.item_Title[i] = NULL;
		rtp_content.item_Artist[i] = NULL;
		rtp_content.info_Stock[i] = NULL;
		rtp_content.info_Sport[i] = NULL;
		rtp_content.info_Lottery[i] = NULL;
		rtp_content.info_Weather[i] = NULL;
		rtp_content.info_Other[i] = NULL;
	}

	rtp_content.info_News = NULL;
	rtp_content.info_NewsLocal = NULL;
	rtp_content.info_DateTime = NULL;
	rtp_content.info_Traffic = NULL;
	rtp_content.info_Alarm = NULL;
	rtp_content.info_Advert = NULL;
	rtp_content.info_Url = NULL;
	rtp_content.prog_Station = NULL;
	rtp_content.prog_Now = NULL;
	rtp_content.prog_Next = NULL;
	rtp_content.prog_Part = NULL;
	rtp_content.prog_Host = NULL;
	rtp_content.prog_EditStaff = NULL;
	rtp_content.prog_Homepage = NULL;
	rtp_content.phone_Hotline = NULL;
	rtp_content.phone_Studio = NULL;
	rtp_content.email_Hotline = NULL;
	rtp_content.email_Studio = NULL;

	// Rass init
	Rass_Show = Rass_Archiv = -1;
	for (int i = 0; i <= 10; i++) {
	for (int ii = 0; ii < 4; ii++)
		Rass_Flags[i][ii] = false;
	}
	Rass_GalStart = Rass_GalEnd = Rass_GalCount = 0;
	for (int i = 0; i < RASS_GALMAX; i++)
		Rass_Gallery[i] = false;
	Rass_SlideFoto = 99;

	if (S_RtFunc < 1) return;

	// RDS-Receiver for seperate Data-PIDs, only Livemode, hardcoded Astra_19E + Hotbird 13E
	int pid = 0;
	if (!replay) {
		switch (chan->Tid()) {
			case 1113:
				switch (pid = chan->Apid(0)) {	// Astra_19.2E - 12633 GHz
				/*	case 0x161: pid = 0x229;	//  radio top40
						break; */
					case 0x400:			//  Hitradio FFH
					case 0x406:			//  planet radio
					case 0x40c:	pid += 1;	//  harmony.ffm
						break;
					default:	return;
				}
				break;
			case 1115:	
				switch (pid = chan->Apid(0)) {	// Astra_19.2E - 12663 GHz
					case 0x1bA:	pid = 0x21e;	//  TruckRadio, only NaviData(0xbf) seen
						break;
					default:	return;
				}
				break;
			case 5300:
				switch (pid = chan->Apid(0)) {	// Hotbird_13E - 11747 GHz, no Radiotext @ moment, only TMC + MECs 25/26
					case 0xdc3:			//  Radio 1
					case 0xdd3:			//  Radio 3
					case 0xddb:			//  Radio 5
					case 0xde3:			//  Radio Exterior
					case 0xdeb:	pid += 1;	//  Radio 4
						break;
					default:	return;
				}
				break;
			default:	return;
		}
		RDSReceiver = new cRDSReceiver(pid);
		rdsdevice = cDevice::ActualDevice();
		rdsdevice->AttachReceiver(RDSReceiver);
	}
}

void cRadioAudio::DisableRadioTextProcessing()
{
	RT_Replay = enabled = false;

	// Radiotext & Rass
	RT_Info = -1;
	RT_ReOpen = false;
	Rass_Show = Rass_Archiv = -1;
	Rass_GalStart = Rass_GalEnd = Rass_GalCount = 0;

	if (RadioTextOsd != NULL)
		RadioTextOsd->Hide();

	if (RDSReceiver != NULL) {
		rdsdevice->Detach(RDSReceiver);
		delete RDSReceiver;
		RDSReceiver = NULL;
		rdsdevice = NULL;
	}
}


// --- cRadioTextOsd ------------------------------------------------------

cBitmap cRadioTextOsd::rds(rds_xpm);
cBitmap cRadioTextOsd::arec(arec_xpm);
cBitmap cRadioTextOsd::rass(rass_xpm);
cBitmap cRadioTextOsd::index(index_xpm);
cBitmap cRadioTextOsd::marker(marker_xpm);
cBitmap cRadioTextOsd::page1(page1_xpm);
cBitmap cRadioTextOsd::pages2(pages2_xpm);
cBitmap cRadioTextOsd::pages3(pages3_xpm);
cBitmap cRadioTextOsd::pages4(pages4_xpm);
cBitmap cRadioTextOsd::no0(no0_xpm);
cBitmap cRadioTextOsd::no1(no1_xpm);
cBitmap cRadioTextOsd::no2(no2_xpm);
cBitmap cRadioTextOsd::no3(no3_xpm);
cBitmap cRadioTextOsd::no4(no4_xpm);
cBitmap cRadioTextOsd::no5(no5_xpm);
cBitmap cRadioTextOsd::no6(no6_xpm);
cBitmap cRadioTextOsd::no7(no7_xpm);
cBitmap cRadioTextOsd::no8(no8_xpm);
cBitmap cRadioTextOsd::no9(no9_xpm);
cBitmap cRadioTextOsd::bok(bok_xpm);
cBitmap cRadioTextOsd::pageE(pageE_xpm);

cRadioTextOsd::cRadioTextOsd()
{
	RadioTextOsd = this;
	osd = NULL;
	qosd = NULL;
	rtclosed = rassclosed = false;
	RT_ReOpen = false;
}

cRadioTextOsd::~cRadioTextOsd()
{
	if (Rass_Archiv >= 0) {
		if (!RT_Replay)
			Rass_Archiv = RassImage(-1, -1, false);
		else {
			Rass_Archiv = -1;
			RadioAudio->SetBackgroundImage(ReplayFile);
		}
	}

	if (osd != NULL)
		delete osd;
	if (qosd != NULL)
		delete qosd;
	RadioTextOsd = NULL;
	RT_ReOpen = !RT_OsdTO;

	cRemote::Put(LastKey);
}

void cRadioTextOsd::Show(void)
{
	LastKey = kNone;
	RT_OsdTO = false;
#ifndef VDRP_CLOSEMENU
	osdtimer.Set();
#endif

	ftitel = cFont::GetFont(fontOsd);
	ftext = cFont::GetFont(fontSml);
	fheight = ftext->Height() + 4;
	int rowoffset = (S_RtOsdTitle == 1) ? 1 : 0;
	bheight = (S_RtOsdTags >=1 ) ? fheight * (S_RtOsdRows+rowoffset+2) : fheight * (S_RtOsdRows+rowoffset);
	(S_RtOsdTitle == 1) ? bheight += 20 : bheight += 12;

	asprintf(&RTp_Titel, "%s - %s", tr("RTplus"), RT_Titel);

	if (S_RtDispl >= 1 && (Rass_Show == -1 || S_RassText >= 2)) {
		RT_MsgShow = (RT_Info >= 1);
		ShowText();
	}
}

void cRadioTextOsd::Hide(void)
{
	RTOsdClose();
	RassOsdClose();
}

void cRadioTextOsd::RTOsdClose(void)
{
	if (osd != NULL) {
		delete osd;
		osd = NULL;
	}
}
#endif

#if ENABLE_RASS
int CRadioText::RassImage(int QArchiv, int QKey, bool DirUp)
{
	int i;

	if (QKey >= 0 && QKey <= 9) {
		if (QArchiv == 0)
			(Rass_Flags[QKey][0]) ? QArchiv = QKey * 1000 : QArchiv = 0;
		else if (QArchiv > 0) {
			if (floor(QArchiv/1000) == QKey) {
				for (i = 3; i >= 0; i--) {
//					if (fmod(QArchiv, pow(10, i)) == 0)
					if ((QArchiv % (i==3 ? 1000 : i==2 ? 100 : i==1 ? 10 : 1)) == 0)
						break;
				}

				if (i > 0) {
					--i;
					QArchiv += QKey * (int) (i==3 ? 1000 : i==2 ? 100 : i==1 ? 10 : 1);
				}
				else
 					QArchiv = QKey * 1000;
				(Rass_Flags[QKey][3-i]) ? : QArchiv = QKey * 1000;
			}
			else
				(Rass_Flags[QKey][0]) ? QArchiv = QKey * 1000 : QArchiv = 0;
		}
	}

	// Gallery
	else if (QKey > 9 && Rass_GalCount >= 0) {
		if (QArchiv < Rass_GalStart || QArchiv > Rass_GalEnd)
			QArchiv = Rass_GalStart - 1; 
		if (DirUp) {
			for (i = QArchiv+1; i <= Rass_GalEnd; i++) {
				if (Rass_Gallery[i])
					break;
			}
			QArchiv = (i <= Rass_GalEnd) ? i : Rass_GalStart;
		}
		else {
			for (i = QArchiv-1; i >= Rass_GalStart; i--) {
			if (Rass_Gallery[i])  
				break;
			}
			QArchiv = (i >= Rass_GalStart) ? i : Rass_GalEnd;
		}
	}

	// show mpeg-still
	char *image;
	if (QArchiv >= 0)
		asprintf(&image, "%s/Rass_%d.mpg", DataDir, QArchiv);
	else
		asprintf(&image, "%s/Rass_show.mpg", DataDir);
// Houdini: SetBackgroundImage() does not accept mpg stills
//	frameBuffer->useBackground(frameBuffer->loadBackground(image));// set useBackground true or false
//	frameBuffer->paintBackground();
//	RadioAudio->SetBackgroundImage(image);
	free(image);

	return QArchiv;
}
#endif

#if 0
void cRadioTextOsd::RassOsd(void)
{
	int fh = ftext->Height();

	if (!qosd && !osd && !Skins.IsOpen() && !cOsd::IsOpen()) {
		qosd = cOsdProvider::NewOsd(Setup.OSDLeft, Setup.OSDTop+Setup.OSDHeight - (29+264-6+36));
		tArea Area = {0, 0, 97, 29+264+5, 4};
		qosd->SetAreas(&Area, 1);
	}

	if (qosd) {
		uint32_t bcolor, fcolor;
		int skin = theme_skin();

		// Logo
		bcolor = radioSkin[skin].clrTitleBack;
		fcolor = radioSkin[skin].clrTitleText;
		qosd->DrawRectangle(0, 1, 97, 29, bcolor);
		qosd->DrawBitmap(25, 5, rass, bcolor, fcolor);

		// Body
		bcolor = radioSkin[skin].clrBack;
		fcolor = radioSkin[skin].clrText;
		int offs = 29 + 2;
		qosd->DrawRectangle(0, offs, 97, 29+264+5, bcolor);

		// Keys+Index
		offs += 4;
		qosd->DrawBitmap(4,     offs, no0, bcolor, fcolor);
		qosd->DrawBitmap(44,    offs, index, bcolor, fcolor);
		qosd->DrawBitmap(4,  24+offs, no1, bcolor, fcolor);
		qosd->DrawBitmap(4,  48+offs, no2, bcolor, fcolor);
		qosd->DrawBitmap(4,  72+offs, no3, bcolor, fcolor);
		qosd->DrawBitmap(4,  96+offs, no4, bcolor, fcolor);
		qosd->DrawBitmap(4, 120+offs, no5, bcolor, fcolor);
		qosd->DrawBitmap(4, 144+offs, no6, bcolor, fcolor);
		qosd->DrawBitmap(4, 168+offs, no7, bcolor, fcolor);
		qosd->DrawBitmap(4, 192+offs, no8, bcolor, fcolor);
		qosd->DrawBitmap(4, 216+offs, no9, bcolor, fcolor);
		qosd->DrawBitmap(4, 240+offs, bok, bcolor, fcolor);

		// Content
		bool mark = false;
		for (int i = 1; i <= 9; i++) {
			// Pages
			if (Rass_Flags[i][0] && Rass_Flags[i][1] && Rass_Flags[i][2] && Rass_Flags[i][3])
				qosd->DrawBitmap(48, (i*24)+offs, pages4, bcolor, fcolor);
			else if (Rass_Flags[i][0] && Rass_Flags[i][1] && Rass_Flags[i][2])
				qosd->DrawBitmap(48, (i*24)+offs, pages3, bcolor, fcolor);
			else if (Rass_Flags[i][0] && Rass_Flags[i][1])
				qosd->DrawBitmap(48, (i*24)+offs, pages2, bcolor, fcolor);
			else if (Rass_Flags[i][0])
				qosd->DrawBitmap(48, (i*24)+offs, page1, bcolor, fcolor);

			// Marker
			if (floor(Rass_Archiv/1000) == i) {
				qosd->DrawBitmap(28, (i*24)+offs, marker, bcolor, fcolor);
				mark = true;
			}
		}

		// Gallery
		if (Rass_GalCount > 0) {
			char *temp;
			qosd->DrawBitmap(48, 240+offs, pageE, bcolor, fcolor);
			asprintf(&temp, "%d", Rass_GalCount);
			qosd->DrawText(67, 240+offs-2, temp, fcolor, clrTransparent, ftext, 97, fh);
			free(temp);
		}

		// Marker gallery/index	
		if (!mark) {
			if (Rass_Archiv > 0 && Rass_Archiv <= RASS_GALMAX)
				qosd->DrawBitmap(30, 240+offs, marker, bcolor, fcolor);
			else
				qosd->DrawBitmap(28, offs, marker, bcolor, fcolor);
		}
		qosd->Flush();
	}
}

void cRadioTextOsd::RassOsdTip(void)
{
	int fh = ftext->Height();

	if (!qosd && !osd && !Skins.IsOpen() && !cOsd::IsOpen()) {
		qosd = cOsdProvider::NewOsd(Setup.OSDLeft, Setup.OSDTop+Setup.OSDHeight - (29+(2*fh)-6+36));
		tArea Area = {0, 0, 97, 29+(2*fh)+5, 4};
		qosd->SetAreas(&Area, 1);
	}

	if (qosd) {
		uint32_t bcolor, fcolor;
		int skin = theme_skin();

		// Title
		bcolor = radioSkin[skin].clrTitleBack;
		fcolor = radioSkin[skin].clrTitleText;
		qosd->DrawRectangle(0, 0, 97, 29, bcolor);
		qosd->DrawBitmap(25, 5, rass, bcolor, fcolor);

		// Body
		bcolor = radioSkin[skin].clrBack;
		fcolor = radioSkin[skin].clrText;
		qosd->DrawRectangle(0, 29+2, 97, 29+(2*fh)+5, bcolor);
		qosd->DrawText(5, 29+4, tr("Records"), fcolor, clrTransparent, ftext, 97, fh);
		qosd->DrawText(5, 29+fh+4, tr("with <0>"), fcolor, clrTransparent, ftext, 97, fh);
		qosd->Flush();
	}
}

void cRadioTextOsd::RassOsdClose(void)
{
	if (qosd != NULL) {
		delete qosd;
		qosd = NULL;
	}
}

void cRadioTextOsd::RassImgSave(char *size, int pos)
{
	char *infile, *outfile, *cmd;
	int filenr = 0, error = 0;
	struct tm *ts, tm_store;
	time_t t = time(NULL);
	ts = localtime_r(&t, &tm_store);

	switch (pos) {
		// all from 1-9
		case 1 ... 9:
			for (int i = 3; i >= 0; i--) {
				filenr += (int) (pos * pow(10, i));
				if (Rass_Flags[pos][3-i]) {
					asprintf(&infile, "%s/Rass_%d.mpg", DataDir, filenr);
					asprintf(&outfile, "%s/Rass_%s-%04d_%02d%02d%02d%02d.jpg", DataDir, RT_Titel, filenr, 
						ts->tm_mon+1, ts->tm_mday, ts->tm_hour, ts->tm_min);
					asprintf(&cmd, "ffmpeg -i %s -s %s -f mjpeg -y %s", infile, size, outfile);
					if ((error = system(cmd)))
						i = -1;
				}
			}
			asprintf(&cmd, "%s '%d'", tr("Rass-Image(s) saved from Archiv "), pos);
			break;

		// all from gallery
		case 10:
			for (int i = Rass_GalStart; i <= Rass_GalEnd; i++) {
				if (Rass_Gallery[i]) {
					asprintf(&infile, "%s/Rass_%d.mpg", DataDir, i);
					asprintf(&outfile, "%s/Rass_%s-Gallery%04d_%02d%02d.jpg", DataDir, RT_Titel, i, 
						ts->tm_mon+1, ts->tm_mday);
					asprintf(&cmd, "ffmpeg -i %s -s %s -f mjpeg -y %s", infile, size, outfile);
					if ((error = system(cmd)))
						i = Rass_GalEnd + 1;
				}
			}
			asprintf(&cmd, "%s", tr("Rass-Image(s) saved from Gallery"));
			break;

		// single
		default:
			asprintf(&infile, "%s/Rass_%d.mpg", DataDir, Rass_Archiv);
			asprintf(&outfile, "%s/Rass_%s-%04d_%02d%02d%02d%02d.jpg", DataDir, RT_Titel, Rass_Archiv, 
	        		ts->tm_mon+1, ts->tm_mday, ts->tm_hour, ts->tm_min);
			asprintf(&cmd, "ffmpeg -i %s -s %s -f mjpeg -y %s", infile, size, outfile);
			error = system(cmd);
			asprintf(&cmd, "%s: %s", tr("Rass-Image saved"), outfile);
	}
	free(infile);

	// Info
	RassOsdClose();
	if (error) {
		asprintf(&cmd, "%s: %s", tr("Rass-Image failed"), outfile);
		Skins.Message(mtError, cmd, Setup.OSDMessageTime);
	}
	else
		Skins.Message(mtInfo, cmd, Setup.OSDMessageTime);

	free(outfile);
	free(cmd);
}

void cRadioTextOsd::rtp_print(void)
{
	struct tm tm_store;
	time_t t = time(NULL);

	printf("\n>>> RTplus-Memoryclasses @ %s", asctime(localtime_r(&t, &tm_store)));
	printf("    on '%s' since %s", RT_Titel, asctime(localtime_r(&rtp_content.start, &tm_store)));

	printf("--- Programme ---\n");
	if (rtp_content.prog_Station != NULL)   printf("     Station: %s\n", rtp_content.prog_Station);
	if (rtp_content.prog_Now != NULL)	    printf("         Now: %s\n", rtp_content.prog_Now);
	if (rtp_content.prog_Next != NULL)	    printf("        Next: %s\n", rtp_content.prog_Next);
	if (rtp_content.prog_Part != NULL)	    printf("        Part: %s\n", rtp_content.prog_Part);
	if (rtp_content.prog_Host != NULL)	    printf("        Host: %s\n", rtp_content.prog_Host);
	if (rtp_content.prog_EditStaff != NULL) printf("    Ed.Staff: %s\n", rtp_content.prog_EditStaff);
	if (rtp_content.prog_Homepage != NULL)  printf("    Homepage: %s\n", rtp_content.prog_Homepage);

	printf("--- Interactivity ---\n");
	if (rtp_content.phone_Hotline != NULL)  printf("    Phone-Hotline: %s\n", rtp_content.phone_Hotline);
	if (rtp_content.phone_Studio != NULL)   printf("     Phone-Studio: %s\n", rtp_content.phone_Studio);
	if (rtp_content.email_Hotline != NULL)  printf("    Email-Hotline: %s\n", rtp_content.email_Hotline);
	if (rtp_content.email_Studio != NULL)   printf("     Email-Studio: %s\n", rtp_content.email_Studio);

	printf("--- Info ---\n");
	if (rtp_content.info_News != NULL) 	    printf("         News: %s\n", rtp_content.info_News);
	if (rtp_content.info_NewsLocal != NULL) printf("    NewsLocal: %s\n", rtp_content.info_NewsLocal);
	if (rtp_content.info_DateTime != NULL)  printf("     DateTime: %s\n", rtp_content.info_DateTime);
	if (rtp_content.info_Traffic != NULL)   printf("      Traffic: %s\n", rtp_content.info_Traffic);
	if (rtp_content.info_Alarm != NULL)     printf("        Alarm: %s\n", rtp_content.info_Alarm);
	if (rtp_content.info_Advert != NULL)    printf("    Advertisg: %s\n", rtp_content.info_Advert);
	if (rtp_content.info_Url != NULL)       printf("          Url: %s\n", rtp_content.info_Url);
	// no sorting
	for (int i = 0; i < MAX_RTPC; i++)
		if (rtp_content.info_Stock[i] != NULL)   printf("      Stock[%02d]: %s\n", i, rtp_content.info_Stock[i]);
	for (int i = 0; i < MAX_RTPC; i++)
		if (rtp_content.info_Sport[i] != NULL)   printf("      Sport[%02d]: %s\n", i, rtp_content.info_Sport[i]);
	for (int i = 0; i < MAX_RTPC; i++)
		if (rtp_content.info_Lottery[i] != NULL) printf("    Lottery[%02d]: %s\n", i, rtp_content.info_Lottery[i]);
	for (int i = 0; i < MAX_RTPC; i++)
		if (rtp_content.info_Weather[i] != NULL) printf("    Weather[%02d]: %s\n", i, rtp_content.info_Weather[i]);
	for (int i = 0; i < MAX_RTPC; i++)
		if (rtp_content.info_Other[i] != NULL)   printf("      Other[%02d]: %s\n", i, rtp_content.info_Other[i]);
/*
	printf("--- Item-Playlist ---\n");
	// no sorting
	if (rtp_content.item_Index >= 0) {
		for (int i = 0; i < MAX_RTPC; i++) {
			if (rtp_content.item_Title[i] != NULL && rtp_content.item_Artist[i] != NULL) {
				struct tm tm_store;
				struct tm *ts = localtime_r(&rtp_content.item_Start[i], &tm_store);
				printf("    [%02d]  %02d:%02d  Title: %s | Artist: %s\n",
					i, ts->tm_hour, ts->tm_min, rtp_content.item_Title[i], rtp_content.item_Artist[i]);
			}
		}
	}

	printf("--- Last seen Radiotext ---\n");
	// no sorting
	if (rtp_content.rt_Index >= 0) {
		for (int i = 0; i < 2*MAX_RTPC; i++)
			if (rtp_content.radiotext[i] != NULL) printf("    [%03d]  %s\n", i, rtp_content.radiotext[i]);
	}
*/
	printf("<<<\n");
}

#define rtplog 0
eOSState cRadioTextOsd::ProcessKey(eKeys Key)
{
	// RTplus Infolog
	if (rtplog == 1 && S_Verbose >= 1) {
		static int ct = 0;
		if (++ct >= 60) {
			ct = 0;
			rtp_print();
		}
	}

	// check end @ replay
	if (RT_Replay) {
		int rplayCur, rplayTot;
		cControl::Control()->GetIndex(rplayCur, rplayTot, false);
		if (rplayCur >= rplayTot-1) {
			Hide();
			return osEnd;
		}
	}

	// Timeout or no Info/Rass
	if (RT_OsdTO || (RT_OsdTOTemp > 0) || (RT_Info < 0)) {
		Hide();
		return osEnd;
	}

	eOSState state = cOsdObject::ProcessKey(Key);
	if (state != osUnknown)  return state;

	// Key pressed ...
	if (Key != kNone && Key < k_Release) {
		if (osd) {				// Radiotext, -plus Osd
			switch (Key) {
				case kBack:	RTOsdClose();
					rtclosed = true;
					//rassclosed = false;
					break;
				case k0:	RTOsdClose();
					RTplus_Osd = true;  
					cRemote::CallPlugin("radio");
					return osEnd;
				default:    	Hide();
					LastKey = (Key == kChanUp || Key == kChanDn) ? kNone : Key;
					return osEnd;
			}
		}
		else if (qosd && Rass_Archiv >= 0) {	// Rass-Archiv Osd
			int i, pos;
			pos = (Rass_Archiv > 0 && Rass_Archiv <= RASS_GALMAX) ? 10 : (int) floor(Rass_Archiv/1000);
			switch (Key) {
				// back to Slideshow
				case kBlue:
				case kBack: 
					if (!RT_Replay)
						Rass_Archiv = RassImage(-1, 0, false);
					else {
						Rass_Archiv = -1;
						RadioAudio->SetBackgroundImage(ReplayFile);
					}
					RassOsdClose();
					rassclosed = rtclosed = false;
					break;

				// Archiv-Sides
				case k0 ... k9:
					Rass_Archiv = RassImage(Rass_Archiv, Key-k0, false);
					RassOsd();
					break;

				case kOk:	
					if (Rass_Flags[10][0]) {
						Rass_Archiv = RassImage(Rass_Archiv, 10, true);
						RassOsd();
					}
					break;

				case kLeft:
				case kRight:
					Rass_Archiv = RassImage(Rass_Archiv, pos, (Key == kRight) ? true : false);
					RassOsd();
					break;
				case kDown:
					(pos == 10) ? i = 0 : i = pos + 1;
					while (i != pos) {
						if (Rass_Flags[i][0]) {
							Rass_Archiv = RassImage(Rass_Archiv, i, true);
							RassOsd();
							return osContinue;
						}
						if (++i > 10) i = 0;
					}
					break;

				case kUp:
					(pos == 0) ? i = 10 : i = pos - 1;
					while (i != pos) {
						if (Rass_Flags[i][0]) {
							Rass_Archiv = RassImage(Rass_Archiv, i, true);
							RassOsd();
							return osContinue;
						}
						if (--i < 0) i = 10;
					}
					break;

				case kRed:
					RassImgSave("1024x576", 0);
					break;

				case kGreen:	
					RassImgSave("1024x576", pos);
					break;

				case kYellow:	
					break;	// todo, what ?

				default:
					Hide();
					LastKey = (Key == kChanUp || Key == kChanDn) ? kNone : Key;
					return osEnd;
			}
		}
		else if (qosd && Rass_Archiv == -1) {	// Rass-Slideshow Osd
			switch (Key) {
				// close
				case kBack:
					RassOsdClose();
					rassclosed = true;
					//rtclosed = false;
					break;

				// Archiv-Index
				case k0:
					if (Rass_Flags[0][0]) {
						RassOsdClose();
						Rass_Archiv = RassImage(0, 0, false);
						RassOsd();
					}
					break;

				default:
					Hide();
					LastKey = (Key == kChanUp || Key == kChanDn) ? kNone : Key;
					return osEnd;
			}
		}
		else {					// no RT && no Rass
			Hide();
			LastKey = (Key == kChanUp || Key == kChanDn) ? kNone : Key;
			return osEnd;
		}
	}
	// no Key pressed ...
#ifndef VDRP_CLOSEMENU
	else if (S_RtOsdTO > 0 && osdtimer.Elapsed()/1000/60 >= (uint)S_RtOsdTO) {
		RT_OsdTO = true;
		Hide();
		return osEnd;
	}
#endif
	else if (Rass_Archiv >= 0)
		RassOsd();
	else if (RT_MsgShow && !rtclosed && (Rass_Show == -1 || S_RassText >= 2 || rassclosed)) {
		RassOsdClose();
		ShowText();
	}
	else if (Rass_Flags[0][0] && !rassclosed && (S_RassText < 2 || rtclosed)) {
		RTOsdClose();
		RassOsdTip();
	}

	return osContinue;
}


// --- cRTplusOsd ------------------------------------------------------

cRTplusOsd::cRTplusOsd(void)
:cOsdMenu(RTp_Titel, 3, 12)
{
	RTplus_Osd = false;

	bcount = helpmode = 0;
	listtyp[0] = tr("Radiotext");
	listtyp[1] = tr("Playlist");
	listtyp[2] = tr("Sports");
	listtyp[3] = tr("Lottery");
	listtyp[4] = tr("Weather");
	listtyp[5] = tr("Stockmarket");
	listtyp[6] = tr("Other");

	Load();
	Display();
}

cRTplusOsd::~cRTplusOsd()
{
}

void cRTplusOsd::Load(void) 
{
	char text[80];

	struct tm tm_store;
	struct tm *ts = localtime_r(&rtp_content.start, &tm_store);
	snprintf(text, sizeof(text), "%s  %02d:%02d", tr("RTplus Memory  since"), ts->tm_hour, ts->tm_min);
	Add(new cOsdItem(hk(text)));
	snprintf(text, sizeof(text), "%s", " ");
	Add(new cOsdItem(hk(text)));

	snprintf(text, sizeof(text), "-- %s --", tr("Programme"));
	Add(new cOsdItem(hk(text)));
	if (rtp_content.prog_Station != NULL) {
		snprintf(text, sizeof(text), "\t%s:\t%s", tr("Station"), rtp_content.prog_Station);
		Add(new cOsdItem(hk(text)));
	}
	if (rtp_content.prog_Now != NULL) {
		snprintf(text, sizeof(text), "\t%s:\t%s", tr("Now"), rtp_content.prog_Now);
		Add(new cOsdItem(hk(text)));
	}
	if (rtp_content.prog_Part != NULL) {
		snprintf(text, sizeof(text), "\t%s:\t%s", tr("...Part"), rtp_content.prog_Part);
		Add(new cOsdItem(hk(text)));
	}
	if (rtp_content.prog_Next != NULL) {
		snprintf(text, sizeof(text), "\t%s:\t%s", tr("Next"), rtp_content.prog_Next);
		Add(new cOsdItem(hk(text)));
	}
	if (rtp_content.prog_Host != NULL) {
		snprintf(text, sizeof(text), "\t%s:\t%s", tr("Host"), rtp_content.prog_Host);
		Add(new cOsdItem(hk(text)));
	}
	if (rtp_content.prog_EditStaff != NULL) {
		snprintf(text, sizeof(text), "\t%s:\t%s", tr("Edit.Staff"), rtp_content.prog_EditStaff);
		Add(new cOsdItem(hk(text)));
	}
	if (rtp_content.prog_Homepage != NULL) {
		snprintf(text, sizeof(text), "\t%s:\t%s", tr("Homepage"), rtp_content.prog_Homepage);
		Add(new cOsdItem(hk(text)));
	}
	snprintf(text, sizeof(text), "%s", " ");
	Add(new cOsdItem(hk(text)));

	snprintf(text, sizeof(text), "-- %s --", tr("Interactivity"));
	Add(new cOsdItem(hk(text)));
	if (rtp_content.phone_Hotline != NULL) {
		snprintf(text, sizeof(text), "\t%s:\t%s", tr("Phone-Hotline"), rtp_content.phone_Hotline);
		Add(new cOsdItem(hk(text)));
	}
	if (rtp_content.phone_Studio != NULL) {
		snprintf(text, sizeof(text), "\t%s:\t%s", tr("Phone-Studio"), rtp_content.phone_Studio);
		Add(new cOsdItem(hk(text)));
	}
	if (rtp_content.email_Hotline != NULL) {
	snprintf(text, sizeof(text), "\t%s:\t%s", tr("Email-Hotline"), rtp_content.email_Hotline);
	Add(new cOsdItem(hk(text)));
	}
	if (rtp_content.email_Studio != NULL) {
		snprintf(text, sizeof(text), "\t%s:\t%s", tr("Email-Studio"), rtp_content.email_Studio);
		Add(new cOsdItem(hk(text)));
	}
	snprintf(text, sizeof(text), "%s", " ");
	Add(new cOsdItem(hk(text)));

	snprintf(text, sizeof(text), "-- %s --", tr("Info"));
	Add(new cOsdItem(hk(text)));
	if (rtp_content.info_News != NULL) {
		snprintf(text, sizeof(text), "\t%s:\t%s", tr("News"), rtp_content.info_News);
		Add(new cOsdItem(hk(text)));
	}
	if (rtp_content.info_NewsLocal != NULL) {
		snprintf(text, sizeof(text), "\t%s:\t%s", tr("NewsLocal"), rtp_content.info_NewsLocal);
		Add(new cOsdItem(hk(text)));
	}
	if (rtp_content.info_DateTime != NULL) {
		snprintf(text, sizeof(text), "\t%s:\t%s", tr("DateTime"), rtp_content.info_DateTime);
		Add(new cOsdItem(hk(text)));
	}
	if (rtp_content.info_Traffic != NULL) {
		snprintf(text, sizeof(text), "\t%s:\t%s", tr("Traffic"), rtp_content.info_Traffic);
		Add(new cOsdItem(hk(text)));
	}
	if (rtp_content.info_Alarm != NULL) {
		snprintf(text, sizeof(text), "\t%s:\t%s", tr("Alarm"), rtp_content.info_Alarm);
		Add(new cOsdItem(hk(text)));
	}
	if (rtp_content.info_Advert != NULL) {
		snprintf(text, sizeof(text), "\t%s:\t%s", tr("Advertising"), rtp_content.info_Advert);
		Add(new cOsdItem(hk(text)));
	}
	if (rtp_content.info_Url != NULL) {
		snprintf(text, sizeof(text), "\t%s:\t%s", tr("Url"), rtp_content.info_Url);
		Add(new cOsdItem(hk(text)));
	}

	for (int i = 0; i <= 6; i++)
		btext[i] = NULL;	
	bcount = 0;
	asprintf(&btext[bcount++], "%s", listtyp[0]);
	if (rtp_content.item_Index >= 0)
		asprintf(&btext[bcount++], "%s", listtyp[1]);
	if (rtp_content.info_SportIndex >= 0)
		asprintf(&btext[bcount++], "%s", listtyp[2]);
	if (rtp_content.info_LotteryIndex >= 0)
		asprintf(&btext[bcount++], "%s", listtyp[3]);
	if (rtp_content.info_WeatherIndex >= 0)
		asprintf(&btext[bcount++], "%s", listtyp[4]);
	if (rtp_content.info_StockIndex >= 0)
		asprintf(&btext[bcount++], "%s", listtyp[5]);
	if (rtp_content.info_OtherIndex >= 0)
		asprintf(&btext[bcount++], "%s", listtyp[6]);
	
	switch (bcount) {
		case 4:	    if (helpmode == 0)
				SetHelp(btext[0], btext[1], btext[2], ">>");
			    else if (helpmode == 1)
				SetHelp("<<", btext[3], NULL, tr("Exit"));
			    break;
		case 5:	    if (helpmode == 0)
				SetHelp(btext[0], btext[1], btext[2], ">>");
			    else if (helpmode == 1)
				SetHelp("<<", btext[3], btext[4], tr("Exit"));
			    break;	    
		case 6:	    if (helpmode == 0)
				SetHelp(btext[0], btext[1], btext[2], ">>");
			    else if (helpmode == 1)
				SetHelp("<<", btext[3], btext[4], ">>");
			    else if (helpmode == 2)
				SetHelp("<<", btext[5], NULL, tr("Exit"));
			    break;
		case 7:	    if (helpmode == 0)
				SetHelp(btext[0], btext[1], btext[2], ">>");
			    else if (helpmode == 1)
				SetHelp("<<", btext[3], btext[4], ">>");
			    else if (helpmode == 2)
				SetHelp("<<", btext[5], btext[6], tr("Exit"));
			    break;
		default:    helpmode = 0;
			    SetHelp(btext[0], btext[1], btext[2], tr("Exit"));
	}
}

void cRTplusOsd::Update(void)
{
	Clear();
	Load();
	Display();
}

int cRTplusOsd::rtptyp(char *btext)
{
	for (int i = 0; i <= 6; i++) {
		if (strcmp(btext, listtyp[i]) == 0)
			return i;
	}
	
	return -1;
}

void cRTplusOsd::rtp_fileprint(void)
{
	struct tm *ts, tm_store;
	char *fname, *fpath;
	FILE *fd;
	int ind, lfd = 0;

	time_t t = time(NULL);
	ts = localtime_r(&t, &tm_store);
	asprintf(&fname, "RTplus_%s_%04d-%02d-%02d.%02d.%02d", RT_Titel, ts->tm_year+1900, ts->tm_mon+1, ts->tm_mday, ts->tm_hour, ts->tm_min);
	asprintf(&fpath, "%s/%s", DataDir, fname);
	if ((fd = fopen(fpath, "w")) != NULL) {
	
		fprintf(fd, ">>> RTplus-Memoryclasses @ %s", asctime(localtime_r(&t, &tm_store)));
		fprintf(fd, "    on '%s' since %s", RT_Titel, asctime(localtime_r(&rtp_content.start, &tm_store)));

		fprintf(fd, "--- Programme ---\n");
		if (rtp_content.prog_Station != NULL)   fprintf(fd, "     Station: %s\n", rtp_content.prog_Station);
		if (rtp_content.prog_Now != NULL)	fprintf(fd, "         Now: %s\n", rtp_content.prog_Now);
		if (rtp_content.prog_Part != NULL)	fprintf(fd, "        Part: %s\n", rtp_content.prog_Part);
		if (rtp_content.prog_Next != NULL)	fprintf(fd, "        Next: %s\n", rtp_content.prog_Next);
		if (rtp_content.prog_Host != NULL)	fprintf(fd, "        Host: %s\n", rtp_content.prog_Host);
		if (rtp_content.prog_EditStaff != NULL) fprintf(fd, "    Ed.Staff: %s\n", rtp_content.prog_EditStaff);
		if (rtp_content.prog_Homepage != NULL)  fprintf(fd, "    Homepage: %s\n", rtp_content.prog_Homepage);

		fprintf(fd, "--- Interactivity ---\n");
		if (rtp_content.phone_Hotline != NULL)  fprintf(fd, "    Phone-Hotline: %s\n", rtp_content.phone_Hotline);
		if (rtp_content.phone_Studio != NULL)   fprintf(fd, "     Phone-Studio: %s\n", rtp_content.phone_Studio);
		if (rtp_content.email_Hotline != NULL)  fprintf(fd, "    Email-Hotline: %s\n", rtp_content.email_Hotline);
		if (rtp_content.email_Studio != NULL)   fprintf(fd, "     Email-Studio: %s\n", rtp_content.email_Studio);

		fprintf(fd, "--- Info ---\n");
		if (rtp_content.info_News != NULL) 	fprintf(fd, "         News: %s\n", rtp_content.info_News);
		if (rtp_content.info_NewsLocal != NULL) fprintf(fd, "    NewsLocal: %s\n", rtp_content.info_NewsLocal);
		if (rtp_content.info_DateTime != NULL)  fprintf(fd, "     DateTime: %s\n", rtp_content.info_DateTime);
		if (rtp_content.info_Traffic != NULL)   fprintf(fd, "      Traffic: %s\n", rtp_content.info_Traffic);
		if (rtp_content.info_Alarm != NULL)     fprintf(fd, "        Alarm: %s\n", rtp_content.info_Alarm);
		if (rtp_content.info_Advert != NULL)    fprintf(fd, "    Advertisg: %s\n", rtp_content.info_Advert);
		if (rtp_content.info_Url != NULL)       fprintf(fd, "          Url: %s\n", rtp_content.info_Url);

		if (rtp_content.item_Index >= 0) {
			fprintf(fd, "--- Item-Playlist ---\n");
			ind = rtp_content.item_Index;
			if (ind < (MAX_RTPC-1) && rtp_content.item_Title[ind+1] != NULL) {
				for (int i = ind+1; i < MAX_RTPC; i++) {
					if (rtp_content.item_Title[i] != NULL && rtp_content.item_Artist[i] != NULL) {
						ts = localtime_r(&rtp_content.item_Start[i], &tm_store);
						fprintf(fd, "    %02d:%02d  Title: '%s' | Artist: '%s'\n", ts->tm_hour, ts->tm_min, rtp_content.item_Title[i], rtp_content.item_Artist[i]);
					}
				}
			}
			for (int i = 0; i <= ind; i++) {
				if (rtp_content.item_Title[i] != NULL && rtp_content.item_Artist[i] != NULL) {
					ts = localtime_r(&rtp_content.item_Start[i], &tm_store);
					fprintf(fd, "    %02d:%02d  Title: '%s' | Artist: '%s'\n", ts->tm_hour, ts->tm_min, rtp_content.item_Title[i], rtp_content.item_Artist[i]);
				}
			}
		}

		if (rtp_content.info_SportIndex >= 0) {
			fprintf(fd, "--- Sports ---\n");
			ind = rtp_content.info_SportIndex;
			if (ind < (MAX_RTPC-1) && rtp_content.info_Sport[ind+1] != NULL) {
				for (int i = ind+1; i < MAX_RTPC; i++) {
					if (rtp_content.info_Sport[i] != NULL)
						fprintf(fd, "    %02d. %s\n", ++lfd, rtp_content.info_Sport[i]);
				}
			}
			for (int i = 0; i <= ind; i++) {
				if (rtp_content.info_Sport[i] != NULL)
					fprintf(fd, "    %02d. %s\n", ++lfd, rtp_content.info_Sport[i]);
			}
		}

		if (rtp_content.info_LotteryIndex >= 0) {
			fprintf(fd, "--- Lottery ---\n");
			ind = rtp_content.info_LotteryIndex;
			if (ind < (MAX_RTPC-1) && rtp_content.info_Lottery[ind+1] != NULL) {
				for (int i = ind+1; i < MAX_RTPC; i++) {
					if (rtp_content.info_Lottery[i] != NULL)
						fprintf(fd, "    %02d. %s\n", ++lfd, rtp_content.info_Lottery[i]);
				}
			}
			for (int i = 0; i <= ind; i++) {
				if (rtp_content.info_Lottery[i] != NULL)
					fprintf(fd, "    %02d. %s\n", ++lfd, rtp_content.info_Lottery[i]);
			}
		}

		if (rtp_content.info_WeatherIndex >= 0) {
			fprintf(fd, "--- Weather ---\n");
			ind = rtp_content.info_WeatherIndex;
			if (ind < (MAX_RTPC-1) && rtp_content.info_Weather[ind+1] != NULL) {
				for (int i = ind+1; i < MAX_RTPC; i++) {
					if (rtp_content.info_Weather[i] != NULL)
					fprintf(fd, "    %02d. %s\n", ++lfd, rtp_content.info_Weather[i]);
				}
			}
			for (int i = 0; i <= ind; i++) {
				if (rtp_content.info_Weather[i] != NULL)
				fprintf(fd, "    %02d. %s\n", ++lfd, rtp_content.info_Weather[i]);
			}
		}

		if (rtp_content.info_StockIndex >= 0) {
			fprintf(fd, "--- Stockmarket ---\n");
			ind = rtp_content.info_StockIndex;
			if (ind < (MAX_RTPC-1) && rtp_content.info_Stock[ind+1] != NULL) {
				for (int i = ind+1; i < MAX_RTPC; i++) {
					if (rtp_content.info_Stock[i] != NULL)
						fprintf(fd, "    %02d. %s\n", ++lfd, rtp_content.info_Stock[i]);
				}
			}
			for (int i = 0; i <= ind; i++) {
				if (rtp_content.info_Stock[i] != NULL)
					fprintf(fd, "    %02d. %s\n", ++lfd, rtp_content.info_Stock[i]);
			}
		}

		if (rtp_content.info_OtherIndex >= 0) {
			fprintf(fd, "--- Other ---\n");
			ind = rtp_content.info_OtherIndex;
			if (ind < (MAX_RTPC-1) && rtp_content.info_Other[ind+1] != NULL) {
				for (int i = ind+1; i < MAX_RTPC; i++) {
					if (rtp_content.info_Other[i] != NULL)
						fprintf(fd, "    %02d. %s\n", ++lfd, rtp_content.info_Other[i]);
				}
			}
			for (int i = 0; i <= ind; i++) {
				if (rtp_content.info_Other[i] != NULL)
					fprintf(fd, "    %02d. %s\n", ++lfd, rtp_content.info_Other[i]);
			}
		}

		fprintf(fd, "--- Last seen Radiotext ---\n");
		ind = rtp_content.rt_Index;
		if (ind < (2*MAX_RTPC-1) && rtp_content.radiotext[ind+1] != NULL) {
			for (int i = ind+1; i < 2*MAX_RTPC; i++) {
				if (rtp_content.radiotext[i] != NULL)
					fprintf(fd, "    %03d. %s\n", ++lfd, rtp_content.radiotext[i]);
			}
		}
		for (int i = 0; i <= ind; i++) {
			if (rtp_content.radiotext[i] != NULL)
				fprintf(fd, "    %03d. %s\n", ++lfd, rtp_content.radiotext[i]);
		}

		fprintf(fd, "<<<\n");
		fclose(fd);

		char *infotext;
		asprintf(&infotext, "%s: %s", tr("RTplus-File saved"), fname);
		Skins.Message(mtInfo, infotext, Setup.OSDMessageTime);
		free(infotext);
	}
	else
		printfesyslog("ERROR vdr-radio: writing RTplus-File failed '%s'", fpath);

	free(fpath);
	free(fname);
}

eOSState cRTplusOsd::ProcessKey(eKeys Key)
{
	int typ, ind;
	eOSState state = cOsdMenu::ProcessKey(Key);

	if (HasSubMenu())
		return osContinue;

	if (state == osUnknown) {
		switch (Key) {
			case kBack:
			case kOk:
				return osEnd;

			case kBlue:
				if (bcount >= 4 && helpmode == 0) {
					helpmode += 1;
					Update();
				}
				else if (bcount >= 6 && helpmode == 1) {
					helpmode += 1;
					Update();
				}
				else 
					return osEnd;
				break;

			case k0:
				Update();
				break;

			case k8:
				rtp_fileprint();
				break;

			case kRed:
				if (helpmode == 0) {
					if (btext[0] != NULL)
						if ((typ = rtptyp(btext[0])) >= 0)
					AddSubMenu(new cRTplusList(typ));
				}
				else {
					helpmode -= 1;
					Update();
				}
				break;

			case kGreen:
				ind = (helpmode*2) + 1;
				if (btext[ind] != NULL) {
					if ((typ = rtptyp(btext[ind])) >= 0)
						AddSubMenu(new cRTplusList(typ));
				}
				break;

			case kYellow:
				ind = (helpmode*2) + 2;
				if (btext[ind] != NULL) {
					if ((typ = rtptyp(btext[ind])) >= 0)
						AddSubMenu(new cRTplusList(typ));
				}
				break;
			default:
				state = osContinue;
		}
	}

	static int ct;
	if (++ct >= 60) {
		ct = 0;
		Update();
	}

	return state;
}


// --- cRTplusList ------------------------------------------------------

cRTplusList::cRTplusList(int Typ)
:cOsdMenu(RTp_Titel, 4)
{
	typ = Typ;
	refresh = false;

	Load();
	Display();
}

cRTplusList::~cRTplusList() 
{
	typ = 0;
}

void cRTplusList::Load(void) 
{
	char text[80];
	struct tm *ts, tm_store;
	int ind, lfd = 0;

	ts = localtime_r(&rtp_content.start, &tm_store);
	switch (typ) {
		case 0:
			snprintf(text, sizeof(text), "-- %s (max. %d) --", tr("last seen Radiotext"), 2*MAX_RTPC);
			Add(new cOsdItem(hk(text)));
			snprintf(text, sizeof(text), "%s", " ");
			Add(new cOsdItem(hk(text)));
			ind = rtp_content.rt_Index;
			if (ind < (2*MAX_RTPC-1) && rtp_content.radiotext[ind+1] != NULL) {
				for (int i = ind+1; i < 2*MAX_RTPC; i++) {
					if (rtp_content.radiotext[i] != NULL) {
						snprintf(text, sizeof(text), "%d.\t%s", ++lfd, rtp_content.radiotext[i]);
						Add(new cOsdItem(hk(text)));
					}
				}
			}
			for (int i = 0; i <= ind; i++) {
				if (rtp_content.radiotext[i] != NULL) {
					snprintf(text, sizeof(text), "%d.\t%s", ++lfd, rtp_content.radiotext[i]);
					Add(new cOsdItem(hk(text)), refresh);
				}
			}
			break;

		case 1:
			SetCols(6, 19, 1);
			snprintf(text, sizeof(text), "-- %s --", tr("Playlist"));
			Add(new cOsdItem(hk(text)));
			snprintf(text, sizeof(text), "%s\t%s\t\t%s", tr("Time"), tr("Title"), tr("Artist"));
			Add(new cOsdItem(hk(text)));
			snprintf(text, sizeof(text), "%s", " ");
			Add(new cOsdItem(hk(text)));
			ind = rtp_content.item_Index;
			if (ind < (MAX_RTPC-1) && rtp_content.item_Title[ind+1] != NULL) {
				for (int i = ind+1; i < MAX_RTPC; i++) {
					if (rtp_content.item_Title[i] != NULL && rtp_content.item_Artist[i] != NULL) {
						ts = localtime_r(&rtp_content.item_Start[i], &tm_store);
						snprintf(text, sizeof(text), "%02d:%02d\t%s\t\t%s", ts->tm_hour, ts->tm_min, rtp_content.item_Title[i], rtp_content.item_Artist[i]);
						Add(new cOsdItem(hk(text)));
					}
				}
			}
			for (int i = 0; i <= ind; i++) {
				if (rtp_content.item_Title[i] != NULL && rtp_content.item_Artist[i] != NULL) {
					ts = localtime_r(&rtp_content.item_Start[i], &tm_store);
					snprintf(text, sizeof(text), "%02d:%02d\t%s\t\t%s", ts->tm_hour, ts->tm_min, rtp_content.item_Title[i], rtp_content.item_Artist[i]);
					Add(new cOsdItem(hk(text)), refresh);
				}
			}
			break;

		case 2:
			snprintf(text, sizeof(text), "-- %s --", tr("Sports"));
			Add(new cOsdItem(hk(text)));
			snprintf(text, sizeof(text), "%s", " ");
			Add(new cOsdItem(hk(text)));
			ind = rtp_content.info_SportIndex;
			if (ind < (MAX_RTPC-1) && rtp_content.info_Sport[ind+1] != NULL) {
				for (int i = ind+1; i < MAX_RTPC; i++) {
					if (rtp_content.info_Sport[i] != NULL) {
						snprintf(text, sizeof(text), "%d.\t%s", ++lfd, rtp_content.info_Sport[i]);
						Add(new cOsdItem(hk(text)));
					}
				}
			}
			for (int i = 0; i <= ind; i++) {
				if (rtp_content.info_Sport[i] != NULL) {
					snprintf(text, sizeof(text), "%d.\t%s", ++lfd, rtp_content.info_Sport[i]);
					Add(new cOsdItem(hk(text)), refresh);
				}
			}
			break;

		case 3:
			snprintf(text, sizeof(text), "-- %s --", tr("Lottery"));
			Add(new cOsdItem(hk(text)));
			snprintf(text, sizeof(text), "%s", " ");
			Add(new cOsdItem(hk(text)));
			ind = rtp_content.info_LotteryIndex;
			if (ind < (MAX_RTPC-1) && rtp_content.info_Lottery[ind+1] != NULL) {
				for (int i = ind+1; i < MAX_RTPC; i++) {
					if (rtp_content.info_Lottery[i] != NULL) {
						snprintf(text, sizeof(text), "%d.\t%s", ++lfd, rtp_content.info_Lottery[i]);
						Add(new cOsdItem(hk(text)));
					}
				}
			}
			for (int i = 0; i <= ind; i++) {
				if (rtp_content.info_Lottery[i] != NULL) {
					snprintf(text, sizeof(text), "%d.\t%s", ++lfd, rtp_content.info_Lottery[i]);
					Add(new cOsdItem(hk(text)), refresh);
				}
			}
			break;

		case 4:	 snprintf(text, sizeof(text), "-- %s --", tr("Weather"));
			Add(new cOsdItem(hk(text)));
			snprintf(text, sizeof(text), "%s", " ");
			Add(new cOsdItem(hk(text)));
			ind = rtp_content.info_WeatherIndex;
			if (ind < (MAX_RTPC-1) && rtp_content.info_Weather[ind+1] != NULL) {
				for (int i = ind+1; i < MAX_RTPC; i++) {
					if (rtp_content.info_Weather[i] != NULL) {
						snprintf(text, sizeof(text), "%d.\t%s", ++lfd, rtp_content.info_Weather[i]);
						Add(new cOsdItem(hk(text)));
					}
				}
			}
			for (int i = 0; i <= ind; i++) {
				if (rtp_content.info_Weather[i] != NULL) {
					snprintf(text, sizeof(text), "%d.\t%s", ++lfd, rtp_content.info_Weather[i]);
					Add(new cOsdItem(hk(text)), refresh);
				}
			}
			break;
		case 5:
			snprintf(text, sizeof(text), "-- %s --", tr("Stockmarket"));
			Add(new cOsdItem(hk(text)));
			snprintf(text, sizeof(text), "%s", " ");
			Add(new cOsdItem(hk(text)));
			ind = rtp_content.info_StockIndex;
			if (ind < (MAX_RTPC-1) && rtp_content.info_Stock[ind+1] != NULL) {
				for (int i = ind+1; i < MAX_RTPC; i++) {
					if (rtp_content.info_Stock[i] != NULL) {
						snprintf(text, sizeof(text), "%d.\t%s", ++lfd, rtp_content.info_Stock[i]);
						Add(new cOsdItem(hk(text)));
					}
				}
			}
			for (int i = 0; i <= ind; i++) {
				if (rtp_content.info_Stock[i] != NULL) {
					snprintf(text, sizeof(text), "%d.\t%s", ++lfd, rtp_content.info_Stock[i]);
					Add(new cOsdItem(hk(text)), refresh);
				}
			}
			break;

		case 6:
			snprintf(text, sizeof(text), "-- %s --", tr("Other"));
			Add(new cOsdItem(hk(text)));
			snprintf(text, sizeof(text), "%s", " ");
			Add(new cOsdItem(hk(text)));
			ind = rtp_content.info_OtherIndex;
			if (ind < (MAX_RTPC-1) && rtp_content.info_Other[ind+1] != NULL) {
				for (int i = ind+1; i < MAX_RTPC; i++) {
					if (rtp_content.info_Other[i] != NULL) {
						snprintf(text, sizeof(text), "%d.\t%s", ++lfd, rtp_content.info_Other[i]);
						Add(new cOsdItem(hk(text)));
					}
				}
			}
			for (int i = 0; i <= ind; i++) {
				if (rtp_content.info_Other[i] != NULL) {
					snprintf(text, sizeof(text), "%d.\t%s", ++lfd, rtp_content.info_Other[i]);
					Add(new cOsdItem(hk(text)), refresh);
				}
			}
			break;
	}

	SetHelp(NULL, NULL , refresh ? tr("Refresh Off") :  tr("Refresh On"), tr("Back"));
}

void cRTplusList::Update(void)
{
	Clear();
	Load();
	Display();
}

eOSState cRTplusList::ProcessKey(eKeys Key)
{
	eOSState state = cOsdMenu::ProcessKey(Key);

	if (state == osUnknown) {
		switch (Key) {
			case k0:
				Update();
				break;
			case kYellow:
				refresh = (refresh) ? false : true;
				Update();
				break;
			case kBack:
			case kOk:
			case kBlue:
				return osBack;
			default:
				state = osContinue;
		}
	}

	static int ct;
	if (refresh) {
		if (++ct >= 20) {
			ct = 0;
			Update();
		}
	}
	
	return state;
}

//--------------- End -----------------------------------------------------------------
#endif

CRadioText::CRadioText(void)
{
	pid = 0;
	audioDemux = NULL;
	init();

	running = true;
	start();
}

CRadioText::~CRadioText(void)
{
	printf("CRadioText::~CRadioText\n");
	running = false;
	radiotext_stop();
	cond.broadcast();
	OpenThreads::Thread::join();
	printf("CRadioText::~CRadioText done\n");
}

void CRadioText::init()
{
	S_Verbose 	= 0;
	S_RtFunc 	= 1;
	S_RtOsd 	= 0;
	S_RtOsdTitle 	= 1;
	S_RtOsdTags 	= 2;
	S_RtOsdPos 	= 2;
	S_RtOsdRows 	= 3;
	S_RtOsdLoop 	= 1;
	S_RtOsdTO 	= 60;
	S_RtSkinColor 	= false;
	S_RtBgCol 	= 0;
	S_RtBgTra 	= 0xA0;
	S_RtFgCol 	= 1;
	S_RtDispl 	= 1;
	S_RtMsgItems 	= 0;
	RT_Index 	= 0;
	RT_PTY 		= 0;

	// Radiotext
	RTP_ItemToggle 	= 1;
	RTP_TToggle 	= 0;
	RT_PlusShow 	= false;
	RT_Replay 	= false;
	RT_ReOpen 	= false;
	for (int i = 0; i < 5; i++)
		RT_Text[i][0] = 0;
	RDS_PTYN[0] = 0;

#if ENABLE_RASS
	// Rass ...
	Rass_Show = -1;		// -1=No, 0=Yes, 1=display
	Rass_Archiv = -1;	// -1=Off, 0=Index, 1000-9990=Slidenr.
#endif
	RT_MsgShow = false; // clear entries from old channel
	have_radiotext	= false;
}

void CRadioText::radiotext_stop(void)
{
	printf("CRadioText::radiotext_stop: ###################### pid 0x%x ######################\n", getPid());
	if (getPid() != 0) {
		mutex.lock();
		pid = 0;
		have_radiotext = false;
		S_RtOsd = 0;
		mutex.unlock();
	}
}

void CRadioText::setPid(uint inPid)
{
	printf("CRadioText::setPid: ###################### old pid 0x%x new pid 0x%x ######################\n", pid, inPid);
	if (pid != inPid) {
		mutex.lock();
		pid = inPid;
		init();
		mutex.unlock();
		cond.broadcast();
	}
}

void CRadioText::run()
{
	set_threadname("n:radiotext");
	uint current_pid = 0;

	printf("CRadioText::run: ###################### Starting thread ######################\n");
#if HAVE_SPARK_HARDWARE || HAVE_DUCKBOX_HARDWARE || HAVE_GENERIC_HARDWARE
	int buflen = 0;
	unsigned char *buf = NULL;
	audioDemux = new cDemux(0); // live demux
#else
	audioDemux = new cDemux(1);
#endif
	audioDemux->Open(DMX_PES_CHANNEL,0,128*1024);

	while(running) {
		mutex.lock();
		if (pid == 0) {
			mutex.unlock();
			audioDemux->Stop();
			pidmutex.lock();
			printf("CRadioText::run: ###################### waiting for pid.. ######################\n");
			cond.wait(&pidmutex);
			pidmutex.unlock();
			mutex.lock();
		}
		if (pid && (current_pid != pid)) {
			current_pid = pid;
			printf("CRadioText::run: ###################### Setting PID 0x%x ######################\n", getPid());
			audioDemux->Stop();
			if (!audioDemux->pesFilter(getPid()) || !audioDemux->Start()) {
				pid = 0;
				printf("CRadioText::run: ###################### failed to start PES filter ######################\n");
			}
		}
		mutex.unlock();
		if (pid) {
#if HAVE_SPARK_HARDWARE || HAVE_DUCKBOX_HARDWARE || HAVE_GENERIC_HARDWARE
			int n;
			unsigned char tmp[6];

			n = audioDemux->Read(tmp, 6, 500);
			if (n != 6) {
				usleep(10000); /* save CPU if nothing read */
				continue;
			}
			if (memcmp(tmp, "\000\000\001\300", 4))
				continue;
			int packlen = ((tmp[4] << 8) | tmp[5]) + 6;

			if (buflen < packlen) {
				if (buf)
					free(buf);
				buf = (unsigned char *) calloc(1, packlen);
				buflen = packlen;
			}
			if (!buf)
				break;
			memcpy(buf, tmp, 6);

			while ((n < packlen) && running) {
				int len = audioDemux->Read(buf + n, packlen - n, 500);
				if (len < 0)
					break;
				n += len;
			}
#else
			int n;
			unsigned char buf[0x1FFFF];

			n = audioDemux->Read(buf, sizeof(buf), 500 /*5000*/);
#endif
			if (n > 0) {
				//printf("."); fflush(stdout);
				mutex.lock();
				PES_Receive(buf, n);
				mutex.unlock();
			}
		}
	}
#if HAVE_SPARK_HARDWARE || HAVE_DUCKBOX_HARDWARE || HAVE_GENERIC_HARDWARE
	if (buf)
		free(buf);
#endif
	delete audioDemux;
	audioDemux = NULL;
	printf("CRadioText::run: ###################### exit ######################\n");
}
