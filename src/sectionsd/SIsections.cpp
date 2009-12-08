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

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/poll.h> // fuer poll()

#include <set>
#include <algorithm>
#include <string>

#include "SIutils.hpp"
#include "SIservices.hpp"
#include "SIevents.hpp"
#ifdef UPDATE_NETWORKS
#include "SIbouquets.hpp"
#include "SInetworks.hpp"
#endif
#include "SIsections.hpp"
#include <dmxapi.h>
#include <zapit/dvbstring.h>
#include <edvbstring.h>
#ifdef ENABLE_FREESATEPG
#include "FreesatTables.hpp"
#endif

#define NOVA		0x3ffe
#define CANALDIGITAAL	0x3fff

//#define DEBUG

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

struct digplus_order_entry {
	unsigned service_id_hi			: 8;
	unsigned service_id_lo			: 8;
	unsigned channel_number_hi		: 8;
	unsigned channel_number_lo		: 8;
} __attribute__ ((packed)) ;

struct bskyb_order_entry {
	unsigned service_id_hi			: 8;
	unsigned service_id_lo			: 8;
	unsigned service_type			: 8;
	unsigned unknown1			: 8;
	unsigned unknown2			: 8;
	unsigned channel_number_hi		: 8;
	unsigned channel_number_lo		: 8;
	unsigned unknown3			: 8;
	unsigned unknown4			: 8;
} __attribute__ ((packed)) ;

struct bskyb_bid {
	unsigned unknown1			: 8;
	unsigned unknown2			: 8;
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
        lang[0] = hi; lang[1] = mid; lang[2] = lo; lang[3] = 0;
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

	char lang[] = {evt->iso_639_2_language_code_hi, evt->iso_639_2_language_code_mid, evt->iso_639_2_language_code_lo, '\0'};
	std::string language(lang);

	while(items < (unsigned char *)(buf + sizeof(struct descr_extended_event_header) + evt->length_of_items)) {

		// TODO What info should be in item & itemDescription?
		// Should I make language filter those as well?  Arzka

		if(*items) {
#if 0
			if(*(items+1) < 0x06) { // other code table
				// 21.07.2005 - collect all extended events in one
				// string, delimit multiple entries with a newline
				e.itemDescription.append(std::string((const char *)(items+2), min(maxlen-((const char *)items+2-buf), (*items)-1)));
				e.itemDescription.append("\n");
			} 
			else 
#endif
			{
				// 21.07.2005 - collect all extended events in one
				// string, delimit multiple entries with a newline
				//e.itemDescription.append(std::string((const char *)(items+1), min(maxlen-((const char *)items+1-buf), *items)));
				e.itemDescription.append(convertDVBUTF8((const char *)(items+1), min(maxlen-((const char *)items+1-buf), *items), table, tsidonid));
				e.itemDescription.append("\n");
			}
		}
		items+=1+*items;
		if(*items) {
			// 21.07.2005 - collect all extended events in one
			// string, delimit multiple entries with a newline
			//e.item.append(std::string((const char *)(items+0), min(maxlen-((const char *)items+1-buf), *items)));
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
#if 0
		if(*(items+1) < 0x06) { // other code table
			e.appendExtendedText(language, std::string((const char *)(items+2), min(maxlen-((const char *)items+2-buf), (*items)-1)));
		} else 
#endif
		{
			//e.appendExtendedText(language, std::string((const char *)(items+1), min(maxlen-((const char *)items+1-buf), *items)));
			e.appendExtendedText(language, convertDVBUTF8((const char *)(items+1), min(maxlen-((const char *)items+2-buf), (*items)), table, tsidonid));
//			printf("Extended Text: %s\n", e.extendedText.c_str());
		}
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

	char lang[] = {evt->language_code_hi, evt->language_code_mid, evt->language_code_lo, '\0'};
	std::string language(lang);

	buf+=sizeof(struct descr_short_event_header);
	if(evt->event_name_length) {
#if 0
		if(*buf < 0x06) { // other code table
#ifdef ENABLE_FREESATEPG
			e.setName(language, buf[1] == 0x1f ? freesatHuffmanDecode(std::string(buf+1, evt->event_name_length-1)) : std::string(buf+1, evt->event_name_length-1));
#else
			e.setName(language, std::string(buf+1, evt->event_name_length-1));
#endif
		} else 
#endif // 0
{
#ifdef ENABLE_FREESATEPG
			// FIXME convertDVBUTF8
			e.setName(language, buf[0] == 0x1f ? freesatHuffmanDecode(std::string(buf, evt->event_name_length)) : std::string(buf, evt->event_name_length));
#else
			//e.setName(language, std::string(buf, evt->event_name_length));
			e.setName(language, convertDVBUTF8(buf, evt->event_name_length, table, tsidonid));
#endif
		}
	}
	buf+=evt->event_name_length;
	unsigned char textlength=*((unsigned char *)buf);
	if(textlength > 2) {
#if 0
		if(*(buf+1) < 0x06) {// other code table
#ifdef ENABLE_FREESATEPG
			e.setText(language, buf[2] == 0x1f ? freesatHuffmanDecode(std::string((++buf)+1, textlength-1)) : std::string((++buf)+1, textlength-1));
#else
			e.setText(language, std::string((++buf)+1, textlength-1));
#endif
		} else 
#endif // 0
{
#ifdef ENABLE_FREESATEPG
			e.setText(language, buf[1] == 0x1f ? freesatHuffmanDecode(std::string(++buf, textlength)) : std::string(++buf, textlength));
#else
			//e.setText(language, std::string(++buf, textlength));
			e.setText(language, convertDVBUTF8((++buf), textlength, table, tsidonid));
#endif
		}
	}

//  printf("Name: %s\n", e.name.c_str());
//  printf("Text: %s\n", e.text.c_str());

}

void SIsectionEIT::parseDescriptors(const char *des, unsigned len, SIevent &e)
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
		if((unsigned)(desc->descriptor_length+2)>len)
			break;
		len-=desc->descriptor_length+2;
		des+=desc->descriptor_length+2;
	}
}

