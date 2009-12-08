/*
 * $Id: pmt.cpp,v 1.40 2004/04/04 20:46:17 obi Exp $
 *
 * (C) 2002 by Andreas Oberritter <obi@tuxbox.org>
 * (C) 2002 by Frank Bormann <happydude@berlios.de>
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
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* zapit */
#include <zapit/settings.h>
#include <zapit/descriptors.h>
#include <zapit/debug.h>
#include <zapit/pmt.h>
#include <dmx_cs.h>

#include <dvb-ci.h>
#include <linux/dvb/dmx.h>

#define PMT_SIZE 1024
#define RECORD_MODE 0x4
extern int currentMode;
extern short scan_runs;
/*
 * Stream types
 * ------------
 * 0x01 ISO/IEC 11172 Video
 * 0x02 ITU-T Rec. H.262 | ISO/IEC 13818-2 Video or ISO/IEC 11172-2 constrained parameter video stream
 * 0x03 ISO/IEC 11172 Audio
 * 0x04 ISO/IEC 13818-3 Audio
 * 0x05 ITU-T Rec. H.222.0 | ISO/IEC 13818-1 private_sections, e.g. MHP Application signalling stream
 * 0x06 ITU-T Rec. H.222.0 | ISO/IEC 13818-1 PES packets containing private data, e.g. teletext or ac3
 * 0x0b ISO/IEC 13818-6 type B
 * 0x81 User Private (MTV)
 * 0x90 User Private (Premiere Mail, BD_DVB)
 * 0xc0 User Private (Canal+)
 * 0xc1 User Private (Canal+)
 * 0xc6 User Private (Canal+)
 */

