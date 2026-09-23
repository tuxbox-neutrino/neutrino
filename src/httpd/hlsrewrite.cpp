/*
 * hlsrewrite.cpp - reading an HLS playlist and pointing its addresses back at this box
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

#include "hlsrewrite.h"

#include <cstring>
#include <vector>

namespace httpd
{

namespace hls
{

namespace
{

const char kHead[] = "#EXTM3U";

/* The mark a file written by a text editor on a machine that puts one there
   begins with. It is not part of the first line and a comparison against
   #EXTM3U made over it fails, so it is stepped past before anything is read. */
const char kBom[] = "\xEF\xBB\xBF";

char lower(char c)
{
	return (c >= 'A' && c <= 'Z') ? (char) (c - 'A' + 'a') : c;
}

bool startsWithNoCase(const std::string &s, const char *prefix)
{
	const size_t n = std::strlen(prefix);
	if (s.size() < n)
		return false;
	for (size_t i = 0; i < n; ++i)
	{
		if (lower(s[i]) != lower(prefix[i]))
			return false;
	}
	return true;
}

/* Where the scheme of a reference ends, or npos for one that carries none.

   A scheme is a letter and then letters, digits and three punctuation marks,
   up to a colon, and nothing else is one. Written out rather than looked for
   as the first colon, because a relative name may carry a colon in it and
   reading that as a scheme would make a segment name into a protocol. */
