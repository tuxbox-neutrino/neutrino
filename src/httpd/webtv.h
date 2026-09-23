/*
 * webtv.h - fetching a channel's stream off a far server and passing it on
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

#ifndef __httpd_webtv_h__
#define __httpd_webtv_h__

#include "endpoint.h"
#include "hlsrewrite.h"
#include "netmatch.h"

#include "coreapi/base/types.h"

#include <cstddef>
#include <string>
#include <vector>

/* Declared rather than included, so that this header costs the library's
   headers nothing: the only thing wanted from it here is a connection to hand
   back to the transport. */
struct MHD_Connection;

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
	TooLarge
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
   this. */
Reached fetchBounded(const std::string &url, size_t cap, Document &out);

/* Remembers an address this box read out of a playlist and answers the token that
   stands for it, which is what goes into the playlist the page is given.

   A token and not the address, because a route that took an address would fetch
   whatever anybody writes into it. What the page can name is a thing this box minted,
   and the address it stands for was one this box had already fetched a playlist from.

   Empty for a mint that could not be made, which is the box being out of random
   bytes. */
std::string mintPart(coreapi::ChannelId id, const std::string &url, hls::UriKind kind);

// False for a token nothing minted and for one that has aged out.
bool findPart(const std::string &token, coreapi::ChannelId &id, std::string &url,
              hls::UriKind &kind);

// How many are held, and a way to drop them all. The second is for a case, and
// for nothing in the product: they age out on their own.
size_t partsHeld();
void   forgetParts();

/* How long a minted address stands, and how many are kept.

   A minted address is worth what the far server's own address is worth, which for these
   lists is a signed address that expires by itself. Kept for a few minutes so a player
   which read a playlist can still fetch what was in it, and no longer. */
unsigned partLifetimeMs();
size_t   partCeiling();

// The two shapes of an answer this file turns a connection into, asked of the
// answer a handler built rather than of the route it came from: what decides
// is whether the handler named an address to fetch.
bool isRelay(const Response &r);

/* The answer a handler gives for a stream that is to be passed through.

   The media type is what goes out if the far server names none of its own or
   does not name one in time, and is never what overrides it: what this hands
   on is the far server's document, and saying it is something else would be
   this box deciding for a player what it is about to decode. */
Response relayOf(const std::string &url, const char *fallback_type);

// What became of turning a connection into a pass through.
enum Opened
{
	// Queued. The connection belongs to this file until it closes.
	RelayOpened,
	// The box is already passing through as many as it will, and the refusal
	// carries the answer that says so.
	RelayRefused,
	// The address was refused, the far server could not be reached, or the
	// library would not take the response.
	RelayFailed
};

/* Attaches a pass through to this connection.

   Called from the access handler and from nowhere else, the library allowing a
   response to be queued for a live connection only from there. */
Opened open(struct MHD_Connection *connection, const Response &r, Response &refusal);

/* Ends every pass through and waits for them to be gone.

   Called where the server is stopped and before the daemon is torn down, for
   the reason the event streams are: a pass through with nothing to hand on is
   a suspended connection, and the library says that stopping a daemon while
   one of its connections is suspended leaks the connection or worse. */
void stopAll();

// How many are open, which is the one number that says a pass through nobody
// is reading was let go.
size_t openRelays();

/* The most that may run at once. Each one is a thread and a socket out, so
   this is what keeps a page opened in ten tabs from being ten fetches of ten
   streams off a box with one aerial's worth of uplink. */
size_t relayCeiling();
void   setRelayCeilingForTest(size_t n);

/* How long the transport waits for the far server's own head before it answers with one
   of its own.

   A wait and not a promise, and short. What the wait buys is the far server's media
   type and its status; what it costs is one of the server's few threads standing still.
   A healthy server answers in a fraction of this and a sick one is a channel that is
   not going to play anyway. */
unsigned headerWaitMs();
void     setHeaderWaitMsForTest(unsigned ms);

} // namespace webtv

// The routes that hand a browser a channel this box plays from somewhere else.
extern const RouteTable webtvTable;

} // namespace httpd

#endif
