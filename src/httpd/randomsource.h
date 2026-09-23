/*
 * randomsource.h - where the server takes random bytes from
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

#ifndef __httpd_randomsource_h__
#define __httpd_randomsource_h__

#include <cstddef>

namespace httpd
{

/* Bytes from the kernel's pool, all of them or none.

   A read from the device is allowed to answer with fewer bytes than were asked for,
   and taking that as the answer would silently shorten a salt or a token to whatever
   happened to be ready. So a partial read is continued and a source that runs out is
   a failure. On every failure out is wiped, so a caller that ignores the answer holds
   zeroes rather than half a secret.

   Not part of what the credential store offers to the rest of the program: a path
   arriving from anywhere but the line below would be a salt drawn from a file
   somebody else nominated. It is a header at all so a case can drive a source that
   runs out, which the device cannot be made to do on demand. */
bool randomBytesFrom(const char *path, unsigned char *out, size_t count);

// The one source anything in this program draws from.
bool randomBytes(unsigned char *out, size_t count);

} // namespace httpd

#endif
