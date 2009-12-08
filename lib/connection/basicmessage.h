/*
 * $Header: /cvs/tuxbox/apps/misc/libs/libconnection/basicmessage.h,v 1.2 2002/10/18 11:56:56 thegoodguy Exp $
 *
 * types used for clientlib <-> daemon communication - d-box2 linux project
 *
 * (C) 2002 by thegoodguy <thegoodguy@berlios.de>
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

#ifndef __basicmessage_h__
#define __basicmessage_h__


class CBasicMessage
{
 public:
	
	typedef unsigned char t_version;
	typedef unsigned char t_cmd;

	struct Header
	{
		t_version version;
		t_cmd     cmd;
	};
};


#endif /* __basicmessage_h__ */
