/*
 * hlsrewrite.cpp - reading an HLS playlist and pointing its addresses back at this box
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (attribute lists, tag rules)
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

/* Which kind the address of a tag is, by the tag it stands on. Asked only for
   the tags whose address is replaced (TagRule::Address): a key and a session key
   lead to a key, a map to the head of a segment, a rendition and an I-frame
   playlist to a playlist. */
UriKind kindOfTag(const std::string &line)
{
	if (startsWithNoCase(line, "#EXT-X-KEY") || startsWithNoCase(line, "#EXT-X-SESSION-KEY"))
		return UriKind::Key;
	if (startsWithNoCase(line, "#EXT-X-MAP"))
		return UriKind::Map;
	return UriKind::Playlist;
}

bool lineHasByteRange(const std::string &line)
{
	if (startsWithNoCase(line, "#EXT-X-BYTERANGE"))
		return true;
	return startsWithNoCase(line, "#EXT-X-MAP") &&
	       line.find("BYTERANGE=") != std::string::npos;
}

/* What is done with a tag. A player acts on the tags it knows and fetches the
   addresses they name, so every tag that can name an address is either one
   whose address is replaced, or left out of the playlist, or refused. */
enum class TagRule
{
	// A tag of the format that names no address, kept as it is.
	Keep,
	// The URI attribute, where the tag carries one, is replaced; any other
	// attribute that names an address refuses the playlist.
	Address,
	// An attribute list that names no address; one that does is refused.
	List,
	// Left out where one of its attributes names an address: an interstitial
	// or a piece of session data the channel plays without.
	Optional,
	// Left out always: content steering, which would send the player to the
	// far server's pathways, and the low latency tags, which have the player
	// ask with directives these routes do not take. The playlist plays on
	// without them, as an ordinary one.
	Drop
};

struct TagSpec
{
	const char *name;
	TagRule     rule;
};

const TagSpec kTagSpecs[] = {
	{ "EXTM3U",                       TagRule::Keep },
	{ "EXTINF",                       TagRule::Keep },
	{ "EXT-X-VERSION",                TagRule::Keep },
	{ "EXT-X-TARGETDURATION",         TagRule::Keep },
	{ "EXT-X-MEDIA-SEQUENCE",         TagRule::Keep },
	{ "EXT-X-DISCONTINUITY-SEQUENCE", TagRule::Keep },
	{ "EXT-X-DISCONTINUITY",          TagRule::Keep },
	{ "EXT-X-PROGRAM-DATE-TIME",      TagRule::Keep },
	{ "EXT-X-PLAYLIST-TYPE",          TagRule::Keep },
	{ "EXT-X-ENDLIST",                TagRule::Keep },
	{ "EXT-X-I-FRAMES-ONLY",          TagRule::Keep },
	{ "EXT-X-INDEPENDENT-SEGMENTS",   TagRule::Keep },
	{ "EXT-X-GAP",                    TagRule::Keep },
	{ "EXT-X-BITRATE",                TagRule::Keep },
	{ "EXT-X-KEY",                    TagRule::Address },
	{ "EXT-X-SESSION-KEY",            TagRule::Address },
	{ "EXT-X-MAP",                    TagRule::Address },
	{ "EXT-X-MEDIA",                  TagRule::Address },
	{ "EXT-X-I-FRAME-STREAM-INF",     TagRule::Address },
	{ "EXT-X-STREAM-INF",             TagRule::List },
	{ "EXT-X-START",                  TagRule::List },
	{ "EXT-X-DEFINE",                 TagRule::List },
	{ "EXT-X-DATERANGE",              TagRule::Optional },
	{ "EXT-X-SESSION-DATA",           TagRule::Optional },
	{ "EXT-X-CONTENT-STEERING",       TagRule::Drop },
	{ "EXT-X-SERVER-CONTROL",         TagRule::Drop },
	{ "EXT-X-PART-INF",               TagRule::Drop },
	{ "EXT-X-PART",                   TagRule::Drop },
	{ "EXT-X-PRELOAD-HINT",           TagRule::Drop },
	{ "EXT-X-RENDITION-REPORT",       TagRule::Drop },
	{ "EXT-X-SKIP",                   TagRule::Drop },
};

// The rule for a tag line by its exact name, false for a tag not listed.
bool tagRule(const std::string &line, TagRule &rule)
{
	const size_t colon = line.find(':');
	const std::string name = line.substr(1, (colon == std::string::npos) ? std::string::npos : colon - 1);
	for (size_t i = 0; i < sizeof(kTagSpecs) / sizeof(kTagSpecs[0]); ++i)
	{
		if (name == kTagSpecs[i].name)
		{
			rule = kTagSpecs[i].rule;
			return true;
		}
	}
	return false;
}

