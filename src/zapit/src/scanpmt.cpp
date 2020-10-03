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
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* zapit */
#include <zapit/settings.h>
#include <zapit/debug.h>
#include <zapit/scanpmt.h>
#include <zapit/scan.h>
#include <eitd/edvbstring.h>
#include <hardware/dmx.h>

#include <dvbsi++/descriptor_tag.h>
#include <dvbsi++/program_map_section.h>
#include <dvbsi++/ca_program_map_section.h>
#include <dvbsi++/registration_descriptor.h>
#include <dvbsi++/iso639_language_descriptor.h>
#include <dvbsi++/stream_identifier_descriptor.h>
#include <dvbsi++/teletext_descriptor.h>
#include <dvbsi++/subtitling_descriptor.h>
#include <dvbsi++/vbi_teletext_descriptor.h>

#define DEBUG_PMT
//#define DEBUG_PMT_UNUSED

CPmt::CPmt(int dnum)
{
	dmxnum = dnum;
}

CPmt::~CPmt()
{
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
	if ((!dmx->sectionFilter(pid, filter, mask, 5)) || (dmx->Read(buffer, PMT_SECTION_SIZE) < 0)) {
		printf("CPmt::Read: pid %x failed\n", pid);
		ret = false;
	}
	delete dmx;
	return ret;
}

bool CPmt::Parse(CZapitChannel * const channel)
{
	printf("[zapit] parsing pmt pid 0x%X (%s)\n", channel->getPmtPid(), channel->getName().c_str());

	if (channel->getPmtPid() == 0)
		return false;

	if(!Read(channel->getPmtPid(), channel->getServiceId()))
		return false;

	ProgramMapSection pmt(buffer);

	DBG("[pmt] pcr pid: old 0x%x new 0x%x\n", channel->getPcrPid(), pmt.getPcrPid());

	channel->resetPids();

	channel->setPmtVersion(pmt.getVersionNumber());
	channel->setPcrPid(pmt.getPcrPid());

	const ElementaryStreamInfoList * eslist = pmt.getEsInfo();
	ElementaryStreamInfoConstIterator it;
	for (it = eslist->begin(); it != eslist->end(); ++it) {
		ElementaryStreamInfo *esinfo = *it;
		ParseEsInfo(esinfo, channel);
	}

	casys_map_t camap;
	MakeCAMap(camap);
	channel->scrambled = !camap.empty();

	if(CServiceScan::getInstance()->Scanning()) {
		channel->setRawPmt(NULL);
	} else {
		channel->camap = camap;
		int pmtlen= pmt.getSectionLength() + 3;
		unsigned char * p = new unsigned char[pmtlen];
		memmove(p, buffer, pmtlen);
		channel->setRawPmt(p, pmtlen);
	}

	channel->setPidsFlag();
	return true;
}