// Die infos aus dem Puffer holen
void SIsectionEIT::parse(void)
{
	const char *actPos;
	const char *bufEnd;
	struct eit_event *evt;
	unsigned short descriptors_loop_length;

	if (!buffer || parsed)
		return;

	if (bufferLength < sizeof(SI_section_EIT_header) + sizeof(struct eit_event)) {
		bufferLength=0;
		return;
	}

	actPos = buffer + sizeof(SI_section_EIT_header);
	bufEnd = buffer + bufferLength;

	while (actPos < bufEnd - sizeof(struct eit_event)) {
		evt = (struct eit_event *) actPos;
		SIevent e(evt);
		e.service_id = service_id();
		e.original_network_id = original_network_id();
		e.transport_stream_id = transport_stream_id();
		descriptors_loop_length = sizeof(struct eit_event) + ((evt->descriptors_loop_length_hi << 8) | evt->descriptors_loop_length_lo);
		parseDescriptors(actPos, min((unsigned)(bufEnd - actPos), descriptors_loop_length), e);
		evts.insert(e);
		actPos += descriptors_loop_length;
	}

	parsed = 1;
}


//-----------------------------------------------------------------------
// Da es vorkommen kann das defekte Packete empfangen werden
// sollte hier alles ueberprueft werden.
// Leider ist das noch nicht bei allen Descriptoren so.
//-----------------------------------------------------------------------
#ifdef ENABLE_PPT
void SIsectionPPT::parseLinkageDescriptor(const char *buf, SIevent &e, unsigned maxlen)
{
  if(maxlen>=sizeof(struct descr_linkage_header))
  {
    SIlinkage l((const struct descr_linkage_header *)buf);
    e.linkage_descs.insert(e.linkage_descs.end(), l);
//    printf("LinkName: %s\n", l.name.c_str());
  }
}

void SIsectionPPT::parseComponentDescriptor(const char *buf, SIevent &e, unsigned maxlen)
{
  if(maxlen>=sizeof(struct descr_component_header))
    e.components.insert(SIcomponent((const struct descr_component_header *)buf));
}

void SIsectionPPT::parseContentDescriptor(const char *buf, SIevent &e, unsigned maxlen)
{
  struct descr_generic_header *cont=(struct descr_generic_header *)buf;
  if(cont->descriptor_length+sizeof(struct descr_generic_header)>maxlen)
    return; // defekt
  const char *classification=buf+sizeof(struct descr_generic_header);
  while(classification<=buf+sizeof(struct descr_generic_header)+cont->descriptor_length-2) {
    e.contentClassification+=std::string(classification, 1);
//    printf("Content: 0x%02hhx\n", *classification);
    e.userClassification+=std::string(classification+1, 1);
//    printf("User: 0x%02hhx\n", *(classification+1));
    classification+=2;
  }
}

void SIsectionPPT::parseParentalRatingDescriptor(const char *buf, SIevent &e, unsigned maxlen)
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

void SIsectionPPT::parseExtendedEventDescriptor(const char *buf, SIevent &e, unsigned maxlen)
{
  struct descr_extended_event_header *evt=(struct descr_extended_event_header *)buf;
  if((evt->descriptor_length+sizeof(descr_generic_header)>maxlen) || (evt->descriptor_length<sizeof(struct descr_extended_event_header)-sizeof(descr_generic_header)))
    return; // defekt

  std::string language;
  language += evt->iso_639_2_language_code_hi + evt->iso_639_2_language_code_mid + evt->iso_639_2_language_code_lo;

  unsigned char *items=(unsigned char *)(buf+sizeof(struct descr_extended_event_header));
  while(items<(unsigned char *)(buf+sizeof(struct descr_extended_event_header)+evt->length_of_items)) {
    if(*items) {
      if(*(items+1) < 0x06) // other code table
        e.itemDescription=std::string((const char *)(items+2), min(maxlen-((const char *)items+2-buf), (*items)-1));
      else
        e.itemDescription=std::string((const char *)(items+1), min(maxlen-((const char *)items+1-buf), *items));
//      printf("Item Description: %s\n", e.itemDescription.c_str());
    }
    items+=1+*items;
    if(*items) {
      e.item=std::string((const char *)(items+1), min(maxlen-((const char *)items+1-buf), *items));
//    printf("Item: %s\n", e.item.c_str());
    }
    items+=1+*items;
  }
  if(*items) {
    if(*(items+1) < 0x06) // other code table
      e.appendExtendedText(language, std::string((const char *)(items+2),min(maxlen-((const char *)items+2-buf), (*items)-1)));
    else
      e.appendExtendedText(language, std::string((const char *)(items+1), min(maxlen-((const char *)items+1-buf), *items)));
//    printf("Extended Text: %s\n", e.extendedText.c_str());
  }
}

void SIsectionPPT::parseShortEventDescriptor(const char *buf, SIevent &e, unsigned maxlen)
{
  struct descr_short_event_header *evt=(struct descr_short_event_header *)buf;
  if((evt->descriptor_length+sizeof(descr_generic_header)>maxlen) || (evt->descriptor_length<sizeof(struct descr_short_event_header)-sizeof(descr_generic_header)))
    return; // defekt

  std::string language;
  language += evt->language_code_hi + evt->language_code_mid + evt->language_code_lo;

  buf+=sizeof(struct descr_short_event_header);
  if(evt->event_name_length) {
    if(*buf < 0x06) // other code table
      e.setName(language, std::string(buf+1, evt->event_name_length-1));
    else
      e.setName(language, std::string(buf, evt->event_name_length));
  }

  buf+=evt->event_name_length;
  unsigned char textlength=*((unsigned char *)buf);
  if(textlength > 2) {
    if(*(buf+1) < 0x06) // other code table
      e.setText(language, std::string((++buf)+1, textlength-1));
    else
      e.setText(language, std::string(++buf, textlength));
  }

//  printf("Name: %s\n", e.name.c_str());
//  printf("Text: %s\n", e.text.c_str());

}

