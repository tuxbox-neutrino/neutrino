/*
 * Copyright (C) 2012 CoolStream International Ltd
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

#ifndef _TRANSPONDER_H_
#define _TRANSPONDER_H_

#include <zapit/types.h>
#include <zapit/frontend_types.h>
#include <string>
#include <map>

class transponder
{
public:
	t_transport_stream_id transport_stream_id;
	t_original_network_id original_network_id;
	transponder_id_t transponder_id;
	t_satellite_position satellitePosition;
	uint8_t deltype;

	FrontendParameters feparams;
	unsigned char polarization;
	bool updated;

	transponder(fe_type_t fType, const transponder_id_t t_id, const FrontendParameters p_feparams, const uint8_t p_polarization = 0);
	transponder();

	bool operator==(const transponder& t) const;
	bool compare (const transponder& t) const;
	void dumpServiceXml(FILE * fd);
	void dump(std::string label = "tp");
	void ddump(std::string label = "tp");
	static char pol(unsigned char pol);
	std::string description();
};

typedef std::map <transponder_id_t, transponder> transponder_list_t;
typedef std::map <transponder_id_t, transponder>::iterator stiterator;
typedef std::pair<transponder_id_t, transponder> transponder_pair_t;

#endif
