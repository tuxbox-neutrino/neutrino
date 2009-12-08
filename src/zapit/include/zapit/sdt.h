/*
 * $Id: sdt.h,v 1.17 2003/03/14 07:31:50 obi Exp $
 *
 * (C) 2002, 2003 by Andreas Oberritter <obi@tuxbox.org>
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

#ifndef __zapit_sdt_h__
#define __zapit_sdt_h__

#include "types.h"

transponder_id_t get_sdt_TsidOnid(t_satellite_position satellitePosition, freq_id_t freq);
int nvod_service_ids(const t_transport_stream_id, const t_original_network_id, const t_service_id, const unsigned int num, t_transport_stream_id * const, t_original_network_id * const, t_service_id * const);
int parse_sdt(t_transport_stream_id* , t_original_network_id*, t_satellite_position satellitePosition, freq_id_t freq);
int parse_current_sdt( const t_transport_stream_id p_transport_stream_id, const t_original_network_id p_original_network_id,
        t_satellite_position satellitePosition, freq_id_t freq);

#endif /* __zapit_sdt_h__ */
