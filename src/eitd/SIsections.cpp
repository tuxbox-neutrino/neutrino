//
// $Id: SIsections.cpp,v 1.61 2009/09/04 18:37:00 dbt Exp $
//
// classes for SI sections (dbox-II-project)
//
//    Homepage: http://dbox2.elxsi.de
//
//    Copyright (C) 2001 fnbrd (fnbrd@gmx.de)
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
//

#include <config.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <set>
#include <algorithm>
#include <string>

#include "SIutils.hpp"
#include "SIservices.hpp"
#include "SIevents.hpp"
#include "SIsections.hpp"
#include <zapit/dvbstring.h>
#include <edvbstring.h>
#ifdef ENABLE_FREESATEPG
#include "FreesatTables.hpp"
#endif

#include <dvbsi++/descriptor_tag.h>

struct descr_generic_header {
	unsigned descriptor_tag			: 8;
	unsigned descriptor_length		: 8;
} __attribute__ ((packed)) ;

struct descr_short_event_header {
	unsigned descriptor_tag			: 8;
	unsigned descriptor_length		: 8;
	unsigned language_code_hi		: 8;
	unsigned language_code_mid		: 8;
	unsigned language_code_lo		: 8;
	unsigned event_name_length		: 8;
} __attribute__ ((packed)) ;

struct descr_service_header {
	unsigned descriptor_tag			: 8;
	unsigned descriptor_length		: 8;
	unsigned service_typ			: 8;
	unsigned service_provider_name_length	: 8;
} __attribute__ ((packed)) ;

struct descr_extended_event_header {
	unsigned descriptor_tag			: 8;
	unsigned descriptor_length		: 8;
	unsigned descriptor_number		: 4;
	unsigned last_descriptor_number		: 4;
	unsigned iso_639_2_language_code_hi	: 8;
	unsigned iso_639_2_language_code_mid	: 8;
	unsigned iso_639_2_language_code_lo	: 8;
	unsigned length_of_items		: 8;
} __attribute__ ((packed)) ;

struct service_list_entry {
	unsigned service_id_hi			: 8;
	unsigned service_id_lo			: 8;
	unsigned service_type			: 8;
} __attribute__ ((packed)) ;

struct private_data_specifier {
	unsigned byte1				: 8;
	unsigned byte2				: 8;
	unsigned byte3				: 8;
	unsigned byte4				: 8;
} __attribute__ ((packed)) ;

inline unsigned min(unsigned a, unsigned b)
{
	return b < a ? b : a;
}

static int get_table(unsigned char hi, unsigned char mid, unsigned char lo)
{
	char lang[4];
	lang[0] = hi;
	lang[1] = mid;
	lang[2] = lo;
	lang[3] = 0;
	if(!strcmp(lang, "pol"))
		return 2;
	else if(!strcmp(lang, "tur"))
		return 9;
	else if(!strcmp(lang, "gre"))
		return 7;
	else if(!strcmp(lang, "rus"))
		return 5;
	else if(!strcmp(lang, "bul"))
		return 5;
	else if(!strcmp(lang, "ara"))
		return 6;
	return 0;
}

bool check_blacklisted(const t_original_network_id onid, const t_transport_stream_id tsid)
{
	if ( (onid == 0x0001) &&
			((tsid == 0x03F0) || (tsid == 0x0408) || (tsid == 0x040E) || (tsid == 0x0412) || (tsid == 0x0416) || (tsid == 0x041E) ||
			 (tsid == 0x0420) || (tsid == 0x0422) || (tsid == 0x0424) || (tsid == 0x0444) ))
		return true;
	else
		return false;
}
//-----------------------------------------------------------------------
// Da es vorkommen kann das defekte Packete empfangen werden
// sollte hier alles ueberprueft werden.
// Leider ist das noch nicht bei allen Descriptoren so.
//-----------------------------------------------------------------------
void SIsectionEIT::parseLinkageDescriptor(const char *buf, SIevent &e, unsigned maxlen)
{
	if(maxlen>=sizeof(struct descr_linkage_header))
	{
		SIlinkage l((const struct descr_linkage_header *)buf);
		e.linkage_descs.insert(e.linkage_descs.end(), l);
		//printf("LinkName: %s\n", l.name.c_str());
	}
}