void SIsectionPPT::parsePrivateContentOrderDescriptor(const char *buf, SIevent &e, unsigned maxlen)
{
  struct descr_short_event_header *evt=(struct descr_short_event_header *)buf;
  if((evt->descriptor_length+sizeof(descr_generic_header)>maxlen) || (evt->descriptor_length<sizeof(struct descr_short_event_header)-sizeof(descr_generic_header)))
    return; // defekt

#if 0
// to be done    
    unsigned char Order_number_length;
    char Order_number[Order_number_length];
    unsigned char Order_price_length;
    char Order_price[Order_price_length];
    unsigned char Order_phone_number_length;
    char Order_phone_number[Order_phone_number_length];
    unsigned char SMS_order_information_length;
    char SMS_order_information[SMS_order_information_length];
    unsigned char URL_order_information_length;
    char URL_order_information[URL_order_information_length];
#endif
}

void SIsectionPPT::parsePrivateParentalInformationDescriptor(const char *buf, SIevent &e, unsigned maxlen)
{
  struct descr_short_event_header *evt=(struct descr_short_event_header *)buf;
  if((evt->descriptor_length+sizeof(descr_generic_header)>maxlen) || (evt->descriptor_length<sizeof(struct descr_short_event_header)-sizeof(descr_generic_header)))
    return; // defekt

  buf+=sizeof(struct descr_generic_header);

  if (sizeof(struct descr_generic_header)+1 < evt->descriptor_length) {
    e.ratings.insert(SIparentalRating(std::string("", 0), *(buf)));
  }
#if 0
    unsigned char rating;
    unsigned char Controll_time_t1[3]; // BCD coded
    unsigned char Controll_time_t2[3]; // BCD coded
    unsigned char Parental_information_length;
    unsigned char Parental_information[Parental_information_length];
#endif
}

void SIsectionPPT::parsePrivateContentTransmissionDescriptor(const char *buf, SIevent &e, unsigned maxlen)
{
	unsigned short starttime_loop_length = 0;
	unsigned char tm_buf[6];
	int i;

	struct descr_short_event_header *evt=(struct descr_short_event_header *)buf;
	if((evt->descriptor_length+sizeof(descr_generic_header)>maxlen) || (evt->descriptor_length<sizeof(struct descr_short_event_header)-sizeof(descr_generic_header)))
		return; // defekt

	//printf("parsePrivateContentTransmissionDescriptor\n");
	const char *p=buf+sizeof(struct descr_generic_header);
	if (sizeof(struct descr_generic_header)+1 <= maxlen) e.transport_stream_id = ((*p)<<8) | (*(p+1));
	if (sizeof(struct descr_generic_header)+3 <= maxlen) e.original_network_id = ((*(p+2))<<8) | (*(p+3));
	if (sizeof(struct descr_generic_header)+5 <= maxlen) e.service_id = ((*(p+4))<<8) | (*(p+5));

	p += 6;
	while(p+6 <= buf + evt->descriptor_length + sizeof(struct descr_generic_header)) {// at least one startdate/looplength/time entry
		tm_buf[0] = *(p);
		tm_buf[1] = *(p+1);
		starttime_loop_length = (*(p+2))/3;
		for (i=0;i<starttime_loop_length; i++) {
			tm_buf[2] = *(p+3*i+3);
			tm_buf[3] = *(p+3*i+4);
			tm_buf[4] = *(p+3*i+5);
			e.times.insert(SItime(changeUTCtoCtime(tm_buf), duration()));
		}
		p+=3 + 3*starttime_loop_length; // goto next starttime entry
	}

	// fake linkage !?
	SIlinkage l;
	l.linkageType = 0; // no linkage descriptor
	l.transportStreamId = e.transport_stream_id;
	l.originalNetworkId = e.original_network_id;
	l.serviceId = e.service_id;
	e.linkage_descs.insert(e.linkage_descs.end(), l);
}

void SIsectionPPT::parseDescriptors(const char *des, unsigned len, SIevent &e)
{
	struct descr_generic_header *desc;
	bool linkage_alreadyseen = false;

	while(len>=sizeof(struct descr_generic_header)) {
		desc=(struct descr_generic_header *)des;

//    printf("Type: %s\n", decode_descr(desc->descriptor_tag));
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
		else if(desc->descriptor_tag==0xF0)
			parsePrivateContentOrderDescriptor((const char *)desc, e, len);
		else if(desc->descriptor_tag==0xF1)
			parsePrivateParentalInformationDescriptor((const char *)desc, e, len);
		else if(desc->descriptor_tag==0xF2) {
			if (linkage_alreadyseen) {
				// Private EPG can have two linkage descriptors with their own date/time parameters for one event
				// not sure if current event system supports it therefore:
				// Generate additional Event(s) if there are more than one linkage descriptor (for repeated transmission)
				SIevent e2(e);
				e2.linkage_descs.clear();
				e2.times.clear();
				parsePrivateContentTransmissionDescriptor((const char *)desc, e2, len);
				evts.insert(e2);
			} else {
				parsePrivateContentTransmissionDescriptor((const char *)desc, e, len);
				linkage_alreadyseen = true;
			}
		}
		if((unsigned)(desc->descriptor_length+2)>len)
			break;
		len-=desc->descriptor_length+2;
		des+=desc->descriptor_length+2;
	}
}

