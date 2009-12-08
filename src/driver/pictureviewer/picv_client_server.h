/*

  structs/defines for client server communication
  between neutrino picviewer and jpeg decode server
  
  (c) Zwen@tuxbox.org   

  License: GPL

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef picv_client_server__
#define picv_client_server__

#define PICV_CLIENT_SERVER_PATHLEN 1000

struct pic_data
{
	uint32_t width;
	uint32_t height;
	uint32_t bpp; /* unused atm */
};

#endif