bool CPmt::ParseEsInfo(ElementaryStreamInfo *esinfo, CZapitChannel * const channel)
{
	std::string description = "";
	unsigned char componentTag = 0xFF;
	CZapitAudioChannel::ZapitAudioChannelType audio_type = CZapitAudioChannel::UNKNOWN;

	uint8_t stream_type = esinfo->getType();

	bool audio = false;

	const DescriptorList * dlist = esinfo->getDescriptors();
	DescriptorConstIterator dit;
	for (dit = dlist->begin(); dit != dlist->end(); ++dit) {
		Descriptor * d = *dit;
		switch (d->getTag()) {
		case REGISTRATION_DESCRIPTOR:
			{
				RegistrationDescriptor *sd = (RegistrationDescriptor*) d;
				switch (sd->getFormatIdentifier()) {
				case 0x44545331:
				case 0x44545332:
				case 0x44545333:
					audio_type = CZapitAudioChannel::DTS;
					break;
				case 0x41432d33:
					audio_type = CZapitAudioChannel::AC3;
					break;
				default:
#ifdef DEBUG_PMT
					printf("PMT: REGISTRATION_DESCRIPTOR: %x\n", sd->getFormatIdentifier());
#endif
					break;
				}
			}
			break;
		case ISO_639_LANGUAGE_DESCRIPTOR:
			{
				Iso639LanguageDescriptor * sd = (Iso639LanguageDescriptor*) d;
				const Iso639LanguageList *languages = sd->getIso639Languages();
				Iso639LanguageConstIterator it;
				for (it = languages->begin(); it != languages->end(); ++it) {
					description = (*it)->getIso639LanguageCode();
					break;
				}
			}
			break;
		case STREAM_IDENTIFIER_DESCRIPTOR:
			{
				StreamIdentifierDescriptor *sd = (StreamIdentifierDescriptor*) d;
				componentTag = sd->getComponentTag();
			}
			break;
		case TELETEXT_DESCRIPTOR:
			{
				TeletextDescriptor *td = (TeletextDescriptor*)d;
				const VbiTeletextList *vbilist = td->getVbiTeletexts();
				VbiTeletextConstIterator it;
				DBG("[pmt] teletext pid %04x\n", esinfo->getPid());
				for (it = vbilist->begin(); it != vbilist->end(); ++it) {
					VbiTeletext * vbi = *it;

					std::string lang = vbi->getIso639LanguageCode();
					uint8_t page = vbi->getTeletextPageNumber();
					uint8_t magazine = vbi->getTeletextMagazineNumber();
					DBG("[pmt] teletext type %d mag %d page %d lang [%s]\n",
							vbi->getTeletextType(), magazine, page, lang.c_str());
					if (vbi->getTeletextType() == 0x01)
						channel->setTeletextLang(lang);
					else if (vbi->getTeletextType() == 0x02)
						channel->addTTXSubtitle(esinfo->getPid(), lang, magazine, page);
					else if (vbi->getTeletextType() == 0x05)
						channel->addTTXSubtitle(esinfo->getPid(), lang, magazine, page, true);
				}
				channel->setTeletextPid(esinfo->getPid());
			}
			break;
		case SUBTITLING_DESCRIPTOR:
			if(stream_type == 0x06) {
				SubtitlingDescriptor *sd = (SubtitlingDescriptor*) d;
				const SubtitlingList *sublist = sd->getSubtitlings();
				SubtitlingConstIterator it;
				for (it = sublist->begin(); it != sublist->end(); ++it) {
					Subtitling * sub = *it;

					uint16_t composition_page_id = sub->getCompositionPageId();
					uint16_t ancillary_page_id = sub->getAncillaryPageId();
					std::string lang = sub->getIso639LanguageCode();

					channel->addDVBSubtitle(esinfo->getPid(), lang, sub->getSubtitlingType(),
							composition_page_id, ancillary_page_id);
				}
			}
			break;
		case AC3_DESCRIPTOR:
			audio_type = CZapitAudioChannel::AC3;
			break;
		case ENHANCED_AC3_DESCRIPTOR:
			audio_type = CZapitAudioChannel::EAC3;
			break;
		case DTS_DESCRIPTOR:
			audio_type = CZapitAudioChannel::DTS;
			break;
		case AAC_DESCRIPTOR:
			audio_type = CZapitAudioChannel::AAC;
			break;
		case 0xC5: /* User Private descriptor - Canal+ Radio */
			if(d->getLength() >= 25) {
				uint8_t *buf = new uint8_t[2 + d->getLength()];
				if(buf){
					d->writeToBuffer(buf);
					int tsidonid =  channel->getTransportStreamId() << 16 | channel->getOriginalNetworkId();
					description = convertDVBUTF8((const char*)&buf[3], 24, 2, tsidonid);
					delete []buf;
				}
			}
			break;
		case AUDIO_STREAM_DESCRIPTOR:
			break;
		case VIDEO_STREAM_DESCRIPTOR:
			break;
		case CA_DESCRIPTOR:
			break;
		default:
			{
#ifdef DEBUG_PMT_UNUSED
				printf("PMT: descriptor %02x: ", d->getTag());
				uint8_t len = 2+d->getLength();
				uint8_t buf[len];
				d->writeToBuffer(buf);
				for(uint8_t i = 0; i < len; i++)
					printf("%02x ", buf[i]);
				printf("\n");
#endif
			}
			break;
		}
	}
	switch (stream_type) {
	case 0x01: // MPEG1 Video
	case 0x02: // MPEG2 Video (H262)
		channel->setVideoPid(esinfo->getPid());
		channel->type = CHANNEL_MPEG2;
		DBG("[pmt] vpid %04x stream %d type %d\n", esinfo->getPid(), stream_type, channel->type);
		break;
	case 0x10: // AVC Video Stream (MPEG4 H263)
	case 0x1b: // AVC Video Stream (MPEG4 H264)
		channel->setVideoPid(esinfo->getPid());
		channel->type = CHANNEL_MPEG4;
		DBG("[pmt] vpid %04x stream %d type %d\n", esinfo->getPid(), stream_type, channel->type);
		break;
	case 0x24: // HEVC Video Stream (MPEG4 H265)
	case 0x27: // SHVC Video Stream (MPEG4 H265 TS)
		channel->setVideoPid(esinfo->getPid());
		channel->type = CHANNEL_HEVC;
		DBG("[pmt] vpid %04x stream %d type %d\n", esinfo->getPid(), stream_type, channel->type);
		break;
	case 0x42: // CAVS Video Stream (China)
		channel->setVideoPid(esinfo->getPid());
		channel->type = CHANNEL_CAVS;
		DBG("[pmt] vpid %04x stream %d type %d\n", esinfo->getPid(), stream_type, channel->type);
		break;
	case 0x03: // MPEG1 Audio
	case 0x04: // MPEG2 Audio
		audio_type = CZapitAudioChannel::MPEG;
		audio = true;
		break;
	case 0x06: // MPEG2 Subtitiles
		if(audio_type != CZapitAudioChannel::UNKNOWN)
			audio = true;
		break;
	case 0x0F: // AAC ADTS (MPEG2)
		audio_type = CZapitAudioChannel::AAC;
		audio = true;
		break;
	case 0x11: // AAC LATM (MPEG4)
		audio_type = CZapitAudioChannel::AACPLUS;
		audio = true;
		break;
	case 0x81: // Dolby Digital
		audio_type = CZapitAudioChannel::AC3;
		audio = true;
		break;
	default:
#ifdef DEBUG_PMT_UNUSED
		printf("PMT: pid %04x stream_type: %02x\n", esinfo->getPid(), stream_type);
#endif
		break;
	}
	if(audio) {
		if(description.empty()) {
			char str[DESC_MAX_LEN];
//			snprintf(str, DESC_MAX_LEN, "Unknown 0x%04x", esinfo->getPid());
			snprintf(str, DESC_MAX_LEN, "Unknown");
			description = str;
		}
		DBG("[pmt] apid %04x stream %02x type %d [%s]\n", esinfo->getPid(), stream_type,
				(int) audio_type, description.c_str());
		if(CServiceScan::getInstance()->Scanning()) {
			if(channel->getPreAudioPid() == 0)
				channel->setAudioPid(esinfo->getPid());
		} else
			channel->addAudioChannel(esinfo->getPid(), audio_type, description, componentTag);
	}
	return true;
}