size_t schemeEnd(const std::string &ref)
{
	if (ref.empty())
		return std::string::npos;
	const char first = ref[0];
	if (!((first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z')))
		return std::string::npos;

	for (size_t i = 1; i < ref.size(); ++i)
	{
		const char c = ref[i];
		if (c == ':')
			return i;
		const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		                (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.';
		if (!ok)
			return std::string::npos;
	}
	return std::string::npos;
}

bool isWebScheme(const std::string &url)
{
	return startsWithNoCase(url, "http://") || startsWithNoCase(url, "https://");
}

/* The three pieces of an absolute address this needs, split once: everything
   up to and including the authority, the path, and the query with its question
   mark. A fragment is dropped, because it never travels to a server and a
   stored address carrying one would only ever be compared wrong. */
struct Parts
{
	std::string root;    // "https://host:port"
	std::string path;    // "/a/b/c" and never empty
	std::string query;   // "?x=1" or empty
};

bool splitAbsolute(const std::string &url, Parts &out)
{
	if (!isWebScheme(url))
		return false;

	const size_t after_scheme = url.find("//");
	if (after_scheme == std::string::npos)
		return false;

	const size_t authority = after_scheme + 2;
	size_t end = url.size();
	for (size_t i = authority; i < url.size(); ++i)
	{
		if (url[i] == '/' || url[i] == '?' || url[i] == '#')
		{
			end = i;
			break;
		}
	}
	// An address with no host at all. Nothing below could resolve against it
	// and curl would be handed a name with no server in it.
	if (end == authority)
		return false;

	out.root = url.substr(0, end);

	std::string rest = url.substr(end);
	const size_t hash = rest.find('#');
	if (hash != std::string::npos)
		rest.erase(hash);

	const size_t mark = rest.find('?');
	if (mark == std::string::npos)
	{
		out.path = rest;
		out.query.clear();
	}
	else
	{
		out.path = rest.substr(0, mark);
		out.query = rest.substr(mark);
	}

	if (out.path.empty())
		out.path = "/";
	return true;
}

/* RFC 3986 section 5.2.4, written as a walk over the segments rather than over the
   characters: a segment of ".." takes the one before it away and a segment of "."
   takes itself away.

   A ".." with nothing before it is dropped rather than kept, which is what the
   specification says and is also the half that matters here: kept, it would be an
   address pointing above the root of a server. */
std::string removeDotSegments(const std::string &path)
{
	std::vector<std::string> out;
	const bool trailing_root = !path.empty() && path[path.size() - 1] == '/';

	size_t i = 0;
	while (i < path.size())
	{
		size_t slash = path.find('/', i);
		if (slash == std::string::npos)
			slash = path.size();
		const std::string seg = path.substr(i, slash - i);
		i = slash + 1;

		if (seg.empty() || seg == ".")
			continue;
		if (seg == "..")
		{
			if (!out.empty())
				out.pop_back();
			continue;
		}
		out.push_back(seg);
	}

	std::string joined;
	for (size_t n = 0; n < out.size(); ++n)
	{
		joined += '/';
		joined += out[n];
	}
	if (joined.empty())
		return "/";
	// A reference ending in a separator names a directory, and dropping the
	// separator would name a file beside it instead.
	if (trailing_root && joined[joined.size() - 1] != '/')
		joined += '/';
	return joined;
}

// Which of the four kinds an address on a tag is, by the tag it stands on.
UriKind kindOfTag(const std::string &line)
{
	if (startsWithNoCase(line, "#EXT-X-KEY") || startsWithNoCase(line, "#EXT-X-SESSION-KEY"))
		return UriKind::Key;
	if (startsWithNoCase(line, "#EXT-X-MAP"))
		return UriKind::Map;
	if (startsWithNoCase(line, "#EXT-X-MEDIA") ||
	    startsWithNoCase(line, "#EXT-X-I-FRAME-STREAM-INF") ||
	    startsWithNoCase(line, "#EXT-X-RENDITION-REPORT"))
		return UriKind::Playlist;
	/* Everything else that carries an address carries one to bytes: the
	   partial segment, the hint at the one coming, and whatever a later
	   version of the format adds. Read as bytes it is passed through
	   untouched, which is right for a segment and merely useless for anything
	   else; read as a playlist it would be parsed, and parsing bytes that are
	   not a playlist is the reading that can go wrong. */
	return UriKind::Segment;
}

bool lineHasByteRange(const std::string &line)
{
	if (startsWithNoCase(line, "#EXT-X-BYTERANGE"))
		return true;
	return startsWithNoCase(line, "#EXT-X-MAP") &&
	       line.find("BYTERANGE=") != std::string::npos;
}

} // namespace

Minter::~Minter()
{
}

bool resolveUri(const std::string &base, const std::string &ref, std::string &out)
{
	out.clear();
	if (ref.empty())
		return false;
	// A string may carry a zero byte and everything below stops at one, so a
	// reference holding one would be resolved on the half of itself that suits
	// it and fetched whole.
	if (ref.find('\0') != std::string::npos || base.find('\0') != std::string::npos)
		return false;

	const size_t scheme = schemeEnd(ref);
	if (scheme != std::string::npos)
	{
		if (!isWebScheme(ref))
			return false;
		Parts p;
		if (!splitAbsolute(ref, p))
			return false;
		out = p.root + removeDotSegments(p.path) + p.query;
		return true;
	}

	Parts b;
	if (!splitAbsolute(base, b))
		return false;

	// A reference that is nothing but a fragment names the playlist itself,
	// which is not an address to fetch anything from.
	if (ref[0] == '#')
		return false;

	std::string tail = ref;
	const size_t hash = tail.find('#');
	if (hash != std::string::npos)
		tail.erase(hash);
	if (tail.empty())
		return false;

	if (tail.size() >= 2 && tail[0] == '/' && tail[1] == '/')
	{
		const size_t colon = base.find(':');
		Parts p;
		if (!splitAbsolute(base.substr(0, colon + 1) + tail, p))
			return false;
		out = p.root + removeDotSegments(p.path) + p.query;
		return true;
	}

	std::string path;
	std::string query;
	const size_t mark = tail.find('?');
	if (mark == std::string::npos)
	{
		path = tail;
	}
	else
	{
		path = tail.substr(0, mark);
		query = tail.substr(mark);
	}

	if (path.empty())
	{
		// A reference of nothing but a query keeps the path of the base.
		out = b.root + removeDotSegments(b.path) + query;
		return true;
	}

	if (path[0] == '/')
	{
		out = b.root + removeDotSegments(path) + query;
		return true;
	}

	// Merged against the directory of the base, which is everything up to its
	// last separator. The base always carries one, because splitAbsolute puts
	// a path of "/" on an address that had none.
	const size_t last = b.path.rfind('/');
	const std::string dir = (last == std::string::npos) ? std::string("/") : b.path.substr(0, last + 1);
	out = b.root + removeDotSegments(dir + path) + query;
	return true;
}

bool looksLikePlaylist(const std::string &body)
{
	size_t at = 0;
	if (body.compare(0, sizeof(kBom) - 1, kBom) == 0)
		at = sizeof(kBom) - 1;
	return body.compare(at, sizeof(kHead) - 1, kHead) == 0;
}

Rewritten rewrite(const std::string &body, const std::string &base, Minter &m,
                  std::string &out, size_t &uris)
{
	out.clear();
	uris = 0;

	if (!looksLikePlaylist(body))
		return Rewritten::NotAPlaylist;

	out.reserve(body.size() + body.size() / 4);

	/* Set by the tag that says the next address is a variant of this stream
	   and cleared by the address that follows it, which is the format's own
	   rule for telling a master playlist from a media one: the tag stands on
	   its own line and the address is the line after it. */
	bool next_is_variant = false;

	size_t at = 0;
	while (at <= body.size())
	{
		size_t nl = body.find('\n', at);
		const bool last = (nl == std::string::npos);
		if (last)
			nl = body.size();

		std::string line = body.substr(at, nl - at);
		at = nl + 1;

		std::string ending;
		if (!last)
		{
			if (!line.empty() && line[line.size() - 1] == '\r')
			{
				line.erase(line.size() - 1);
				ending = "\r\n";
			}
			else
			{
				ending = "\n";
			}
		}

		if (line.empty())
		{
			out += ending;
			if (last)
				break;
			continue;
		}

		if (line[0] == '#')
		{
			if (lineHasByteRange(line))
				return Rewritten::HasByteRange;

			if (startsWithNoCase(line, "#EXT-X-STREAM-INF"))
				next_is_variant = true;

			const size_t key = line.find("URI=\"");
			if (key != std::string::npos)
			{
				const size_t from = key + 5;
				const size_t to = line.find('"', from);
				if (to == std::string::npos)
					return Rewritten::BadReference;
				if (uris >= kMaxUris)
					return Rewritten::TooManyUris;

				std::string absolute;
				if (!resolveUri(base, line.substr(from, to - from), absolute))
					return Rewritten::BadReference;
				std::string minted;
				if (!m.mint(absolute, kindOfTag(line), minted))
					return Rewritten::BadReference;

				out += line.substr(0, from);
				out += minted;
				out += line.substr(to);
				out += ending;
				++uris;
				if (last)
					break;
				continue;
			}

			out += line;
			out += ending;
			if (last)
				break;
			continue;
		}

		if (uris >= kMaxUris)
			return Rewritten::TooManyUris;

		std::string absolute;
		if (!resolveUri(base, line, absolute))
			return Rewritten::BadReference;
		std::string minted;
		if (!m.mint(absolute, next_is_variant ? UriKind::Playlist : UriKind::Segment, minted))
			return Rewritten::BadReference;

		next_is_variant = false;
		out += minted;
		out += ending;
		++uris;
		if (last)
			break;
	}

	return Rewritten::Ok;
}

} // namespace hls

} // namespace httpd
