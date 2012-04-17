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

#include <stdlib.h>
#include <zapit/transponder.h>
#include <zapit/frontend_c.h>
#include <zapit/debug.h>

transponder::transponder(fe_type_t fType, const transponder_id_t t_id, const struct dvb_frontend_parameters p_feparams, const uint8_t p_polarization)
{
	transponder_id      = t_id;
	transport_stream_id = GET_TRANSPORT_STREAM_ID_FROM_TRANSPONDER_ID(t_id);
	original_network_id = GET_ORIGINAL_NETWORK_ID_FROM_TRANSPONDER_ID(t_id);
	feparams            = p_feparams;
	polarization        = p_polarization;
	updated = 0;
	satellitePosition   = GET_SATELLITEPOSITION_FROM_TRANSPONDER_ID(transponder_id);
	if (satellitePosition & 0xF000)
		satellitePosition = -(satellitePosition & 0xFFF);
	else
		satellitePosition = satellitePosition & 0xFFF;
	type = fType;
}

transponder::transponder()
{
	memset(&feparams, 0, sizeof(struct dvb_frontend_parameters));
	transponder_id	= 0;
	transport_stream_id = 0;
	original_network_id = 0;
	polarization = 0;
	satellitePosition = 0;
	type = FE_QPSK;
}

bool transponder::operator==(const transponder& t) const
{
	return (
			(satellitePosition == t.satellitePosition) &&
			//(transport_stream_id == t.transport_stream_id) &&
			//(original_network_id == t.original_network_id) &&
			((polarization & 1) == (t.polarization & 1)) &&
			(abs((int) feparams.frequency - (int)t.feparams.frequency) <= 3000)
	       );
}

bool transponder::compare(const transponder& t) const
{
	bool ret = false;
	if (type == FE_QAM) {
		ret = (
				(t == (*this)) &&
				(feparams.u.qam.symbol_rate == t.feparams.u.qam.symbol_rate) &&
				(feparams.u.qam.fec_inner == t.feparams.u.qam.fec_inner ||
				 feparams.u.qam.fec_inner == FEC_AUTO || t.feparams.u.qam.fec_inner == FEC_AUTO) &&
				(feparams.u.qam.modulation == t.feparams.u.qam.modulation ||
				 feparams.u.qam.modulation == QAM_AUTO || t.feparams.u.qam.modulation == QAM_AUTO)
		      );
	} else {
		ret = (
				(t == (*this)) &&
				(feparams.u.qpsk.symbol_rate == t.feparams.u.qpsk.symbol_rate) &&
				(feparams.u.qpsk.fec_inner == t.feparams.u.qpsk.fec_inner ||
				 feparams.u.qpsk.fec_inner == FEC_AUTO || t.feparams.u.qpsk.fec_inner == FEC_AUTO)
		      );
	}
	return ret;
}

void transponder::dumpServiceXml(FILE * fd)
{
	if (type == FE_QAM) {
		fprintf(fd, "\t\t<TS id=\"%04x\" on=\"%04x\" frq=\"%u\" inv=\"%hu\" sr=\"%u\" fec=\"%hu\" mod=\"%hu\">\n",
				transport_stream_id, original_network_id,
				feparams.frequency, feparams.inversion,
				feparams.u.qam.symbol_rate, feparams.u.qam.fec_inner,
				feparams.u.qam.modulation);

	} else {
		fprintf(fd, "\t\t<TS id=\"%04x\" on=\"%04x\" frq=\"%u\" inv=\"%hu\" sr=\"%u\" fec=\"%hu\" pol=\"%hu\">\n",
				transport_stream_id, original_network_id,
				feparams.frequency, feparams.inversion,
				feparams.u.qpsk.symbol_rate, feparams.u.qpsk.fec_inner,
				polarization);
	}
}

void transponder::dump(std::string label) 
{
	if (type == FE_QAM)
		printf("%s tp-id %016llx freq %d rate %d fec %d mod %d\n", label.c_str(),
				transponder_id, feparams.frequency, feparams.u.qam.symbol_rate,
				feparams.u.qam.fec_inner, feparams.u.qam.modulation);
	else
		printf("%s tp-id %016llx freq %d rate %d fec %d pol %d\n", label.c_str(),
				transponder_id, feparams.frequency, feparams.u.qpsk.symbol_rate,
				feparams.u.qpsk.fec_inner, polarization);
}

void transponder::ddump(std::string label) 
{
	if (zapit_debug)
		dump(label);
}

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
	char * f, *s, *m;
	switch(type) {
		case FE_QPSK:
			CFrontend::getDelSys(type, feparams.u.qpsk.fec_inner, dvbs_get_modulation(feparams.u.qpsk.fec_inner),  f, s, m);
			snprintf(buf, sizeof(buf), "%d %c %d %s %s %s ", feparams.frequency/1000, pol(polarization), feparams.u.qpsk.symbol_rate/1000, f, s, m);
			break;
		case FE_QAM:
			CFrontend::getDelSys(type, feparams.u.qam.fec_inner, feparams.u.qam.modulation, f, s, m);
			snprintf(buf, sizeof(buf), "%d %d %s %s %s ", feparams.frequency/1000, feparams.u.qam.symbol_rate/1000, f, s, m);
			break;
		case FE_OFDM:
		case FE_ATSC:
			break;
	}
	return std::string(buf);
}