void CPmt::MakeCAMap(casys_map_t &camap)
{
	ProgramMapSection pmt(buffer);
	CaProgramMapSection capmt(&pmt, 0x03, 0x01);
	DescriptorConstIterator dit;
	for (dit = capmt.getDescriptors()->begin(); dit != capmt.getDescriptors()->end(); ++dit) {
		if ((*dit)->getTag() == CA_DESCRIPTOR ) {
			CaDescriptor * d = (CaDescriptor*) *dit;
			//printf("%02X: casys %04X capid %04X, ", d->getTag(), d->getCaSystemId(), d->getCaPid());
			camap.insert(d->getCaSystemId());
		}
	}
	const ElementaryStreamInfoList * eslist = pmt.getEsInfo();
	ElementaryStreamInfoConstIterator it;
	for (it = eslist->begin(); it != eslist->end(); ++it) {
		ElementaryStreamInfo *esinfo = *it;
		const DescriptorList * dlist = esinfo->getDescriptors();
		for (dit = dlist->begin(); dit != dlist->end(); ++dit) {
			if ((*dit)->getTag() == CA_DESCRIPTOR ) {
				CaDescriptor * d = (CaDescriptor*) *dit;
				camap.insert(d->getCaSystemId());
			}
		}
	}
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

#if 0 //HAVE_CST_HARDWARE
	printf("[pmt] set update filter, sid 0x%x pid 0x%x version %x\n", channel->getServiceId(), channel->getPmtPid(), channel->getCaPmt()->version_number);
	filter[3] = (((channel->getCaPmt()->version_number + 1) & 0x01) << 1) | 0x01;
	mask[3] = (0x01 << 1) | 0x01;
	pmtDemux->sectionFilter(channel->getPmtPid(), filter, mask, 5);
#else
	printf("[pmt] set update filter, sid 0x%x pid 0x%x version %x\n", channel->getServiceId(), channel->getPmtPid(), channel->getPmtVersion());
	filter[3] = (channel->getPmtVersion() << 1) | 0x01;
	mask[3] = (0x1F << 1) | 0x01;
	mode[3] = 0x1F << 1;
	pmtDemux->sectionFilter(channel->getPmtPid(), filter, mask, 5, 0, mode);
#endif

	*fd = 1;
	return 0;
}

int pmt_stop_update_filter(int * fd)
{
	DBG("[pmt] stop update filter\n");

	if(pmtDemux)
		pmtDemux->Stop();

	*fd = -1;
	return 0;
}
