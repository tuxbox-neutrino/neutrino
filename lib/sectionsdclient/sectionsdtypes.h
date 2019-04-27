/*
 * $Id: sectionsdtypes.h,v 1.1 2004/02/13 14:36:19 thegoodguy Exp $
 *
 * sectionsd's types which are used by the clientlib - d-box2 linux project
 *
 * (C) 2004 by thegoodguy <thegoodguy@berlios.de>
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

#ifndef __sectionsdtypes_h__
#define __sectionsdtypes_h__

#include <zapit/client/zapittypes.h>  /* t_channel_id, t_service_id, t_original_network_id, t_transport_stream_id; */

typedef uint64_t t_event_id;
#define CREATE_EVENT_ID(channel_id,event_nr) ((((t_event_id)channel_id) << 16) | event_nr)
#define GET_CHANNEL_ID_FROM_EVENT_ID(event_id) ((t_channel_id)((event_id) >> 16))

#endif /* __sectionsdtypes_h__ */