void SIsectionEIT::parsePDCDescriptor(const char *buf, SIevent &e, unsigned maxlen)
{
	if (maxlen >= sizeof(struct descr_pdc_header))
	{
		const struct descr_pdc_header *s = (struct descr_pdc_header *)buf;
		time_t now = time(NULL);
		struct tm tm_r;
		struct tm t = *localtime_r(&now, &tm_r); // this initializes the time zone in 't'
		t.tm_isdst = -1; // makes sure mktime() will determine the correct DST setting
		int month = t.tm_mon;
		t.tm_mon = ((s->pil1 >> 3) & 0x0F) - 1;
		t.tm_mday = ((s->pil0 & 0x0F) << 1) | ((s->pil1 & 0x80) >> 7);
		t.tm_hour = ((s->pil1 & 0x07) << 2) | ((s->pil2 & 0xC0) >> 6);
		t.tm_min = s->pil2 & 0x3F;
		t.tm_sec = 0;
		if (month == 11 && t.tm_mon == 0) // current month is dec, but event is in jan
			t.tm_year++;
		else if (month == 0 && t.tm_mon == 11) // current month is jan, but event is in dec
			t.tm_year--;
		e.vps = mktime(&t);
		// fprintf(stderr, "SIsectionEIT::parsePDCDescriptor: vps: %ld %s", e.vps, ctime(&e.vps));
	}
}

void SIsectionEIT::parseComponentDescriptor(const char *buf, SIevent &e, unsigned maxlen)
{
	if(maxlen>=sizeof(struct descr_component_header))
		e.components.insert(SIcomponent((const struct descr_component_header *)buf));
}

void SIsectionEIT::parseContentDescriptor(const char *buf, SIevent &e, unsigned maxlen)
{
	struct descr_generic_header *cont=(struct descr_generic_header *)buf;
	if(cont->descriptor_length+sizeof(struct descr_generic_header)>maxlen)
		return; // defekt
	const char *classification=buf+sizeof(struct descr_generic_header);
	while(classification<=buf+sizeof(struct descr_generic_header)+cont->descriptor_length-2) {
		e.contentClassification+=std::string(classification, 1);
		// printf("Content: 0x%02hhx\n", *classification);
		e.userClassification+=std::string(classification+1, 1);
		// printf("User: 0x%02hhx\n", *(classification+1));
		classification+=2;
	}
}

void SIsectionEIT::parseParentalRatingDescriptor(const char *buf, SIevent &e, unsigned maxlen)
{
	struct descr_generic_header *cont=(struct descr_generic_header *)buf;
	if(cont->descriptor_length+sizeof(struct descr_generic_header)>maxlen)
		return; // defekt
	const char *s=buf+sizeof(struct descr_generic_header);
	while(s<buf+sizeof(struct descr_generic_header)+cont->descriptor_length-4) {
		e.ratings.insert(SIparentalRating(std::string(s, 3), *(s+3)));
		s+=4;
	}
}

void SIsectionEIT::parseExtendedEventDescriptor(const char *buf, SIevent &e, unsigned maxlen)
{
	struct descr_extended_event_header *evt=(struct descr_extended_event_header *)buf;
	if((evt->descriptor_length+sizeof(descr_generic_header)>maxlen) || (evt->descriptor_length<sizeof(struct descr_extended_event_header)-sizeof(descr_generic_header)))
		return; // defekt
	unsigned char *items=(unsigned char *)(buf+sizeof(struct descr_extended_event_header));
	int tsidonid = (e.transport_stream_id << 16) | e.original_network_id;
	int table = get_table(evt->iso_639_2_language_code_hi, evt->iso_639_2_language_code_mid, evt->iso_639_2_language_code_lo);

	char lang[] = {tolower(evt->iso_639_2_language_code_hi), tolower(evt->iso_639_2_language_code_mid), tolower(evt->iso_639_2_language_code_lo), '\0'};
	std::string language(lang);

	while(items < (unsigned char *)(buf + sizeof(struct descr_extended_event_header) + evt->length_of_items)) {

		// TODO What info should be in item & itemDescription?
		// Should I make language filter those as well?  Arzka

		if(*items) {
			// 21.07.2005 - collect all extended events in one
			// string, delimit multiple entries with a newline
			e.itemDescription.append(convertDVBUTF8((const char *)(items+1), min(maxlen-((const char *)items+1-buf), *items), table, tsidonid));
			e.itemDescription.append("\n");
		}
		items+=1+*items;
		if(*items) {
			// 21.07.2005 - collect all extended events in one
			// string, delimit multiple entries with a newline
			e.item.append(convertDVBUTF8((const char *)(items+1), min(maxlen-((const char *)items+1-buf), *items), table, tsidonid));
			e.item.append("\n");
		}
		items+=1+*items;
	}
//  if (0 != e.itemDescription.length()) {
//	printf("Item Description: %s\n", e.itemDescription.c_str());
//	printf("Item: %s\n", e.item.c_str());
//  }
	if(*items) {
		e.appendExtendedText(language, convertDVBUTF8((const char *)(items+1), min(maxlen-((const char *)items+1-buf), (*items)), table, tsidonid));
		//printf("Extended Text: %s\n", e.extendedText.c_str());
	}
}

