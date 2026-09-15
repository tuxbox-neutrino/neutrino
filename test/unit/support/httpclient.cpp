/*
 * httpclient.cpp - a small HTTP client the server tests answer
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

#include "httpclient.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <stdint.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace testhttp
{

namespace
{

// A reply that never arrives has to end the case rather than the run, so every
// socket carries a deadline of its own.
const int kTimeoutSeconds = 5;

// The range the protocol defines. A first line that yields anything else was
// not a status line, whatever it parsed as.
const int kLowestCode  = 100;
const int kHighestCode = 599;

std::string lowered(const std::string &s)
{
	std::string out(s);
	for (size_t i = 0; i < out.size(); ++i)
	{
		if (out[i] >= 'A' && out[i] <= 'Z')
			out[i] = (char) (out[i] - 'A' + 'a');
	}
	return out;
}

std::string trimmed(const std::string &s)
{
	size_t b = 0;
	size_t e = s.size();
	while (b < e && (s[b] == ' ' || s[b] == '\t'))
		++b;
	while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r'))
		--e;
	return s.substr(b, e - b);
}

bool namedIn(const std::vector<std::pair<std::string, std::string> > &headers, const char *name)
{
	const std::string want = lowered(name);
	for (size_t i = 0; i < headers.size(); ++i)
	{
		if (lowered(headers[i].first) == want)
			return true;
	}
	return false;
}

void setDeadline(int fd)
{
	struct timeval tv;
	std::memset(&tv, 0, sizeof(tv));
	tv.tv_sec = kTimeoutSeconds;
	::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

// Connects to an address literal of either family. Returns -1 for anything the
// caller should read as the server not being there.
int connectTo(const std::string &host, int port)
{
	struct in_addr  v4;
	struct in6_addr v6;
	std::memset(&v4, 0, sizeof(v4));
	std::memset(&v6, 0, sizeof(v6));

	if (::inet_pton(AF_INET, host.c_str(), &v4) == 1)
	{
		struct sockaddr_in a;
		std::memset(&a, 0, sizeof(a));
		a.sin_family = AF_INET;
		a.sin_port = htons((uint16_t) port);
		a.sin_addr = v4;

		const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
		if (fd < 0)
			return -1;
		setDeadline(fd);
		if (::connect(fd, (const struct sockaddr *) &a, sizeof(a)) != 0)
		{
			::close(fd);
			return -1;
		}
		return fd;
	}

	if (::inet_pton(AF_INET6, host.c_str(), &v6) == 1)
	{
		struct sockaddr_in6 a;
		std::memset(&a, 0, sizeof(a));
		a.sin6_family = AF_INET6;
		a.sin6_port = htons((uint16_t) port);
		a.sin6_addr = v6;

		const int fd = ::socket(AF_INET6, SOCK_STREAM, 0);
		if (fd < 0)
			return -1;
		setDeadline(fd);
		if (::connect(fd, (const struct sockaddr *) &a, sizeof(a)) != 0)
		{
			::close(fd);
			return -1;
		}
		return fd;
	}

	return -1;
}

// An IPv6 literal carries colons of its own, so the port has to be told apart
// from the address by brackets.
std::string hostHeader(const std::string &host, int port)
{
	char tail[16];
	std::snprintf(tail, sizeof(tail), ":%d", port);
	if (host.find(':') != std::string::npos)
		return "[" + host + "]" + tail;
	return host + tail;
}

/* A server that refuses a request before the whole of it has been sent closes while
   this is still writing, and a write to a closed connection raises a signal whose
   default action ends the process: the suite would go down in the middle of the case
   that provoked it rather than failing it. The signal is asked for as an error code
   instead.

   Defensive rather than demonstrated: no case in this suite has been seen to need it,
   because the one that provokes a refusal mid write reads the answer as it goes. */
bool sendAll(int fd, const std::string &data)
{
	size_t sent = 0;
	while (sent < data.size())
	{
		const ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
		if (n <= 0)
			return false;
		sent += (size_t) n;
	}
	return true;
}

// Splits the head off once it is whole. Returns the offset the body starts at,
// or npos while the terminator has not been seen.
size_t headEnd(const std::string &buf)
{
	const size_t crlf = buf.find("\r\n\r\n");
	if (crlf != std::string::npos)
		return crlf + 4;
	const size_t lf = buf.find("\n\n");
	if (lf != std::string::npos)
		return lf + 2;
	return std::string::npos;
}

// The declared body length, or npos when the head does not state one. Read off
// the raw head rather than the parsed reply, because the read loop needs it
// before there is a parsed reply to ask.
size_t declaredLength(const std::string &head)
{
	size_t pos = 0;
	while (pos < head.size())
	{
		size_t eol = head.find('\n', pos);
		if (eol == std::string::npos)
			eol = head.size();
		const std::string line = head.substr(pos, eol - pos);
		const size_t colon = line.find(':');
		if (colon != std::string::npos && lowered(trimmed(line.substr(0, colon))) == "content-length")
			return (size_t) std::strtoul(trimmed(line.substr(colon + 1)).c_str(), NULL, 10);
		pos = eol + 1;
	}
	return std::string::npos;
}

std::string firstLine(const std::string &head)
{
	size_t eol = head.find('\n');
	if (eol == std::string::npos)
		eol = head.size();
	return trimmed(head.substr(0, eol));
}

