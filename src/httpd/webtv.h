/*
 * webtv.h - fetching a channel's stream off a far server and passing it on
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (carried fetches)
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

#ifndef __httpd_webtv_h__
#define __httpd_webtv_h__

#include "carry.h"
#include "endpoint.h"
#include "hlsrewrite.h"
#include "netmatch.h"

#include "coreapi/base/types.h"

#include <cstddef>
#include <string>
#include <vector>

namespace httpd
{

/* The half of web television this server does: the channels of the box that are played
   from an address somewhere else, fetched here and handed on to a browser from this
   box's own name.

   WHY THERE IS ANYTHING TO DO AT ALL. A page cannot put a far server's stream into its
   own video element unless that server says it may, and the servers these lists point
   at do not say it. A page that opens the address in a tab of its own escapes the rule
   and leaves the channel playing outside the interface. Fetched here, the only server
   the page talks to is this one.

   WHAT MAY BE FETCHED, WHICH IS THE WHOLE OF THE SECURITY OF THIS FILE. No route here
   takes an address. A route takes a channel of this box and looks the address up in the
   box's own lists, or it takes a token this box minted for an address it read out of a
   playlist it had already fetched. There is no third way. On top of that, every
   connection that is actually opened, the first and every redirection after it, has its
   address compared against the networks below and refused if it is inside one. */
namespace webtv
{

/* The networks nothing here will connect to: loopback, the private ranges, the carrier
   range, link local, the multicast and reserved blocks, and the same in the second
   family.

   A refusal list and not the local network list next door, and wider than it on
   purpose. What that one answers is which callers this box treats as near; this one
   answers where this box may send a request of its own. The one that matters most is
   the one they share: an address on this machine, which is where everything on this box
   that answers without a credential lives.

   The comparison is over the address a connection is about to be made to and never over
   the name it was written with. A name is whatever a far server's nameserver says it
   is, and that answer can be an address on this network. */
const std::vector<NetPrefix> &refusedNetworks();

/* Whether one address is inside any of them. Answers true for an address that
   cannot be read at all, so a form nothing here understands is refused rather
   than let through for want of a rule about it. */
bool addressRefused(const std::string &addr);

/* Whether an address is one this may even try: http or https, and nothing
   else. Checked before curl is handed anything, so a scheme naming a file or a
   device never reaches a library that knows how to open one. */
bool urlAcceptable(const std::string &url);

/* Puts a different refusal list in place, and NULL puts the one above back.
   Installed by nothing in the product. A case that needs a server it can
   actually reach has to run one on this machine, and this machine is the first
   thing the shipped list refuses, so a case either drives the refusal with the
   shipped list or drives the fetching with a list of its own. */
void setRefusedNetworksForTest(const std::vector<NetPrefix> *list);

// What became of reaching out.
enum class Reached
{
	Ok,
	// The address, or one it was redirected to, is inside a refused network,
	// or is not an address this fetches at all.
	Refused,
	// Nothing came back: no route, no name, no answer, or the far server went
	// quiet part way.
	Unreachable,
	// More than the ceiling that was given, which is a document that is not
	// the kind of document that was asked for.
	TooLarge,
	// Given up on the way, because the browser it was fetched for went away or
	// the server is stopping: there is nobody left to answer.
	Cancelled
};

// What came back from a fetch that ran to the end.
struct Document
{
	// The far server's own status, and zero where nothing answered.
	long        code;
	std::string body;
	// After redirection, which is what the addresses inside it stand against.
	std::string url;
	std::string content_type;

