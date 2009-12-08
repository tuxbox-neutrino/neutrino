/*
 * $Header: /cvs/tuxbox/apps/misc/libs/libconnection/basicclient.h,v 1.7 2004/04/08 07:19:00 thegoodguy Exp $
 *
 * Basic Client Class - The Tuxbox Project
 *
 * (C) 2002-2003 by thegoodguy <thegoodguy@berlios.de>
 *
 * License: GPL
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

#ifndef __basicclient__
#define __basicclient__

#include <malloc.h>
#include <sys/types.h>

class CBasicClient
{
 private:
	int sock_fd;

 protected:
	virtual const unsigned char   getVersion   () const = 0;
	virtual const          char * getSocketName() const = 0;

	bool open_connection();
	bool send_data(const char * data, const size_t size);
	bool send_string(const char * data);
	bool receive_data(char* data, const size_t size, bool use_max_timeout = false);
	bool send(const unsigned char command, const char* data = NULL, const unsigned int size = 0);
	void close_connection();
	
	CBasicClient();
};

#endif
