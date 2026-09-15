/*
 * http.cpp - HTTP parsing, and the pieces of a request
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

#include "http.h"

namespace httpd
{

/* Here and not beside the daemon, because everything that routes a request
   needs these two and nothing about them needs a server to be running. Where
   they used to live, anything that linked the router pulled the daemon in with
   it, and with it the library the daemon is built on. */

Method methodFromString(const std::string &s)
{
	if (s == "GET")
		return Get;
	if (s == "POST")
		return Post;
	if (s == "PUT")
		return Put;
	if (s == "PATCH")
		return Patch;
	if (s == "DELETE")
		return Delete;
	if (s == "HEAD")
		return Head;
	if (s == "OPTIONS")
		return Options;
	return UnknownMethod;
}

const char *methodName(Method m)
{
	switch (m)
	{
		case Get:     return "GET";
		case Post:    return "POST";
		case Put:     return "PUT";
		case Patch:   return "PATCH";
		case Delete:  return "DELETE";
		case Head:    return "HEAD";
		case Options: return "OPTIONS";
		default:      break;
	}
	return "";
}

} // namespace httpd
