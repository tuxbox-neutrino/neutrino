/*
 * test_netmatch.cpp - tests for local network matching
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

#include "support/catch.hpp"
#include "httpd/netmatch.h"

#include <string>
#include <vector>

namespace
{

// One line read, for a case that only wants the prefix and would otherwise
// spell out the same three lines around every one of them.
httpd::NetPrefix mustParse(const std::string &text)
{
	httpd::NetPrefix p;
	REQUIRE(httpd::parsePrefix(text, &p));
	return p;
}

} // namespace

TEST_CASE("a v4 address inside its prefix matches and one outside does not", "[net]")
{
	httpd::NetPrefix p;
	REQUIRE(httpd::parsePrefix("192.168.0.0/16", &p));
	REQUIRE(httpd::addressInPrefix("192.168.1.1", p));
	REQUIRE(httpd::addressInPrefix("192.168.255.255", p));
	REQUIRE_FALSE(httpd::addressInPrefix("192.169.0.1", p));
	REQUIRE_FALSE(httpd::addressInPrefix("10.0.0.1", p));
}

TEST_CASE("matching is on the parsed address and not on its text", "[net]")
{
	/* The shape this replaces asked whether one address was found inside the
	   text of another, which answers yes for an address that merely begins with
	   the entry and cannot tell an entry that is not there from one that is. */
	httpd::NetPrefix p;
	REQUIRE(httpd::parsePrefix("110.0.0.1/32", &p));
	REQUIRE_FALSE(httpd::addressInPrefix("10.0.0.1", p));
	REQUIRE_FALSE(httpd::parsePrefix("", &p));
	REQUIRE_FALSE(httpd::parsePrefix("/8", &p));
	REQUIRE_FALSE(httpd::parsePrefix("10.0.0.0", &p));

	// the other direction of the same mistake: an entry that is a prefix of the
	// address rather than the address a prefix of the entry
	httpd::NetPrefix q;
	REQUIRE(httpd::parsePrefix("10.0.0.1/32", &q));
	REQUIRE_FALSE(httpd::addressInPrefix("10.0.0.10", q));
	REQUIRE_FALSE(httpd::addressInPrefix("110.0.0.1", q));
}

TEST_CASE("prefix lengths at both ends behave", "[net]")
{
	httpd::NetPrefix all, one;
	REQUIRE(httpd::parsePrefix("0.0.0.0/0", &all));
	REQUIRE(httpd::addressInPrefix("8.8.8.8", all));
	REQUIRE(httpd::parsePrefix("127.0.0.1/32", &one));
	REQUIRE(httpd::addressInPrefix("127.0.0.1", one));
	REQUIRE_FALSE(httpd::addressInPrefix("127.0.0.2", one));
	REQUIRE_FALSE(httpd::parsePrefix("10.0.0.0/33", &all));
	REQUIRE_FALSE(httpd::parsePrefix("10.0.0.0/-1", &all));
}

TEST_CASE("a length that covers everything covers only its own family", "[net]")
{
	// /0 is a real entry and matches every address of its family, which is why
	// a value nobody filled in must not look like one
	const httpd::NetPrefix v4 = mustParse("0.0.0.0/0");
	const httpd::NetPrefix v6 = mustParse("::/0");

	REQUIRE(httpd::addressInPrefix("0.0.0.0", v4));
	REQUIRE(httpd::addressInPrefix("255.255.255.255", v4));
	REQUIRE_FALSE(httpd::addressInPrefix("::1", v4));

	REQUIRE(httpd::addressInPrefix("::1", v6));
	REQUIRE(httpd::addressInPrefix("2001:db8::1", v6));
	REQUIRE_FALSE(httpd::addressInPrefix("8.8.8.8", v6));
}

