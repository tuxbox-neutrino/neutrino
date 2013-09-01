/*
 * SIsections.hpp, classes for SI sections (dbox-II-project)
 *
 * (C) 2001 by fnbrd (fnbrd@gmx.de),
 * Copyright (C) 2003 Andreas Oberritter <obi@tuxbox.org>
 *
 * Copyright (C) 2011-2012 CoolStream International Ltd
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

#ifndef SISECTIONS_HPP
#define SISECTIONS_HPP

#include <dvbsi++/event_information_section.h>
#include <dvbsi++/service_description_section.h>

#define USE_DVBSI_EVENTS

struct SI_section_EIT_header {
	unsigned table_id                       : 8;
#if __BYTE_ORDER == __BIG_ENDIAN
	unsigned section_syntax_indicator       : 1;
	unsigned reserved_future_use            : 1;
	unsigned reserved1                      : 2;
	unsigned section_length_hi              : 4;
#else
	unsigned section_length_hi              : 4;
	unsigned reserved1                      : 2;
	unsigned reserved_future_use            : 1;
	unsigned section_syntax_indicator       : 1;
#endif
	unsigned section_length_lo              : 8;
	unsigned service_id_hi                  : 8;
	unsigned service_id_lo                  : 8;
#if __BYTE_ORDER == __BIG_ENDIAN
	unsigned reserved2                      : 2;
	unsigned version_number                 : 5;
	unsigned current_next_indicator         : 1;
#else
	unsigned current_next_indicator         : 1;
	unsigned version_number                 : 5;
	unsigned reserved2                      : 2;
#endif
	unsigned section_number                 : 8;
	unsigned last_section_number            : 8;
	unsigned transport_stream_id_hi         : 8;
	unsigned transport_stream_id_lo         : 8;
	unsigned original_network_id_hi         : 8;
	unsigned original_network_id_lo         : 8;
	unsigned segment_last_section_number    : 8;
	unsigned last_table_id                  : 8;
} __attribute__ ((packed)) ; // 14 bytes

#ifndef USE_DVBSI_EVENTS
struct SI_section_header {
	unsigned table_id                       : 8;
#if __BYTE_ORDER == __BIG_ENDIAN
	unsigned section_syntax_indicator       : 1;
	unsigned reserved_future_use            : 1;
	unsigned reserved1                      : 2;
	unsigned section_length_hi              : 4;
#else
	unsigned section_length_hi              : 4;
	unsigned reserved1                      : 2;
	unsigned reserved_future_use            : 1;
	unsigned section_syntax_indicator       : 1;
#endif
	unsigned section_length_lo              : 8;
	unsigned table_id_extension_hi          : 8;
	unsigned table_id_extension_lo          : 8;
#if __BYTE_ORDER == __BIG_ENDIAN
	unsigned reserved2                      : 2;
	unsigned version_number                 : 5;
	unsigned current_next_indicator         : 1;
#else
	unsigned current_next_indicator         : 1;
	unsigned version_number                 : 5;
	unsigned reserved2                      : 2;
#endif
	unsigned section_number                 : 8;
	unsigned last_section_number            : 8;
} __attribute__ ((packed)) ; // 8 bytes
#endif 
class SIsectionEIT 
#ifdef USE_DVBSI_EVENTS
	: public EventInformationSection
#endif
{
protected:
	uint8_t * buffer;
	SIevents evts;
	int parsed;
	void parse(void);
public:
	SIsectionEIT(uint8_t *buf) 
#ifdef USE_DVBSI_EVENTS
		: EventInformationSection(buf) 
#endif
	{
		buffer = buf;
		parsed = 0;
		parse();
	}

	const SIevents &events(void) const {
		return evts;
	}

	int is_parsed(void) const {
		return parsed;
	}
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
};

class SIsectionSDT : public ServiceDescriptionSection
{
private:
	SIservices svs;
	int parsed;
	void parse(void);
public:
	SIsectionSDT(uint8_t *buf) : ServiceDescriptionSection(buf) {
		parsed = 0;
		parse();
	}

#if 0 // TODO ?
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
		return svs;
	}

};

class SIsectionTIME
{
	private:
		time_t dvbtime;
		int parsed;
		void parse(uint8_t *buf);
	public:
		SIsectionTIME(uint8_t *buf)
		{
			parsed = 0;
			parse(buf);
		}
		time_t getTime() { return dvbtime; }
		int is_parsed(void) const { return parsed; }
};

#endif // SISECTIONS_HPP
