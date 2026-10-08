/*
 * keysource_real.cpp - the remote control keys a binding may hold, by name
 *
 * Copyright (C) 2026 NI-Team
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
 */

#include "coreapi/base/deps.h"

#include <stdint.h>

#include <linux/input.h>

#include <driver/rcinput.h>

namespace coreapi
{

namespace
{

/* The stored code is a signed int the settings file writes as such, so the
   code that means no key arrives as the second of two negative numbers. */
const long kNone = (int32_t) CRCInput::RC_nokey;

// A key of the table the input layer names, without the held flag.
bool namedPlain(unsigned long base)
{
	if (base == 0 || base > (unsigned long) KEY_MAX)
		return false;
	// Neither lookup prints for a code it lacks, which matters because all() asks
	// about every code there is.
	return *CRCInput::getUnicodeValue((neutrino_msg_t) base) != '\0' ||
	       CRCInput::findSpecialKeyName((unsigned int) base) != NULL;
}

/* Its own translation unit, so that the input layer is pulled in only by a
   binary that installs this. */
class RealKeySource : public KeySource
{
	public:
		bool known(long code) const
		{
			if (code == kNone)
				return true;
			if (code <= 0 || (unsigned long) code > (unsigned long) CRCInput::RC_MaxRC)
				return false;

			/* Any code the input layer can deliver, named or not: the key chooser stores
			   what it is sent. Only the held flag may ride on a key. A release is an
			   event and not a key, and its bit leaves what is left past the table. */
			const unsigned long base = (unsigned long) code & ~(unsigned long) CRCInput::RC_Repeat;
			return base >= 1 && base <= (unsigned long) KEY_MAX;
		}

		std::string name(long code) const
		{
			if (code != kNone && !namedPlain((unsigned long) code & ~(unsigned long) CRCInput::RC_Repeat))
				return std::string();
			if (!known(code))
				return std::string();
			return CRCInput::getKeyName((unsigned int) code);
		}

		std::vector<KeyName> all() const
		{
			std::vector<KeyName> out;
			KeyName none;
			none.code = kNone;
			none.name = name(kNone);
			out.push_back(none);

			for (int held = 0; held < 2; ++held)
			{
				for (long plain = 1; plain <= (long) KEY_MAX; ++plain)
				{
					if (!namedPlain((unsigned long) plain))
						continue;
					KeyName k;
					k.code = held ? (plain | (long) CRCInput::RC_Repeat) : plain;
					k.name = name(k.code);
					out.push_back(k);
				}
			}
			return out;
		}
};

RealKeySource g_real_keys;

} // anonymous namespace

void installRealKeySource()
{
	setKeySource(&g_real_keys);
}

} // namespace coreapi
