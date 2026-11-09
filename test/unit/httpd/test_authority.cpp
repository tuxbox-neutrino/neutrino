/*
 * test_authority.cpp - the names a request may reach this box under
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include "support/catch.hpp"

#include "httpd/authority.h"
#include "httpd/webconfig.h"
#include "support/wireserver.h"

#include <cstdio>
#include <string>
#include <vector>

#include <unistd.h>

using namespace httpd;

namespace
{

BoxNames h7()
{
	BoxNames n;
	n.host_name = "h7";
	n.suffixes.push_back("fritz.box");
	n.suffixes.push_back("lan");
	n.extra.push_back("tv.example.org");
	return n;
}

void writeText(const std::string &path, const char *text)
{
	std::FILE *f = std::fopen(path.c_str(), "w");
	REQUIRE(f != NULL);
	std::fputs(text, f);
	REQUIRE(std::fclose(f) == 0);
}

/* The name sources, their clock and the configuration in effect, put back
   however the case ends, with the files the case wrote. */
struct NameSources
{
	const WebConfig          before;
	std::vector<std::string> files;

	NameSources() : before(config()) {}

	~NameSources()
	{
		setNameClockForTest(0);
		setNameSourcesForTest("", "");
		setConfigForTest(before);
		for (size_t i = 0; i < files.size(); ++i)
			::unlink(files[i].c_str());
	}

	std::string file(const char *what, const char *text)
	{
		char name[96];
		std::snprintf(name, sizeof(name), "/tmp/authority-%s-%d", what, (int) getpid());
		writeText(name, text);
		files.push_back(name);
		return name;
	}
};

} // namespace

TEST_CASE("the boundary takes no Host, an address, a name of the home network and a listed name", "[authority]")
{
	const BoxNames names = h7();
	const char *const taken[] = {
		"", "192.168.1.5", "192.168.1.5:80", "192.168.1.5:8080", "10.0.0.1:65535",
		"[::1]", "[fe80::1]:80", "[2001:db8::5]", "[::ffff:192.168.1.5]:80",
		"localhost", "LOCALHOST:8080", "localhost.",
		"h7", "H7:80", "h7.local", "h7.local.", "h7.fritz.box", "H7.Fritz.Box.:80", "h7.lan",
		// whatever the router or another device calls the box
		"wohnzimmer", "hd51-2", "hd51-2.local", "wohnzimmer.fritz.box", "Wohnzimmer.Fritz.Box.:80",
		"hd51-2.lan",
		"tv.example.org", "TV.Example.ORG:443",
	};
	for (size_t i = 0; i < sizeof(taken) / sizeof(taken[0]); ++i)
	{
		INFO(taken[i]);
		CHECK(authorityAccepted(taken[i], names));
	}
}

TEST_CASE("the boundary refuses every other name and whatever is not a name or an address", "[authority]")
{
	const BoxNames names = h7();
	const char *const refused[] = {
		// another site's name, the box's label under another domain, a domain alone, two labels
		// under a zone of the home network
		"evil.example", "h7.evil.example", "fritz.box", "x.h7.fritz.box", "a.b.lan", "x.h7.local",
		"h7.local.evil.example", "localhost.evil.example", "tv.example.org.evil.example",
		"192.168.1.5.evil.example", "evil.example:80", "local.evil.example",
		// an IPv6 address unbracketed, half bracketed, with a zone, with a bad port
		"::1", "fe80::1", "[::1", "[::1]x", "[::1]:", "[::1]:99999", "[fe80::1%25eth0]", "[h7]",
		// ports that are none
		"192.168.1.5:", "192.168.1.5:65536", "192.168.1.5:80:80", "192.168.1.5:abc", "h7:80x",
		"h7:-1", "h7:123456",
		// not a name
		"exa mple", "-h7", "h7-", "h7_", "h7..lan", ".h7", ".", "h7/x", "h7@evil.example",
	};
	for (size_t i = 0; i < sizeof(refused) / sizeof(refused[0]); ++i)
	{
		INFO(refused[i]);
		CHECK_FALSE(authorityAccepted(refused[i], names));
	}

	// A zero byte ends what the system reads, not what the header holds.
	CHECK_FALSE(authorityAccepted(std::string("192.168.1.5\0.evil.example", 25), names));
}

TEST_CASE("a host name that carries a domain the resolver does not search counts whole", "[authority]")
{
	BoxNames n;
	n.host_name = "h7.home.arpa";
	CHECK(authorityAccepted("h7.home.arpa", n));
	CHECK_FALSE(authorityAccepted("tv.home.arpa", n));
	CHECK_FALSE(authorityAccepted("h7.other.example", n));
	CHECK_FALSE(authorityAccepted("home.arpa", n));

	// Without names: what needs nothing read, and nothing under a domain.
	const BoxNames none;
	CHECK(authorityAccepted("192.168.1.5", none));
	CHECK(authorityAccepted("localhost", none));
	CHECK(authorityAccepted("h7", none));
	CHECK(authorityAccepted("h7.local", none));
	CHECK_FALSE(authorityAccepted("h7.fritz.box", none));
}