TEST_CASE("a prefix nobody filled in matches nothing", "[net]")
{
	/* A member left as the memory found it, and what a refused line leaves
	   behind. Either of them reading as a network that covers everything is
	   how a box ends up answering the outside world as though it were on the
	   local network. */
	httpd::NetPrefix blank;
	blank.family = 0;
	blank.len = 0;
	for (size_t i = 0; i < sizeof(blank.bits); ++i)
		blank.bits[i] = 0;

	REQUIRE_FALSE(httpd::addressInPrefix("127.0.0.1", blank));
	REQUIRE_FALSE(httpd::addressInPrefix("8.8.8.8", blank));
	REQUIRE_FALSE(httpd::addressInPrefix("::1", blank));

	httpd::NetPrefix refused = mustParse("10.0.0.0/8");
	REQUIRE(httpd::addressInPrefix("10.0.0.1", refused));
	REQUIRE_FALSE(httpd::parsePrefix("not an address/8", &refused));
	REQUIRE_FALSE(httpd::addressInPrefix("10.0.0.1", refused));
}

TEST_CASE("the byte a length ends inside is compared by its bits", "[net]")
{
	/* Both edges of a length that is not a whole number of bytes. A comparison
	   that stopped at whole bytes would take the first byte of 172 as the whole
	   of the answer and let in three times what the line states. */
	const httpd::NetPrefix p = mustParse("172.16.0.0/12");

	REQUIRE_FALSE(httpd::addressInPrefix("172.15.255.255", p));
	REQUIRE(httpd::addressInPrefix("172.16.0.0", p));
	REQUIRE(httpd::addressInPrefix("172.31.255.255", p));
	REQUIRE_FALSE(httpd::addressInPrefix("172.32.0.0", p));

	// and a length of one bit, where the remainder byte is the only byte read
	const httpd::NetPrefix half = mustParse("0.0.0.0/1");
	REQUIRE(httpd::addressInPrefix("127.255.255.255", half));
	REQUIRE_FALSE(httpd::addressInPrefix("128.0.0.0", half));
}

TEST_CASE("a line that states a host inside its network states the network", "[net]")
{
	/* The bits past the length are cleared rather than the line refused, so
	   that what is compared does not depend on which address inside the network
	   the author happened to write. */
	const httpd::NetPrefix written = mustParse("10.1.2.3/8");
	const httpd::NetPrefix meant = mustParse("10.0.0.0/8");

	REQUIRE(written.len == meant.len);
	REQUIRE(written.family == meant.family);
	for (size_t i = 0; i < sizeof(written.bits); ++i)
		REQUIRE(written.bits[i] == meant.bits[i]);

	REQUIRE(httpd::addressInPrefix("10.9.9.9", written));
	REQUIRE_FALSE(httpd::addressInPrefix("11.1.2.3", written));
}

TEST_CASE("a v6 prefix matches v6 and never v4", "[net]")
{
	httpd::NetPrefix p;
	REQUIRE(httpd::parsePrefix("fc00::/7", &p));
	REQUIRE(httpd::addressInPrefix("fd12:3456::1", p));
	REQUIRE_FALSE(httpd::addressInPrefix("fe80::1", p));
	REQUIRE_FALSE(httpd::addressInPrefix("10.0.0.1", p));
	REQUIRE_FALSE(httpd::parsePrefix("fc00::/129", &p));
}

TEST_CASE("both edges of a v6 length that is not a whole byte", "[net]")
{
	const httpd::NetPrefix p = mustParse("fc00::/7");

	REQUIRE_FALSE(httpd::addressInPrefix("fbff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", p));
	REQUIRE(httpd::addressInPrefix("fc00::", p));
	REQUIRE(httpd::addressInPrefix("fdff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", p));
	REQUIRE_FALSE(httpd::addressInPrefix("fe00::", p));

	// the far end, where every one of the sixteen bytes has to be read
	const httpd::NetPrefix one = mustParse("2001:db8::dead:beef/128");
	REQUIRE(httpd::addressInPrefix("2001:db8::dead:beef", one));
	REQUIRE(httpd::addressInPrefix("2001:0db8:0000:0000:0000:0000:dead:beef", one));
	REQUIRE_FALSE(httpd::addressInPrefix("2001:db8::dead:bef0", one));
}

