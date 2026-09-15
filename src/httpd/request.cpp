/*
 * request.cpp - one request while it is being answered
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

#include "endpoint.h"

#include <cstring>

namespace httpd
{

namespace
{

/* What every accessor answers for a name the endpoint does not declare. One object
   rather than one per call, because asString hands back a reference and a temporary
   would leave the caller holding a dangling one. Built on first use, so this file
   needs nothing to have run before main. */
const std::string &nothing()
{
	static const std::string empty;
	return empty;
}

} // namespace

Request::Request() : granted_(AuthLevel::Public)
{
}

const Request::Bound *Request::find(const char *name) const
{
	if (name == NULL)
		return NULL;
	for (size_t i = 0; i < bound_.size(); ++i)
	{
		if (std::strcmp(bound_[i].name.c_str(), name) == 0)
			return &bound_[i];
	}
	return NULL;
}

bool Request::has(const char *name) const
{
	return find(name) != NULL;
}

/* Each of these answers the member the declared type filled in, and the empty value
   for a name nobody declared. They do not convert between the members: asInt on a
   parameter declared as a channel identifier answers zero rather than the identifier,
   because the row that declared it is what says which member carries the value. */
long Request::asInt(const char *name) const
{
	const Bound *b = find(name);
	if (b == NULL || b->value.type != ParamType::Int)
		return 0;
	return (long) b->value.number;
}

unsigned long Request::asUInt(const char *name) const
{
	const Bound *b = find(name);
	if (b == NULL || b->value.type != ParamType::UInt)
		return 0;
	return (unsigned long) b->value.unumber;
}

bool Request::asBool(const char *name) const
{
	const Bound *b = find(name);
	if (b == NULL || b->value.type != ParamType::Bool)
		return false;
	return b->value.flag;
}

uint64_t Request::asChannelId(const char *name) const
{
	const Bound *b = find(name);
	if (b == NULL || b->value.type != ParamType::ChannelId)
		return 0;
	return (uint64_t) b->value.unumber;
}

time_t Request::asTime(const char *name) const
{
	const Bound *b = find(name);
	if (b == NULL || b->value.type != ParamType::Time)
		return (time_t) 0;
	return (time_t) b->value.number;
}

// The text of any type, so a handler that wants the bytes a number arrived as
// can have them, and the one type whose value is only ever text.
const std::string &Request::asString(const char *name) const
{
	const Bound *b = find(name);
	if (b == NULL)
		return nothing();
	return b->value.text;
}

const std::string &Request::body() const
{
	return body_;
}

const std::string &Request::peer() const
{
	return peer_;
}

const std::string &Request::reportedPeer() const
{
	// The peer when nothing forwarded anything, so a handler reading this one
	// on a box with no proxy in front of it reads the address off the socket
	// rather than an empty string.
	return reported_peer_.empty() ? peer_ : reported_peer_;
}

AuthLevel Request::granted() const
{
	return granted_;
}

const std::string &Request::scope() const
{
	return scope_;
}

const std::string &Request::session() const
{
	return session_;
}

const std::string &Request::host() const
{
	return host_;
}

void Request::bind(const char *name, const Value &v)
{
	if (name == NULL || name[0] == '\0')
		return;
	Bound b;
	b.name = name;
	b.value = v;
	bound_.push_back(b);
}

void Request::setBody(const std::string &b)
{
	body_ = b;
}

void Request::setPeer(const std::string &p)
{
	peer_ = p;
}

void Request::setReportedPeer(const std::string &p)
{
	reported_peer_ = p;
}

void Request::setScope(const std::string &s)
{
	scope_ = s;
}

void Request::setGranted(AuthLevel a)
{
	granted_ = a;
}

void Request::setSession(const std::string &t)
{
	session_ = t;
}

void Request::setHost(const std::string &h)
{
	host_ = h;
}

} // namespace httpd
