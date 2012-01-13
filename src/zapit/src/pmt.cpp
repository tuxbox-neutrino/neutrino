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
#include <zapit/scan.h>
#include <sectionsd/edvbstring.h>
#include <dmx.h>

#include <ca_cs.h>
//#include <linux/dvb/dmx.h>

#define PMT_SIZE 1024
#define RECORD_MODE 0x4

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

CPmt::CPmt(int dnum)
{
	dmxnum = dnum;
}

CPmt::~CPmt()
{
}

unsigned short CPmt::ParseES(const unsigned char * const buff, CZapitChannel * const channel, CCaPmt * const caPmt)
{
	unsigned short ES_info_length;
	unsigned short pos;
	unsigned char descriptor_tag;
	unsigned char descriptor_length;
	unsigned char i;

	bool isAc3 = false;
	bool isDts = false;
	bool isAac = false;
	bool descramble = false;
	std::string description = "";
	unsigned char componentTag = 0xFF;

	/* elementary stream info for ca pmt */
	CEsInfo *esInfo = new CEsInfo();

	esInfo->stream_type = buff[0];
	esInfo->reserved1 = buff[1] >> 5;
	esInfo->elementary_PID = ((buff[1] & 0x1F) << 8) | buff[2];
	esInfo->reserved2 = buff[3] >> 4;

	ES_info_length = ((buff[3] & 0x0F) << 8) | buff[4];

	for (pos = 5; pos < ES_info_length + 5; pos += descriptor_length + 2) {
		descriptor_tag = buff[pos];
		descriptor_length = buff[pos + 1];
		unsigned char fieldCount = descriptor_length / 5;

		switch (descriptor_tag) {
			case 0x05:
				if (descriptor_length >= 3)
					if (!strncmp((const char*)&buff[pos + 2], "DTS", 3))
						isDts = true;
				break;

			case 0x09:
				esInfo->addCaDescriptor(buff + pos);
				descramble = true;
				break;

			case 0x0A: /* ISO_639_language_descriptor */
#if 0
printf("descr 0x0A: %02X %02X %02X\n", buff[pos+2], buff[pos+3], buff[pos+4]);
#endif
				/* FIXME cyfra+ radio -> 41 20 31 ?? */
				if (description != "" && buff[pos + 3] == ' ') {
					description += buff[pos + 3];
					description += buff[pos + 4];
				} else {
				for (i = 0; i < 3; i++)
					description += tolower(buff[pos + i + 2]);
				}
				break;

			case 0x52: /* stream_identifier_descriptor */
				componentTag = buff[pos + 2];
				break;

			case 0x56: /* teletext descriptor */
				char tmp_Lang[4];
				//printf("[pmt] teletext pid %x: %s\n", esInfo->elementary_PID, tmp_Lang);
				printf("[pmt] teletext pid %x\n", esInfo->elementary_PID);
				for (unsigned char fIdx = 0; fIdx < fieldCount; fIdx++) {
					memmove(tmp_Lang, &buff[pos + 5*fIdx + 2], 3);
					tmp_Lang[3] = '\0';
					unsigned char teletext_type=buff[pos + 5*fIdx + 5]>> 3;
					unsigned char teletext_magazine_number = buff[pos + 5*fIdx + 5] & 7;
					unsigned char teletext_page_number=buff[pos + 5*fIdx + 6];
					printf("[pmt] teletext type %d mag %d page %d lang %s\n", teletext_type, teletext_magazine_number, teletext_page_number, tmp_Lang);
					if (teletext_type==0x01)
						channel->setTeletextLang(tmp_Lang);
					if (teletext_type==0x02){
						channel->addTTXSubtitle(esInfo->elementary_PID,tmp_Lang,teletext_magazine_number,teletext_page_number);
					} else {
						if (teletext_type==0x05){
							channel->addTTXSubtitle(esInfo->elementary_PID,tmp_Lang,teletext_magazine_number,teletext_page_number,true);
						}
					}
				}

				channel->setTeletextPid(esInfo->elementary_PID);
				descramble = true;//FIXME ORF HD scramble txt ?
				break;

			case 0x59:
				if (esInfo->stream_type==0x06) {
					unsigned char fieldCount1=descriptor_length/8;
					for (unsigned char fIdx=0;fIdx<fieldCount1;fIdx++){
						char tmpLang[4];
						memmove(tmpLang,&buff[pos + 8*fIdx + 2],3);
						tmpLang[3] = '\0';
						unsigned char subtitling_type=buff[pos+8*fIdx+5];
						unsigned short composition_page_id=
							*((unsigned short*)(&buff[pos + 8*fIdx + 6]));
						unsigned short ancillary_page_id=
							*((unsigned short*)(&buff[pos + 8*fIdx + 8]));
						channel->addDVBSubtitle(esInfo->elementary_PID,tmpLang,subtitling_type,composition_page_id,ancillary_page_id);
					}
					descramble = true;//FIXME MGM / 10E scrambling subtitles ?
				}

				subtitling_descriptor(buff + pos);
				break;

			case 0x6A: /* AC3 descriptor */
				isAc3 = true;
				break;

			case 0x7B:
				isDts = true;
				break;

			case 0x7C: //FIXME AAC
				isAac = true;
				break;

			case 0xC5: /* User Private descriptor - Canal+ Radio */
				description = convertDVBUTF8((const char*)&buff[pos+3], 24, 2, channel->getTransportStreamId() << 16 | channel->getOriginalNetworkId());
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
				description = " ";
			if(CServiceScan::getInstance()->Scanning()) {
				if(channel->getPreAudioPid() == 0)
					channel->setAudioPid(esInfo->elementary_PID);
			} else
				channel->addAudioChannel(esInfo->elementary_PID, CZapitAudioChannel::MPEG, description, componentTag);
			descramble = true;
			printf("[pmt] apid %x: %s\n", esInfo->elementary_PID, description.c_str());
			break;

		case 0x05:// private section
			{
				int tmp=0;
				// Houdini: shameless stolen from enigma dvbservices.cpp
				for (pos = 5; pos < ES_info_length + 5; pos += descriptor_length + 2) {
					descriptor_tag = buff[pos];
					descriptor_length = buff[pos + 1];

					switch (descriptor_tag) {
						case 0x5F: //DESCR_PRIV_DATA_SPEC:
							if ( ((buff[pos + 2]<<24) | (buff[pos + 3]<<16) | (buff[pos + 4]<<8) | (buff[pos + 5])) == 190 )
								tmp |= 1;
							break;
						case 0x90:
							{
								if ( descriptor_length == 4 && !buff[pos + 2] && !buff[pos + 3] && buff[pos + 4] == 0xFF && buff[pos + 5] == 0xFF )
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
				description = "Unknown";
			description += " (AC3)";
			isAc3 = true;
			descramble = true;
			if(!CServiceScan::getInstance()->Scanning())
				channel->addAudioChannel(esInfo->elementary_PID, CZapitAudioChannel::AC3, description, componentTag);
			break;
		case 0x06:
			if ((isAc3) || (isDts) || (isAac)) {
				if (description == "") {
					description = esInfo->elementary_PID;
					if (isAc3)
						description += " (AC3)";
					else if (isDts)
						description += " (DTS)";
					else if (isAac)
						description += " (AAC)";
				}

				if(!CServiceScan::getInstance()->Scanning()) {
					CZapitAudioChannel::ZapitAudioChannelType Type;
					if (isAc3)
						Type = CZapitAudioChannel::AC3;
					else if (isDts)
						Type = CZapitAudioChannel::DTS;
					else if (isAac)
						Type = CZapitAudioChannel::AAC;
					else
						Type = CZapitAudioChannel::UNKNOWN;
					channel->addAudioChannel(esInfo->elementary_PID, Type, description, componentTag);
				}

				descramble = true;
			}
			break;

		case 0x0F: // AAC ADTS
		case 0x11: // AAC LATM
			if (description == "")
				description	= esInfo->elementary_PID;
			description	+= " (AAC)";
			isAac		= true;
			descramble	= true;
			if(!CServiceScan::getInstance()->Scanning())
				channel->addAudioChannel(esInfo->elementary_PID, CZapitAudioChannel::AAC, description, componentTag);
			break;
		default:
			INFO("stream_type: %02x\n", esInfo->stream_type);
			break;
	}

	if (descramble)
		caPmt->es_info.insert(caPmt->es_info.end(), esInfo);
	else
		delete esInfo;

	return ES_info_length;
}

bool CPmt::Read(unsigned short pid, unsigned short sid)
{
	bool ret = true;
	unsigned char filter[DMX_FILTER_SIZE];
	unsigned char mask[DMX_FILTER_SIZE];

	cDemux * dmx = new cDemux(dmxnum);
	dmx->Open(DMX_PSI_CHANNEL);

	memset(filter, 0x00, DMX_FILTER_SIZE);
	memset(mask, 0x00, DMX_FILTER_SIZE);

	filter[0] = 0x02;	/* table_id */
	filter[1] = sid >> 8;
	filter[2] = sid;
	filter[3] = 0x01;	/* current_next_indicator */
	filter[4] = 0x00;	/* section_number */
	mask[0] = 0xFF;
	mask[1] = 0xFF;
	mask[2] = 0xFF;
	mask[3] = 0x01;
	mask[4] = 0xFF;
	if ((dmx->sectionFilter(pid, filter, mask, 5) < 0) || (dmx->Read(buffer, PMT_SIZE) < 0)) {
		printf("CPmt::Read: pid %x failed\n", pid);
		ret = false;
	}
	delete dmx;
	return ret;
}

void CPmt::MakeCAMap(casys_map_t &camap)
{
	int pmtlen;
	int pos;
	unsigned char descriptor_length=0;

	pmtlen= ((buffer[1]&0xf)<<8) + buffer[2] +3;
	pos=10;
	camap.clear();
	while(pos + 2 < pmtlen) {
		int dpmtlen=((buffer[pos] & 0x0f) << 8) | buffer[pos+1];
		for (int ia = pos+2; ia < (dpmtlen+pos+2);ia += descriptor_length + 2) {
			descriptor_length = buffer[ia+1];
			if (ia < pmtlen - 4)
				if(buffer[ia] == 0x09 && buffer[ia + 1] > 0) {
					int caid = (buffer[ia+2] & 0x1f) << 8 | buffer[ia+3];;
					camap.insert(caid);
				}
		}
		pos += dpmtlen + 5;
	}
}

bool CPmt::parse_pmt(CZapitChannel * const channel)
{
	unsigned short i;

	/* length of elementary stream description */
	unsigned short ES_info_length;

	printf("[zapit] parsing pmt pid 0x%X\n", channel->getPmtPid());

	if (channel->getPmtPid() == 0)
		return false;

	if(!Read(channel->getPmtPid(), channel->getServiceId()))
		return false;

	/* ca pmt */
	CCaPmt *caPmt = new CCaPmt(buffer);
	caPmt->ca_pmt_pid = channel->getPmtPid();

	printf("[pmt] pcr pid: old 0x%x new 0x%x\n", channel->getPcrPid(), ((buffer[8] & 0x1F) << 8) + buffer[9]);

	if(channel->getCaPmt() != 0) {
		if(channel->getCaPmt()->version_number != caPmt->version_number)
			channel->resetPids();
	}
	/* pmt */

	channel->setPcrPid(((buffer[8] & 0x1F) << 8) + buffer[9]);

	unsigned short program_info_length = ((buffer[10] & 0x0F) << 8) | buffer[11];
	if (program_info_length) {
		for (i = 12; i < 12 + program_info_length; i += buffer[i + 1] + 2)
			switch (buffer[i]) {
				case 0x09:
					caPmt->addCaDescriptor(buffer + i);
					break;
				default:
					DBG("decriptor_tag: %02x\n", buffer[i]);
					break;
			}
	}

	/* pmt */
	unsigned short section_length = ((buffer[1] & 0x0F) << 8) + buffer[2];
	for (i = 12 + program_info_length; i < section_length - 1; i += ES_info_length + 5)
		ES_info_length = ParseES(buffer + i, channel, caPmt);

	if(CServiceScan::getInstance()->Scanning()) {
		channel->setCaPmt(NULL);
		channel->setRawPmt(NULL);
		delete caPmt;
	} else {
		MakeCAMap(channel->camap);
		channel->setCaPmt(caPmt);
		int pmtlen= ((buffer[1]&0xf)<<8) + buffer[2] + 3;
		unsigned char * p = new unsigned char[pmtlen];
		memmove(p, buffer, pmtlen);
		channel->setRawPmt(p, pmtlen);
		channel->scrambled = !channel->camap.empty();
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

	return true;
}

bool CPmt::haveCaSys(int pmtpid, int service_id )
{
	if(!Read(pmtpid, service_id))
		return false;

	casys_map_t camap;
	MakeCAMap(camap);
	return !camap.empty();
}

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
#if 0 //HAVE_COOL_HARDWARE
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
#if HAVE_TRIPLEDRAGON
	if (pmtDemux)
		delete pmtDemux;
	/* apparently a close/reopen is needed on TD... */
	pmtDemux = NULL;
#else
	if(pmtDemux)
		pmtDemux->Stop();
#endif

	*fd = -1;
        return 0;
}
