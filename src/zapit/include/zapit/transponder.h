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
//private:
public:
	t_transport_stream_id transport_stream_id;
	t_original_network_id original_network_id;
	transponder_id_t transponder_id;
	t_satellite_position satellitePosition;

	FrontendParameters feparams;
	bool updated;
public:
	transponder(const transponder_id_t t_id, const FrontendParameters p_feparams);
	transponder();

	bool operator==(const transponder& t) const;
	bool compare (const transponder& t) const;
	void dumpServiceXml(FILE * fd);
	void dump(std::string label = "tp");
	void ddump(std::string label = "tp");
	static char pol(unsigned char pol);
	std::string description(void);
	static std::string getPLSMode(const uint8_t pls_mode);
	static std::string getPLSCode(const uint32_t pls_code);

	delivery_system_t getDelSys(void) { return feparams.delsys; }
	t_transport_stream_id getTransportStreamId(void) { return transport_stream_id; }
	void setTransportStreamId(t_transport_stream_id _transport_stream_id) { transport_stream_id = _transport_stream_id; }
	t_original_network_id getOriginalNetworkId(void) { return original_network_id; }
	void setOriginalNetworkId(t_original_network_id _original_network_id) { original_network_id = _original_network_id; }
	transponder_id_t getTransponderId(void) { return transponder_id; }
	void setTransponderId(transponder_id_t _transponder_id) { transponder_id = _transponder_id; }
	t_satellite_position getSatellitePosition(void) { return satellitePosition; }
	void setSatellitePosition(t_satellite_position SatellitePosition) { satellitePosition = SatellitePosition; }
	const FrontendParameters *getFEParams(void) const { return &feparams; }
	void setUpdated(bool Updated = true) { updated = Updated; }
	bool getUpdated(void) const { return updated; }
	uint32_t getFrequency(void) const { return feparams.frequency; }
	uint32_t getSymbolRate(void) const { return feparams.symbol_rate; }
	fe_bandwidth_t getBandwidth(void) const { return feparams.bandwidth; }
	fe_modulation_t getModulation(void) const { return feparams.modulation; }
	fe_code_rate_t getCodeRateHP(void) const { return feparams.code_rate_HP; }
	fe_code_rate_t getCodeRateLP(void) const { return feparams.code_rate_LP; }
	fe_code_rate_t getFecInner(void) const { return feparams.fec_inner; }
	fe_hierarchy_t getHierarchy(void) const { return feparams.hierarchy; }
	fe_guard_interval_t getGuardInterval(void) const { return feparams.guard_interval; }
	uint8_t getPolarization(void) const { return feparams.polarization; }
};

typedef std::map <transponder_id_t, transponder> transponder_list_t;
typedef std::map <transponder_id_t, transponder>::iterator stiterator;
typedef std::pair<transponder_id_t, transponder> transponder_pair_t;

#endif