// One attribute: its name, and where its value lies in the line, inside the
// quotes of a quoted one.
struct Attribute
{
	std::string name;
	size_t      from;
	size_t      to;
	bool        quoted;
};

bool attributeNameChar(char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
	       c == '-' || c == '_';
}

/* The attribute list after a tag's colon, as the format writes it: NAME=value
   separated by commas, the value quoted, with no quote inside, or plain up to
   the next comma. False for anything else, and for a name given twice: a
   reader that takes the first of two and one that takes the last read two
   different playlists out of one line. */
bool readAttributes(const std::string &line, size_t at, std::vector<Attribute> &out)
{
	out.clear();
	while (at < line.size())
	{
		while (at < line.size() && (line[at] == ' ' || line[at] == '\t'))
			++at;
		size_t end = at;
		while (end < line.size() && attributeNameChar(line[end]))
			++end;
		if (end == at || end >= line.size() || line[end] != '=')
			return false;

		Attribute a;
		a.name = line.substr(at, end - at);
		for (size_t i = 0; i < out.size(); ++i)
		{
			if (out[i].name == a.name)
				return false;
		}

		size_t next = 0;
		if (end + 1 < line.size() && line[end + 1] == '"')
		{
			const size_t close = line.find('"', end + 2);
			if (close == std::string::npos)
				return false;
			a.quoted = true;
			a.from = end + 2;
			a.to = close;
			next = close + 1;
			if (next < line.size() && line[next] != ',')
				return false;
		}
		else
		{
			a.quoted = false;
			a.from = end + 1;
			next = line.find(',', a.from);
			if (next == std::string::npos)
				next = line.size();
			a.to = next;
			if (a.to == a.from || line.find('"', a.from) < next)
				return false;
		}
		out.push_back(a);

		if (next >= line.size())
			break;
		// a comma that ends the line leaves an attribute without a name
		at = next + 1;
		if (at >= line.size())
			return false;
	}
	return !out.empty();
}

// Whether an attribute names an address, whatever the tag: URI, a name ending
// in -URI or -LIST, or one that says URL.
bool namesAddress(const std::string &name)
{
	std::string upper(name);
	for (size_t i = 0; i < upper.size(); ++i)
	{
		if (upper[i] >= 'a' && upper[i] <= 'z')
			upper[i] = (char) (upper[i] - 'a' + 'A');
	}
	const size_t n = upper.size();
	return upper == "URI" || upper.find("URL") != std::string::npos ||
	       (n > 4 && upper.compare(n - 4, 4, "-URI") == 0) ||
	       (n > 5 && upper.compare(n - 5, 5, "-LIST") == 0);
}

/* A reference a player would read otherwise than this box does: one that holds
   a variable the player fills in. The box resolves the text as it stands, so
   what it fetched and what the player meant would be two addresses. */
bool readsOtherwise(const std::string &ref)
{
	return ref.find("{$") != std::string::npos;
}

/* Whether a line, its line feed and a carriage return before it taken off, is
   one line to every reader. A carriage return or another control character
   inside it (a tab aside), and the two Unicode separators a script's line
   matching stops at, would let a reader see a second line this box never read. */
bool oneLine(const std::string &line)
{
	for (size_t i = 0; i < line.size(); ++i)
	{
		const unsigned char c = (unsigned char) line[i];
		if ((c < 0x20 && c != '\t') || c == 0x7F)
			return false;
		if (c == 0xE2 && i + 2 < line.size() && (unsigned char) line[i + 1] == 0x80 &&
		    ((unsigned char) line[i + 2] == 0xA8 || (unsigned char) line[i + 2] == 0xA9))
			return false;
	}
	return true;
}

// The line without the spaces and tabs around it, as a player reads an address.
std::string trimmedLine(const std::string &line)
{
	size_t from = 0;
	size_t to = line.size();
	while (from < to && (line[from] == ' ' || line[from] == '\t'))
		++from;
	while (to > from && (line[to - 1] == ' ' || line[to - 1] == '\t'))
		--to;
	return line.substr(from, to - from);
}

/* A segment's duration as the format writes it: digits, and after a point
   more digits, or no point at all. A player reads a duration as far as it is
   one of these and the rest of the line as an address, so "4." or ".5" would
   be a duration and a second line to it. The title after the comma is not
   read, and nothing reads it. False for anything else. */
