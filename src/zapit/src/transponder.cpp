/*
 * Copyright (C) 2012 CoolStream International Ltd
 * Copyright (C) 2012 Stefan Seyfried
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

transponder::transponder(fe_type_t fType, const transponder_id_t t_id, const FrontendParameters p_feparams, const uint8_t p_polarization)
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
	deltype = fType;
}

transponder::transponder()
{
	memset(&feparams, 0, sizeof(FrontendParameters));
	transponder_id	= 0;
	transport_stream_id = 0;
	original_network_id = 0;
	polarization = 0;
	satellitePosition = 0;
	deltype = FE_QPSK;
}

bool transponder::operator==(const transponder& t) const
{
	if (deltype != FE_OFDM)
		return (
			(satellitePosition == t.satellitePosition) &&
			//(transport_stream_id == t.transport_stream_id) &&
			//(original_network_id == t.original_network_id) &&
			((polarization & 1) == (t.polarization & 1)) &&
			(abs((int) feparams.dvb_feparams.frequency - (int)t.feparams.dvb_feparams.frequency) <= 3000)
	       );
	return ((satellitePosition == t.satellitePosition) &&
		//(transport_stream_id == t.transport_stream_id) &&
		//(original_network_id == t.original_network_id) &&
		((polarization & 1) == (t.polarization & 1)) &&
		(abs((int) feparams.dvb_feparams.frequency - (int)t.feparams.dvb_feparams.frequency) <= 100)
	       );
}

bool transponder::compare(const transponder& t) const
{
	bool ret = false;
	const struct dvb_frontend_parameters *dvb_feparams1 = &feparams.dvb_feparams;
	const struct dvb_frontend_parameters *dvb_feparams2 = &t.feparams.dvb_feparams;

	if (deltype == FE_QAM) {
		ret = (
				(t == (*this)) &&
				(dvb_feparams1->u.qam.symbol_rate == dvb_feparams2->u.qam.symbol_rate) &&
				(dvb_feparams1->u.qam.fec_inner == dvb_feparams2->u.qam.fec_inner ||
				 dvb_feparams1->u.qam.fec_inner == FEC_AUTO || dvb_feparams2->u.qam.fec_inner == FEC_AUTO) &&
				(dvb_feparams1->u.qam.modulation == dvb_feparams2->u.qam.modulation ||
				 dvb_feparams1->u.qam.modulation == QAM_AUTO || dvb_feparams2->u.qam.modulation == QAM_AUTO)
		      );
	} else if (deltype == FE_QPSK) {
		ret = (
				(t == (*this)) &&
				(dvb_feparams1->u.qpsk.symbol_rate == dvb_feparams2->u.qpsk.symbol_rate) &&
				(dvb_feparams1->u.qpsk.fec_inner == dvb_feparams2->u.qpsk.fec_inner ||
				 dvb_feparams1->u.qpsk.fec_inner == FEC_AUTO || dvb_feparams2->u.qpsk.fec_inner == FEC_AUTO)
		      );
	} else if (deltype == FE_OFDM) {
		ret = (	(t == (*this)) &&
			(dvb_feparams1->u.ofdm.bandwidth == dvb_feparams2->u.ofdm.bandwidth) &&
			(dvb_feparams1->u.ofdm.code_rate_HP == dvb_feparams2->u.ofdm.code_rate_HP ||
			 dvb_feparams1->u.ofdm.code_rate_HP == FEC_AUTO || dvb_feparams2->u.ofdm.code_rate_HP == FEC_AUTO) &&
			(dvb_feparams1->u.ofdm.code_rate_LP == dvb_feparams2->u.ofdm.code_rate_LP ||
			 dvb_feparams1->u.ofdm.code_rate_LP == FEC_AUTO || dvb_feparams2->u.ofdm.code_rate_LP == FEC_AUTO) &&
			(dvb_feparams1->u.ofdm.constellation == dvb_feparams2->u.ofdm.constellation ||
			 dvb_feparams1->u.ofdm.constellation == QAM_AUTO || dvb_feparams2->u.ofdm.constellation == QAM_AUTO)
		      );
	}
	return ret;
}

void transponder::dumpServiceXml(FILE * fd)
{
	struct dvb_frontend_parameters *dvb_feparams = &feparams.dvb_feparams;

	if (deltype == FE_QAM) {
		fprintf(fd, "\t\t<TS id=\"%04x\" on=\"%04x\" frq=\"%u\" inv=\"%hu\" sr=\"%u\" fec=\"%hu\" mod=\"%hu\">\n",
				transport_stream_id, original_network_id,
				dvb_feparams->frequency, dvb_feparams->inversion,
				dvb_feparams->u.qam.symbol_rate, dvb_feparams->u.qam.fec_inner,
				dvb_feparams->u.qam.modulation);

	} else if (deltype == FE_QPSK) {
		fprintf(fd, "\t\t<TS id=\"%04x\" on=\"%04x\" frq=\"%u\" inv=\"%hu\" sr=\"%u\" fec=\"%hu\" pol=\"%hu\">\n",
				transport_stream_id, original_network_id,
				dvb_feparams->frequency, dvb_feparams->inversion,
				dvb_feparams->u.qpsk.symbol_rate, dvb_feparams->u.qpsk.fec_inner,
				polarization);
	} else if (deltype == FE_OFDM) {
		fprintf(fd, "\t\t<TS id=\"%04x\" on=\"%04x\" frq=\"%u\" inv=\"%hu\" bw=\"%u\" hp=\"%hu\" lp=\"%hu\" con=\"%u\" tm=\"%u\" gi=\"%u\" hi=\"%u\">\n",
				transport_stream_id, original_network_id,
				dvb_feparams->frequency, dvb_feparams->inversion,
				dvb_feparams->u.ofdm.bandwidth, dvb_feparams->u.ofdm.code_rate_HP,
				dvb_feparams->u.ofdm.code_rate_LP, dvb_feparams->u.ofdm.constellation,
				dvb_feparams->u.ofdm.transmission_mode, dvb_feparams->u.ofdm.guard_interval,
				dvb_feparams->u.ofdm.hierarchy_information);
	}
}

void transponder::dump(std::string label) 
{
	struct dvb_frontend_parameters *dvb_feparams = &feparams.dvb_feparams;

	if (deltype == FE_QAM)
		printf("%s tp-id %016" PRIx64 " freq %d rate %d fec %d mod %d\n", label.c_str(),
				transponder_id, dvb_feparams->frequency, dvb_feparams->u.qam.symbol_rate,
				dvb_feparams->u.qam.fec_inner, dvb_feparams->u.qam.modulation);
	else if (deltype == FE_QPSK)
		printf("%s tp-id %016" PRIx64 " freq %d rate %d fec %d pol %d\n", label.c_str(),
				transponder_id, dvb_feparams->frequency, dvb_feparams->u.qpsk.symbol_rate,
				dvb_feparams->u.qpsk.fec_inner, polarization);
	else if (deltype == FE_OFDM)
		printf("%s tp-id %016" PRIx64 " freq %d bw %d coderate %d\n", label.c_str(),
				transponder_id, dvb_feparams->frequency, dvb_feparams->u.ofdm.bandwidth,
				dvb_feparams->u.ofdm.code_rate_HP);
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
	char *f, *s, *m, *f2;
	struct dvb_frontend_parameters *dvb_feparams = &feparams.dvb_feparams;
	const char *bw[4] = { "8MHz", "7MHz", "6MHz", "auto" };
	int b;

	switch(deltype) {
		case FE_QPSK:
			CFrontend::getDelSys(deltype, dvb_feparams->u.qpsk.fec_inner, dvbs_get_modulation(dvb_feparams->u.qpsk.fec_inner),  f, s, m);
			snprintf(buf, sizeof(buf), "%d %c %d %s %s %s ", dvb_feparams->frequency/1000, pol(polarization), dvb_feparams->u.qpsk.symbol_rate/1000, f, s, m);
			break;
		case FE_QAM:
			CFrontend::getDelSys(deltype, dvb_feparams->u.qam.fec_inner, dvb_feparams->u.qam.modulation, f, s, m);
			snprintf(buf, sizeof(buf), "%d %d %s %s %s ", dvb_feparams->frequency/1000, dvb_feparams->u.qam.symbol_rate/1000, f, s, m);
			break;
		case FE_OFDM:
			CFrontend::getDelSys(deltype, dvb_feparams->u.ofdm.code_rate_HP, dvb_feparams->u.ofdm.constellation, f, s, m);
			CFrontend::getDelSys(deltype, dvb_feparams->u.ofdm.code_rate_LP, dvb_feparams->u.ofdm.constellation, f2, s, m);
			b = dvb_feparams->u.ofdm.bandwidth;
			if (b > 3)
				b = 3;
			snprintf(buf, sizeof(buf), "%d %s %s %s %s ", dvb_feparams->frequency, bw[b], f, f2, m);
			break;
		case FE_ATSC:
			snprintf(buf, sizeof(buf), "ATSC not yet supported ");
			break;
	}
	return std::string(buf);
}
