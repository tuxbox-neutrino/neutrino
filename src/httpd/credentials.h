/*
 * credentials.h - the user and password the server checks against
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

#ifndef __httpd_credentials_h__
#define __httpd_credentials_h__

#include <cstddef>
#include <string>

namespace httpd
{

/* A password or a token written down in one field that carries everything needed to check a
   guess against it again:

       pbkdf2-sha256$<iterations>$<base64 salt>$<base64 hash>

   The salt is drawn fresh for every call, so two accounts that share a password do not
   share a stored form. On a box with a single account it is also what stops the stored form
   from being a constant that can be looked up.

   Answers an empty string when it cannot produce one, and an empty string is a form nothing
   verifies against. */
std::string hashSecret(const std::string &plain, unsigned iterations = 10000);

/* Whether plain is the secret that stored was made from.

   The iteration count is read back out of stored rather than taken from the default above.
   The default is meant to rise as the boxes get faster, and a check that used the current
   one would stop recognising every credential already written down the moment it did, which
   locks out every account on the box at once.

   Everything else about the stored form is checked before a key is derived, and anything
   that is not the shape this file writes is refused: another scheme name, a count that is
   not a plain positive number or is larger than the box could answer within a request, a
   field that is not base64, a salt or a hash of an unusable length, and a field count that
   is not four.

   The hash length is pinned where the salt length is not, because the scheme name states
   the digest and a stored hash of another width was not written against that name.

   The comparison is over the whole derived key however wrong the guess is. */
bool verifySecret(const std::string &plain, const std::string &stored);

/* A bearer token as lower case hex, twice as many characters as bytes asked for.
   Empty when the bytes could not be drawn or when the count asked for is not one a
   token has: there is no token, rather than a shorter one. Hex and not base64 because
   a token travels in a header and in a URL and this is the one encoding that needs no
   escaping in either. */
std::string randomToken(size_t bytes = 32);

/* The first characters of a token, as the key a stored record is found under without
   the token itself being stored anywhere. Empty for anything that is not a token this
   could have issued: a lookup key built out of an unchecked string is a key an
   attacker chooses, and answering nothing for one leaves the caller with no record
   found. */
std::string tokenLookupPrefix(const std::string &token);

} // namespace httpd

#endif