TEST_CASE("a v4 mapped v6 peer is judged as the v4 address it is", "[net]")
{
	// A dual stack listener reports a v4 peer as ::ffff:10.0.0.1, and judging
	// that as v6 would put every LAN client outside every v4 prefix.
	httpd::NetPrefix p;
	REQUIRE(httpd::parsePrefix("10.0.0.0/8", &p));
	REQUIRE(httpd::addressInPrefix("::ffff:10.0.0.1", p));
}

TEST_CASE("one network has one meaning in both of its spellings", "[net]")
{
	// the mapped form on the side of the line as well as on the side of the
	// peer, so that a line written either way says the same thing
	const httpd::NetPrefix p = mustParse("::ffff:10.0.0.0/8");
	REQUIRE(p.family == AF_INET);
	REQUIRE(p.len == 8);
	REQUIRE(httpd::addressInPrefix("10.0.0.1", p));
	REQUIRE(httpd::addressInPrefix("::ffff:10.0.0.1", p));
	REQUIRE_FALSE(httpd::addressInPrefix("11.0.0.1", p));

	// the mapped range does not swallow the v6 loopback, which is inside the
	// older form of the same idea and is not 0.0.0.1
	const httpd::NetPrefix lo4 = mustParse("0.0.0.0/8");
	REQUIRE_FALSE(httpd::addressInPrefix("::1", lo4));
	REQUIRE(httpd::addressInPrefix("::1", mustParse("::/64")));
}

TEST_CASE("an address that is not one matches nothing", "[net]")
{
	const httpd::NetPrefix v4 = mustParse("0.0.0.0/0");
	const httpd::NetPrefix v6 = mustParse("::/0");

	const char *const junk[] =
	{
		"",
		" ",
		"10.0.0.1 ",
		" 10.0.0.1",
		"10.0.0.256",
		"999.1.1.1",
		"10.0.0",
		"10.0.0.1.2",
		"10.0.0.-1",
		"0x0a000001",
		"localhost",
		"10.0.0.1/8",
		"::ffff:10.0.0",
		"fe80::1::2",
		"gggg::1",
		"2001:db8:::1",
		"fe80::1 "
	};

	for (size_t i = 0; i < sizeof(junk) / sizeof(junk[0]); ++i)
	{
		INFO("entry " << junk[i]);
		REQUIRE_FALSE(httpd::addressInPrefix(junk[i], v4));
		REQUIRE_FALSE(httpd::addressInPrefix(junk[i], v6));
	}
}

TEST_CASE("an address is read whole or not at all", "[net]")
{
	// a string may carry a zero byte where the parser below stops at one, so
	// this would otherwise be judged on the half of itself that suits it
	std::string split("10.0.0.1");
	split.push_back('\0');
	split += "junk";

	const httpd::NetPrefix p = mustParse("10.0.0.0/8");
	REQUIRE(httpd::addressInPrefix("10.0.0.1", p));
	REQUIRE_FALSE(httpd::addressInPrefix(split, p));
}

