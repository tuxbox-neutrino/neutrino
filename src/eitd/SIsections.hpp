#ifndef SISECTIONS_HPP
#define SISECTIONS_HPP
//
//    $Id: SIsections.hpp,v 1.28 2009/07/26 17:02:46 rhabarber1848 Exp $
//
//    classes for SI sections (dbox-II-project)
//
//    Homepage: http://dbox2.elxsi.de
//
//    Copyright (C) 2001 fnbrd (fnbrd@gmx.de)
//    Copyright (C) 2003 Andreas Oberritter <obi@tuxbox.org>
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

#include <endian.h>
#include <dvbsi++/event_information_section.h>

struct SI_section_SDT_header {
	unsigned table_id			: 8;
#if __BYTE_ORDER == __BIG_ENDIAN
	unsigned section_syntax_indicator	: 1;
	unsigned reserved_future_use		: 1;
	unsigned reserved1			: 2;
	unsigned section_length_hi		: 4;
#else
	unsigned section_length_hi		: 4;
	unsigned reserved1			: 2;
	unsigned reserved_future_use		: 1;
	unsigned section_syntax_indicator	: 1;
#endif
	unsigned section_length_lo		: 8;
	unsigned transport_stream_id_hi		: 8;
	unsigned transport_stream_id_lo		: 8;
#if __BYTE_ORDER == __BIG_ENDIAN
	unsigned reserved2			: 2;
	unsigned version_number			: 5;
	unsigned current_next_indicator		: 1;
#else
	unsigned current_next_indicator		: 1;
	unsigned version_number			: 5;
	unsigned reserved2			: 2;
#endif
	unsigned section_number			: 8;
	unsigned last_section_number		: 8;
	unsigned original_network_id_hi		: 8;
	unsigned original_network_id_lo		: 8;
	unsigned reserved_future_use2		: 8;
} __attribute__ ((packed)) ; // 11 bytes

#if 0
struct SI_section_EIT_header {
	unsigned table_id			: 8;
#if __BYTE_ORDER == __BIG_ENDIAN
	unsigned section_syntax_indicator	: 1;
	unsigned reserved_future_use		: 1;
	unsigned reserved1			: 2;
	unsigned section_length_hi		: 4;
#else
	unsigned section_length_hi		: 4;
	unsigned reserved1			: 2;
	unsigned reserved_future_use		: 1;
	unsigned section_syntax_indicator	: 1;
#endif
	unsigned section_length_lo		: 8;
	unsigned service_id_hi			: 8;
	unsigned service_id_lo			: 8;
#if __BYTE_ORDER == __BIG_ENDIAN
	unsigned reserved2			: 2;
	unsigned version_number			: 5;
	unsigned current_next_indicator		: 1;
#else
	unsigned current_next_indicator		: 1;
	unsigned version_number			: 5;
	unsigned reserved2			: 2;
#endif
	unsigned section_number			: 8;
	unsigned last_section_number		: 8;
	unsigned transport_stream_id_hi		: 8;
	unsigned transport_stream_id_lo		: 8;
	unsigned original_network_id_hi		: 8;
	unsigned original_network_id_lo		: 8;
	unsigned segment_last_section_number	: 8;
	unsigned last_table_id			: 8;
} __attribute__ ((packed)) ; // 14 bytes
#endif
// Muss evtl. angepasst werden falls damit RST, TDT und TOT gelesen werden sollen
// ^^^
//   RST usw. haben section_syntax_indicator == 0, andere == 1 (obi)
struct SI_section_header {
	unsigned table_id			: 8;
#if __BYTE_ORDER == __BIG_ENDIAN
	unsigned section_syntax_indicator	: 1;
	unsigned reserved_future_use		: 1;
	unsigned reserved1			: 2;
	unsigned section_length_hi		: 4;
#else
	unsigned section_length_hi		: 4;
	unsigned reserved1			: 2;
	unsigned reserved_future_use		: 1;
	unsigned section_syntax_indicator	: 1;
#endif
	unsigned section_length_lo		: 8;
	unsigned table_id_extension_hi		: 8;
	unsigned table_id_extension_lo		: 8;
#if __BYTE_ORDER == __BIG_ENDIAN
	unsigned reserved2			: 2;
	unsigned version_number			: 5;
	unsigned current_next_indicator		: 1;
#else
	unsigned current_next_indicator		: 1;
	unsigned version_number			: 5;
	unsigned reserved2			: 2;
#endif
	unsigned section_number			: 8;
	unsigned last_section_number		: 8;
} __attribute__ ((packed)) ; // 8 bytes


class SIsection //: public LongSection
{
public:
	//SIsection(void) { buffer = 0; bufferLength = 0;}

	SIsection(uint8_t *buf) //: LongSection(buf)
	{
		buffer = NULL;
		bufferLength = 0;
		//unsigned bufLength = 3 + getSectionLength();
		unsigned bufLength = 3 + (((struct SI_section_header*) buf)->section_length_hi << 8 | ((struct SI_section_header*) buf)->section_length_lo);
		if ((buf) && (bufLength >= sizeof(struct SI_section_header))) {
			buffer = buf;
			bufferLength = bufLength;
		}
	}

