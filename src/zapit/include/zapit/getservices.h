/*
 * $Id: getservices.h,v 1.51.2.7 2003/06/10 12:37:19 digi_casi Exp $
 *
 * (C) 2002 by Andreas Oberritter <obi@tuxbox.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#ifndef __getservices_h__
#define __getservices_h__

#include <linux/dvb/frontend.h>

#include <eventserver.h>

#include "ci.h"
#include "descriptors.h"
#include "sdt.h"
#include "types.h"
#include "xmlinterface.h"

#include <map>
#define zapped_chan_is_nvod 0x80

//#define NONE 0x0000
//#define INVALID 0x1FFF

void ParseTransponders(xmlNodePtr xmltransponder, t_satellite_position satellitePosition);
void ParseChannels    (xmlNodePtr node, const t_transport_stream_id transport_stream_id, const t_original_network_id original_network_id, t_satellite_position satellitePosition, freq_id_t freq);
void FindTransponder  (xmlNodePtr root);
int LoadServices      (fe_type_t FrontendType, diseqc_t, bool only_current);
void SaveServices(bool tocopy);
void SaveMotorPositions();

struct transponder
{
	t_transport_stream_id transport_stream_id;
	t_original_network_id original_network_id;
	struct dvb_frontend_parameters feparams;
	unsigned char polarization;
	bool updated;

	transponder (t_transport_stream_id p_transport_stream_id, struct dvb_frontend_parameters p_feparams)
	{
		transport_stream_id = p_transport_stream_id;
		feparams = p_feparams;
		polarization = 0;
		original_network_id = 0;
		updated = 0;
	}

	transponder(const t_transport_stream_id p_transport_stream_id, const t_original_network_id p_original_network_id, const struct dvb_frontend_parameters p_feparams, const uint8_t p_polarization = 0)
        {
                transport_stream_id = p_transport_stream_id;
                original_network_id = p_original_network_id;
                feparams            = p_feparams;
                polarization        = p_polarization;
		updated = 0;
        }

	transponder (t_transport_stream_id p_transport_stream_id, struct dvb_frontend_parameters p_feparams, unsigned short p_polarization, t_original_network_id p_original_network_id)
	{
		transport_stream_id = p_transport_stream_id;
		feparams = p_feparams;
		polarization = p_polarization;
		original_network_id = p_original_network_id;
		updated = 0;
	}
};
typedef std::map<transponder_id_t, transponder> transponder_list_t;
typedef std::map <transponder_id_t, transponder>::iterator stiterator;
typedef std::map<transponder_id_t, bool> sdt_tp_t;
extern transponder_list_t scantransponders;
extern transponder_list_t transponders;

#endif /* __getservices_h__ */