// Die infos aus dem Puffer holen
void SIsectionPPT::parse(void)
{
	const char *actPos;
	unsigned short descriptors_loop_length;

	if (!buffer || parsed)
		return;

	if (bufferLength < sizeof(SI_section_PPT_header)) {
		bufferLength=0;
		return;
	}

	actPos = &buffer[sizeof(SI_section_PPT_header)];
	
	/*while (actPos < &buffer[bufferLength])*/ {
		SIevent e;
		descriptors_loop_length = (((SI_section_PPT_header*)buffer)->descriptor_section_length_hi << 8) | ((SI_section_PPT_header*)buffer)->descriptor_section_length_lo;
		e.eventID = (unsigned short)(content_id()); // ??
		parseDescriptors(actPos, min((unsigned)(buffer + bufferLength - actPos), descriptors_loop_length), e);
		evts.insert(e);
		actPos += descriptors_loop_length;
	}

	parsed = 1;
}
#endif

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

void SIsectionSDT::parseDescriptors(const char *des, unsigned len, SIservice &s)
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
	const char *actPos;
	const char *bufEnd;
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
#ifdef UPDATE_NETWORKS
/************************************/
/*
//Within the Service List all Channels of a bouquet are specified
int SIsectionBAT::parseServiceListDescriptor(const char *buf, SIbouquet &s, int section_no, int count)
{
  struct descr_generic_header *sv=(struct descr_generic_header *)buf;
  buf+=sizeof(struct descr_generic_header);
  unsigned short len = sv->descriptor_length;
  while(len >= sizeof(struct service_list_entry)) {
  	struct service_list_entry *sl=(struct service_list_entry *)buf;
	buf+=sizeof(struct service_list_entry);
	SIbouquet bs(s);
	bs.service_id=(sl->service_id_hi << 8) | sl->service_id_lo;
	bs.serviceTyp=sl->service_type;
	bs.position = (uint16_t) (((section_no & 0x1f) << 11) + (count & 0x7ff));
//	printf("Section Number: %d Count: %d position: %04x\n", section_no, count, bs.position);
	bsv.insert(bs);
	len -= sizeof(struct service_list_entry);
	count++;
  }
  return count;
}

void SIsectionBAT::parseBouquetNameDescriptor(const char *buf, SIbouquet &s)
{
  struct descr_generic_header *sv=(struct descr_generic_header *)buf;
  buf+=sizeof(struct descr_generic_header);
  if(sv->descriptor_length) {
  
//    if(*buf < 0x06) // other code table
  //    s.bouquetName=std::string(buf+1, sv->descriptor_length-1);
    //else
      //s.bouquetName=std::string(buf, sv->descriptor_length);

    s.bouquetName  = CDVBString((const char *)(buf), sv->descriptor_length).getContent();
  }
  //printf("Bouquet-Name: %s\n", s.bouquetName.c_str());
}

int SIsectionBAT::parseDescriptors(const char *des, unsigned len, SIbouquet &s, int section_no, int count, const char *bouquetName)
{
  struct descr_generic_header *desc;
  while(len>=sizeof(struct descr_generic_header)) {
    desc=(struct descr_generic_header *)des;
//    printf("Type: %s\n", decode_descr(desc->descriptor_tag));
//    printf("Length: %hhu\n", desc->descriptor_length);
    if(desc->descriptor_tag==0x41) {
//      printf("Found service list descriptor\n");
      count = parseServiceListDescriptor((const char *)desc, s, section_no, count);
    }
//    else if(desc->descriptor_tag==0x47) {
//      printf("Found bouquet name descriptor\n");
//      parseBouquetNameDescriptor((const char *)desc, s);
//    }
    len-=desc->descriptor_length+2;
    des+=desc->descriptor_length+2;
  }
  return count;
}
*/
// Die infos aus dem Puffer holen
void SIsectionBAT::parse(void)
{
	const char *actPos;
	struct bat_service *sv;
//	struct bouquet_ident bi;
	struct SI_section_BAT_header *bh;
	unsigned short descriptors_loop_length;
	unsigned short descriptors_length;
	uint32_t current_private_data_specifier;
	struct loop_len *ll;
//	std::string bouquetName = "";

	if (!buffer || parsed)
		return;

	if (bufferLength < sizeof(SI_section_BAT_header) + sizeof(struct bat_service)) {
		printf("BAT fix?\n");	//No Services possible - length too short
		bufferLength=0;
		return;
	}

	actPos = &buffer[0];				// We need Bouquet ID and bouquet descriptor length from header
	bh = (struct SI_section_BAT_header *)actPos;	// Header
	t_bouquet_id bouquet_id = (bh->bouquet_id_hi << 8) | bh->bouquet_id_lo;
//	printf("hi: %hu lo: %hu\n", bh->bouquet_descriptors_length_hi, bh->bouquet_descriptors_length_lo);
	descriptors_loop_length = (bh->bouquet_descriptors_length_hi << 8) | bh->bouquet_descriptors_length_lo;
//	SIbouquet s((bh->bouquet_id_hi << 8) | bh->bouquet_id_lo);	//Create a new Bouquet entry
//	printf("ident: %hu actpos: %p buf+bl: %p desclen: %hu\n", bi.bouquet_id, actPos, buffer+bufferLength, descriptors_loop_length);
	int count = 0;
	 //Fill out Bouquet Name
	//count = parseDescriptors(((const char *)bh) + sizeof(SI_section_BAT_header), descriptors_loop_length, s, bh->section_number, count);
	const char *des = ((const char *)bh) + sizeof(SI_section_BAT_header);
	unsigned len = descriptors_loop_length;
	struct descr_generic_header *desc;
	struct descr_generic_header *privdesc = NULL;
	std::string bouquetName = "";
	current_private_data_specifier = 0;
	while(len>=sizeof(struct descr_generic_header)) {
		desc=(struct descr_generic_header *)des;
		if(desc->descriptor_tag==0x47) {
//      printf("Found bouquet name descriptor\n");
			//parseBouquetNameDescriptor((const char *)desc, s);
			const char *buf = (const char *) desc;
			//struct descr_generic_header *sv=(struct descr_generic_header *)buf;
			buf+=sizeof(struct descr_generic_header);
			if(desc->descriptor_length) {
  /*
    if(*buf < 0x06) // other code table
      s.bouquetName=std::string(buf+1, sv->descriptor_length-1);
    else
      s.bouquetName=std::string(buf, sv->descriptor_length);
  */
				//Mega stupid providers do not reserve their own bouquet_id
				//So we do it for them... f*ck that's stupid too, but what else can we do...
				if (bouquet_id == 0x0001) {
					if (!strncmp((const char *) buf, "NOVA", 4))
						bouquet_id = NOVA;
					if (!strncmp((const char *) buf, "CanalDigitaal", 13))
						bouquet_id = CANALDIGITAAL;
				}

				bouquetName  = CDVBString((const char *)(buf), desc->descriptor_length).getContent();
			}
		}
		if (desc->descriptor_tag == 0x5f) {
			const char *buf = (const char *) desc;
			//struct descr_generic_header *sv=(struct descr_generic_header *)buf;
			buf+=sizeof(struct descr_generic_header);
			struct private_data_specifier *pds=(struct private_data_specifier *)buf;
			buf+=sizeof(struct private_data_specifier);
			current_private_data_specifier=(((pds->byte1 << 24) | (pds->byte2 << 16)) | (pds->byte3 << 8)) | pds->byte4; 
					//printf("Private Data Specifier: %08x\n", current_private_data_specifier);
		}
		len-=desc->descriptor_length+2;
		des+=desc->descriptor_length+2;
	}
	//s.bouquetName = bouquetName;
 	
	actPos += sizeof(SI_section_BAT_header) + descriptors_loop_length;
	
	ll = (struct loop_len *)actPos;
	descriptors_loop_length = (ll->descriptors_loop_length_hi << 8) | ll->descriptors_loop_length_lo; 	//len is not used at the moment
//	printf("desclen: %hu\n", descriptors_loop_length);
	actPos += sizeof(loop_len);
		
	if (bouquet_id != 0x0001) {
		while (actPos <= &buffer[bufferLength - sizeof(struct bat_service)]) {
			sv = (struct bat_service *)actPos;
		//s.transport_stream_id = (sv->transport_stream_id_hi << 8) | sv->transport_stream_id_lo;
		//s.original_network_id = (sv->original_network_id_hi << 8) | sv->original_network_id_lo;
	//	s.position = (uint16_t) (((bh->section_number & 0x1f) << 11) + (count & 0x7ff));
	//	printf("Section Number: %d Count: %d position: %04x\n", bh->section_number, count, s.position);
			descriptors_length = (sv->descriptors_loop_length_hi << 8) | sv->descriptors_loop_length_lo;
		 // Transport Stream Loop
		//count = parseDescriptors(((const char *)sv) + sizeof(struct bat_service), descriptors_length, s, bh->section_number, count,	
		 //(const char *) bouquetName[0]);
		 	bool found = false;
			int loop_count = 1;
		 	while (loop_count >= 0) {
			
				const char *des_ = ((const char *)sv) + sizeof(struct bat_service);
				len = descriptors_length;
			
				while(len>=sizeof(struct descr_generic_header)) {
    					desc=(struct descr_generic_header *)des_;
//    printf("Type: %s\n", decode_descr(desc->descriptor_tag));
//    printf("Length: %hhu\n", desc->descriptor_length);
					const char *buf = (const char *) desc;
					buf+=sizeof(struct descr_generic_header);
					unsigned short dlen = desc->descriptor_length;
	    				if ((desc->descriptor_tag==0x41) && (loop_count == 0) && (current_private_data_specifier != 0x00000002)) {
//      printf("Found service list descriptor\n");
				//count = parseServiceListDescriptor((const char *)desc, s, section_no, count);
				//struct descr_generic_header *sv=(struct descr_generic_header *)buf;
					
	  					while(dlen >= sizeof(struct service_list_entry)) {
  							struct service_list_entry *sl=(struct service_list_entry *)buf;
							buf+=sizeof(struct service_list_entry);
							SIbouquet bs(bouquet_id);
							bs.bouquetName = bouquetName;
							bs.transport_stream_id = (sv->transport_stream_id_hi << 8) | sv->transport_stream_id_lo;
							bs.original_network_id = (sv->original_network_id_hi << 8) | sv->original_network_id_lo;
							bs.service_id=(sl->service_id_hi << 8) | sl->service_id_lo;
							bs.serviceTyp=sl->service_type;
							bs.position = (uint16_t) (((bh->section_number & 0x1f) << 11) + (count & 0x7ff));
							if (found) {
								const char *privbuf = (const char *) privdesc;
								privbuf+=sizeof(struct descr_generic_header);
								unsigned short privdlen = privdesc->descriptor_length;
								bool found_posi = false;
								int order_entry_size;
								order_entry_size = sizeof(struct digplus_order_entry);
								while ((privdlen >= order_entry_size) && (!found_posi)) {
									struct digplus_order_entry *oe = (struct digplus_order_entry *)privbuf;
									privbuf+=order_entry_size;
									/*
									printf("Search: %04x Service_id: %04x Posi:%04x\n", 
										(sl->service_id_hi << 8) | sl->service_id_lo,
										(oe->service_id_hi << 8) | oe->service_id_lo,
										(oe->channel_number_hi << 8) | oe->channel_number_lo);
									*/
									if (	((sl->service_id_hi << 8) | sl->service_id_lo) == 
										((oe->service_id_hi << 8) | oe->service_id_lo)) {
										bs.position = (oe->channel_number_hi << 8) | oe->channel_number_lo;
										/*
										printf("Found Search: %04x Service_id: %04x Posi:%04x\n", 
										(sl->service_id_hi << 8) | sl->service_id_lo,
										current_service_id,
										current_channel_number);
										*/
										found_posi=true;
									}
									privdlen -= order_entry_size;
								}
								if (!found_posi)
									bs.position = 0xffff;
							}
//	printf("Section Number: %d Count: %d position: %04x\n", section_no, count, bs.position);
							//printf("Set Position: %04x\n",bs.position);
							bsv.insert(bs);
							dlen -= sizeof(struct service_list_entry);
							count++;
  						}
					}
					if (desc->descriptor_tag == 0x5f) {
						struct private_data_specifier *pds=(struct private_data_specifier *)buf;
						buf+=sizeof(struct private_data_specifier);
						if ((pds->byte1 != 0) || (pds->byte2 != 0) || (pds->byte3 != 0) || (pds->byte4 != 0))
							current_private_data_specifier=(((pds->byte1 << 24) | (pds->byte2 << 16)) | 
											(pds->byte3 << 8)) | pds->byte4; 
					//printf("Private Data Specifier: %08x\n", current_private_data_specifier);
					}
					if (desc->descriptor_tag == 0x81) {
						if (current_private_data_specifier == 0x55504300) {
							//printf("UPC Bouquet ordering descriptor found!\n");
							privdesc = (struct descr_generic_header *)desc;
							found = true;
						}
					}					
					if (desc->descriptor_tag == 0x82) {
						if ((current_private_data_specifier == 0x00000010) || (bouquet_id == 0x0021)) {
							//printf("TPS Bouquet ordering descriptor found!\n");
							privdesc = (struct descr_generic_header *)desc;
							found = true;
						}
					}
					if (desc->descriptor_tag == 0x83) {
						if ((current_private_data_specifier == 0x000000c0) || (current_private_data_specifier == 0x0000003a)) {
							//printf("Canal+ Bouquet ordering descriptor found!\n");
							privdesc = (struct descr_generic_header *)desc;
							found = true;
						}
					}
					if (desc->descriptor_tag == 0x93) {
						if ((current_private_data_specifier == 0x00362275) || (bouquet_id == CANALDIGITAAL)) {
							//printf("Irdeto Bouquet ordering descriptor found!\n");
							privdesc = (struct descr_generic_header *)desc;
							found = true;
						}
					}
					if ((desc->descriptor_tag == 0xb1) && (loop_count == 0)) {
						if (current_private_data_specifier == 0x00000002) {
							//printf("BSkyB Bouquet ordering descriptor found!\n");
							const char *privbuf = (const char *) desc;
							privbuf+=sizeof(struct descr_generic_header);
							unsigned short privdlen = desc->descriptor_length - 2;
							//Nirvana 27.4.06: first 2 bytes still unknown: always 0xffff on sky italia?
							//check if resulting bouquets on 28.2E look more sensible...
							struct bskyb_bid *bid = (struct bskyb_bid *)privbuf;
							if ((bid->unknown1 == 0xff) && (bid->unknown2 == 0xff)) {
							
								privbuf+=2; //first 2 bytes of each 0xb1 desc unknown
							
								while (privdlen >= sizeof(struct bskyb_order_entry)) {
									struct bskyb_order_entry *oe = (struct bskyb_order_entry *)privbuf;
									privbuf+=sizeof(struct bskyb_order_entry);
									SIbouquet bs(bouquet_id);
									bs.bouquetName = bouquetName;
									bs.transport_stream_id = (sv->transport_stream_id_hi << 8) | 
													sv->transport_stream_id_lo;
									bs.original_network_id = (sv->original_network_id_hi << 8) | 
													sv->original_network_id_lo;
									bs.service_id = (oe->service_id_hi << 8) | oe->service_id_lo;
									bs.serviceTyp = oe->service_type;
									bs.position = (oe->channel_number_hi << 8) | oe->channel_number_lo;
									bsv.insert(bs);
									privdlen -= sizeof(struct bskyb_order_entry);
								}
							}

						}
					}
	    				len-=desc->descriptor_length+2;
    					des_+=desc->descriptor_length+2;
  				}
				loop_count--;	
			}
		 
			actPos += sizeof(struct bat_service) + descriptors_length;
	//	count++;
		}
	}
	parsed = 1;
}