unsigned short parse_ES_info(const unsigned char * const buffer, CZapitChannel * const channel, CCaPmt * const caPmt)
{
	unsigned short ES_info_length;
	unsigned short pos;
	unsigned char descriptor_tag;
	unsigned char descriptor_length;
	unsigned char i;

	bool isAc3 = false;
	bool isDts = false;
	bool descramble = false;
	std::string description = "";
	unsigned char componentTag = 0xFF;

	/* elementary stream info for ca pmt */
	CEsInfo *esInfo = new CEsInfo();

	esInfo->stream_type = buffer[0];
	esInfo->reserved1 = buffer[1] >> 5;
	esInfo->elementary_PID = ((buffer[1] & 0x1F) << 8) | buffer[2];
	esInfo->reserved2 = buffer[3] >> 4;

	ES_info_length = ((buffer[3] & 0x0F) << 8) | buffer[4];

	for (pos = 5; pos < ES_info_length + 5; pos += descriptor_length + 2) {
		descriptor_tag = buffer[pos];
		descriptor_length = buffer[pos + 1];
		unsigned char fieldCount = descriptor_length / 5;

		switch (descriptor_tag) {
			case 0x02:
				video_stream_descriptor(buffer + pos);
				break;

			case 0x03:
				audio_stream_descriptor(buffer + pos);
				break;

			case 0x05:
				if (descriptor_length >= 3)
					if (!strncmp((const char*)&buffer[pos + 2], "DTS", 3))
						isDts = true;
				break;

			case 0x09:
				esInfo->addCaDescriptor(buffer + pos);
				break;

			case 0x0A: /* ISO_639_language_descriptor */
				for (i = 0; i < 3; i++)
					description += buffer[pos + i + 2];
				break;

			case 0x13: /* Defined in ISO/IEC 13818-6 */
				break;

			case 0x0E:
				Maximum_bitrate_descriptor(buffer + pos);
				break;

			case 0x0F:
				Private_data_indicator_descriptor(buffer + pos);
				break;

			case 0x11:
				STD_descriptor(buffer + pos);
				break;

			case 0x45:
				VBI_data_descriptor(buffer + pos);
				break;

			case 0x52: /* stream_identifier_descriptor */
				componentTag = buffer[pos + 2];
				break;

			case 0x56: /* teletext descriptor */
				for (unsigned char fIdx=0;fIdx<fieldCount;fIdx++) {
					char tmpLang[4];
					memcpy(tmpLang, &buffer[pos + 5*fIdx + 2], 3);
					tmpLang[3] = '\0';
					unsigned char teletext_type=buffer[pos + 5*fIdx + 5]>> 3;
					unsigned char teletext_magazine_number = buffer[pos + 5*fIdx + 5] & 7;
					unsigned char teletext_page_number=buffer[pos + 5*fIdx + 6];
					if (teletext_type==0x02){
						channel->addTTXSubtitle(esInfo->elementary_PID,tmpLang,teletext_magazine_number,teletext_page_number);
					} else {
						if (teletext_type==0x05){
							channel->addTTXSubtitle(esInfo->elementary_PID,tmpLang,teletext_magazine_number,teletext_page_number,true);
						}
					}
				}

				channel->setTeletextPid(esInfo->elementary_PID);
				descramble = true;//FIXME ORF HD scramble txt ?
				break;

			case 0x59:
				if (esInfo->stream_type==0x06) {
					unsigned char fieldCount=descriptor_length/8;
					for (unsigned char fIdx=0;fIdx<fieldCount;fIdx++){
						char tmpLang[4];
						memcpy(tmpLang,&buffer[pos + 8*fIdx + 2],3);
						tmpLang[3] = '\0';
						unsigned char subtitling_type=buffer[pos+8*fIdx+5];
						unsigned short composition_page_id=
							*((unsigned short*)(&buffer[pos + 8*fIdx + 6]));
						unsigned short ancillary_page_id=
							*((unsigned short*)(&buffer[pos + 8*fIdx + 8]));
						channel->addDVBSubtitle(esInfo->elementary_PID,tmpLang,subtitling_type,composition_page_id,ancillary_page_id);
					}
					descramble = true;//FIXME MGM / 10E scrambling subtitles ?
				}

				subtitling_descriptor(buffer + pos);
				break;

			case 0x5F:
				private_data_specifier_descriptor(buffer + pos);
				break;

			case 0x66:
				data_broadcast_id_descriptor(buffer + pos);
				break;

			case 0x6A: /* AC3 descriptor */
				isAc3 = true;
				break;

			case 0x6F: /* unknown, Astra 19.2E */
				break;

			case 0x7B:
				isDts = true;
				break;

			case 0x7C: //FIXME AAC
				break;

			case 0x90: /* unknown, Astra 19.2E */
				break;

			case 0xB1: /* unknown, Astra 19.2E */
				break;

			case 0xC0: /* unknown, Astra 19.2E */
				break;

			case 0xC1: /* unknown, Astra 19.2E */
				break;

			case 0xC2: /* User Private descriptor - Canal+ */
#if 0
				DBG("0xC2 dump:");
				for (i = 0; i < descriptor_length; i++) {
					printf("%c", buffer[pos + 2 + i]);
					if (((i+1) % 8) == 0)
						printf("\n");
				}
#endif
				break;

			case 0xC5: /* User Private descriptor - Canal+ Radio */
				for (i = 0; i < 24; i++)
					description += buffer[pos + i + 3];
				break;

			case 0xC6: /* unknown, Astra 19.2E */
				break;

			case 0xFD: /* unknown, Astra 19.2E */
				break;

			case 0xFE: /* unknown, Astra 19.2E */
				break;

			default:
				DBG("descriptor_tag: %02x\n", descriptor_tag);
				break;
		}
	}

	switch (esInfo->stream_type) {
		case 0x01:
		case 0x02:
		case 0x1b: // AVC Video Stream (MPEG4 H264)
			channel->setVideoPid(esInfo->elementary_PID);
			descramble = true;
			channel->type = (esInfo->stream_type == 0x1b); //FIXME
			printf("[pmt] vpid %x stream %d type %d\n", esInfo->elementary_PID, esInfo->stream_type, channel->type);
			break;

		case 0x03:
		case 0x04:
			if (description == "")
				description = esInfo->elementary_PID;
			if(scan_runs) {
				if(channel->getPreAudioPid() == 0)
					channel->setAudioPid(esInfo->elementary_PID);
			} else
				channel->addAudioChannel(esInfo->elementary_PID, false, description, componentTag);
			descramble = true;
			printf("[pmt] apid %x: %s\n", esInfo->elementary_PID, description.c_str());
			break;

		case 0x05:// private section
			{
				int tmp=0;	
				// Houdini: shameless stolen from enigma dvbservices.cpp
				for (pos = 5; pos < ES_info_length + 5; pos += descriptor_length + 2) {
					descriptor_tag = buffer[pos];
					descriptor_length = buffer[pos + 1];

					switch (descriptor_tag) {
						case 0x5F: //DESCR_PRIV_DATA_SPEC:
							if ( ((buffer[pos + 2]<<24) | (buffer[pos + 3]<<16) | (buffer[pos + 4]<<8) | (buffer[pos + 5])) == 190 )
								tmp |= 1;
							break;
						case 0x90:
							{
								if ( descriptor_length == 4 && !buffer[pos + 2] && !buffer[pos + 3] && buffer[pos + 4] == 0xFF && buffer[pos + 5] == 0xFF )
									tmp |= 2;
							}
							//break;??
						default:
							break;
					}
				}
				if ( tmp == 3 ) {
					channel->setPrivatePid(esInfo->elementary_PID);
					DBG("channel->setPrivatePid(%x)\n", esInfo->elementary_PID);
				}
				descramble = true;
				break;
			}
		case 0x81:
			esInfo->stream_type = 0x6;
			if (description == "")
				description = esInfo->elementary_PID;
			description += " (AC3)";
			isAc3 = true;
			descramble = true;
			if(!scan_runs)
				channel->addAudioChannel(esInfo->elementary_PID, true, description, componentTag);
			break;
		case 0x06:
			if ((isAc3) || (isDts)) {
				if (description == "") {
					description = esInfo->elementary_PID;
					if (isAc3)
						description += " (AC3)";
					else if (isDts)
						description += " (DTS)";
				}
				if(!scan_runs)
					channel->addAudioChannel(esInfo->elementary_PID, true, description, componentTag);
				descramble = true;
			}
			break;

		case 0x0B:
			break;

		case 0x90:
			break;

		case 0x93:
			break;

		case 0xC0:
			break;

		case 0xC1:
			break;

		case 0xC6:
			break;

		default:
			DBG("stream_type: %02x\n", esInfo->stream_type);
			break;
	}

	if (descramble)
		caPmt->es_info.insert(caPmt->es_info.end(), esInfo);
	else
		delete esInfo;

	return ES_info_length;
}