#ifdef ENABLE_FREESATEPG
std::string SIsectionEIT::freesatHuffmanDecode(std::string input)
{
	const char *src = input.c_str();
	uint size = input.length();

	if (src[1] == 1 || src[1] == 2)
	{
		std::string uncompressed(size * 3, ' ');
		uint p = 0;
		struct hufftab *table;
		unsigned table_length;
		if (src[1] == 1)
		{
			table = fsat_huffman1;
			table_length = sizeof(fsat_huffman1) / sizeof(fsat_huffman1[0]);
		}
		else
		{
			table = fsat_huffman2;
			table_length = sizeof(fsat_huffman2) / sizeof(fsat_huffman2[0]);
		}
		unsigned value = 0, byte = 2, bit = 0;
		while (byte < 6 && byte < size)
		{
			value |= src[byte] << ((5-byte) * 8);
			byte++;
		}
		char lastch = START;

		do
		{
			bool found = false;
			unsigned bitShift = 0;
			if (lastch == ESCAPE)
			{
				found = true;
				// Encoded in the next 8 bits.
				// Terminated by the first ASCII character.
				char nextCh = (value >> 24) & 0xff;
				bitShift = 8;
				if ((nextCh & 0x80) == 0)
					lastch = nextCh;
				if (p >= uncompressed.length())
					uncompressed.resize(p+10);
				uncompressed[p++] = nextCh;
			}
			else
			{
				for (unsigned j = 0; j < table_length; j++)
				{
					if (table[j].last == lastch)
					{
						unsigned mask = 0, maskbit = 0x80000000;
						for (short kk = 0; kk < table[j].bits; kk++)
						{
							mask |= maskbit;
							maskbit >>= 1;
						}
						if ((value & mask) == table[j].value)
						{
							char nextCh = table[j].next;
							bitShift = table[j].bits;
							if (nextCh != STOP && nextCh != ESCAPE)
							{
								if (p >= uncompressed.length())
									uncompressed.resize(p+10);
								uncompressed[p++] = nextCh;
							}
							found = true;
							lastch = nextCh;
							break;
						}
					}
				}
			}
			if (found)
			{
				// Shift up by the number of bits.
				for (unsigned b = 0; b < bitShift; b++)
				{
					value = (value << 1) & 0xfffffffe;
					if (byte < size)
						value |= (src[byte] >> (7-bit)) & 1;
					if (bit == 7)
					{
						bit = 0;
						byte++;
					}
					else bit++;
				}
			}
			else
			{
				// Entry missing in table.
				uncompressed.resize(p);
				uncompressed.append("...");
				return uncompressed;
			}
		} while (lastch != STOP && value != 0);

		uncompressed.resize(p);
		return uncompressed;
	}
	else return input;
}
#endif

void SIsectionEIT::parseShortEventDescriptor(const char *buf, SIevent &e, unsigned maxlen)
{
	struct descr_short_event_header *evt=(struct descr_short_event_header *)buf;
	if((evt->descriptor_length+sizeof(descr_generic_header)>maxlen) || (evt->descriptor_length<sizeof(struct descr_short_event_header)-sizeof(descr_generic_header)))
		return; // defekt
	int tsidonid = (e.transport_stream_id << 16) | e.original_network_id;
	int table = get_table(evt->language_code_hi, evt->language_code_mid, evt->language_code_lo);

	char lang[] = {tolower(evt->language_code_hi), tolower(evt->language_code_mid), tolower(evt->language_code_lo), '\0'};
	std::string language(lang);

	buf+=sizeof(struct descr_short_event_header);
	if(evt->event_name_length) {
#ifdef ENABLE_FREESATEPG
		std::string tmp_str = buf[0] == 0x1f ? freesatHuffmanDecode(std::string(buf, evt->event_name_length)) : std::string(buf, evt->event_name_length);
		e.setName(language, convertDVBUTF8(tmp_str.c_str(), tmp_str.size(), table, tsidonid));
#else
		e.setName(language, convertDVBUTF8(buf, evt->event_name_length, table, tsidonid));
#endif
	}
	buf+=evt->event_name_length;
	unsigned char textlength=*((unsigned char *)buf);
	if(textlength > 2) {
#ifdef ENABLE_FREESATEPG
		std::string tmp_str = buf[1] == 0x1f ? freesatHuffmanDecode(std::string(++buf, textlength)) : std::string(++buf, textlength);
		e.setText(language, convertDVBUTF8(tmp_str.c_str(), tmp_str.size(), table, tsidonid));
#else
		e.setText(language, convertDVBUTF8((++buf), textlength, table, tsidonid));
#endif
	}

//  printf("Name: %s\n", e.name.c_str());
//  printf("Text: %s\n", e.text.c_str());

}

