/*
 * static_stub.cpp - the one thing the document asks the static file server
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * The source's static.cpp serves its pages; nhttpd serves ours. What the
 * OpenAPI route still needs from it is the reading of Accept-Encoding,
 * so the document can go out gzipped where the caller takes it. The
 * function and its helpers are taken as written.
 */

#include "httpd/static.h"

#include <string>

namespace httpd
{

namespace
{

bool equalsNoCase(const std::string &a, const char *b)
{
	size_t i = 0;
	for (; i < a.size() && b[i] != '\0'; ++i)
	{
		char x = a[i];
		char y = b[i];
		if (x >= 'A' && x <= 'Z')
			x = (char)(x - 'A' + 'a');
		if (y >= 'A' && y <= 'Z')
			y = (char)(y - 'A' + 'a');
		if (x != y)
			return false;
	}
	return i == a.size() && b[i] == '\0';
}

bool isSpace(char c)
{
	return c == ' ' || c == '\t';
}

std::string trimmed(const std::string &s)
{
	size_t from = 0;
	size_t to = s.size();
	while (from < to && isSpace(s[from]))
		++from;
	while (to > from && isSpace(s[to - 1]))
		--to;
	return s.substr(from, to - from);
}

/* One element of a comma- or semicolon-separated header, from at onward;
   at is moved past it. The separator is a parameter because the two levels
   of one header are separated differently. */
bool nextElement(const std::string &list, char separator, size_t &at, std::string &out)
{
	if (at > list.size())
		return false;

	const size_t cut = list.find(separator, at);
	if (cut == std::string::npos)
	{
		out = list.substr(at);
		at = list.size() + 1;
	}
	else
	{
		out = list.substr(at, cut - at);
		at = cut + 1;
	}
	return true;
}

/* A q-weight that is above zero. The grammar allows three decimals and
   nothing else, and anything outside it answers no: guessing the other
   way sends a body the caller cannot unpack. */
bool weightAboveZero(const std::string &value)
{
	if (value.empty())
		return false;

	size_t i = 0;
	bool above = false;
	if (value[i] == '1')
		above = true;
	else if (value[i] != '0')
		return false;
	++i;

	if (i == value.size())
		return above;

	if (value[i] != '.')
		return false;
	++i;

	size_t decimals = 0;
	for (; i < value.size(); ++i, ++decimals)
	{
		if (decimals == 3)
			return false;
		if (value[i] < '0' || value[i] > '9')
			return false;
		if (value[i] != '0')
			above = true;
	}
	return above;
}

// Whether the weight written beside a coding allows it, and a coding with no
// weight beside it carries a full one.
bool weightAllows(const std::string &params)
{
	size_t at = 0;
	std::string one;
	while (nextElement(params, ';', at, one))
	{
		const std::string p = trimmed(one);
		const size_t eq = p.find('=');
		if (eq == std::string::npos)
			continue;
		if (!equalsNoCase(trimmed(p.substr(0, eq)), "q"))
			continue;
		return weightAboveZero(trimmed(p.substr(eq + 1)));
	}
	return true;
}

} // namespace

bool acceptsGzip(const std::string &accept_encoding)
{
	bool named = false;
	bool named_ok = false;
	bool named_refused = false;
	bool star_ok = false;

	size_t at = 0;
	std::string element;
	while (nextElement(accept_encoding, ',', at, element))
	{
		const size_t semi = element.find(';');
		const std::string token =
			trimmed((semi == std::string::npos) ? element : element.substr(0, semi));
		if (token.empty())
			continue;

		const bool is_gzip = equalsNoCase(token, "gzip");
		const bool is_star = (token == "*");
		if (!is_gzip && !is_star)
			continue;

		const bool takes =
			(semi == std::string::npos) ? true : weightAllows(element.substr(semi + 1));

		if (is_gzip)
		{
			named = true;
			if (takes)
				named_ok = true;
			else
				named_refused = true;
		}
		else if (takes)
			star_ok = true;
	}

	if (named)
		return named_ok && !named_refused;
	return star_ok;
}

} // namespace httpd
