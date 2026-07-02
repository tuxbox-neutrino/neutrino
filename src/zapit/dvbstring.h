/*
 * $Header: /cvsroot/tuxbox/apps/dvb/zapit/include/zapit/dvbstring.h,v 1.5 2002/10/29 22:46:50 thegoodguy Exp $
 *
 * Strings conforming to the DVB Standard - d-box2 linux project
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

#ifndef __dvbstring_h__
#define __dvbstring_h__

#include <string>

class CDVBString
{
 private:
	enum t_encoding {
		UNKNOWN        = 0x00,
		ISO_8859_5     = 0x01,
		ISO_8859_6     = 0x02,
		ISO_8859_7     = 0x03,
		ISO_8859_8     = 0x04,
		ISO_8859_9     = 0x05,
		ISO_6937       = 0x20
	};

	t_encoding encoding;

	std::string content;

	unsigned int add_character(const unsigned char character, const unsigned char next_character);
	
 public:
	CDVBString(const char * the_content, const int size);

	bool operator==(const CDVBString s);

	bool operator!=(const CDVBString s);

	std::string getContent();
};

#endif /* __dvbstring_h__ */