void SIsectionNIT::copyDeliveryDescriptor(const char *buf, SInetwork &s)
{
  //struct descr_generic_header *sv=(struct descr_generic_header *)buf;
  buf+=sizeof(struct descr_generic_header);
  memcpy(s.delivery_descriptor, buf, sizeof(struct satellite_delivery_descriptor));  //same size as cable...
  //printf("Bouquet-Name: %s\n", s.bouquetName.c_str());
}

void SIsectionNIT::parseDescriptors(const char *des, unsigned len, SInetwork &s)
{
//  struct satellite_delivery_descriptor *sdd;
//  const char *ddp;
//  t_transport_stream_id tsid;
//  t_original_network_id onid;
//  unsigned short orbital_pos;

  struct descr_generic_header *desc;
  des += sizeof(struct nit_transponder);
  len -= sizeof(struct nit_transponder);

  while(len>=sizeof(struct descr_generic_header)) {
    desc=(struct descr_generic_header *)des;
//    printf("Type: %s\n", decode_descr(desc->descriptor_tag));
//    printf("Length: %hhu\n", desc->descriptor_length);
    if ( (desc->descriptor_tag==0x43) || (desc->descriptor_tag==0x44) ) {
      s.delivery_type = desc->descriptor_tag;
//      printf("Found satellite_delivery_system_descriptor\n");
      copyDeliveryDescriptor((const char *)desc, s);
//      ddp = &s.delivery_descriptor[0];
//      sdd = (struct satellite_delivery_descriptor *)ddp;
//      tsid = s.transport_stream_id;
//      onid = s.original_network_id;
//      orbital_pos = (sdd->orbital_pos_hi << 8) | sdd->orbital_pos_lo;
//      printf("ONID: %04x TSID: %04x Orbital Position: %d\n", onid, tsid, orbital_pos);
    }
    len-=desc->descriptor_length+2;
    des+=desc->descriptor_length+2;
  }
}