int curpmtpid;
int pmt_caids[10] = {0,0,0,0,0,0,0,0,0,0};

int parse_pmt(CZapitChannel * const channel)
{
	int pmtlen;
	int ia, dpmtlen, pos;
	unsigned char descriptor_length=0;
	FILE *fout;

	unsigned char buffer[PMT_SIZE];

	/* current position in buffer */
	unsigned short i;
	for(i=0;i<10;i++)
		pmt_caids[i] = 0;

	/* length of elementary stream description */
	unsigned short ES_info_length;

	/* TS_program_map_section elements */
	unsigned short section_length;
	unsigned short program_info_length;

	unsigned char filter[DMX_FILTER_SIZE];
	unsigned char mask[DMX_FILTER_SIZE];

	printf("[zapit] parsing pmt pid 0x%X\n", channel->getPmtPid());

	if (channel->getPmtPid() == 0){
		return -1;
	}

	cDemux * dmx = new cDemux();
	dmx->Open(DMX_PSI_CHANNEL);

	memset(filter, 0x00, DMX_FILTER_SIZE);
	memset(mask, 0x00, DMX_FILTER_SIZE);

	filter[0] = 0x02;	/* table_id */
	filter[1] = channel->getServiceId() >> 8;
	filter[2] = channel->getServiceId();
	filter[3] = 0x01;	/* current_next_indicator */
	filter[4] = 0x00;	/* section_number */
	mask[0] = 0xFF;
	mask[1] = 0xFF;
	mask[2] = 0xFF;
	mask[3] = 0x01;
	mask[4] = 0xFF;

	if ((dmx->sectionFilter(channel->getPmtPid(), filter, mask, 5) < 0) || (dmx->Read(buffer, PMT_SIZE) < 0)) {
		delete dmx;
		return -1;
	}
	delete dmx;

	extern cDvbCi *ci;

	curpmtpid = channel->getPmtPid();
	pmtlen= ((buffer[1]&0xf)<<8) + buffer[2] +3;

	if(!(currentMode & RECORD_MODE) && !scan_runs) {
		ci->SendPMT(buffer, pmtlen);
		fout = fopen("/tmp/pmt.tmp","wb"); 
		if(fout != NULL) {
			if ((int) fwrite(buffer, sizeof(unsigned char), pmtlen, fout) != pmtlen) {
				unlink("/tmp/pmt.tmp");
			}
			fclose(fout);
		}
	}

	dpmtlen=0;
	pos=10;
	if(!scan_runs) {
		while(pos+2<pmtlen) {
			dpmtlen=((buffer[pos] & 0x0f) << 8) | buffer[pos+1];
			for ( ia=pos+2;ia<(dpmtlen+pos+2);ia +=descriptor_length+2 ) {
				descriptor_length = buffer[ia+1];
				if ( ia < pmtlen - 4 )
					if(buffer[ia]==0x09 && buffer[ia+1]>0) {
						switch(buffer[ia+2]) {
							case 0x06: pmt_caids[0] = 1;
							case 0x17: pmt_caids[0] = 1;
								   break;
							case 0x01: pmt_caids[1] = 1;
								   break;
							case 0x05: pmt_caids[2] = 1;
								   break;
							case 0x18: pmt_caids[3] = 1;
								   break;
							case 0x0B: pmt_caids[4] = 1;
								   break;
							case 0x0D: pmt_caids[5] = 1;
								   break;
							case 0x09: pmt_caids[6] = 1;
								   break;
							case 0x26: pmt_caids[7] = 1;
								   break;
							case 0x4a: pmt_caids[8] = 1;
								   break;
							case 0x0E: pmt_caids[9] = 1;
								   break;
						} //switch
					} // if
			} // for
			pos+=dpmtlen+5;
		} // while
#if 0
		fout=fopen("/tmp/caids.info","w");
		if(fout) {
			fprintf(fout, "%d %d %d %d %d %d %d %d %d %d\n", caids[0],caids[1],caids[2],caids[3],caids[4],caids[5], caids[6], caids[7], caids[8], caids[9]);
			fclose(fout);
		}

#endif
	} /* if !scan_runs */
	CCaPmt *caPmt = new CCaPmt();

	/* ca pmt */
	caPmt->program_number = (buffer[3] << 8) + buffer[4];
	caPmt->reserved1 = buffer[5] >> 6;
	caPmt->version_number = (buffer[5] >> 1) & 0x1F;
	caPmt->current_next_indicator = buffer[5] & 0x01;
	caPmt->reserved2 = buffer[10] >> 4;

	printf("[pmt] pcr pid: old 0x%x new 0x%x\n", channel->getPcrPid(), ((buffer[8] & 0x1F) << 8) + buffer[9]);

	if(channel->getCaPmt() != 0) {
		if(channel->getCaPmt()->version_number != caPmt->version_number)
			channel->resetPids();
	}
	/* pmt */
	section_length = ((buffer[1] & 0x0F) << 8) + buffer[2];

	//if(!channel->getPidsFlag()) 
		channel->setPcrPid(((buffer[8] & 0x1F) << 8) + buffer[9]);

	program_info_length = ((buffer[10] & 0x0F) << 8) | buffer[11];

	if (program_info_length)
		for (i = 12; i < 12 + program_info_length; i += buffer[i + 1] + 2)
			switch (buffer[i]) {
				case 0x09:
					caPmt->addCaDescriptor(buffer + i);
					break;
				default:
					DBG("decriptor_tag: %02x\n", buffer[i]);
					break;
			}

	/* pmt */
	for (i = 12 + program_info_length; i < section_length - 1; i += ES_info_length + 5)
		ES_info_length = parse_ES_info(buffer + i, channel, caPmt);

	if(scan_runs) {
		if(channel->getCaPmt() != 0)
			delete channel->getCaPmt();
		channel->setCaPmt(NULL);
		delete caPmt;
	} else {
		if(channel->getCaPmt() != 0) 
			delete channel->getCaPmt();
		channel->setCaPmt(caPmt);
	}
#if 0
	//Quick&Dirty Hack to support Premiere's EPG not only on the portal but on the subchannels as well
	if (channel->getOriginalNetworkId() == 0x0085) {
		if (channel->getTransportStreamId() ==0x0003)
			channel->setPrivatePid(0x0b12);
		if (channel->getTransportStreamId() ==0x0004)
			channel->setPrivatePid(0x0b11);
	}
#endif
	channel->setPidsFlag();

	return 0;
}
#ifndef DMX_SET_NEGFILTER_MASK
        #define DMX_SET_NEGFILTER_MASK   _IOW('o',48,uint8_t *)