TEST_CASE("the resolver's domains are read under search and domain, each once", "[authority]")
{
	const std::vector<std::string> got = resolverSuffixes(
		"# Generated\n"
		"nameserver 192.168.1.1\n"
		"search Fritz.Box lan.\n"
		"# search evil.example\n"
		"; domain evil2.example\n"
		"searchx evil3.example\n"
		"domain fritz.box\n"
		"  search\tlocal.example not_a_name # evil4.example\n"
		"options ndots:1\n");
	REQUIRE(got.size() == 3u);
	CHECK(got[0] == "fritz.box");
	CHECK(got[1] == "lan");
	CHECK(got[2] == "local.example");
	CHECK(resolverSuffixes("").empty());
}

TEST_CASE("a name for allowed_hosts is labels of letters, digits and hyphens", "[authority]")
{
	CHECK(isHostName("dyn.example.net"));
	CHECK(isHostName("DYN-1.example.net."));
	CHECK(isHostName("h7"));
	CHECK_FALSE(isHostName(""));
	CHECK_FALSE(isHostName("dyn.example.net:8443"));
	CHECK_FALSE(isHostName("*.example.net"));
	CHECK_FALSE(isHostName("dyn..example.net"));
	CHECK_FALSE(isHostName(std::string(64, 'a') + ".example"));
	CHECK(isHostName(std::string(63, 'a') + ".example"));
	CHECK(comparableName("Dyn.Example.NET.") == "dyn.example.net");
}

TEST_CASE("the box's names are read where the system keeps them, with the access file's list", "[authority]")
{
	NameSources sources;
	const std::string path = sources.file("resolv", "search home.example\n");
	setNameSourcesForTest("Box7", path);

	const BoxNames n = boxNames();
	CHECK(n.host_name == "box7");
	REQUIRE(n.suffixes.size() == 1u);
	CHECK(n.suffixes[0] == "home.example");
	CHECK(hostAccepted("box7.home.example"));
	CHECK(hostAccepted("tv.home.example"));
	CHECK(hostAccepted("BOX7.local:80"));
	CHECK(hostAccepted("192.168.1.5"));
	CHECK(hostAccepted(""));
	CHECK_FALSE(hostAccepted("box7.other.example"));
	CHECK_FALSE(hostAccepted("dyn.example.net"));

	// A name the owner listed counts at once, under any port.
	WebConfig c = defaultWebConfig();
	c.allowed_hosts.push_back("dyn.example.net");
	setConfigForTest(c);
	CHECK(hostAccepted("dyn.example.net:8443"));
	CHECK(hostAccepted("Dyn.Example.Net."));

	// The domain a new lease brings, once the sources are read again.
	writeText(path, "domain office.example\n");
	setNameSourcesForTest("Box7", path);
	CHECK(hostAccepted("box7.office.example"));
	CHECK_FALSE(hostAccepted("box7.home.example"));

	// A system name that is no name names nothing.
	setNameSourcesForTest("(none)", path);
	CHECK(boxNames().host_name.empty());
}

TEST_CASE("the access file's names and the resolver's domains are looked at again every 10 seconds", "[authority]")
{
	NameSources sources;
	const std::string resolv = sources.file("resolv", "search home.example\n");
	const std::string access = sources.file("access", "allowed_hosts=dyn.example.net\n");
	REQUIRE(load(access));
	setNameSourcesForTest("box7", resolv);
	setNameClockForTest(1000);

	CHECK(hostAccepted("dyn.example.net"));
	CHECK(hostAccepted("tv.home.example"));
	CHECK_FALSE(hostAccepted("proxy.example.org"));

	// The owner adds a name by hand and a lease brings another domain: nothing
	// changes inside the interval, everything at its end, without a load.
	writeText(access, "allowed_hosts=dyn.example.net, proxy.example.org\n");
	writeText(resolv, "search office.example\n");
	setNameClockForTest(1009);
	CHECK_FALSE(hostAccepted("proxy.example.org"));
	CHECK_FALSE(hostAccepted("tv.office.example"));
	setNameClockForTest(1010);
	CHECK(hostAccepted("proxy.example.org"));
	CHECK(hostAccepted("tv.office.example"));
	CHECK_FALSE(hostAccepted("tv.home.example"));
	REQUIRE(boxNames().extra.size() == 2u);

	// A name taken off goes the same way; a file that cannot be read keeps what was read.
	writeText(access, "allowed_hosts=proxy.example.org\n");
	setNameClockForTest(1020);
	CHECK_FALSE(hostAccepted("dyn.example.net"));
	CHECK(hostAccepted("proxy.example.org"));
	::unlink(access.c_str());
	setNameClockForTest(1030);
	CHECK(hostAccepted("proxy.example.org"));
}