// Zero for a first line that was not a status line, which the range check then
// refuses. Needed before there is a parsed reply, because what the read loop
// waits for depends on the code.
int statusCode(const std::string &head)
{
	const std::string line = firstLine(head);
	const size_t sp = line.find(' ');
	if (line.compare(0, 5, "HTTP/") != 0 || sp == std::string::npos)
		return 0;
	return (int) std::strtol(line.c_str() + sp + 1, NULL, 10);
}

// The replies that state the length a body would have had and then send no
// body. Counting bytes against the declared length would refuse every one of
// them, which is the protocol working rather than a reply that stopped early.
bool carriesNoBody(const std::string &method, int code)
{
	return method == "HEAD" || code == 204 || code == 304;
}

void parse(const std::string &buf, size_t body_at, Reply &r)
{
	const std::string head = buf.substr(0, body_at);
	r.body = buf.substr(body_at);
	r.status_line = firstLine(head);
	r.code = statusCode(head);

	size_t pos = 0;
	bool first = true;
	while (pos < head.size())
	{
		size_t eol = head.find('\n', pos);
		if (eol == std::string::npos)
			eol = head.size();
		const std::string line = trimmed(head.substr(pos, eol - pos));
		pos = eol + 1;

		if (first)
		{
			first = false;
			continue;
		}
		if (line.empty())
			break;
		const size_t colon = line.find(':');
		if (colon != std::string::npos)
			r.headers.push_back(std::make_pair(trimmed(line.substr(0, colon)),
			                                   trimmed(line.substr(colon + 1))));
	}
}

} // namespace

std::string Reply::header(const std::string &name) const
{
	const std::string want = lowered(name);
	for (size_t i = 0; i < headers.size(); ++i)
	{
		if (lowered(headers[i].first) == want)
			return headers[i].second;
	}
	return std::string();
}

Reply requestOn(const std::string &host, int port,
                const std::string &method, const std::string &path,
                const std::vector<std::pair<std::string, std::string> > &headers,
                const std::string &body)
{
	const Reply failed;

	const int fd = connectTo(host, port);
	if (fd < 0)
		return failed;

	std::string req = method + " " + path + " HTTP/1.1\r\n";
	// A caller naming its own "Host" among headers is asking for that literal
	// value on the wire instead of the address this connected to - the only
	// way a case can drive what the server's own Host lookup actually reads,
	// as opposed to what this transport happened to dial. Two Host lines
	// would leave it to the server which one MHD_lookup_connection_value
	// returns, so the automatic one is left out rather than sent alongside.
	if (!namedIn(headers, "Host"))
		req += "Host: " + hostHeader(host, port) + "\r\n";
	for (size_t i = 0; i < headers.size(); ++i)
		req += headers[i].first + ": " + headers[i].second + "\r\n";
	// The read loop below stops on end of file when no length was declared, so
	// the reply has to be the last thing on the connection unless the caller
	// has said otherwise.
	if (!namedIn(headers, "Connection"))
		req += "Connection: close\r\n";
	if (!namedIn(headers, "Content-Length") && !namedIn(headers, "Transfer-Encoding")
	    && (!body.empty() || method == "POST" || method == "PUT" || method == "PATCH"))
	{
		char lenline[48];
		std::snprintf(lenline, sizeof(lenline), "Content-Length: %lu\r\n", (unsigned long) body.size());
		req += lenline;
	}
	req += "\r\n";
	req += body;

	if (!sendAll(fd, req))
	{
		::close(fd);
		return failed;
	}

	std::string buf;
	char chunk[4096];
	bool eof = false;
	bool bodyless = false;
	size_t body_at = std::string::npos;
	size_t declared = std::string::npos;

	for (;;)
	{
		if (body_at == std::string::npos)
		{
			body_at = headEnd(buf);
			if (body_at != std::string::npos)
			{
				const std::string head = buf.substr(0, body_at);
				declared = declaredLength(head);
				bodyless = carriesNoBody(method, statusCode(head));
			}
		}
		// Waiting for a body that is not coming would cost the deadline on
		// every one of these, so a reply that carries none is done at its head.
		if (body_at != std::string::npos
		    && (bodyless || (declared != std::string::npos && buf.size() - body_at >= declared)))
			break;

		const ssize_t n = ::recv(fd, chunk, sizeof(chunk), 0);
		if (n == 0)
		{
			eof = true;
			break;
		}
		if (n < 0)
			break;                     // the deadline expired, or the peer reset
		buf.append(chunk, (size_t) n);
	}
	::close(fd);

	// Everything below decides whether what arrived is a whole reply. A head
	// that never terminated is not one, whatever the reply was going to be. A
	// body is whole when it is as long as it said it would be, or, where no
	// length was stated, when the peer closed rather than went quiet. A reply
	// that carries no body is held to the head alone: the length it states is
	// the length a body would have had and no bytes are owed against it.
	if (body_at == std::string::npos)
		return failed;
	if (!bodyless)
	{
		const bool whole = (declared != std::string::npos)
			? (buf.size() - body_at >= declared)
			: eof;
		if (!whole)
			return failed;
	}

	Reply r;
	parse(buf, body_at, r);
	if (r.code < kLowestCode || r.code > kHighestCode)
		return failed;
	if (!bodyless && declared != std::string::npos)
		r.body.resize(declared);

	r.transport_ok = true;
	return r;
}

Reply request(int port, const std::string &method, const std::string &path,
              const std::vector<std::pair<std::string, std::string> > &headers,
              const std::string &body)
{
	return requestOn("127.0.0.1", port, method, path, headers, body);
}

} // namespace testhttp