	// Destruktor
	virtual ~SIsection(void) {
		bufferLength = 0;
	}

protected:
	uint8_t *buffer;
	unsigned bufferLength;
};

class SIsectionEIT : /*public SIsection,*/ public EventInformationSection
{
public:
	SIsectionEIT(uint8_t *buf) : /*SIsection(buf),*/ EventInformationSection(buf) 
	{
		parsed = 0;
		parse();
	}
#if 0
	t_service_id service_id(void) const {
		return buffer ? ((((struct SI_section_EIT_header *)buffer)->service_id_hi << 8) |
				((struct SI_section_EIT_header *)buffer)->service_id_lo): 0;
	}

	t_original_network_id original_network_id(void) const {
		return buffer ? ((((struct SI_section_EIT_header *)buffer)->original_network_id_hi << 8) |
				((struct SI_section_EIT_header *)buffer)->original_network_id_lo) : 0;
	}

	t_transport_stream_id transport_stream_id(void) const {
		return buffer ? ((((struct SI_section_EIT_header *)buffer)->transport_stream_id_hi << 8) |
				((struct SI_section_EIT_header *)buffer)->transport_stream_id_lo) : 0;
	}
	struct SI_section_EIT_header const *header(void) const {
		return (struct SI_section_EIT_header *)buffer;
	}
#endif

	const SIevents &events(void) const {
		return evts;
	}

	int is_parsed(void) const {
		return parsed;
	}

protected:
	SIevents evts;
	int parsed;
	void parse(void);
#if 0
	void parseDescriptors(const uint8_t *desc, unsigned len, SIevent &e);
	void parseShortEventDescriptor(const char *buf, SIevent &e, unsigned maxlen);
	void parseExtendedEventDescriptor(const char *buf, SIevent &e, unsigned maxlen);
	void parseContentDescriptor(const char *buf, SIevent &e, unsigned maxlen);
	void parseComponentDescriptor(const char *buf, SIevent &e, unsigned maxlen);
	void parseParentalRatingDescriptor(const char *buf, SIevent &e, unsigned maxlen);
	void parseLinkageDescriptor(const char *buf, SIevent &e, unsigned maxlen);
	void parsePDCDescriptor(const char *buf, SIevent &e, unsigned maxlen);
#endif
#ifdef ENABLE_FREESATEPG
	std::string freesatHuffmanDecode(std::string input);
#endif
};


class SIsectionSDT : public SIsection
{
public:
	SIsectionSDT(const SIsection &s) : SIsection(s) {
		parsed = 0;
		parse();
	}

#if 0
	// Std-Copy
	SIsectionSDT(const SIsectionSDT &s) : SIsection(s) {
		svs = s.svs;
		parsed = s.parsed;
	}
#endif
	// Benutzt den uebergebenen Puffer (sollte mit new char[n] allokiert sein)
	SIsectionSDT(uint8_t *buf) : SIsection(buf) {
		parsed = 0;
		parse();
	}

	t_transport_stream_id transport_stream_id(void) const {
		return buffer ? ((((struct SI_section_SDT_header *)buffer)->transport_stream_id_hi << 8) |
				((struct SI_section_SDT_header *)buffer)->transport_stream_id_lo) : 0;
	}

	struct SI_section_SDT_header const *header(void) const {
		return (struct SI_section_SDT_header *)buffer;
	}

	t_original_network_id original_network_id(void) const {
		return buffer ? ((((struct SI_section_SDT_header *)buffer)->original_network_id_hi << 8) |
				((struct SI_section_SDT_header *)buffer)->original_network_id_lo) : 0;
	}
#if 0
	static void dump(const struct SI_section_SDT_header *header) {
		if (!header)
			return;
		SIsection::dump1((const struct SI_section_header *)header);
		printf("transport_stream_id: 0x%02x%02x\n", header->transport_stream_id_hi, header->transport_stream_id_lo);
		SIsection::dump2((const struct SI_section_header *)header);
		printf("original_network_id: 0x%02x%02x\n", header->original_network_id_hi, header->original_network_id_lo);
	}

	static void dump(const SIsectionSDT &s) {
		dump((struct SI_section_SDT_header *)s.buffer);
		for_each(s.svs.begin(), s.svs.end(), printSIservice());
	}

	void dump(void) const {
		dump((struct SI_section_SDT_header *)buffer);
		for_each(svs.begin(), svs.end(), printSIservice());
	}
#endif
	const SIservices &services(void) const {
		//if(!parsed)
		//	parse(); -> nicht const
		return svs;
	}

private:
	SIservices svs;
	int parsed;
	void parse(void);
	void parseDescriptors(const uint8_t *desc, unsigned len, SIservice &s);
	void parseServiceDescriptor(const char *buf, SIservice &s);
	void parsePrivateDataDescriptor(const char *buf, SIservice &s);
	void parseNVODreferenceDescriptor(const char *buf, SIservice &s);
};

#endif // SISECTIONS_HPP