	Document() : code(0) {}
};

/* Fetches a whole document, up to a ceiling, and answers what became of it.

   For the documents that are documents: a playlist is a few kilobytes and is
   read here rather than passed on, because every address inside it has to be
   replaced before a browser sees it. Nothing that is a stream goes through
   this.

   Fetched for a carried answer, job is that answer's: the fetch is then given up
   within a second of its browser leaving or of the server stopping. */
Reached fetchBounded(const std::string &url, size_t cap, Document &out,
                     carry::Job *job = NULL);

/* Remembers an address this box read out of a playlist and answers the token that
   stands for it, which is what goes into the playlist the page is given.

   A token and not the address, because a route that took an address would fetch
   whatever anybody writes into it. What the page can name is a thing this box minted,
   and the address it stands for was one this box had already fetched a playlist from.

   Empty for a mint that could not be made, which is the box being out of random
   bytes. */
std::string mintPart(coreapi::ChannelId id, const std::string &url, hls::UriKind kind);

/* False for a token nothing minted for channel id and for one that has aged out.
   A token that is found is in use, and its time starts again (partLifetimeMs);
   one asked for under another channel is neither answered nor kept. */
bool findPart(const std::string &token, coreapi::ChannelId id, std::string &url,
              hls::UriKind &kind);

// How many are held, and a way to drop them all. The second is for a case, and
// for nothing in the product: they age out on their own.
size_t partsHeld();
void   forgetParts();

/* How long a minted address stands, and how many are kept.

   A minted address is worth what the far server's own address is worth, which for these
   lists is a signed address that expires by itself. Kept for a few minutes after it was
   last minted or asked for, so a player which read a playlist can still fetch what was
   in it and a playlist a player keeps reading stays, and no longer. */
unsigned partLifetimeMs();
// Another lifetime for a case, and 0 for the one above; nothing in the product sets it.
void     setPartLifetimeMsForTest(unsigned ms);
size_t   partCeiling();

// The most distinct addresses one playlist may have minted.
size_t   partsPerPlaylist();

/* What an address handed to a browser is, kept in the answer (Response::relay_kind)
   for the job that carries it. A playlist is read and rewritten before it is
   answered; everything else is passed on as it arrives. */
enum RelayKind
{
	// A channel's own address, when it is a stream rather than a playlist.
	RelayStream   = 0,
	RelaySegment  = 1,
	RelayMap      = 2,
	RelayKey      = 3,
	RelayPlaylist = 4,
	/* A playlist named by the channel's own one. It is read and rewritten as
	   well, but names no further playlist: the format nests two levels, and a
	   chain that goes on is a far server sending this box round in circles. */
	RelayNestedPlaylist = 5
};

/* The answer a handler gives for an address to be fetched: a carried answer
   (carry.h), which the bridge hands to a job of its own. The handler adds the
   channel for a playlist (Response::relay_channel), whose addresses are minted
   for it.

   The media type is what goes out if the far server names none it may pass on
   (safeType below); for a playlist it is the type of the rewritten playlist. */
Response relayOf(const std::string &url, RelayKind kind, const char *fallback_type);

/* The media type a passed on answer of a kind goes out with, out of the one the
   far server named: in lower case and without its parameters, and only where it
   is one that kind may be. Anything else becomes the kind's own type: a page, a
   picture or a script, a type for another kind, none at all. A key is always
   bytes.

   What a browser is handed under this box's address is media to play and never
   a document to open, because a document opened from here runs with this box's
   origin, and with it the session of whoever is signed in. */
std::string safeType(RelayKind kind, const std::string &named);

/* Puts on an answer the policy that keeps a browser from running anything in it,
   should it be opened as a document after all: no script, no form, no plugin,
   and no request of its own. Every answer of these routes carries it. */
void sandbox(Response &r);

/* Carries an answer relayOf() built for anything but a playlist, on its job:
   the far server's head is
   waited for; a far server that refuses, an address this box will not
   connect to and one that cannot be reached are answered as refusals while
   nothing has been written; the header goes out with the first bytes of the
   body, stating the far server's length where it named one. Ends when the far
   server does, when the browser stops reading or leaves, and when the server
   stops. */
void relay(carry::Job &job);

/* The most a passed on answer of a kind may carry, and 0 for no ceiling. A
   segment, a map and a key are finite and small; past this they are not what
   the playlist said they were, and a far server could otherwise keep a seat
   and the box's line busy for as long as it cares to send. A channel's stream
   has none: it runs for as long as somebody watches. */
uint64_t byteCeilingOf(RelayKind kind);

/* Holds every connection this file is about to open for ms first, as a slow name
   lookup would: nothing reaches a fetch that waits there. 0 takes it away.
   Nothing in the product sets it. */
void setOpenDelayMsForTest(unsigned ms);

/* Whether ep is one of the routes below. Their answers are carried, so they can
   meet the refusals the transport gives itself, and the document states those
   for them by this. */
bool carriedRoute(const Endpoint &ep);

} // namespace webtv

// The routes that hand a browser a channel this box plays from somewhere else.
extern const RouteTable webtvTable;

} // namespace httpd

#endif