void SIsectionEIT::parseDescriptors(const uint8_t *des, unsigned len, SIevent &e)
{
	struct descr_generic_header *desc;
	/* we pass the buffer including the eit_event header, so we have to
	   skip it here... */
	des += sizeof(struct eit_event);
	len -= sizeof(struct eit_event);
	while(len>=sizeof(struct descr_generic_header)) {
		desc=(struct descr_generic_header *)des;
		// printf("Type: %s\n", decode_descr(desc->descriptor_tag));
		if(desc->descriptor_tag==0x4D)
			parseShortEventDescriptor((const char *)desc, e, len);
		else if(desc->descriptor_tag==0x4E)
			parseExtendedEventDescriptor((const char *)desc, e, len);
		else if(desc->descriptor_tag==0x54)
			parseContentDescriptor((const char *)desc, e, len);
		else if(desc->descriptor_tag==0x50)
			parseComponentDescriptor((const char *)desc, e, len);
		else if(desc->descriptor_tag==0x55)
			parseParentalRatingDescriptor((const char *)desc, e, len);
		else if(desc->descriptor_tag==0x4A)
			parseLinkageDescriptor((const char *)desc, e, len);
		else if(desc->descriptor_tag==0x69)
			parsePDCDescriptor((const char *)desc, e, len);
		if((unsigned)(desc->descriptor_length+2)>len)
			break;
		len-=desc->descriptor_length+2;
		des+=desc->descriptor_length+2;
	}
}

// Die infos aus dem Puffer holen
void SIsectionEIT::parse(void)
{
	if (!buffer || parsed)
		return;

#if 0
	const uint8_t *actPos;
	const uint8_t *bufEnd;
	struct eit_event *evt;
	unsigned short descriptors_loop_length;

	if (bufferLength < sizeof(SI_section_EIT_header) + sizeof(struct eit_event)) {
		bufferLength=0;
		return;
	}

	unsigned char table_id = header()->table_id;
	unsigned char version_number = header()->version_number;
	actPos = buffer + sizeof(SI_section_EIT_header);
	bufEnd = buffer + bufferLength;

	while (actPos < bufEnd - sizeof(struct eit_event)) {
		evt = (struct eit_event *) actPos;
		SIevent e(evt);
		e.service_id = service_id();
		e.original_network_id = original_network_id();
		e.transport_stream_id = transport_stream_id();
		e.table_id = table_id;
		e.version = version_number;
		descriptors_loop_length = sizeof(struct eit_event) + ((evt->descriptors_loop_length_hi << 8) | evt->descriptors_loop_length_lo);
		parseDescriptors(actPos, min((unsigned)(bufEnd - actPos), descriptors_loop_length), e);
		evts.insert(e);
		actPos += descriptors_loop_length;
	}
#endif
#if 1
	const EventList &elist = *getEvents();

	if(elist.empty())
		return;

        t_service_id		sid = getTableIdExtension();
        t_original_network_id	onid = getOriginalNetworkId();
        t_transport_stream_id	tsid = getTransportStreamId();
	unsigned char		tid = getTableId();
	unsigned char		version = getVersionNumber();

	for (EventConstIterator eit = elist.begin(); eit != elist.end(); ++eit) {
		Event &event = (**eit);

		SIevent e(onid, tsid, sid, event.getEventId());
		e.table_id = tid;
		e.version = version;

		e.parse(event);
		evts.insert(e);
	}
#endif
	parsed = 1;
}

/********************/
void SIsectionSDT::parseNVODreferenceDescriptor(const char *buf, SIservice &s)
{
	struct descr_generic_header *hdr=(struct descr_generic_header *)buf;
	unsigned short *spointer=(unsigned short *)(buf+sizeof(struct descr_generic_header));
	while((const char *)spointer<=buf+sizeof(struct descr_generic_header)+hdr->descriptor_length-2) {
		unsigned short transportStreamID=*spointer++;
		unsigned short originalNetworkID=*spointer++;
		unsigned short sID=*spointer++;
		s.nvods.insert(SInvodReference(transportStreamID, originalNetworkID, sID));
	}
}

