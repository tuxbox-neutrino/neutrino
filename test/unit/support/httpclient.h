/*
 * httpclient.h - a small HTTP client the server tests answer
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

#ifndef __test_httpclient_h__
#define __test_httpclient_h__

#include <string>
#include <utility>
#include <vector>

namespace testhttp
{

struct Reply
{
	int         code;
	std::string status_line;
	std::vector<std::pair<std::string, std::string> > headers;
	std::string body;
	bool        transport_ok;

	Reply() : code(0), transport_ok(false) {}
	std::string header(const std::string &name) const;   // case-insensitive, "" when absent
};

// Sends one request to 127.0.0.1 on port and reads the whole reply. Sets transport_ok
// false and leaves code 0 when the socket work fails, so a case distinguishes a server
// that refused from a server that was not there.
//
// A reply that stopped partway is socket work that failed and comes back the same way:
// a status line that never terminated, a head that never terminated, a body shorter
// than the length it declared, and a code outside the range the protocol defines.
//
// The body is not counted for the replies that state the length a body would have had
// and then send none: an answer to HEAD, a 204 and a 304.
Reply request(int port, const std::string &method, const std::string &path,
              const std::vector<std::pair<std::string, std::string> > &headers
                  = std::vector<std::pair<std::string, std::string> >(),
              const std::string &body = std::string());

// The same, to an address the case names rather than the loopback. Takes an
// IPv4 or an IPv6 literal, so a server bound to something else can be reached.
Reply requestOn(const std::string &host, int port,
                const std::string &method, const std::string &path,
                const std::vector<std::pair<std::string, std::string> > &headers
                    = std::vector<std::pair<std::string, std::string> >(),
                const std::string &body = std::string());

} // namespace testhttp

#endif