TEST_CASE("a refused name is told how to reach the box and where a name is added", "[authority]")
{
	NameSources sources;
	setConfigForTest(defaultWebConfig());
	const char lan[] = "192.168.1.20";

	const std::string page = unknownHostPage("192.168.1.5:80", lan, false);
	CHECK(page.find("http://192.168.1.5/") != std::string::npos);
	CHECK(page.find("http://192.168.1.5/", page.find("http://192.168.1.5/") + 1) != std::string::npos);
	CHECK(page.find("allowed_hosts") != std::string::npos);
	CHECK(page.find("nhttpd-access.conf in the configuration folder of the box") != std::string::npos);
	CHECK(page.find("nhttpd-access.conf im Konfigurationsordner der Box") != std::string::npos);
	CHECK(page.find("This box does not answer to this name.") != std::string::npos);
	CHECK(page.find("Diese Box antwortet nicht auf diesen Namen.") != std::string::npos);

	CHECK(unknownHostPage("192.168.1.5:8080", lan, false).find("http://192.168.1.5:8080/") != std::string::npos);
	// Under the scheme the server speaks, its own port left out.
	CHECK(unknownHostPage("192.168.1.5:443", lan, true).find("https://192.168.1.5/") != std::string::npos);
	CHECK(unknownHostPage("192.168.1.5:8443", lan, true).find("https://192.168.1.5:8443/") != std::string::npos);
	// The loopback and nothing at all name no address a person can open.
	CHECK(unknownHostPage("127.0.0.1:80", lan, false).find("http://") == std::string::npos);
	CHECK(unknownHostPage("", lan, false).find("http://") == std::string::npos);
	CHECK(unknownHostPage("", lan, false).find("allowed_hosts") != std::string::npos);

	// The box's address goes only to a peer on the home network with no proxy in
	// front: through a forwarded port or a proxy the peer can be anybody.
	CHECK(unknownHostPage("192.168.1.5:80", "203.0.113.9", false).find("192.168.1.5") == std::string::npos);
	CHECK(unknownHostPage("192.168.1.5:80", "127.0.0.1", false).find("http://192.168.1.5/") != std::string::npos);
	WebConfig proxied = defaultWebConfig();
	proxied.trusted_proxies_named = true;
	setConfigForTest(proxied);
	CHECK(unknownHostPage("192.168.1.5:80", lan, false).find("192.168.1.5") == std::string::npos);

	// The file the box reads, where it was told one.
	const std::string access = sources.file("page-access", "allowed_hosts=\n");
	REQUIRE(load(access));
	CHECK(unknownHostPage("192.168.1.5:80", lan, false).find(access) != std::string::npos);

	CHECK(std::string(unknownHostDetail()).find("allowed_hosts") != std::string::npos);
}

TEST_CASE("a Host line twice on the wire is refused, the same name or not, in any case", "[authority][bridge]")
{
	/* Read off a socket by nhttpd's own reader, which keeps the header in a map: two
	   lines of one spelling were one entry there, the last one, and the boundary saw a
	   single name. An API target, a page and a form alike. */
	NameSources names;
	WireServer serving(1, 5000, WireServer::Serial);

	{
		WireStream once(serving.port, "GET /api/v1/session HTTP/1.0\r\nHost: 127.0.0.1\r\n\r\n");
		REQUIRE(once.ok());
		CHECK(once.code() == 200);
	}

	const char *const twice[] = {
		"GET /api/v1/session HTTP/1.0\r\nHost: 127.0.0.1\r\nHost: 127.0.0.1\r\n\r\n",
		"GET /api/v1/session HTTP/1.0\r\nHost: evil.example\r\nHost: 127.0.0.1\r\n\r\n",
		"GET /api/v1/session HTTP/1.0\r\nHost: 127.0.0.1\r\nHost: evil.example\r\n\r\n",
		"GET /api/v1/session HTTP/1.0\r\nHost: 127.0.0.1\r\nhost: 127.0.0.1\r\n\r\n",
		"GET /api/v1/session HTTP/1.0\r\nHOST: 127.0.0.1\r\nHost: 127.0.0.1\r\nhost: 127.0.0.1\r\n\r\n",
		"GET /control/info HTTP/1.0\r\nHost: 127.0.0.1\r\nHost: 127.0.0.1\r\n\r\n",
		"POST /control/info HTTP/1.0\r\nHost: 127.0.0.1\r\nHost: 127.0.0.1\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\nContent-Length: 3\r\n\r\na=b",
	};
	for (size_t i = 0; i < sizeof(twice) / sizeof(twice[0]); ++i)
	{
		INFO(twice[i]);
		WireStream s(serving.port, twice[i]);
		REQUIRE(s.ok());
		CHECK(s.code() == 400);
		CHECK(s.readToEof(2000).find("allowed_hosts") != std::string::npos);
	}
}
