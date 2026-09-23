/*
 * hlsrewrite.h - reading an HLS playlist and pointing its addresses back at this box
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

#ifndef __httpd_hlsrewrite_h__
#define __httpd_hlsrewrite_h__

#include <cstddef>
#include <string>

namespace httpd
{

namespace hls
{

/* What one address inside a playlist leads to, which decides what this box does
   when the address it handed out in its place is asked for: a playlist is fetched
   and read again, everything else is passed through as bytes. The kind is settled
   out of the tag the address stands on rather than by looking at what comes back:
   a guess made from the answer would be a guess made after a request has gone. */
enum class UriKind
{
	Playlist,
	Segment,
	Key,
	Map
};

/* Turns a reference found in a playlist into the whole address it names, against
   the address that playlist was actually read from.

   RFC 3986 section 5, and the reason all of it is here rather than the easy half: a
   master playlist in the wild reaches its variants with five levels of "..", and a
   resolver that only handles the leading slash and the plain relative name builds an
   address that answers 404. The base is the address after redirection.

   Only http and https come out of this, so nothing below is ever handed a file name
   or a scheme this box would have to decide about. */
bool resolveUri(const std::string &base, const std::string &ref, std::string &out);

// Whether these bytes are a playlist at all. The format says the first line is
// #EXTM3U, so seven bytes settle it and nothing has to be read further.
bool looksLikePlaylist(const std::string &body);

/* Hands back the address the page is to ask for instead of the one that was written
   in the playlist.

   An interface and not a function pointer with a cookie, because what mints an
   address has to keep what it minted. Answering false is what a minter that has run
   out of room says, and it stops the rewrite rather than leaving one address in the
   playlist pointing at somebody else's server. */
class Minter
{
	public:
		virtual ~Minter();
		virtual bool mint(const std::string &absolute, UriKind kind, std::string &out) = 0;
};

// Why a playlist was not rewritten, for a caller that has to say so.
enum class Rewritten
{
	Ok,
	// The bytes do not begin with #EXTM3U.
	NotAPlaylist,
	// A reference this cannot turn into an http address, or one the minter
	// would not take.
	BadReference,
	/* A playlist carrying EXT-X-BYTERANGE, on the tag or as an attribute of
	   EXT-X-MAP. Refused rather than passed through: the page would send the range
	   to this box, this box would answer the whole resource, and what the player
	   then decoded would be wrong in a way nothing reports. Refused rather than
	   supported, because the reading of a range with no offset depends on the range
	   before it. */
	HasByteRange,
	// More addresses than this will hold, which is a playlist that is not a
	// playlist of a channel.
	TooManyUris
};

/* Reads a playlist and writes it out with every address it carries replaced by one
   the minter gave.

   Every address, and that is the point: an address left alone is one the browser
   fetches from the far server itself, which is the request the same origin rule
   turns down. So both forms are read, the line that is nothing but an address and
   the URI attribute of a tag, and a tag this does not know that carries one still
   has it rewritten.

   uris answers how many were replaced. */
Rewritten rewrite(const std::string &body, const std::string &base, Minter &m,
                  std::string &out, size_t &uris);

// The most addresses one playlist may carry. A live playlist of a channel
// holds a handful and a long catch up list a few thousand; past this it is
// something else, and reading it would be this box minting a token per line
// for whatever it was handed.
const size_t kMaxUris = 8192;

} // namespace hls

} // namespace httpd

#endif