bool segmentDuration(const std::string &line, std::string &out)
{
	const size_t from = std::strlen("#EXTINF:");
	const size_t comma = line.find(',', from);
	out = trimmedLine(line.substr(from, (comma == std::string::npos) ? std::string::npos : comma - from));
	size_t i = 0;
	while (i < out.size() && out[i] >= '0' && out[i] <= '9')
		++i;
	if (i == 0)
		return false;
	if (i == out.size())
		return true;
	if (out[i] != '.')
		return false;
	const size_t fraction = ++i;
	while (i < out.size() && out[i] >= '0' && out[i] <= '9')
		++i;
	return i > fraction && i == out.size();
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

	// written without the mark of the encoding, so #EXTM3U is the first line
	const size_t bom = sizeof(kBom) - 1;

	/* Set by the tag that says the next address is a variant of this stream
	   and cleared by the address that follows it, which is the format's own
	   rule for telling a master playlist from a media one: the tag stands on
	   its own line and the address is the line after it. */
	bool next_is_variant = false;

	size_t at = (body.compare(0, bom, kBom) == 0) ? bom : 0;
	while (at <= body.size())
	{
		size_t nl = body.find('\n', at);
		const bool last = (nl == std::string::npos);
		if (last)
			nl = body.size();

		std::string line = body.substr(at, nl - at);
		at = nl + 1;

		/* A carriage return before the line feed, or before the end of the text,
		   belongs to the line's end; the last line gets its end after the walk. */
		std::string ending = last ? "" : "\n";
		if (!line.empty() && line[line.size() - 1] == '\r')
		{
			line.erase(line.size() - 1);
			if (!last)
				ending = "\r\n";
		}

		if (!oneLine(line))
			return Rewritten::BadReference;

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

			/* A player that looks for its tags anywhere in the text, and not only
			   at the start of a line, would find one inside another line. */
			if (line.find("#EXT", 1) != std::string::npos)
				return Rewritten::BadReference;

			if (startsWithNoCase(line, "#EXT-X-STREAM-INF"))
				next_is_variant = true;

			TagRule rule = TagRule::List;
			if (!startsWithNoCase(line, "#EXT") || !tagRule(line, rule))
			{
				/* A comment, or a tag no player acts on, stays as it is, unless it
				   looks as though it names an address: then it is left out. */
				if (line.find("://") == std::string::npos && line.find("URI=") == std::string::npos)
				{
					out += line;
					out += ending;
				}
				if (last)
					break;
				continue;
			}

			if (rule == TagRule::Drop)
			{
				if (last)
					break;
				continue;
			}
			if (rule == TagRule::Keep)
			{
				std::string duration;
				if (!startsWithNoCase(line, "#EXTINF:"))
					out += line;
				else if (segmentDuration(line, duration))
					out += "#EXTINF:" + duration + ",";
				else
					return Rewritten::BadReference;
				out += ending;
				if (last)
					break;
				continue;
			}

			std::vector<Attribute> attrs;
			const size_t colon = line.find(':');
			if (colon == std::string::npos)
			{
				// a tag without a list names nothing
				out += line;
				out += ending;
				if (last)
					break;
				continue;
			}
			if (!readAttributes(line, colon + 1, attrs))
				return Rewritten::BadReference;

			const Attribute *uri = NULL;
			bool other_address = false;
			for (size_t i = 0; i < attrs.size(); ++i)
			{
				if (!namesAddress(attrs[i].name))
					continue;
				if (rule == TagRule::Address && attrs[i].name == "URI")
					uri = &attrs[i];
				else
					other_address = true;
			}

			if (rule == TagRule::Optional && (other_address || uri != NULL))
			{
				if (last)
					break;
				continue;
			}
			if (other_address)
				return Rewritten::BadReference;

			if (uri != NULL)
			{
				if (uris >= kMaxUris)
					return Rewritten::TooManyUris;

				const std::string ref = line.substr(uri->from, uri->to - uri->from);
				std::string absolute;
				if (readsOtherwise(ref) || !resolveUri(base, ref, absolute))
					return Rewritten::BadReference;
				std::string minted;
				if (!m.mint(absolute, kindOfTag(line), minted))
					return Rewritten::BadReference;

				// written quoted whichever way it came
				const size_t value_from = uri->quoted ? uri->from - 1 : uri->from;
				const size_t value_to = uri->quoted ? uri->to + 1 : uri->to;
				out += line.substr(0, value_from);
				out += '"';
				out += minted;
				out += '"';
				out += line.substr(value_to);
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

		/* An address as a player reads it, without the space around it; a line
		   of nothing but space is no address, and is left out. */
		const std::string ref = trimmedLine(line);
		if (ref.empty())
		{
			if (last)
				break;
			continue;
		}

		if (uris >= kMaxUris)
			return Rewritten::TooManyUris;

		std::string absolute;
		if (readsOtherwise(ref) || !resolveUri(base, ref, absolute))
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

	// the last line ends, so nothing after it can be read into it
	if (!out.empty() && out[out.size() - 1] != '\n')
		out += '\n';
	return Rewritten::Ok;
}

} // namespace hls

} // namespace httpd
