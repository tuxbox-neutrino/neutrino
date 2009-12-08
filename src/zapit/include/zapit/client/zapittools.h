/*
 * $Header: /cvs/tuxbox/apps/dvb/zapit/include/zapit/client/zapittools.h,v 1.1 2004/04/02 13:26:56 thegoodguy Exp $
 *
 * some tools for zapit and its clients - d-box2 linux project
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

#ifndef __zapittools__
#define __zapittools__

#include <string>

namespace ZapitTools
{
	std::string UTF8_to_Latin1 (const char *);
	std::string UTF8_to_UTF8XML(const char *);
	std::string Latin1_to_UTF8 (const char *);
};

#endif
