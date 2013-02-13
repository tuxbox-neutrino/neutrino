/*
 * $Header: /cvs/tuxbox/apps/dvb/zapit/lib/zapittools.cpp,v 1.3 2004/04/04 13:22:13 thegoodguy Exp $
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

#include <zapit/client/zapittools.h>
#include <string.h>

namespace ZapitTools {

	std::string UTF8_to_Latin1(const char * s)
	{
		std::string r;

		while ((*s) != 0)
		{
			if (((*s) & 0xf0) == 0xf0)      /* skip (can't be encoded in Latin1) */
			{
				s++;
				if ((*s) == 0)
					return r;
				s++;
				if ((*s) == 0)
					return r;
				s++;
				if ((*s) == 0)
					return r;
			}
			else if (((*s) & 0xe0) == 0xe0) /* skip (can't be encoded in Latin1) */
			{
				s++;
				if ((*s) == 0)
					return r;
				s++;
				if ((*s) == 0)
					return r;
			}
			else if (((*s) & 0xc0) == 0xc0)
			{
				char c = (((*s) & 3) << 6);
				s++;
				if ((*s) == 0)
					return r;
				r += (c | ((*s) & 0x3f));
			}
			else r += *s;
			s++;
		}
		return r;
	}

	std::string UTF8_to_UTF8XML(const char * s)
	{
		std::string r;

		while ((*s) != 0)
		{
			/* cf. http://www.w3.org/TR/xhtml1/dtds.html */
			switch (*s)
			{
			case '<':
				r += "&lt;";
				break;
			case '>':
				r += "&gt;";
				break;
			case '&':
				r += "&amp;";
				break;
			case '\"':
				r += "&quot;";
				break;
			case '\'':
				r += "&apos;";
				break;
			default:
				r += *s;
			}
			s++;
		}
		return r;
	}

	std::string Latin1_to_UTF8(const char * s)
	{
		std::string r;

		while((*s) != 0)
		{
			unsigned char c = *s;

			if (c < 0x80)
				r += c;
			else
			{
				unsigned char d = 0xc0 | (c >> 6);
				r += d;
				d = 0x80 | (c & 0x3f);
				r += d;
			}

			s++;
		}
		return r;
	}
	std::string Latin1_to_UTF8(const std::string & s)
	{
		return Latin1_to_UTF8(s.c_str());
	}
	
	void replace_char(char * p_act)
	{
		const char repchars[]="/ \"%&-\t`'~<>!,:;?^$\\=*#@|";
		do {
			p_act +=  strcspn(p_act, repchars );
			if (*p_act) {
				*p_act++ = '_';
			}
		} while (*p_act);
	}

}