void SIsectionSDT::parseServiceDescriptor(const char *buf, SIservice &s)
{
	bool is_blacklisted;

	struct descr_service_header *sv=(struct descr_service_header *)buf;
	buf+=sizeof(struct descr_service_header);
	s.serviceTyp=sv->service_typ;
	is_blacklisted = check_blacklisted(s.original_network_id, s.transport_stream_id);
	if(sv->service_provider_name_length) {
		//if(*buf < 0x06) // other code table
//      s.providerName=std::string(buf+1, sv->service_provider_name_length-1);
//    else
//      s.providerName=std::string(buf, sv->service_provider_name_length);
		if ((*buf > 0x05) && (is_blacklisted))
			s.providerName  = CDVBString(("\x05" + std::string((const char *)(buf))).c_str(), sv->service_provider_name_length+1).getContent();
		else
			s.providerName  = CDVBString((const char *)(buf), sv->service_provider_name_length).getContent();
	}
	buf+=sv->service_provider_name_length;
	unsigned char servicenamelength=*((unsigned char *)buf);
	if(servicenamelength) {
		if ((*buf+1 > 0x05) && (is_blacklisted))
			s.serviceName  = CDVBString(("\x05" + std::string((const char *)(++buf))).c_str(), servicenamelength+1).getContent();
		else
			s.serviceName  = CDVBString((const char *)(++buf), servicenamelength).getContent();
	}
//  printf("Provider-Name: %s\n", s.providerName.c_str());
//  printf("Service-Name: %s\n", s.serviceName.c_str());
}

void SIsectionSDT::parsePrivateDataDescriptor(const char *buf, SIservice &s)
{
	buf+=sizeof(struct descr_generic_header);
	struct private_data_specifier *pds=(struct private_data_specifier *)buf;
	if ((((((pds->byte1 << 24) | (pds->byte2 << 16)) | (pds->byte3 << 8)) | pds->byte4) == 0x000000c0) && (s.serviceTyp == 0xc3))
		s.serviceTyp = 0x01;
}

void SIsectionSDT::parseDescriptors(const uint8_t *des, unsigned len, SIservice &s)
{
	struct descr_generic_header *desc;
	des += sizeof(struct sdt_service);
	len -= sizeof(struct sdt_service);
	while(len>=sizeof(struct descr_generic_header)) {
		desc=(struct descr_generic_header *)des;
//    printf("Type: %s\n", decode_descr(desc->descriptor_tag));
//    printf("Length: %hhu\n", desc->descriptor_length);
		if(desc->descriptor_tag==0x48) {
//      printf("Found service descriptor\n");
			parseServiceDescriptor((const char *)desc, s);
		}
		else if(desc->descriptor_tag==0x4b) {
//      printf("Found NVOD reference descriptor\n");
			parseNVODreferenceDescriptor((const char *)desc, s);
		}
		else if(desc->descriptor_tag==0x5f) {
//      printf("Found Private Data Specifier\n");
			parsePrivateDataDescriptor((const char *)desc, s);
		}
		// hotfix for ARD crash
		if ((int) len<desc->descriptor_length+2) break;
		len-=desc->descriptor_length+2;
		des+=desc->descriptor_length+2;
	}
}

// Die infos aus dem Puffer holen
void SIsectionSDT::parse(void)
{
	const uint8_t *actPos;
	const uint8_t *bufEnd;
	struct sdt_service *sv;
	unsigned short descriptors_loop_length;

	if (!buffer || parsed)
		return;

	if (bufferLength < sizeof(SI_section_SDT_header) + sizeof(struct sdt_service)) {
		bufferLength=0;
		return;
	}

	actPos = buffer + sizeof(SI_section_SDT_header);
	bufEnd = buffer + bufferLength;

	while (actPos <= bufEnd - sizeof(struct sdt_service)) {
		sv = (struct sdt_service *)actPos;
		SIservice s(sv);
		s.original_network_id = original_network_id();
		s.transport_stream_id = transport_stream_id();
		descriptors_loop_length = sizeof(struct sdt_service) + ((sv->descriptors_loop_length_hi << 8) | sv->descriptors_loop_length_lo);
		//printf("actpos: %p buf+bl: %p sid: %hu desclen: %hu\n", actPos, buffer+bufferLength, sv->service_id, sv->descriptors_loop_length);
		parseDescriptors(actPos, min((unsigned)(bufEnd - actPos), descriptors_loop_length), s);
		svs.insert(s);
		actPos += descriptors_loop_length;
	}

	parsed = 1;
}