TEST_CASE("a line that is not a network is refused in both families", "[net]")
{
	httpd::NetPrefix p;

	const char *const junk[] =
	{
		"",
		"/",
		"/8",
		"8",
		"10.0.0.0",
		"10.0.0.0/",
		"10.0.0.0//8",
		"10.0.0.0/8/8",
		"10.0.0.0/ 8",
		"10.0.0.0/8 ",
		"10.0.0.0/+8",
		"10.0.0.0/-1",
		"10.0.0.0/33",
		"10.0.0.0/0008",
		"10.0.0.0/x",
		"10.0.0.256/8",
		"not an address/8",
		"fc00::/129",
		"fc00::/-7",
		"fc00::/ 7",
		"gggg::/7",
		"fc00::",
		"::ffff:10.0.0.0/104"
	};

	for (size_t i = 0; i < sizeof(junk) / sizeof(junk[0]); ++i)
	{
		INFO("entry " << junk[i]);
		REQUIRE_FALSE(httpd::parsePrefix(junk[i], &p));
		// and what a refused line leaves behind matches nothing
		REQUIRE_FALSE(httpd::addressInPrefix("10.0.0.1", p));
		REQUIRE_FALSE(httpd::addressInPrefix("fc00::1", p));
	}

	// nowhere to put the answer is refused rather than written through
	REQUIRE_FALSE(httpd::parsePrefix("10.0.0.0/8", 0));
}

TEST_CASE("a length written with leading zeros is read as decimal", "[net]")
{
	/* Read as decimal and never as octal, so "/010" is ten and not eight.
	   Decimal narrows the network where octal would widen it, which is the
	   side to be wrong on for a list that admits callers rather than one that
	   turns them away. At most three digits are read, so the same number
	   written with one zero too many is refused for its length, which is
	   inconsistent to look at and safe either way. */
	httpd::NetPrefix p;

	REQUIRE(httpd::parsePrefix("10.0.0.0/008", &p));
	REQUIRE(p.len == 8);
	REQUIRE(httpd::addressInPrefix("10.255.255.255", p));
	REQUIRE_FALSE(httpd::addressInPrefix("11.0.0.0", p));

	REQUIRE(httpd::parsePrefix("1.2.3.4/010", &p));
	REQUIRE(p.len == 10);
	// ten and not eight: the network is a quarter of what an octal reading
	// would have made it
	REQUIRE(httpd::addressInPrefix("1.63.255.255", p));
	REQUIRE_FALSE(httpd::addressInPrefix("1.64.0.0", p));

	REQUIRE(httpd::parsePrefix("1.2.3.4/07", &p));
	REQUIRE(p.len == 7);

	REQUIRE(httpd::parsePrefix("1.2.3.4/00", &p));
	REQUIRE(p.len == 0);

	// and one digit past what is read
	REQUIRE_FALSE(httpd::parsePrefix("10.0.0.0/0008", &p));
	REQUIRE_FALSE(httpd::parsePrefix("fc00::/0007", &p));
}

TEST_CASE("the seeded list is what ships", "[net]")
{
	std::vector<httpd::NetPrefix> l = httpd::defaultLanPrefixes();
	REQUIRE(l.size() == 6);
	REQUIRE(httpd::addressInAnyPrefix("127.0.0.1", l));
	REQUIRE(httpd::addressInAnyPrefix("::1", l));
	REQUIRE(httpd::addressInAnyPrefix("10.1.2.3", l));
	REQUIRE(httpd::addressInAnyPrefix("172.16.0.1", l));
	REQUIRE_FALSE(httpd::addressInAnyPrefix("172.32.0.1", l));
	REQUIRE(httpd::addressInAnyPrefix("192.168.4.5", l));
	REQUIRE(httpd::addressInAnyPrefix("fd00::1", l));
	REQUIRE_FALSE(httpd::addressInAnyPrefix("8.8.8.8", l));
}

TEST_CASE("both halves of loopback are inside the seeded list", "[net]")
{
	/* A host that has both address families resolves its own name to ::1
	   before 127.0.0.1, so a list that seeds only the v4 half judges a browser
	   or a script running on the box itself as coming from somewhere else. It
	   is the one caller the exemption exists for. */
	const std::vector<httpd::NetPrefix> l = httpd::defaultLanPrefixes();

	REQUIRE(httpd::addressInAnyPrefix("127.0.0.1", l));
	REQUIRE(httpd::addressInAnyPrefix("::1", l));
	REQUIRE(httpd::addressInAnyPrefix("::ffff:127.0.0.1", l));
	REQUIRE(httpd::addressInAnyPrefix("0000:0000:0000:0000:0000:0000:0000:0001", l));

	// and it is one address and not a network around it
	REQUIRE_FALSE(httpd::addressInAnyPrefix("::2", l));
	REQUIRE_FALSE(httpd::addressInAnyPrefix("::", l));
	REQUIRE_FALSE(httpd::addressInAnyPrefix("2001:db8::1", l));
}

