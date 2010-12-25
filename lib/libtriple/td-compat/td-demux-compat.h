/*
 * compatibility stuff for Tripledragon demux API
 *
 * (C) 2009 Stefan Seyfried
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
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

#ifndef __td_demux_compat_h__
#define __td_demux_compat_h__

#include <sys/types.h>
#include <xp/xp_osd_user.h>
// types
#define dmx_output_t		OutDevice
#define dmx_pes_type_t		PesType
#define dmx_sct_filter_params	demux_filter_para
#define dmx_pes_filter_params	demux_pes_para
#define pes_type		pesType
// defines
#define DMX_FILTER_SIZE		FILTER_LENGTH
#define DMX_ONESHOT		XPDF_ONESHOT
#define DMX_CHECK_CRC		0			// TD checks CRC by default
#define DMX_IMMEDIATE_START	XPDF_IMMEDIATE_START
#define DMX_OUT_DECODER		OUT_DECODER
// ioctls
#define DMX_SET_FILTER		DEMUX_FILTER_SET
#define DMX_SET_PES_FILTER	DEMUX_FILTER_PES_SET
#define DMX_START		DEMUX_START
#define DMX_STOP		DEMUX_STOP
#define DMX_SET_BUFFER_SIZE	DEMUX_SET_BUFFER_SIZE

#endif /* __td_demux_compat_h__ */