#endif

cDemux * pmtDemux;

int pmt_set_update_filter(CZapitChannel * const channel, int * fd)
{
	unsigned char filter[DMX_FILTER_SIZE];
	unsigned char mask[DMX_FILTER_SIZE];
	unsigned char mode[DMX_FILTER_SIZE];

	if(pmtDemux == NULL) {
		pmtDemux = new cDemux(0);
		pmtDemux->Open(DMX_PSI_CHANNEL);
	}

	if (channel->getPmtPid() == 0)
		return -1;

	memset(filter, 0x00, DMX_FILTER_SIZE);
	memset(mask, 0x00, DMX_FILTER_SIZE);
	memset(mode, 0x00, DMX_FILTER_SIZE);

	filter[0] = 0x02;	/* table_id */
	filter[1] = channel->getServiceId() >> 8;
	filter[2] = channel->getServiceId();
	filter[4] = 0x00;	/* section_number */

	mask[0] = 0xFF;
	mask[1] = 0xFF;
	mask[2] = 0xFF;
	mask[4] = 0xFF;

	printf("[pmt] set update filter, sid 0x%x pid 0x%x version %x\n", channel->getServiceId(), channel->getPmtPid(), channel->getCaPmt()->version_number);
#if 1
	filter[3] = (((channel->getCaPmt()->version_number + 1) & 0x01) << 1) | 0x01;
	mask[3] = (0x01 << 1) | 0x01;
	pmtDemux->sectionFilter(channel->getPmtPid(), filter, mask, 5);
#else
	filter[3] = (channel->getCaPmt()->version_number << 1) | 0x01;
	mask[3] = (0x1F << 1) | 0x01;
	mode[3] = 0x1F << 1;
	pmtDemux->sectionFilter(channel->getPmtPid(), filter, mask, 5, 0, mode);
#endif

	*fd = 1;
	return 0;
}

int pmt_stop_update_filter(int * fd)
{       
printf("[pmt] stop update filter\n");
	if(pmtDemux)
		pmtDemux->Stop();

	*fd = -1;
        return 0;
}