// Die infos aus dem Puffer holen
void SIsectionNIT::parse(void)
{

	const char *actPos;
	const char *bufEnd;
	struct nit_transponder *tp;
	struct SI_section_NIT_header *nh;
	unsigned short descriptors_loop_length;
	unsigned short descriptors_length;
	struct loop_len *ll;
	//t_network_id network_id;

	if (!buffer || parsed)
		return;

	if (bufferLength < sizeof(SI_section_NIT_header) + sizeof(struct nit_transponder)) {
		printf("NIT fix?\n");	//No Services possible - length too short
		bufferLength=0;
		return;
	}
	
	actPos = buffer;				// We need Bouquet ID and bouquet descriptor length from header
	bufEnd = buffer + bufferLength;
	nh = (struct SI_section_NIT_header *)actPos;	// Header
//	printf("hi: %hu lo: %hu\n", bh->bouquet_descriptors_length_hi, bh->bouquet_descriptors_length_lo);
	descriptors_loop_length = (nh->network_descriptors_length_hi << 8) | nh->network_descriptors_length_lo;
//	SIbouquet s((bh->bouquet_id_hi << 8) | bh->bouquet_id_lo);	//Create a new Bouquet entry
//	printf("ident: %hu actpos: %p buf+bl: %p desclen: %hu\n", bi.bouquet_id, actPos, buffer+bufferLength, descriptors_loop_length);
//	parseDescriptors(((const char *)bh) + sizeof(SI_section_BAT_header), descriptors_loop_length, s); //Fill out Bouquet Name
	actPos += sizeof(SI_section_NIT_header) + descriptors_loop_length;
	
	ll = (struct loop_len *)actPos;
	descriptors_loop_length = (ll->descriptors_loop_length_hi << 8) | ll->descriptors_loop_length_lo; 	//len is not used at the moment
//	printf("desclen: %hu\n", descriptors_loop_length);
	actPos += sizeof(loop_len);
	
	while (actPos <= bufEnd - sizeof(struct nit_transponder)) {
		tp = (struct nit_transponder *)actPos;
		SInetwork s(tp);
		s.network_id = (nh->network_id_hi << 8) | nh->network_id_lo;
		s.transport_stream_id = (tp->transport_stream_id_hi << 8) | tp->transport_stream_id_lo;
		s.original_network_id = (tp->original_network_id_hi << 8) | tp->original_network_id_lo;
		descriptors_length = sizeof(struct nit_transponder) + ((tp->descriptors_loop_length_hi << 8) | tp->descriptors_loop_length_lo);
		parseDescriptors(actPos, min((unsigned)(bufEnd - actPos), descriptors_length), s); // Transport Stream Loop
		ntw.insert(s);
		actPos += descriptors_length;
	}
	parsed = 1;
}
#endif
#ifndef DO_NOT_INCLUDE_STUFF_NOT_NEEDED_FOR_SECTIONSD
// Liest n Bytes aus einem Socket per read
// Liefert 0 bei timeout
// und -1 bei Fehler
// ansonsten die Anzahl gelesener Bytes
inline int readNbytes(int fd, char *buf, int n, unsigned timeoutInSeconds)
{
int j;
  for(j=0; j<n;) {
    struct pollfd ufds;
//    memset(&ufds, 0, sizeof(ufds));
    ufds.fd=fd;
    ufds.events=POLLIN|POLLPRI;
    ufds.revents=0;
    int rc=poll(&ufds, 1, timeoutInSeconds*1000);
    if(!rc)
      return 0; // timeout
    else if(rc<0) {
      perror ("poll");
      return -1;
    }
    int r=read (fd, buf, n-j);
    if(r<=0) {
      perror ("read");
      return -1;
    }
    j+=r;
    buf+=r;
  }
  return j;
}

