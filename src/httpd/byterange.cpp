/*
 * byterange.cpp - range requests, and the partial answers they get
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

#include "byterange.h"

#include <cstddef>
#include <string>

#include <stdint.h>

namespace httpd
{

namespace
{

const char kUnit[] = "bytes=";

bool sameNoCase(const std::string &s, size_t at, const char *lower, size_t n)
{
	if (s.size() - at < n)
		return false;
	for (size_t i = 0; i < n; ++i)
	{
		char c = s[at + i];
		if (c >= 'A' && c <= 'Z')
			c = (char)(c - 'A' + 'a');
		if (c != lower[i])
			return false;
	}
	return true;
}

bool isSpace(char c)
{
	return c == ' ' || c == '\t';
}

/* One run of digits as a number, and false for a run that is not digits or that
   would not fit. The overflow is checked before the digit is added rather than
   after, because a sum that has already wrapped is a number this cannot tell
   from a small one. */
bool digitsAt(const std::string &s, size_t &at, uint64_t &out)
{
	const size_t from = at;
	uint64_t n = 0;
	while (at < s.size() && s[at] >= '0' && s[at] <= '9')
	{
		const uint64_t d = (uint64_t)(s[at] - '0');
		if (n > (UINT64_MAX - d) / 10)
			return false;
		n = n * 10 + d;
		++at;
	}
	if (at == from)
		return false;
	out = n;
	return true;
}

} // namespace

RangeAnswer byteRangeOf(const std::string &header, uint64_t length,
                        uint64_t &first, uint64_t &last)
{
	size_t at = 0;
	while (at < header.size() && isSpace(header[at]))
		++at;

	if (!sameNoCase(header, at, kUnit, sizeof(kUnit) - 1))
		return RangeAnswer::Whole;
	at += sizeof(kUnit) - 1;

	while (at < header.size() && isSpace(header[at]))
		++at;

	uint64_t from = 0;
	uint64_t to = 0;
	bool have_from = false;

	if (at < header.size() && header[at] == '-')
	{
		/* The last so many bytes, which is the one form that names no first
		   byte. Nought of them names nothing and is the second of the two
		   refusals this makes. */
		++at;
		uint64_t suffix = 0;
		if (!digitsAt(header, at, suffix))
			return RangeAnswer::Whole;
		if (suffix == 0 || length == 0)
			return RangeAnswer::Unsatisfiable;
		from = (suffix >= length) ? 0 : length - suffix;
		to = length - 1;
	}
	else
	{
		if (!digitsAt(header, at, from))
			return RangeAnswer::Whole;
		have_from = true;
		if (at >= header.size() || header[at] != '-')
			return RangeAnswer::Whole;
		++at;

		if (at < header.size() && header[at] >= '0' && header[at] <= '9')
		{
			if (!digitsAt(header, at, to))
				return RangeAnswer::Whole;
			// Read and understood, and the wrong way round, which names no
			// stretch at all rather than one the file does not hold.
			if (to < from)
				return RangeAnswer::Whole;
		}
		else
		{
			// From there to the end, which is what a resumed download asks
			// for. A file of no bytes has no such place and falls to the
			// refusal below.
			to = (length == 0) ? 0 : length - 1;
		}
	}

	while (at < header.size() && isSpace(header[at]))
		++at;
	/* Anything left is a second stretch or something this does not read, and
	   either way what came back is not the whole of what was asked for. Sending
	   the first stretch of several would be answering a different request. */
	if (at != header.size())
		return RangeAnswer::Whole;

	if (have_from && (length == 0 || from >= length))
		return RangeAnswer::Unsatisfiable;
	if (to >= length)
		to = length - 1;

	first = from;
	last = to;
	return RangeAnswer::Partial;
}

} // namespace httpd
