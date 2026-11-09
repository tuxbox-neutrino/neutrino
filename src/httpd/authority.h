/*
 * authority.h - the names a request may reach this box under
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * A request names the box it is for in its Host header, and a browser fills
 * that in with the name of the page that made the request. A page whose own
 * name resolves to this box (a name its owner points at the box's address)
 * is answered as if it were a page of this box: the browser counts the
 * answers as that page's own and lets it read them, and it reads what the
 * home network may read without a password. So the request boundary asks,
 * before any other hook sees a request, whether the name is one this box
 * goes by: one only the home network answers for, or one the owner listed.
 *
 * Read from src/nhttpd, which is built without exceptions: hostAccepted and
 * the texts catch whatever the layer under them throws.
 */

#ifndef __httpd_authority_h__
#define __httpd_authority_h__

#include <map>
#include <string>
#include <vector>

#include <time.h>

namespace httpd
{

/* What the names a request may use are judged against, beyond an address, a
   single label and one label under .local, which need nothing read.

   suffixes are the domains the resolver searches (search and domain in
   resolv.conf), the zone a router names the devices of its network in: one
   label under one of them counts, h7.fritz.box or wohnzimmer.fritz.box.
   host_name is what the system calls the box, which counts whole where it
   carries a domain the resolver does not search. extra is the access file's
   allowed_hosts: a DynDNS name, the name of a proxy in front of the box.

   Every member lowered and without a dot at the end. */
struct BoxNames
{
	std::string              host_name;
	std::vector<std::string> suffixes;
	std::vector<std::string> extra;
};

/* Whether host, a Host header as the request carried it, names this box.

   Taken: an empty one, which is a request without the header as an old
   script sends it; an IPv4 address; an IPv6 address in brackets; a single
   label, localhost and every name the router or another device answers for
   without a domain among them; one label under .local or under one of the
   suffixes; host_name; and a name of extra. Each with or without a port, and a
   name also with the dot that ends a fully qualified one. Case does not count.

   Refused: every other name, which is a name somebody can register and have
   lead to the box, an IPv6 address without brackets, an address carrying a
   zone, a port that is not a number up to 65535, and anything that is not a
   name or an address at all. */
bool authorityAccepted(const std::string &host, const BoxNames &names);

/* The names in effect: the host name and the resolver's domains as the
   system has them, and the allowed_hosts line of the access file the
   configuration was read from (configPath), all looked at again every 10
   seconds, so a domain a lease brings and a name the owner adds to the file
   count without a restart. The file is read again only when it changed; until
   it could be read, and where no file is in use, the list is the one config()
   holds. */
BoxNames boxNames();

/* authorityAccepted against boxNames(). False where anything inside fails. */
bool hostAccepted(const std::string &host);

/* hostAccepted over the headers of a request as nhttpd's reader keeps them, one
   entry per spelling of a name, the later of two equal spellings in it: the Host
   header named in more than one spelling is refused, so every later reader of
   it reads the value checked here. The request boundary asks this of every
   request (CAuthorityGate), and the reader once more of a POST before its body
   is read. */
bool hostHeaderAccepted(const std::map<std::string, std::string> &headers);

/* The domains a resolver configuration names under search and domain, in
   the order written, each once, lowered and without a dot at the end. */
std::vector<std::string> resolverSuffixes(const std::string &text);

/* Whether text is a host name: labels of letters, digits and hyphens, none
   empty, longer than 63 or starting or ending with a hyphen, at most 253
   in all, one dot at the end allowed. What allowed_hosts takes. */
bool isHostName(const std::string &text);

// text lowered and without a dot at its end, which is how every name above is compared.
std::string comparableName(const std::string &text);

/* What a refused request is told. The page is plain text in English and
   German and says how to reach the box by its address and where a name is
   added: the access file the box reads (configPath), or its usual name where
   none is known. local_authority is the address and port the request arrived
   on ("192.168.1.5:80"), named under https where the server speaks it, and
   only to a peer on the home network (the private ranges and the loopback)
   while no proxy is configured: through a forwarded port or a proxy the peer
   can be anybody. Left out where it is empty or the loopback. The detail is
   the sentence an API refusal carries. Neither repeats the refused name or
   names any the box takes. */
std::string unknownHostPage(const std::string &local_authority, const std::string &peer, bool https);
const char *unknownHostDetail();

/* Where the box's own names are read from: the host name and the resolver
   file. A case sets both; two empty strings give the system's back. */
void setNameSourcesForTest(const std::string &host_name, const std::string &resolv_conf);

/* What boxNames takes the time to be, in seconds of a monotonic clock; nought
   gives the real one back. A case walks the interval with it. */
void setNameClockForTest(time_t now);

} // namespace httpd

#endif /* __httpd_authority_h__ */