//
// Beachtung der Stuffing tables (ST) fehlt noch
//
int SIsections :: readSections(const unsigned short pid, const unsigned char filter, const unsigned char mask, int readNext, unsigned timeoutInSeconds)
{
	int fd;
	struct SI_section_header header;
	unsigned long long firstKey=(unsigned long long)-1;
	SIsections missingSections;
	char *buf;
	unsigned short section_length;

	if ((fd = open(DEMUX_DEVICE, O_RDWR)) == -1) {
		perror(DEMUX_DEVICE);
		return 1;
	}

	if (!setfilter(fd, pid, filter, mask, DMX_IMMEDIATE_START | DMX_CHECK_CRC)) {
		close(fd);
		return 2;
	}

	time_t szeit = time(NULL);

	// Erstes Segment lesen

	do {
		if (time(NULL) > szeit + (long)timeoutInSeconds) {
			close(fd);
			return 0; // timeout -> kein EPG
		}

		int rc = readNbytes(fd, (char *)&header, sizeof(header), timeoutInSeconds);

		if(!rc) {
			close(fd);
			return 0; // timeout -> kein EPG
		}

		else if(rc<0) {
			close(fd);
			//perror ("read header");
			return 3;
		}

		section_length = (header.section_length_hi << 8) | header.section_length_lo;

		buf = new char[sizeof(header) + section_length - 5];

		if (!buf) {
			close(fd);
			fprintf(stderr, "Not enough memory!\n");
			return 4;
		}

		// Den Header kopieren
		memcpy(buf, &header, sizeof(header));

		rc = readNbytes(fd, &buf[sizeof(header)], section_length - 5, timeoutInSeconds);

		if (!rc) {
			close(fd);
			delete[] buf;
			return 0; // timeout -> kein EPG
		}

		else if (rc<0) {
			close(fd);
			//perror ("read section");
			delete[] buf;
			return 5;
		}

		if ((readNext) || (header.current_next_indicator)) {
			// Wir wollen nur aktuelle sections
			insert(SIsection(sizeof(header) + section_length - 5, buf));
			firstKey = SIsection::key(&header);

			// Sonderfall: Nur eine Section
			// d.h. wir sind fertig
			if ((!header.section_number) && (!header.last_section_number)) {
				close(fd);
				return 0;
			}
		}

		else {
			delete[] buf;
		}

	} while (firstKey == (unsigned long long) -1);

	// Die restlichen Segmente lesen
	szeit = time(NULL);

	for (;;) {
		if (time(NULL) > szeit + (long)timeoutInSeconds)
			break; // timeout

		int rc = readNbytes(fd, (char *)&header, sizeof(header), timeoutInSeconds);

		if(!rc)
			break; // timeout

		else if(rc<0) {
			close(fd);
			//perror ("read header");
			return 6;
		}

		if (firstKey==SIsection::key(&header))
			// Wir haben die 1. section wieder gefunden
			break;

		section_length = (header.section_length_hi << 8) | header.section_length_lo;

		buf = new char[sizeof(header) + section_length - 5];

		if (!buf) {
			close(fd);
			fprintf(stderr, "Not enough memory!\n");
			return 7;
		}

		// Den Header kopieren (evtl. malloc und realloc nehmen)
		memcpy(buf, &header, sizeof(header));

		// den Rest der Section einlesen
		rc = readNbytes(fd, &buf[sizeof(header)], section_length - 5, timeoutInSeconds);

		if (!rc) {
			delete[] buf;
			break; // timeout
		}

		else if (rc < 0) {
			close(fd);
			delete[] buf;
			//perror ("read section");
			return 8;
		}

		if ((readNext) || (header.current_next_indicator))
			insert(SIsection(sizeof(header) + section_length - 5, buf));
		else
			delete[] buf;
	}

	close(fd);

#ifdef DEBUG
	// Die Sections ausgeben
	printf("----------------Found sections-----------------------\n");
	//  for_each(begin(), end(), printSIsection());
	for_each(begin(), end(), printSIsectionEIT());
	printf("-----------------------------------------------------\n");
#endif // DEBUG


	// Jetzt erstellen wir eine Liste der fehlenden Sections
	unsigned short actualTableIDextension = (unsigned short) -1;
	unsigned char actualTableID = (unsigned char) -1;
	unsigned char maxNr = 0;
	unsigned char lastNr = 0;

	for (SIsections::iterator k = begin(); k != end(); k++) {
		if ((k->tableIDextension() != actualTableIDextension) || (k->tableID() != actualTableID)) {
			// Neue Table-ID-Extension
			maxNr = k->lastSectionNumber();
			actualTableIDextension = k->tableIDextension();
			actualTableID = k->tableID();
		}

		else if (k->sectionNumber() != (unsigned char)(lastNr + 1)) {
			// Es fehlen Sections
			for (unsigned l = lastNr + 1; l < k->sectionNumber(); l++) {
				//printf("Debug: t: 0x%02x s: %u nr: %u last: %u max: %u l: %u\n", actualTableID, actualTableIDextension, k->sectionNumber(), lastNr, maxNr, l);

				struct SI_section_header h;
				memcpy(&h, k->header(), sizeof(struct SI_section_header));
				h.section_number = l;
				missingSections.insert(SIsection(&h));
			}
		}

		lastNr = k->sectionNumber();
	}

#ifdef DEBUG
	printf("Sections read: %d\n\n", size());
#endif // DEBUG

	if (!missingSections.size())
		return 0;

#ifdef DEBUG
	printf("----------------Missing sections---------------------\n");
	for_each(missingSections.begin(), missingSections.end(), printSmallSIsectionHeader());
	printf("-----------------------------------------------------\n");
	printf("Sections read: %d\n\n", size());
	printf("Sections misssing: %d\n", missingSections.size());
	printf("Searching missing sections\n");
#endif // DEBUG

	szeit = time(NULL);

	if ((fd = open(DEMUX_DEVICE, O_RDWR)) == -1) {
		perror(DEMUX_DEVICE);
		return 9;
	}

	if (!setfilter(fd, pid, filter, mask, DMX_IMMEDIATE_START | DMX_CHECK_CRC)) {
		close(fd);
		return 10;
	}

	// Jetzt lesen wir die fehlenden Sections ein
	for(;;) {
		if (time(NULL) > szeit + (long)timeoutInSeconds)
			break; // Timeout

		int rc = readNbytes(fd, (char *)&header, sizeof(header), timeoutInSeconds);

		if(!rc)
			break; // timeout

		else if (rc < 0) {
			close(fd);
			//perror ("read header");
			return 11;

		}

		section_length = (header.section_length_hi << 8) | header.section_length_lo;
		
		buf = new char[sizeof(header) + section_length - 5];

		if (!buf) {
			close(fd);
			fprintf(stderr, "Not enough memory!\n");
			return 12;
		}

		// Den Header kopieren (evtl. malloc und realloc nehmen)
		memcpy(buf, &header, sizeof(header));
		// den Rest der Section einlesen
		rc = readNbytes(fd, &buf[sizeof(header)], section_length - 5, timeoutInSeconds);

		if (!rc) {
			delete[] buf;
			break; // timeout
		}

		else if (rc < 0) {
			close(fd);
			delete[] buf;
			//perror ("read section");
			return 13;
		}

		if (missingSections.find(SIsection(&header)) != missingSections.end()) {
#ifdef DEBUG
			printf("Find missing section:");
			SIsection::dumpSmallSectionHeader(&header);
#endif  // DEBUG
			// War bisher vermisst
			// In die Menge einfuegen
			insert(SIsection(sizeof(header) + section_length - 5, buf));

			// Und aus der vermissten Menge entfernen
			missingSections.erase(SIsection(&header));
#ifdef DEBUG
			printf("Sections misssing: %d\n", missingSections.size());
#endif // DEBUG
		}

		else {
			// Puffer wieder loeschen
			delete[] buf;
		}
	}

	close(fd);
	return 0;
}
#endif
