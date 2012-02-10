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

#ifndef SISECTIONS_HPP
#define SISECTIONS_HPP

#include <dvbsi++/event_information_section.h>
#include <dvbsi++/service_description_section.h>

class SIsectionEIT : public EventInformationSection
{
protected:
	SIevents evts;
	int parsed;
	void parse(void);
public:
	SIsectionEIT(uint8_t *buf) : EventInformationSection(buf) 
	{
		parsed = 0;
		parse();
	}

	const SIevents &events(void) const {
		return evts;
	}

	int is_parsed(void) const {
		return parsed;
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

#endif // SISECTIONS_HPP