TEST_CASE("the seeded list is exactly the six networks it names", "[net]")
{
	const std::vector<httpd::NetPrefix> l = httpd::defaultLanPrefixes();
	REQUIRE(l.size() == 6);

	const char *const want[] =
	{
		"127.0.0.0/8",
		"::1/128",
		"10.0.0.0/8",
		"172.16.0.0/12",
		"192.168.0.0/16",
		"fc00::/7"
	};

	for (size_t i = 0; i < l.size(); ++i)
	{
		INFO("entry " << i);
		const httpd::NetPrefix p = mustParse(want[i]);
		REQUIRE(l[i].family == p.family);
		REQUIRE(l[i].len == p.len);
		for (size_t b = 0; b < sizeof(p.bits); ++b)
			REQUIRE(l[i].bits[b] == p.bits[b]);
	}

	// the edges of each of them, which is where a list transcribed by hand
	// tends to be one network wider or narrower than it says
	REQUIRE(httpd::addressInAnyPrefix("127.255.255.255", l));
	REQUIRE_FALSE(httpd::addressInAnyPrefix("126.255.255.255", l));
	REQUIRE_FALSE(httpd::addressInAnyPrefix("128.0.0.0", l));
	REQUIRE(httpd::addressInAnyPrefix("10.255.255.255", l));
	REQUIRE_FALSE(httpd::addressInAnyPrefix("11.0.0.0", l));
	REQUIRE_FALSE(httpd::addressInAnyPrefix("172.15.255.255", l));
	REQUIRE(httpd::addressInAnyPrefix("172.31.255.255", l));
	REQUIRE(httpd::addressInAnyPrefix("192.168.255.255", l));
	REQUIRE_FALSE(httpd::addressInAnyPrefix("192.169.0.0", l));
	REQUIRE_FALSE(httpd::addressInAnyPrefix("192.167.255.255", l));
	REQUIRE(httpd::addressInAnyPrefix("fdff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", l));
	REQUIRE_FALSE(httpd::addressInAnyPrefix("fe80::1", l));
	REQUIRE_FALSE(httpd::addressInAnyPrefix("2001:db8::1", l));
	REQUIRE(httpd::addressInAnyPrefix("::1", l));
	REQUIRE_FALSE(httpd::addressInAnyPrefix("::2", l));
	REQUIRE_FALSE(httpd::addressInAnyPrefix("fbff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", l));
	REQUIRE_FALSE(httpd::addressInAnyPrefix("fe00::", l));
}

TEST_CASE("a list nobody filled in permits nothing", "[net]")
{
	/* The list level of the same rule the empty entry is refused by: a check
	   written the other way round, where an empty list means no restriction,
	   turns a box with nothing configured into a box open to everything. */
	const std::vector<httpd::NetPrefix> none;
	REQUIRE_FALSE(httpd::addressInAnyPrefix("127.0.0.1", none));
	REQUIRE_FALSE(httpd::addressInAnyPrefix("10.0.0.1", none));
	REQUIRE_FALSE(httpd::addressInAnyPrefix("", none));

	// and one entry answers for itself only
	std::vector<httpd::NetPrefix> one;
	one.push_back(mustParse("10.0.0.0/8"));
	REQUIRE(httpd::addressInAnyPrefix("10.0.0.1", one));
	REQUIRE_FALSE(httpd::addressInAnyPrefix("127.0.0.1", one));
}
