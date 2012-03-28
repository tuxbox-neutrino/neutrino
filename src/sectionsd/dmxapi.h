/*
 * $Header: /cvs/tuxbox/apps/tuxbox/neutrino/daemons/sectionsd/dmxapi.h,v 1.3 2003/03/01 19:26:51 thegoodguy Exp $
 *
 * DMX low level functions (sectionsd) - d-box2 linux project
 *
 * (C) 2003 by thegoodguy <thegoodguy@berlios.de>
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

#ifndef __sectionsd__dmxapi_h__
#define __sectionsd__dmxapi_h__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdint.h>
#if 0 // !HAVE_TRIPLEDRAGON
#include <linux/dvb/dmx.h>
#endif

bool setfilter(const int fd, const uint16_t pid, const uint8_t filter, const uint8_t mask, const uint32_t flags);

typedef struct UTC_time
{
	uint64_t time : 40;
} __attribute__ ((packed)) UTC_t;

bool getUTC(UTC_t * const UTC, const bool TDT = true);

#endif /* __sectionsd__dmxapi_h__ */

