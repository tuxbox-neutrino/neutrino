/*
 * Copyright (C) 2012 CoolStream International Ltd
 * Copyright (C) 2012,2014 Stefan Seyfried
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

#include <stdlib.h>
#include <zapit/transponder.h>
#include <zapit/frontend_c.h>
#include <zapit/debug.h>

transponder::transponder(const transponder_id_t t_id, const FrontendParameters p_feparams)
{
	transponder_id      = t_id;
	transport_stream_id = GET_TRANSPORT_STREAM_ID_FROM_TRANSPONDER_ID(t_id);
	original_network_id = GET_ORIGINAL_NETWORK_ID_FROM_TRANSPONDER_ID(t_id);
	feparams            = p_feparams;
	updated             = false;
	satellitePosition   = GET_SATELLITEPOSITION_FROM_TRANSPONDER_ID(transponder_id);
	if (satellitePosition & 0xF000)
		satellitePosition = -(satellitePosition & 0xFFF);
	else
		satellitePosition = satellitePosition & 0xFFF;
}

transponder::transponder()
{
	memset(&feparams, 0, sizeof(FrontendParameters));
	transponder_id	= 0;
	transport_stream_id = 0;
	original_network_id = 0;
	satellitePosition = 0;
	updated = false;
}

bool transponder::operator==(const transponder& t) const
{
	if (CFrontend::isSat(feparams.delsys))
		return (
		(satellitePosition == t.satellitePosition) &&
		//(transport_stream_id == t.transport_stream_id) &&
		//(original_network_id == t.original_network_id) &&
		((getFEParams()->polarization & 1) == (t.getFEParams()->polarization & 1)) &&
		(abs((int) getFEParams()->frequency - (int)t.getFEParams()->frequency) <= 3000) &&
		(getFEParams()->plp_id == t.getFEParams()->plp_id) &&
		(getFEParams()->pls_mode == t.getFEParams()->pls_mode) &&
		(getFEParams()->pls_code == t.getFEParams()->pls_code)
		);
	else if (CFrontend::isTerr(feparams.delsys))
		return ((satellitePosition == t.satellitePosition) &&
		//(transport_stream_id == t.transport_stream_id) &&
		//(original_network_id == t.original_network_id) &&
		((getFEParams()->polarization & 1) == (t.getFEParams()->polarization & 1)) &&
		(abs((int) getFEParams()->frequency - (int)t.getFEParams()->frequency) <= 100) &&
		(getFEParams()->plp_id == t.getFEParams()->plp_id)
		);
	else
		return (
		(satellitePosition == t.satellitePosition) &&
		//(transport_stream_id == t.transport_stream_id) &&
		//(original_network_id == t.original_network_id) &&
		((getFEParams()->polarization & 1) == (t.getFEParams()->polarization & 1)) &&
		(abs((int) getFEParams()->frequency - (int)t.getFEParams()->frequency) <= 3000)
		);
}

bool transponder::compare(const transponder& t) const
{
	bool ret = false;

	if (CFrontend::isCable(feparams.delsys)) {
		ret = (
				(t == (*this)) &&
				(getFEParams()->symbol_rate == t.getFEParams()->symbol_rate) &&
				(getFEParams()->fec_inner == t.getFEParams()->fec_inner ||
				 getFEParams()->fec_inner == FEC_AUTO || t.getFEParams()->fec_inner == FEC_AUTO) &&
				(getFEParams()->modulation == t.getFEParams()->modulation ||
				 getFEParams()->modulation == QAM_AUTO || t.getFEParams()->modulation == QAM_AUTO)
		      );
	}
	else if (CFrontend::isSat(feparams.delsys)) {
		ret = (
				(t == (*this)) &&
				(getFEParams()->symbol_rate == t.getFEParams()->symbol_rate) &&
				(getFEParams()->delsys == t.getFEParams()->delsys) &&
				(getFEParams()->modulation == t.getFEParams()->modulation) &&
				(getFEParams()->fec_inner == t.getFEParams()->fec_inner ||
				 getFEParams()->fec_inner == FEC_AUTO || t.getFEParams()->fec_inner == FEC_AUTO) &&
				(getFEParams()->plp_id == t.getFEParams()->plp_id) &&
				(getFEParams()->pls_mode == t.getFEParams()->pls_mode) &&
				(getFEParams()->pls_code == t.getFEParams()->pls_code)
		      );
	}
	else if (CFrontend::isTerr(feparams.delsys)) {
		ret = (	(t == (*this)) &&
			(getFEParams()->bandwidth == t.getFEParams()->bandwidth) &&
			(getFEParams()->code_rate_HP == t.getFEParams()->code_rate_HP ||
			 getFEParams()->code_rate_HP == FEC_AUTO || t.getFEParams()->code_rate_HP == FEC_AUTO) &&
			(getFEParams()->code_rate_LP == t.getFEParams()->code_rate_LP ||
			 getFEParams()->code_rate_LP == FEC_AUTO || t.getFEParams()->code_rate_LP == FEC_AUTO) &&
			(getFEParams()->modulation == t.getFEParams()->modulation ||
			 getFEParams()->modulation == QAM_AUTO || t.getFEParams()->modulation == QAM_AUTO) &&
			(getFEParams()->plp_id == t.getFEParams()->plp_id)
		      );
	}

	return ret;
}

void transponder::dumpServiceXml(FILE * fd)
{
	if (CFrontend::isCable(feparams.delsys)) {
		fprintf(fd, "\t\t<TS id=\"%04x\" on=\"%04x\" frq=\"%u\" inv=\"%hu\" sr=\"%u\" fec=\"%hu\" mod=\"%hu\" sys=\"%hu\">\n",
				transport_stream_id, original_network_id,
				getFEParams()->frequency,
				getFEParams()->inversion,
				getFEParams()->symbol_rate,
				getFEParams()->fec_inner,
				getFEParams()->modulation,
				CFrontend::getXMLDeliverySystem(getFEParams()->delsys));
	} else if (CFrontend::isSat(feparams.delsys)) {
		if (getFEParams()->plp_id == NO_STREAM_ID_FILTER)
		{
		fprintf(fd, "\t\t<TS id=\"%04x\" on=\"%04x\" frq=\"%u\" inv=\"%hu\" sr=\"%u\" fec=\"%hu\" pol=\"%hu\" mod=\"%hu\" sys=\"%hu\">\n",
				transport_stream_id, original_network_id,
				getFEParams()->frequency,
				getFEParams()->inversion,
				getFEParams()->symbol_rate,
				getFEParams()->fec_inner,
				getFEParams()->polarization,
				getFEParams()->modulation,
				CFrontend::getXMLDeliverySystem(getFEParams()->delsys));
		}
		else
		{
		fprintf(fd, "\t\t<TS id=\"%04x\" on=\"%04x\" frq=\"%u\" inv=\"%hu\" sr=\"%u\" fec=\"%hu\" pol=\"%hu\" mod=\"%hu\" pli=\"%u\" plc=\"%u\" plm=\"%u\" sys=\"%hu\">\n",
				transport_stream_id, original_network_id,
				getFEParams()->frequency,
				getFEParams()->inversion,
				getFEParams()->symbol_rate,
				getFEParams()->fec_inner,
				getFEParams()->polarization,
				getFEParams()->modulation,
				getFEParams()->plp_id,
				getFEParams()->pls_code,
				getFEParams()->pls_mode,
				CFrontend::getXMLDeliverySystem(getFEParams()->delsys));
		}
	} else if (CFrontend::isTerr(feparams.delsys)) {
		if (getFEParams()->plp_id == NO_STREAM_ID_FILTER)
		{
		fprintf(fd, "\t\t<TS id=\"%04x\" on=\"%04x\" frq=\"%u\" inv=\"%hu\" bw=\"%u\" hp=\"%hu\" lp=\"%hu\" con=\"%u\" tm=\"%u\" gi=\"%u\" hi=\"%u\" sys=\"%hu\">\n",
				transport_stream_id, original_network_id,
				getFEParams()->frequency,
				getFEParams()->inversion,
				getFEParams()->bandwidth,
				getFEParams()->code_rate_HP,
				getFEParams()->code_rate_LP,
				getFEParams()->modulation,
				getFEParams()->transmission_mode,
				getFEParams()->guard_interval,
				getFEParams()->hierarchy,
				CFrontend::getXMLDeliverySystem(getFEParams()->delsys));
		}
		else
		{
		fprintf(fd, "\t\t<TS id=\"%04x\" on=\"%04x\" frq=\"%u\" inv=\"%hu\" bw=\"%u\" hp=\"%hu\" lp=\"%hu\" con=\"%u\" tm=\"%u\" gi=\"%u\" hi=\"%u\" pli=\"%u\" sys=\"%hu\">\n",
				transport_stream_id, original_network_id,
				getFEParams()->frequency,
				getFEParams()->inversion,
				getFEParams()->bandwidth,
				getFEParams()->code_rate_HP,
				getFEParams()->code_rate_LP, 
				getFEParams()->modulation,
				getFEParams()->transmission_mode,
				getFEParams()->guard_interval,
				getFEParams()->hierarchy,
				getFEParams()->plp_id,
				CFrontend::getXMLDeliverySystem(getFEParams()->delsys));
		}
	}
}

void transponder::dump(std::string label)
{
	if (CFrontend::isCable(feparams.delsys)) {
		printf("%s tp-id %016" PRIx64 " freq %d rate %d fec %d mod %d sys %d\n", label.c_str(),
				transponder_id,
				getFEParams()->frequency,
				getFEParams()->symbol_rate,
				getFEParams()->fec_inner,
				getFEParams()->modulation,
				getFEParams()->delsys);
	} else if (CFrontend::isSat(feparams.delsys)) {
		if (getFEParams()->plp_id == NO_STREAM_ID_FILTER)
		{
			printf("%s tp-id %016" PRIx64 " freq %d rate %d fec %d pol %d mod %d sys %d\n", label.c_str(),
				transponder_id,
				getFEParams()->frequency,
				getFEParams()->symbol_rate,
				getFEParams()->fec_inner,
				getFEParams()->polarization,
				getFEParams()->modulation,
				getFEParams()->delsys);
		}
		else
		{
			printf("%s tp-id %016" PRIx64 " freq %d rate %d fec %d pol %d mod %d pli %u plc %u plm %u sys %d\n", label.c_str(),
				transponder_id,
				getFEParams()->frequency,
				getFEParams()->symbol_rate,
				getFEParams()->fec_inner,
				getFEParams()->polarization,
				getFEParams()->modulation,
				getFEParams()->plp_id,
				getFEParams()->pls_code,
				getFEParams()->pls_mode,
				getFEParams()->delsys);
		}
	} else if (CFrontend::isTerr(feparams.delsys)) {
		if (getFEParams()->plp_id == NO_STREAM_ID_FILTER)
		{
			printf("%s tp-id %016" PRIx64 " freq %d bw %d coderate_HP %d coderate_LP %d const %d guard %d sys %d\n", label.c_str(),
				transponder_id,
				getFEParams()->frequency,
				getFEParams()->bandwidth,
				getFEParams()->code_rate_HP,
				getFEParams()->code_rate_LP,
				getFEParams()->modulation,
				getFEParams()->guard_interval,
				getFEParams()->delsys);
		}
		else
		{
			printf("%s tp-id %016" PRIx64 " freq %d bw %d coderate_HP %d coderate_LP %d const %d guard %d pli %u sys %d\n", label.c_str(),
				transponder_id,
				getFEParams()->frequency,
				getFEParams()->bandwidth,
				getFEParams()->code_rate_HP,
				getFEParams()->code_rate_LP,
				getFEParams()->modulation,
				getFEParams()->guard_interval,
				getFEParams()->plp_id,
				getFEParams()->delsys);
		}
	}
}

#if 0 
//never used
void transponder::ddump(std::string label) 
{
	if (zapit_debug)
		dump(label);
}
#endif

char transponder::pol(unsigned char p)
{
	if (p == 0)
		return 'H';
	else if (p == 1)
		return 'V';
	else if (p == 2)
		return 'L';
	else
		return 'R';
}

std::string transponder::description()
{
	char buf[128] = {0};
	const char *f, *s, *m, *f2;

	if (CFrontend::isSat(feparams.delsys)) {
		CFrontend::getDelSys(feparams.delsys, getFEParams()->fec_inner, getFEParams()->modulation,  f, s, m);
		if (getFEParams()->plp_id == NO_STREAM_ID_FILTER)
			snprintf(buf, sizeof(buf), "%d %c %d %s %s %s", getFEParams()->frequency/1000, pol(getFEParams()->polarization), getFEParams()->symbol_rate/1000, f, s, m);			
		else
			snprintf(buf, sizeof(buf), "%d %c %d %s %s %s (%u/%s/%s)", getFEParams()->frequency/1000, pol(getFEParams()->polarization), getFEParams()->symbol_rate/1000, f, s, m, getFEParams()->plp_id, getPLSCode(getFEParams()->pls_code).c_str(), getPLSMode(getFEParams()->pls_mode).c_str());
	} else if (CFrontend::isCable(feparams.delsys)) {
		CFrontend::getDelSys(feparams.delsys, getFEParams()->fec_inner, getFEParams()->modulation, f, s, m);
		snprintf(buf, sizeof(buf), "%d %d %s %s %s ", getFEParams()->frequency/1000, getFEParams()->symbol_rate/1000, f, s, m);
	} else if (CFrontend::isTerr(feparams.delsys)) {
		CFrontend::getDelSys(feparams.delsys, getFEParams()->code_rate_HP, getFEParams()->modulation, f, s, m);
		CFrontend::getDelSys(feparams.delsys, getFEParams()->code_rate_LP, getFEParams()->modulation, f2, s, m);
		if (getFEParams()->plp_id == NO_STREAM_ID_FILTER)
			snprintf(buf, sizeof(buf), "%d %d %s %s %s ", getFEParams()->frequency/1000, CFrontend::getFEBandwidth(getFEParams()->bandwidth)/1000, f, f2, m);
		else
			snprintf(buf, sizeof(buf), "%d %d %s %s %s %u ", getFEParams()->frequency/1000, CFrontend::getFEBandwidth(getFEParams()->bandwidth)/1000, f, f2, m, getFEParams()->plp_id);
	}

	return std::string(buf);
}

std::string transponder::getPLSMode(const uint8_t pls_mode)
{
	switch (pls_mode) {
	case 0x00:
		return "Root";
	case 0x01:
		return "Gold";
	case 0x02:
		return "Combo";
	case 0x03:
		return "Unknown";
	default:
		return "Root";
	}
}

std::string transponder::getPLSCode(const uint32_t pls_code)
{
	switch (pls_code) {
	case 0x00:
		return "Default_Gold_Code";
	case 0x01:
		return "Default_Root_Code";
	default:
		return "Default_Gold_Code";
	}
}
