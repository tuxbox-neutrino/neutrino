/*
 * test_webconfig.cpp - tests for the webserver configuration
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
#include "support/fakes.h"

#include <config.h>

#include "httpd/webconfig.h"
#include "httpd/auth.h"
#include "httpd/credentials.h"
#include "httpd/netmatch.h"
#include "httpd/server.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <csignal>
#include <cstring>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace
{

/* A path nothing else in the suite writes, removed when the case ends whether it
   passed or not. Every case makes its own rather than reading what the case above it
   left behind: cases that share a file pass only in the order they were written, and
   the harness here takes a selection. */
class Temp
{
	public:
		explicit Temp(const char *what)
		{
			char buf[128];
			static int counter = 0;
			std::snprintf(buf, sizeof(buf), "/tmp/ni-webconf-%s-%d-%d", what, (int) getpid(), ++counter);
			path_ = buf;
			removeIt();
		}

		~Temp() { removeIt(); }

		const std::string &path() const { return path_; }
		const char *c_str() const { return path_.c_str(); }

	private:
		void removeIt()
		{
			::unlink(path_.c_str());
			::rmdir(path_.c_str());
		}

		std::string path_;
};

void writeFile(const std::string &path, const std::string &text)
{
	std::ofstream f(path.c_str(), std::ios::out | std::ios::trunc);
	REQUIRE(f.good());
	f << text;
	f.close();
}

std::string readFile(const std::string &path)
{
	std::ifstream f(path.c_str());
	std::ostringstream out;
	out << f.rdbuf();
	return out.str();
}

// What the shipped old file says, in the four lines this reads and with the
// switch that turned authentication off.
const char kShippedOldFile[] =
	"WebsiteMain.port=80\n"
	"WebsiteMain.host=0.0.0.0\n"
	"mod_auth.authenticate=false\n"
	"mod_auth.no_auth_client=\n"
	"mod_auth.password=ni\n"
	"mod_auth.username=root\n"
	"server.chroot=\n"
	"mod_cache.cache_directory=/tmp/.cache\n"
	"server.no_keep-alive_ips=\n"
	"webserver.websites=WebsiteMain\n";

/* The lines of a written file the parser reads as settings, which is every
   line carrying the separator. No comment this program writes carries one, so
   what this returns is everything the file actually says. */
std::vector<std::string> settingLines(const std::string &text)
{
	std::vector<std::string> out;
	std::istringstream lines(text);
	std::string line;

	while (std::getline(lines, line))
	{
		if (line.find('=') != std::string::npos)
			out.push_back(line);
	}
	return out;
}

/* The comments of a written file as one flowed line with its runs of space
   collapsed, which is how somebody reads them rather than how they are stored.
   A sentence in that file is broken wherever the width ran out, so looking for
   one in the bytes finds it only where the break happened to fall
   elsewhere. */
std::string prose(const std::string &text)
{
	std::string out;
	std::istringstream lines(text);
	std::string line;

	while (std::getline(lines, line))
	{
		if (line.empty() || line[0] != '#')
			continue;
		out += ' ';
		out += line.substr(1);
	}

	std::string flat;
	for (size_t i = 0; i < out.size(); ++i)
	{
		const bool space = (out[i] == ' ' || out[i] == '\t');
		if (space && !flat.empty() && flat[flat.size() - 1] == ' ')
			continue;
		flat += space ? ' ' : out[i];
	}
	return flat;
}

/* Whether a file says something under a key, as against merely naming the word
   somewhere. The written file documents every key it does not write, so the
   question "is this word in the file" now answers yes for keys the file was
   careful not to set, and the question worth asking is about the lines that
   take effect. */
bool anySettingMentions(const std::string &text, const std::string &needle)
{
	const std::vector<std::string> lines = settingLines(text);
	for (size_t i = 0; i < lines.size(); ++i)
	{
		if (lines[i].find(needle) != std::string::npos)
			return true;
	}
	return false;
}

bool anyProblemMentions(const std::string &needle)
{
	const std::vector<std::string> &p = httpd::configProblems();
	for (size_t i = 0; i < p.size(); ++i)
	{
		if (p[i].find(needle) != std::string::npos)
			return true;
	}
	return false;
}

mode_t modeOf(const std::string &path)
{
	struct stat st;
	REQUIRE(stat(path.c_str(), &st) == 0);
	return (mode_t)(st.st_mode & 07777);
}

/* What every case here puts back after it, and what every case here needs in front of
   it. The sources are installed because start() refuses outright while they are not,
   and it refuses before it looks at the address it was given. Six cases in this file
   ask the binder to refuse an address and two of them have nothing else standing
   between the answer and the assertion, so without this they were answered by the
   wrong refusal: the zero byte guard in the server could be deleted whole and every
   one of them stayed green. */
struct Restore
{
	InstalledDependencies wired;

	~Restore() { httpd::setConfigForTest(httpd::defaultWebConfig()); }
};

} // namespace

TEST_CASE("a missing file leaves what was in effect standing", "[webconf]")
{
	Restore back;

	httpd::WebConfig marker = httpd::defaultWebConfig();
	marker.username = "someone-else";
	httpd::setConfigForTest(marker);

	REQUIRE_FALSE(httpd::load("/tmp/there-is-no-such-file-4711"));

	// Not the defaults: what was there before. A reload of a file somebody
	// deleted must not empty the policy.
	REQUIRE(httpd::config().username == "someone-else");
	REQUIRE(anyProblemMentions("cannot read"));
}

TEST_CASE("the values a box that was never configured runs on", "[webconf]")
{
	const httpd::WebConfig c = httpd::defaultWebConfig();

	REQUIRE(c.server.port == 80);
	REQUIRE(c.server.bind_address == "0.0.0.0");
	REQUIRE(c.username == "root");
	REQUIRE(c.password_hash.empty());
	REQUIRE(c.session_lifetime_s == 86400);
	REQUIRE(c.sse_max_streams == 8);
	REQUIRE(c.legacy_enabled);
	REQUIRE(c.docroot == std::string(PRIVATE_HTTPDDIR) + "/ni-web");
	REQUIRE(c.trusted_proxies.empty());

	// Six and not five. Loopback is two addresses and a browser running on the
	// box resolves the name of the box to the second of them first, so a list
	// seeded with only the first refuses the one caller it exists for.
	REQUIRE(c.lan_read.size() == 6);
	REQUIRE(httpd::addressInAnyPrefix("127.0.0.1", c.lan_read));
	REQUIRE(httpd::addressInAnyPrefix("::1", c.lan_read));
	REQUIRE(httpd::addressInAnyPrefix("192.168.1.1", c.lan_read));
	REQUIRE(httpd::addressInAnyPrefix("10.1.2.3", c.lan_read));
	REQUIRE(httpd::addressInAnyPrefix("172.16.0.1", c.lan_read));
	REQUIRE(httpd::addressInAnyPrefix("fd00::1", c.lan_read));
	REQUIRE_FALSE(httpd::addressInAnyPrefix("8.8.8.8", c.lan_read));

	// An empty password verifies against nothing, so a box nobody gave one to
	// refuses every request needing one rather than accepting them all.
	REQUIRE_FALSE(httpd::verifySecret("", c.password_hash));
	REQUIRE_FALSE(httpd::verifySecret("ni", c.password_hash));
}

TEST_CASE("the listening half is the one the server is started from", "[webconf]")
{
	/* One structure describes the socket. Two of them is how a file saying one
	   port ends up in front of a daemon started from a default saying another,
	   and this whole file exists because one particular port must not be
	   carried. */
	const httpd::WebConfig c = httpd::defaultWebConfig();
	REQUIRE(c.server.port == httpd::defaultConfig().port);
	REQUIRE(c.server.bind_address == httpd::defaultConfig().bind_address);
	REQUIRE(c.server.max_body_bytes == httpd::defaultConfig().max_body_bytes);
}

TEST_CASE("the port a box nobody ever configured answers on", "[webconf]")
{
	/* Eighty, and the number matters rather than being one free port among many: it is
	   where the old server answered, so it is what every bookmark, every plugin and
	   every panel somebody wired up has written down, and none of them is asked before
	   an upgrade. Held against both, because a box reaches this through
	   defaultWebConfig and the daemon is started from the ServerConfig inside it. */
	Restore back;

	REQUIRE(httpd::defaultConfig().port == 80);
	REQUIRE(httpd::defaultWebConfig().server.port == 80);

	/* And it is a default and not a value the load happens to leave standing:
	   a file naming other keys and no port lands on it too. The port stays an
	   ordinary key, so the line under this is what a box that names one gets,
	   and nothing here nails the number down for a box that chose. */
	Temp conf("conf");
	writeFile(conf.path(), "username=admin\n");
	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::config().server.port == 80);

	writeFile(conf.path(), "port=8080\n");
	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::config().server.port == 8080);
}

TEST_CASE("every start that finds no configuration leaves one behind", "[webconf]")
{
	/* The three states a box can be in, in the order it passes through them. That
	   decision is what stands between an upgraded box carrying its credential over and
	   one answering nothing that needs a password. Every state is checked by what is at
	   the name afterwards as well as by the answer, so a call that answered correctly
	   and wrote anyway, or answered correctly and wrote nothing, is caught. */
	Restore back;
	Temp old("old"), out("out");

	/* Nothing at either name. A box that never had the old server has no
	   credential to carry and used to get no file, which left its owner with a
	   web interface that refuses everything that changes anything and says so
	   nowhere. It gets the login the image ships with instead. */
	struct stat st;
	REQUIRE(::lstat(out.c_str(), &st) != 0);
	REQUIRE_FALSE(anyProblemMentions("old configuration"));
	REQUIRE(httpd::ensureConfigFile(old.path(), out.path()));
	REQUIRE(::lstat(out.c_str(), &st) == 0);
	REQUIRE(modeOf(out.path()) == 0600);
	REQUIRE(httpd::load(out.path()));
	REQUIRE(httpd::config().username == "root");
	REQUIRE(httpd::verifySecret("ni", httpd::config().password_hash));

	/* Nothing is said about a migration that never happened: there was no old
	   file, so there is no refusal to report and no line to carry into the
	   written one. */
	REQUIRE_FALSE(anyProblemMentions("old configuration"));

	/* Every start after that one. What is at the name is left exactly as it is and
	   nothing is said: an attempt refused and reported on every boot for the rest of a
	   box's life is the right outcome told wrongly. The read above put the list in a
	   state that says nothing of the kind, which is what lets the read below mean
	   something. */
	writeFile(old.path(), kShippedOldFile);
	const std::string before = readFile(out.path());
	REQUIRE_FALSE(anyProblemMentions("cannot create"));
	REQUIRE_FALSE(httpd::ensureConfigFile(old.path(), out.path()));
	REQUIRE(readFile(out.path()) == before);
	REQUIRE_FALSE(anyProblemMentions("cannot create"));

	/* A name taken by something that is not a file at all counts as taken, so
	   that a link pointing nowhere is neither followed nor written through nor
	   complained about once a boot. That is the one state this cannot write a
	   box out of, and it is deliberate: a name somebody else can create is not
	   a name a credential is written through. */
	Temp link("link");
	REQUIRE(::symlink("/tmp/there-is-no-such-target-4713", link.c_str()) == 0);
	REQUIRE(httpd::load(out.path()));
	REQUIRE_FALSE(anyProblemMentions("cannot create"));
	REQUIRE_FALSE(httpd::ensureConfigFile(old.path(), link.path()));
	REQUIRE_FALSE(anyProblemMentions("cannot create"));
	REQUIRE(::lstat(link.c_str(), &st) == 0);
	REQUIRE(S_ISLNK(st.st_mode));
	REQUIRE(::stat(link.c_str(), &st) != 0);
}

TEST_CASE("the old file of a box that was locked out is what it logs in with", "[webconf]")
{
	/* The shape two owners reported: a name and a password in the old file, no
	   file of this server's, and a box that answered nothing they could change.
	   What they typed is what it answers to afterwards. */
	Restore back;
	Temp old("old"), out("out");

	writeFile(old.path(), "mod_auth.username=root\nmod_auth.password=ni\n");

	REQUIRE(httpd::ensureConfigFile(old.path(), out.path()));
	REQUIRE(httpd::load(out.path()));
	REQUIRE(httpd::config().username == "root");
	REQUIRE(httpd::verifySecret("ni", httpd::config().password_hash));

	// And the file says where that login came from, so that the one line about
	// it is not only in a boot log.
	const std::string said = prose(readFile(out.path()));
	REQUIRE(said.find(old.path()) != std::string::npos);
	REQUIRE(said.find("LOGIN: user \"root\", with the password the old web interface used") !=
		std::string::npos);
}

TEST_CASE("a migration that is refused still leaves a login, and the file says why", "[webconf]")
{
	/* The other half of the report. Every refusal in the call above used to end
	   at a box with no file: the owner is locked out, and the reason went to a
	   standard error nobody was reading. Now the reason is in the file, at the
	   name the owner opens, and under a login they can use. */
	Restore back;
	Temp old("old"), out("out");

	writeFile(old.path(),
		  "WebsiteMain.host=www.example.invalid\n"
		  "mod_auth.username=root\n"
		  "mod_auth.password=zwiebelfisch\n");

	REQUIRE(httpd::ensureConfigFile(old.path(), out.path()));
	REQUIRE(modeOf(out.path()) == 0600);

	const std::string written = readFile(out.path());
	const std::string said = prose(written);
	REQUIRE(said.find("WebsiteMain.host") != std::string::npos);
	REQUIRE(said.find("is not an address") != std::string::npos);
	REQUIRE(said.find(old.path()) != std::string::npos);

	/* The password that was not carried is not in the file either. A refusal
	   that wrote the reason and the value it refused would publish a
	   credential in a comment. */
	REQUIRE(written.find("zwiebelfisch") == std::string::npos);

	REQUIRE(httpd::load(out.path()));
	REQUIRE(httpd::config().username == "root");
	REQUIRE(httpd::verifySecret("ni", httpd::config().password_hash));
	REQUIRE_FALSE(httpd::verifySecret("zwiebelfisch", httpd::config().password_hash));

	// Whatever the written file says, nothing in it reads back as a setting
	// this did not write, the reason it carries having come out of the old
	// file rather than out of this program.
	REQUIRE(httpd::configProblems().empty());
}

TEST_CASE("a name that cannot be written leaves nothing behind and the next start tries again", "[webconf]")
{
	/* The one failure this cannot write a box out of, and the reason the attempt
	   stays on every start rather than being made once: with nowhere to write, a
	   box still has no file, and the start after it is all that stands between
	   that box and never having one.

	   The directory is missing rather than write protected, because a case run as
	   root goes straight through a mode that refuses everybody else and would be
	   green for the wrong reason. */
	Restore back;
	Temp old("old"), dir("dir");

	const std::string out = dir.path() + "/ni-web.conf";
	struct stat st;

	REQUIRE_FALSE(httpd::ensureConfigFile(old.path(), out));
	REQUIRE(::lstat(out.c_str(), &st) != 0);
	REQUIRE(anyProblemMentions("cannot create"));

	REQUIRE(::mkdir(dir.c_str(), 0755) == 0);

	if (::geteuid() != 0)
	{
		REQUIRE(::chmod(dir.c_str(), 0555) == 0);
		REQUIRE_FALSE(httpd::ensureConfigFile(old.path(), out));
		REQUIRE(::lstat(out.c_str(), &st) != 0);
		REQUIRE(::chmod(dir.c_str(), 0755) == 0);
	}

	// The next start, with somewhere to write it.
	REQUIRE(httpd::ensureConfigFile(old.path(), out));
	REQUIRE(::lstat(out.c_str(), &st) == 0);
	REQUIRE(httpd::load(out));
	REQUIRE(httpd::verifySecret("ni", httpd::config().password_hash));

	::unlink(out.c_str());
	::rmdir(dir.c_str());
}

TEST_CASE("the written file names every key it does not write and none of them takes effect", "[webconf]")
{
	/* What is not in a file is what nobody knows about, and a box following a
	   default it cannot be told by reading its own configuration is how somebody
	   comes to look at an almost empty file and learn nothing.

	   Written out as lines they would freeze the defaults of the day, and three of
	   them worse than that: the page directory is where the image puts its pages,
	   the network list is the policy for who may read without a password, and a
	   proxy list that is merely present turns that reading off. So they stand as
	   prose, with the value read out of the program at the moment of writing. */
	Restore back;
	Temp old("old"), out("out");

	REQUIRE(httpd::ensureConfigFile(old.path(), out.path()));
	const std::string written = readFile(out.path());
	const httpd::WebConfig d = httpd::defaultWebConfig();

	const std::vector<std::string> seed = httpd::defaultLanPrefixTexts();
	REQUIRE(seed.size() == d.lan_read.size());
	for (size_t i = 0; i < seed.size(); ++i)
	{
		INFO("seeded network " << seed[i]);
		REQUIRE(written.find(seed[i]) != std::string::npos);
	}

	REQUIRE(written.find(d.docroot) != std::string::npos);
	REQUIRE(written.find(std::to_string(d.session_lifetime_s)) != std::string::npos);
	REQUIRE(written.find(std::to_string(d.sse_max_streams)) != std::string::npos);

	static const char *const named[] =
	{
		"lan_read", "trusted_proxies", "docroot", "session_lifetime_s",
		"sse_max_streams", "legacy_enabled", "channel_logos", "api_tokens"
	};
	for (size_t i = 0; i < sizeof(named) / sizeof(named[0]); ++i)
	{
		INFO("key " << named[i]);
		REQUIRE(written.find(named[i]) != std::string::npos);
	}

	/* And every one of them inert. A key named on a line the parser reads
	   would be the default of today written into a box that will never see
	   tomorrow's, and a key named on a line it reads and cannot use would be a
	   complaint on every start for the life of the box. */
	REQUIRE(httpd::load(out.path()));
	REQUIRE(httpd::configProblems().empty());
	REQUIRE(httpd::config().lan_read.size() == d.lan_read.size());
	REQUIRE(httpd::config().trusted_proxies.empty());
	REQUIRE_FALSE(httpd::proxiesConfigured(httpd::config()));
	REQUIRE(httpd::config().docroot == d.docroot);
	REQUIRE(httpd::config().session_lifetime_s == d.session_lifetime_s);
	REQUIRE(httpd::config().sse_max_streams == d.sse_max_streams);
	REQUIRE(httpd::config().legacy_enabled == d.legacy_enabled);
	REQUIRE(httpd::config().channel_logos == d.channel_logos);
}

TEST_CASE("the first thing in a written file is the login", "[webconf]")
{
	/* Whoever opens this over a shell has just been refused one and is looking
	   for two sentences, not for a file format. So the login, where to change
	   it and how to get back to the shipped one stand in front of everything
	   else in the file, and the shipped password is named in words where it is
	   the one in force, a hash saying nothing to somebody locked out. */
	Restore back;
	Temp old("old"), out("out");

	REQUIRE(httpd::ensureConfigFile(old.path(), out.path()));
	const std::string written = readFile(out.path());
	const std::string said = prose(written);

	const size_t login = said.find("LOGIN: user \"root\", password \"ni\"");
	const size_t change = said.find("TO CHANGE IT:");
	const size_t over = said.find("TO START OVER:");
	const size_t syntax = said.find("HOW THIS FILE IS READ");
	const size_t keys = said.find("WHAT ELSE MAY BE WRITTEN HERE");

	REQUIRE(login != std::string::npos);
	REQUIRE(change != std::string::npos);
	REQUIRE(over != std::string::npos);
	REQUIRE(syntax != std::string::npos);
	REQUIRE(keys != std::string::npos);

	REQUIRE(login < change);
	REQUIRE(change < over);
	REQUIRE(over < syntax);
	REQUIRE(syntax < keys);

	/* Within the first few lines of the file itself and not merely somewhere
	   above the rest of the prose. */
	const size_t at = written.find("LOGIN: user");
	REQUIRE(at != std::string::npos);
	REQUIRE(std::count(written.begin(), written.begin() + (long) at, '\n') < 3);

	// And it says that the one it names is the shell login of the same box, so
	// that whoever changes it changes one thing and not two.
	REQUIRE(said.find("over the network for a shell") != std::string::npos);
}

TEST_CASE("the migration carries host and user and hashes the password", "[webconf]")
{
	Restore back;
	Temp old("old"), out("out");

	writeFile(old.path(), kShippedOldFile);
	REQUIRE(httpd::migrateFrom(old.path(), out.path()));
	REQUIRE(httpd::load(out.path()));

	REQUIRE(httpd::config().username == "root");
	REQUIRE(httpd::config().server.bind_address == "0.0.0.0");

	// The cleartext is gone and the hash answers for it.
	REQUIRE(httpd::config().password_hash.compare(0, 14, "pbkdf2-sha256$") == 0);
	REQUIRE(httpd::verifySecret("ni", httpd::config().password_hash));
	REQUIRE_FALSE(httpd::verifySecret("not-ni", httpd::config().password_hash));

	const std::string written = readFile(out.path());
	REQUIRE(written.find("=ni\n") == std::string::npos);
	// Under a key and not anywhere in the file: the written file quotes the
	// complaint list, which names the keys of the old file it did not carry.
	REQUIRE_FALSE(anySettingMentions(written, "mod_auth"));
}

TEST_CASE("the password is carried whatever the switch said", "[webconf]")
{
	/* Five shapes of that file, and the only one that writes no password is the one that
	   carried none.

	   The switch its own server checks passwords under is not read for this. That was tried
	   the other way round, on the reasoning that a value under a switch that is off was
	   never compared with anything, and it is wrong for these boxes: the same string is the
	   root login they take over the network, so carrying it opens nothing that is not open,
	   while refusing it leaves an upgraded box with a web interface nobody can write
	   through. */
	Restore back;

	struct Shape
	{
		const char *what;
		const char *file;
		const char *secret;
		bool        carried;
		bool        shipped;
	};

	static const Shape shapes[] =
	{
		{ "the file as it ships",
		  "mod_auth.username=root\nmod_auth.authenticate=false\nmod_auth.password=ni\n",
		  "ni", true, true },
		{ "checking on and the password every image carries",
		  "mod_auth.username=root\nmod_auth.authenticate=true\nmod_auth.password=ni\n",
		  "ni", true, true },
		{ "checking on and a password of somebody's own",
		  "mod_auth.username=root\nmod_auth.authenticate=true\nmod_auth.password=hunter2\n",
		  "hunter2", true, false },
		{ "checking off and a password of somebody's own",
		  "mod_auth.username=root\nmod_auth.authenticate=false\nmod_auth.password=hunter2\n",
		  "hunter2", true, false },
		{ "no password at all",
		  "mod_auth.username=root\nmod_auth.authenticate=true\nmod_auth.password=\n",
		  "", false, false }
	};

	for (size_t i = 0; i < sizeof(shapes) / sizeof(shapes[0]); ++i)
	{
		INFO(shapes[i].what);
		Temp old("old"), out("out");
		writeFile(old.path(), shapes[i].file);

		REQUIRE(httpd::migrateFrom(old.path(), out.path()));
		const std::string written = readFile(out.path());

		/* A password of two letters is a string the words of this file's own comment
		   contain, so what is looked for is a value and not a spelling. The shapes
		   carrying a password no comment here can hold are looked for whole. The shape
		   carrying none is asked neither question. */
		if (shapes[i].carried)
		{
			REQUIRE(written.find(std::string("=") + shapes[i].secret + "\n") == std::string::npos);
			if (!shapes[i].shipped)
				REQUIRE(written.find(shapes[i].secret) == std::string::npos);
		}

		/* The one line this round is about: what is carried is the value the
		   image ships with, said once so that an owner who changed the login
		   this box takes over the network and never opened that file learns
		   they have a web password they did not choose. Said for that value
		   and for no other, or it would be a line about every migration. */
		REQUIRE(anyProblemMentions("carried across as the password") == shapes[i].shipped);
		if (shapes[i].shipped)
			REQUIRE(anyProblemMentions("\"ni\""));

		REQUIRE(httpd::load(out.path()));
		REQUIRE(httpd::config().username == "root");
		REQUIRE(httpd::verifySecret(shapes[i].secret, httpd::config().password_hash) == shapes[i].carried);
		if (!shapes[i].carried)
		{
			REQUIRE(httpd::config().password_hash.empty());
			// And the file says so where somebody comes looking for it later.
			REQUIRE(written.find("no password") != std::string::npos);
		}
	}
}

TEST_CASE("no line of the written file reads back as the cleartext", "[webconf]")
{
	/* The check above looks for the one spelling a copied line would have. This
	   one looks for the password anywhere at all, so a migration that carried it
	   under another key or on a comment line is caught too. The password is a
	   string no part of the rest of the file can contain by accident. */
	Restore back;
	Temp old("old"), out("out");

	writeFile(old.path(),
		  "WebsiteMain.host=0.0.0.0\n"
		  "mod_auth.username=root\n"
		  "mod_auth.password=zwiebelfisch\n");

	REQUIRE(httpd::migrateFrom(old.path(), out.path()));
	REQUIRE(readFile(out.path()).find("zwiebelfisch") == std::string::npos);
	REQUIRE(httpd::load(out.path()));
	REQUIRE(httpd::verifySecret("zwiebelfisch", httpd::config().password_hash));
}

TEST_CASE("the switch that turned authentication off is not carried over", "[webconf]")
{
	// Reproducing it in any form preserves the one property this work exists to
	// remove, so the word itself must not appear.
	Restore back;
	Temp old("old"), out("out");

	writeFile(old.path(), kShippedOldFile);
	REQUIRE(httpd::migrateFrom(old.path(), out.path()));

	const std::string written = readFile(out.path());
	REQUIRE_FALSE(anySettingMentions(written, "authenticate"));
	REQUIRE_FALSE(anySettingMentions(written, "no_auth_client"));

	// Dropped with a word rather than in silence, and the word is in the file
	// and not only on a standard error that is gone by morning.
	REQUIRE(anyProblemMentions("mod_auth.authenticate"));
	REQUIRE(written.find("mod_auth.authenticate") != std::string::npos);
}

TEST_CASE("the migration takes the old port", "[webconf]")
{
	/* Nothing is listening on it once this runs, and it is the number whoever
	   upgrades already has in a bookmark. Written out rather than left to the
	   default that happens to agree, so that a box which was moved elsewhere is
	   carried by the same line as this one. */
	Restore back;
	Temp old("old"), out("out");

	writeFile(old.path(), kShippedOldFile);
	REQUIRE(httpd::migrateFrom(old.path(), out.path()));

	REQUIRE(readFile(out.path()).find("port=80\n") != std::string::npos);

	REQUIRE(httpd::load(out.path()));
	REQUIRE(httpd::config().server.port == 80);
}

TEST_CASE("a box that moved its old port keeps the port it moved to", "[webconf]")
{
	/* The case the default cannot answer. Whoever put the old server on
	   another number did it because eighty was taken or unreachable on their
	   network, and handing them eighty back is the one outcome that is wrong
	   for the box that said so out loud. */
	Restore back;
	Temp old("old"), out("out");

	writeFile(old.path(),
		  "WebsiteMain.port=8080\n"
		  "WebsiteMain.host=0.0.0.0\n"
		  "mod_auth.username=root\n"
		  "mod_auth.password=ni\n");

	REQUIRE(httpd::migrateFrom(old.path(), out.path()));
	REQUIRE(readFile(out.path()).find("port=8080\n") != std::string::npos);

	REQUIRE(httpd::load(out.path()));
	REQUIRE(httpd::config().server.port == 8080);
	REQUIRE(httpd::config().username == "root");
}

TEST_CASE("an old port that is not one leaves the default and says so", "[webconf]")
{
	/* The same answer the load gives such a line, and for the same reason: a
	   port is where the box answers and not who may reach it, so there is no
	   least port to fall back to and nothing is granted by refusing. Refusing
	   the whole migration over it would cost the credential beside it, which
	   is what this call exists to carry. */
	Restore back;

	const char *const bad[] = { "0", "65536", "eighty", "", "99999999999" };
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
	{
		Temp old("old"), out("out");
		writeFile(old.path(),
			  std::string("WebsiteMain.port=") + bad[i] + "\n" +
			  "mod_auth.username=root\n"
			  "mod_auth.password=ni\n");

		INFO("old port value " << bad[i]);
		REQUIRE(httpd::migrateFrom(old.path(), out.path()));
		REQUIRE(anyProblemMentions("WebsiteMain.port"));

		REQUIRE(httpd::load(out.path()));
		REQUIRE(httpd::config().server.port == httpd::defaultConfig().port);
		REQUIRE(httpd::verifySecret("ni", httpd::config().password_hash));
	}
}

TEST_CASE("an old port line cut short by a number sign is not half a port", "[webconf]")
{
	/* The parser keeps what came before the number sign, and what it keeps here
	   ends in a space, so the value is not a number and the default stands. A
	   reader that trimmed it would carry a port whose line nobody meant as
	   one. */
	Restore back;
	Temp old("old"), out("out");

	writeFile(old.path(),
		  "WebsiteMain.port=8080 # the one we chose\n"
		  "mod_auth.username=root\n"
		  "mod_auth.password=ni\n");

	REQUIRE(httpd::migrateFrom(old.path(), out.path()));
	REQUIRE(anyProblemMentions("WebsiteMain.port"));

	REQUIRE(httpd::load(out.path()));
	REQUIRE(httpd::config().server.port == httpd::defaultConfig().port);
}

TEST_CASE("the migration does not carry the old page directory", "[webconf]")
{
	// Those pages belong to the other server, and pointing this one at them
	// would serve the old site from the new port.
	Restore back;
	Temp old("old"), out("out");

	writeFile(old.path(),
		  "WebsiteMain.directory=/share/tuxbox/neutrino/httpd\n"
		  "WebsiteMain.override_directory=/var/httpd\n"
		  "mod_auth.username=root\n"
		  "mod_auth.password=ni\n");

	REQUIRE(httpd::migrateFrom(old.path(), out.path()));
	const std::string written = readFile(out.path());
	REQUIRE_FALSE(anySettingMentions(written, "docroot"));
	REQUIRE(written.find("/var/httpd") == std::string::npos);
	REQUIRE(httpd::load(out.path()));
	// Not carried and not left empty either: a key the migrated file does not
	// name falls back to this server's own default, which is its own docs
	// page and never the site the migration just refused to carry.
	REQUIRE(httpd::config().docroot == std::string(PRIVATE_HTTPDDIR) + "/ni-web");
}

TEST_CASE("the written file is readable by its owner alone", "[webconf]")
{
	Restore back;
	Temp old("old"), out("out");

	writeFile(old.path(), kShippedOldFile);
	REQUIRE(httpd::migrateFrom(old.path(), out.path()));
	REQUIRE(modeOf(out.path()) == 0600);
}

TEST_CASE("the mode is the one asked for and not one the umask happened to leave", "[webconf]")
{
	/* A creation that asks for a wide mode and relies on the process umask to
	   narrow it is a file that comes out wide wherever the umask is not the one
	   whoever wrote it was running under, and a daemon started from an init
	   script is exactly there. Clearing the umask is what tells the two
	   apart. */
	Restore back;
	Temp old("old"), out("out");

	writeFile(old.path(), kShippedOldFile);

	const mode_t saved = ::umask(0);
	const bool done = httpd::migrateFrom(old.path(), out.path());
	::umask(saved);

	REQUIRE(done);
	REQUIRE(modeOf(out.path()) == 0600);
}

TEST_CASE("an output that is already there is refused and left alone", "[webconf]")
{
	// What it would overwrite is the file holding the credential the box is
	// running on.
	Restore back;
	Temp old("old"), out("out");

	writeFile(old.path(), kShippedOldFile);
	writeFile(out.path(), "port=9999\n");

	REQUIRE_FALSE(httpd::migrateFrom(old.path(), out.path()));
	REQUIRE(readFile(out.path()) == "port=9999\n");
	REQUIRE(anyProblemMentions("cannot create"));
}

TEST_CASE("an output that is a link is refused and its target is not created", "[webconf]")
{
	/* Following one would write a credential through a name somebody else chose
	   the target of. The link is left dangling on purpose: that is the shape
	   that a create rather than an open has to refuse, because a link pointing
	   at nothing is the one an open would happily bring into being. */
	Restore back;
	Temp old("old"), link("link"), target("target");

	writeFile(old.path(), kShippedOldFile);
	REQUIRE(::symlink(target.c_str(), link.c_str()) == 0);

	REQUIRE_FALSE(httpd::migrateFrom(old.path(), link.path()));

	struct stat st;
	REQUIRE(stat(target.c_str(), &st) != 0);

	::unlink(link.c_str());
}

TEST_CASE("an output that cannot be created leaves nothing behind", "[webconf]")
{
	// The parent is a file rather than a directory, which fails the create for
	// a reason that does not depend on who is running the case.
	Restore back;
	Temp old("old"), blocker("blocker");

	writeFile(old.path(), kShippedOldFile);
	writeFile(blocker.path(), "not a directory\n");

	const std::string out = blocker.path() + "/ni-web.conf";
	REQUIRE_FALSE(httpd::migrateFrom(old.path(), out));

	struct stat st;
	REQUIRE(stat(out.c_str(), &st) != 0);
}

TEST_CASE("a source that cannot be read writes nothing", "[webconf]")
{
	Restore back;
	Temp out("out");

	REQUIRE_FALSE(httpd::migrateFrom("/tmp/there-is-no-such-file-4712", out.path()));

	struct stat st;
	REQUIRE(stat(out.c_str(), &st) != 0);
	REQUIRE(anyProblemMentions("cannot read"));
}

TEST_CASE("a directory in place of a file is not a file holding nothing", "[webconf]")
{
	/* A directory opens for reading and gives back no lines, so a load that
	   only asked whether the open succeeded would answer that it read a file
	   carrying no settings and install the defaults over whatever was in
	   effect. */
	Restore back;
	Temp dir("dir"), out("out");

	REQUIRE(::mkdir(dir.c_str(), 0755) == 0);

	REQUIRE_FALSE(httpd::load(dir.path()));
	REQUIRE(anyProblemMentions("not a regular file"));

	REQUIRE_FALSE(httpd::migrateFrom(dir.path(), out.path()));

	struct stat st;
	REQUIRE(stat(out.c_str(), &st) != 0);

	::rmdir(dir.c_str());
}

TEST_CASE("a password the old file does not carry is said out loud", "[webconf]")
{
	// The file is still written, so there is something to put a hash into, and
	// nothing verifies against what is in it meanwhile.
	Restore back;
	Temp old("old"), out("out");

	writeFile(old.path(), "WebsiteMain.host=0.0.0.0\nmod_auth.username=root\nmod_auth.password=\n");

	REQUIRE(httpd::migrateFrom(old.path(), out.path()));
	REQUIRE(anyProblemMentions("mod_auth.password"));
	REQUIRE(httpd::load(out.path()));
	REQUIRE(httpd::config().password_hash.empty());
	REQUIRE_FALSE(httpd::verifySecret("", httpd::config().password_hash));
	REQUIRE_FALSE(httpd::verifySecret("ni", httpd::config().password_hash));
}

TEST_CASE("a host that is not an address stops the migration", "[webconf]")
{
	/* Dropping it would fall back to the wildcard, which answers a line naming
	   one interface by listening on all of them. Nothing is written, and the
	   box keeps the server it already has. */
	Restore back;
	Temp old("old"), out("out");

	writeFile(old.path(), "WebsiteMain.host=www.example.invalid\nmod_auth.username=root\nmod_auth.password=ni\n");

	REQUIRE_FALSE(httpd::migrateFrom(old.path(), out.path()));

	struct stat st;
	REQUIRE(stat(out.c_str(), &st) != 0);
	REQUIRE(anyProblemMentions("WebsiteMain.host"));
}

TEST_CASE("a user name that would not read back stops the migration", "[webconf]")
{
	Restore back;
	Temp spaced("old"), spaced_out("out"), empty_old("old"), empty_out("out");

	// A space at the end is kept by the parser and shown by nothing, so two
	// names that differ look the same wherever anyone reads them.
	writeFile(spaced.path(), "mod_auth.username=root \nmod_auth.password=ni\n");
	REQUIRE_FALSE(httpd::migrateFrom(spaced.path(), spaced_out.path()));

	writeFile(empty_old.path(), "mod_auth.username=\nmod_auth.password=ni\n");
	REQUIRE_FALSE(httpd::migrateFrom(empty_old.path(), empty_out.path()));

	struct stat st;
	REQUIRE(stat(spaced_out.c_str(), &st) != 0);
	REQUIRE(stat(empty_out.c_str(), &st) != 0);
}

TEST_CASE("the migration carries the name the old server compared against", "[webconf]")
{
	/* Both servers read that file through the same parser, which cuts a line
	   off at the first number sign. So a name written with one in it was
	   already the shorter name everywhere the old server ever compared it, and
	   carrying the longer one would change who can log in rather than keep
	   it. */
	Restore back;
	Temp old("old"), out("out");

	writeFile(old.path(), "mod_auth.username=ro#ot\nmod_auth.password=ni\n");

	REQUIRE(httpd::migrateFrom(old.path(), out.path()));
	REQUIRE(httpd::load(out.path()));
	REQUIRE(httpd::config().username == "ro");
}

TEST_CASE("a host the old file does not carry keeps the wildcard", "[webconf]")
{
	Restore back;
	Temp old("old"), out("out");

	writeFile(old.path(), "mod_auth.username=admin\nmod_auth.password=ni\n");

	REQUIRE(httpd::migrateFrom(old.path(), out.path()));
	REQUIRE(httpd::load(out.path()));
	REQUIRE(httpd::config().server.bind_address == "0.0.0.0");
	REQUIRE(httpd::config().username == "admin");
}

TEST_CASE("the migrated file names no network, so the seeded list is what stands", "[webconf]")
{
	/* Writing the seeded list out would freeze the list of today into every box
	   that was ever migrated, and the list is the policy for who may read
	   without a password. */
	Restore back;
	Temp old("old"), out("out");

	writeFile(old.path(), kShippedOldFile);
	REQUIRE(httpd::migrateFrom(old.path(), out.path()));

	REQUIRE_FALSE(anySettingMentions(readFile(out.path()), "lan_read"));
	REQUIRE(httpd::load(out.path()));
	REQUIRE(httpd::config().lan_read.size() == 6);
	REQUIRE(httpd::config().trusted_proxies.empty());
}

TEST_CASE("nothing the migration writes is read back as something else", "[webconf]")
{
	// Every line of the written file goes back through the parser it was
	// written for, including the comment block, which carries no separator for
	// exactly this reason.
	Restore back;
	Temp old("old"), out("out");

	writeFile(old.path(), kShippedOldFile);
	REQUIRE(httpd::migrateFrom(old.path(), out.path()));

	const std::string written = readFile(out.path());
	std::istringstream lines(written);
	std::string line;
	while (std::getline(lines, line))
	{
		if (line.empty() || line[0] != '#')
			continue;
		REQUIRE(line.find('=') == std::string::npos);
	}

	REQUIRE(httpd::load(out.path()));
	REQUIRE(httpd::config().username == "root");
	REQUIRE(httpd::config().server.port == 80);
	REQUIRE(httpd::configProblems().empty());
}

TEST_CASE("a prefix list in the file replaces the seeded one", "[webconf]")
{
	Restore back;
	Temp conf("conf");

	writeFile(conf.path(), "lan_read=10.9.0.0/16\nport=9090\n");

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::config().lan_read.size() == 1);
	REQUIRE(httpd::addressInAnyPrefix("10.9.1.1", httpd::config().lan_read));
	REQUIRE_FALSE(httpd::addressInAnyPrefix("192.168.1.1", httpd::config().lan_read));
	REQUIRE_FALSE(httpd::addressInAnyPrefix("127.0.0.1", httpd::config().lan_read));
	REQUIRE(httpd::config().server.port == 9090);
	REQUIRE(httpd::configProblems().empty());
}

TEST_CASE("a proxy line this cannot deliver leaves the read exemption off", "[webconf]")
{
	/* The one key where the least value and the least meaning point opposite ways. An empty
	   proxy list believes nobody, which is the narrow half, and it is also how a box with
	   nothing in front of it is described, which is the box where the private ranges may
	   read without a password. So a line naming a proxy the parser cannot deliver, read as
	   an empty list and nothing more, turns that exemption back on.

	   Driven to the level the gate answers and not to the list, because the list is empty in
	   the wide case and in the narrow one alike. */
	Restore back;

	httpd::Credentials caller;
	caller.peer = "127.0.0.1";

	const std::string lan = "lan_read=127.0.0.0/8\n";

	// The control. No proxy line at all is the box the exemption is for, and
	// without this the case would pass on a gate that granted nobody anything.
	Temp none("conf");
	writeFile(none.path(), lan);
	REQUIRE(httpd::load(none.path()));
	REQUIRE(httpd::config().trusted_proxies.empty());
	REQUIRE(httpd::granted(caller) == httpd::AuthLevel::Read);

	// A list this can read. The exemption is off and the list says why.
	Temp whole("conf");
	writeFile(whole.path(), lan + "trusted_proxies=127.0.0.1/32\n");
	REQUIRE(httpd::load(whole.path()));
	REQUIRE(httpd::config().trusted_proxies.size() == 1);
	REQUIRE(httpd::granted(caller) == httpd::AuthLevel::Public);

	/* And the three shapes that leave nothing in the list. A trailing comment,
	   which the parser cuts the line at; a name spelled in a way the parser
	   hands over under no key of ours; and a line every entry of which is
	   dropped, where the file was delivered and said nothing this can use. */
	static const char *const cut[] =
	{
		"trusted_proxies=127.0.0.1/32 # our nginx\n",
		"TRUSTED_PROXIES=127.0.0.1/32\n",
		"  trusted_proxies = 127.0.0.1/32\n",
		"trusted_proxies=nginx.local\n"
	};

	for (size_t i = 0; i < sizeof(cut) / sizeof(cut[0]); ++i)
	{
		INFO(cut[i]);
		Temp conf("conf");
		writeFile(conf.path(), lan + cut[i]);

		REQUIRE(httpd::load(conf.path()));
		REQUIRE(httpd::config().trusted_proxies.empty());
		REQUIRE(httpd::proxiesConfigured(httpd::config()));
		REQUIRE(httpd::granted(caller) == httpd::AuthLevel::Public);
		REQUIRE(anyProblemMentions("trusted_proxies"));
	}
}

TEST_CASE("a list written with nothing after it admits nobody", "[webconf]")
{
	// A key emptied on purpose is not the same as a key nobody wrote, and
	// seeding the defaults over it would put back what somebody removed.
	Restore back;
	Temp conf("conf");

	writeFile(conf.path(), "lan_read=\n");

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::config().lan_read.empty());
	REQUIRE_FALSE(httpd::addressInAnyPrefix("127.0.0.1", httpd::config().lan_read));
}

TEST_CASE("an entry that does not parse is dropped and said out loud", "[webconf]")
{
	Restore back;
	Temp conf("conf");

	writeFile(conf.path(), "lan_read=10.9.0.0/16,not-a-prefix,192.168.0.0/16\nport=9091\n");

	REQUIRE(httpd::load(conf.path()));

	// Two and not three: a bad entry kept as a zero prefix matches nothing, so
	// an address check alone cannot tell it from an entry that was dropped.
	REQUIRE(httpd::config().lan_read.size() == 2);
	REQUIRE(httpd::config().server.port == 9091);

	// And dropped loudly. Silence would make a mistyped network look exactly
	// like a network the file never named.
	REQUIRE(anyProblemMentions("not-a-prefix"));
	REQUIRE(anyProblemMentions("lan_read"));
}

TEST_CASE("a bare address without a length is dropped rather than read as a host", "[webconf]")
{
	Restore back;
	Temp conf("conf");

	writeFile(conf.path(), "lan_read=10.0.0.0/8,192.168.1.1\n");

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::config().lan_read.size() == 1);
	REQUIRE(anyProblemMentions("192.168.1.1"));
}

TEST_CASE("a proxy list is read the same way and is empty when unwritten", "[webconf]")
{
	Restore back;
	Temp conf("conf");

	writeFile(conf.path(), "trusted_proxies=10.0.0.1/32,nonsense\n");

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::config().trusted_proxies.size() == 1);
	REQUIRE(httpd::addressInAnyPrefix("10.0.0.1", httpd::config().trusted_proxies));
	REQUIRE(anyProblemMentions("trusted_proxies"));
}

TEST_CASE("a port that is not one keeps the default and is said out loud", "[webconf]")
{
	Restore back;

	const char *const bad[] = { "0", "65536", "eighty", "80 ", "-1", "", "8081x", "9999999999" };
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
	{
		Temp conf("conf");
		writeFile(conf.path(), std::string("port=") + bad[i] + "\n");

		REQUIRE(httpd::load(conf.path()));
		INFO("port value " << bad[i]);
		REQUIRE(httpd::config().server.port == httpd::defaultConfig().port);
		REQUIRE(anyProblemMentions("port"));
	}
}

TEST_CASE("a bind address that is not one is carried through unusable", "[webconf]")
{
	/* The one value that is not dropped when it does not read. Every other
	   default admits less than a wrong value would; this one is the wildcard,
	   so falling back to it answers a line naming one interface by listening on
	   all of them. Kept, the call that binds refuses it. */
	Restore back;
	Temp conf("conf");

	writeFile(conf.path(), "bind_address=localhost\n");

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::config().server.bind_address == "localhost");
	REQUIRE(httpd::config().server.bind_address != "0.0.0.0");
	REQUIRE(anyProblemMentions("bind_address"));

	// Asked first, because a daemon another case left running would refuse the
	// start below for a reason that has nothing to do with the address.
	REQUIRE_FALSE(httpd::running());
	REQUIRE_FALSE(httpd::start(httpd::config().server));
	REQUIRE_FALSE(httpd::running());
}

TEST_CASE("a bind address that is one is taken in either family", "[webconf]")
{
	Restore back;
	Temp v4("conf"), v6("conf");

	writeFile(v4.path(), "bind_address=127.0.0.1\n");
	REQUIRE(httpd::load(v4.path()));
	REQUIRE(httpd::config().server.bind_address == "127.0.0.1");
	REQUIRE(httpd::configProblems().empty());

	writeFile(v6.path(), "bind_address=::1\n");
	REQUIRE(httpd::load(v6.path()));
	REQUIRE(httpd::config().server.bind_address == "::1");
	REQUIRE(httpd::configProblems().empty());
}

TEST_CASE("a text value that would not read back is dropped", "[webconf]")
{
	/* The parser cuts a value off at the first number sign and keeps a space at
	   either end, so a value carrying either is not the value that was written.
	   Keeping it would put a user name in effect that nothing can be typed
	   to. */
	Restore back;
	Temp sharp("conf"), space("conf");

	/* A line carrying a number sign is not read at all rather than read short. The
	   parse has already thrown the tail away by the time anything here sees the value,
	   so a check on what arrived would be looking at "ad" and find nothing wrong with
	   it. What stands in its place is not the default name but no name at all, and a
	   name nothing answers to takes the password with it. */
	writeFile(sharp.path(), "username=ad#min\npassword_hash=pbkdf2-sha256$1$AA$BB\n");
	REQUIRE(httpd::load(sharp.path()));
	REQUIRE(httpd::config().username.empty());
	REQUIRE(httpd::config().username != "root");
	REQUIRE(httpd::config().username != "ad");
	REQUIRE(httpd::config().password_hash.empty());
	REQUIRE(anyProblemMentions("username"));
	REQUIRE(anyProblemMentions("number sign"));

	writeFile(space.path(), "username=admin \n");
	REQUIRE(httpd::load(space.path()));
	REQUIRE(httpd::config().username.empty());
	REQUIRE(httpd::config().username != "root");
	REQUIRE(anyProblemMentions("username"));
}

TEST_CASE("a stored form that was mistyped is said out loud and still judged elsewhere", "[webconf]")
{
	/* Kept rather than cleared, because deciding here what a stored form may be
	   is a second judge that would refuse a scheme the one judge learns later.
	   It verifies against nothing either way, so the box is closed and the
	   operator is told why. */
	Restore back;
	Temp conf("conf");

	writeFile(conf.path(), "password_hash=md5$deadbeef\n");

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::config().password_hash == "md5$deadbeef");
	REQUIRE_FALSE(httpd::verifySecret("ni", httpd::config().password_hash));
	REQUIRE(anyProblemMentions("password_hash"));
}

TEST_CASE("a stored form this wrote is read back whole", "[webconf]")
{
	// The stored form carries a dollar and base64, none of which the file cuts
	// a value off at, so it survives the round trip unchanged.
	Restore back;
	Temp conf("conf");

	const std::string stored = httpd::hashSecret("hunter2", 1000);
	REQUIRE_FALSE(stored.empty());

	writeFile(conf.path(), "password_hash=" + stored + "\n");

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::config().password_hash == stored);
	REQUIRE(httpd::verifySecret("hunter2", httpd::config().password_hash));
	REQUIRE(httpd::configProblems().empty());
}

TEST_CASE("a session lifetime outside what one may be keeps the default", "[webconf]")
{
	Restore back;

	/* The default is a day, which is longer than any hand written line is
	   likely to be, so a refused value falls back to the shortest a session may
	   be rather than to the default. A token that outlives what the file asked
	   for is the failure that matters here. */
	const char *const bad[] = { "0", "59", "2592001", "forever", "" };
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
	{
		Temp conf("conf");
		writeFile(conf.path(), std::string("session_lifetime_s=") + bad[i] + "\n");

		REQUIRE(httpd::load(conf.path()));
		INFO("lifetime value " << bad[i]);
		REQUIRE(httpd::config().session_lifetime_s == 60);
		REQUIRE(httpd::config().session_lifetime_s != 86400);
		REQUIRE(anyProblemMentions("session_lifetime_s"));
	}

	Temp good("conf");
	writeFile(good.path(), "session_lifetime_s=3600\n");
	REQUIRE(httpd::load(good.path()));
	REQUIRE(httpd::config().session_lifetime_s == 3600);
	REQUIRE(httpd::configProblems().empty());
}

TEST_CASE("a stream count of none is a real answer and one above the ceiling is not", "[webconf]")
{
	Restore back;
	Temp none("conf"), over("conf");

	writeFile(none.path(), "sse_max_streams=0\n");
	REQUIRE(httpd::load(none.path()));
	REQUIRE(httpd::config().sse_max_streams == 0);
	REQUIRE(httpd::configProblems().empty());

	writeFile(over.path(), "sse_max_streams=65\n");
	REQUIRE(httpd::load(over.path()));
	REQUIRE(httpd::config().sse_max_streams == 0);
	REQUIRE(httpd::config().sse_max_streams != 8);
	REQUIRE(anyProblemMentions("sse_max_streams"));
}

TEST_CASE("the legacy switch reads only the four words it has", "[webconf]")
{
	Restore back;

	Temp on("conf"), off("conf"), junk("conf");

	writeFile(on.path(), "legacy_enabled=true\n");
	REQUIRE(httpd::load(on.path()));
	REQUIRE(httpd::config().legacy_enabled);

	writeFile(off.path(), "legacy_enabled=0\n");
	REQUIRE(httpd::load(off.path()));
	REQUIRE_FALSE(httpd::config().legacy_enabled);

	/* The library's own reader answers true for anything that is not the word
	   false and not a zero, which turns a typo into the switch being on. */
	writeFile(junk.path(), "legacy_enabled=yes\n");
	REQUIRE(httpd::load(junk.path()));
	REQUIRE_FALSE(httpd::config().legacy_enabled);
	REQUIRE(anyProblemMentions("legacy_enabled"));
}

TEST_CASE("a bad line does not stop the lines around it", "[webconf]")
{
	Restore back;
	Temp conf("conf");

	writeFile(conf.path(),
		  "port=notaport\n"
		  "username=admin\n"
		  "lan_read=10.0.0.0/8,rubbish\n"
		  "session_lifetime_s=7200\n"
		  "sse_max_streams=nope\n"
		  "legacy_enabled=true\n");

	REQUIRE(httpd::load(conf.path()));

	REQUIRE(httpd::config().server.port == httpd::defaultConfig().port);
	REQUIRE(httpd::config().username == "admin");
	REQUIRE(httpd::config().lan_read.size() == 1);
	REQUIRE(httpd::config().session_lifetime_s == 7200);
	REQUIRE(httpd::config().sse_max_streams == 0);
	REQUIRE(httpd::config().legacy_enabled);
	REQUIRE(httpd::configProblems().size() == 3);
}

TEST_CASE("a key the file does not carry keeps its default without a word", "[webconf]")
{
	// The difference between a value the file never named and one it named
	// wrongly is the whole of what the list beside the load is for.
	Restore back;
	Temp conf("conf");

	writeFile(conf.path(), "port=9000\n");

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::config().server.port == 9000);
	REQUIRE(httpd::config().username == "root");
	REQUIRE(httpd::config().lan_read.size() == 6);
	REQUIRE(httpd::configProblems().empty());
}

TEST_CASE("a whole configuration can be put in front of the code that reads one", "[webconf]")
{
	Restore back;

	httpd::WebConfig c = httpd::defaultWebConfig();
	c.username = "operator";
	c.lan_read.clear();
	c.session_lifetime_s = 120;

	httpd::setConfigForTest(c);

	REQUIRE(httpd::config().username == "operator");
	REQUIRE(httpd::config().lan_read.empty());
	REQUIRE(httpd::config().session_lifetime_s == 120);
	REQUIRE_FALSE(httpd::addressInAnyPrefix("127.0.0.1", httpd::config().lan_read));
}

TEST_CASE("the shipped old file migrates and loads", "[webconf]")
{
	/* The file as the image installs it, read from the tree rather than
	   transcribed, so a line added to it that this cannot carry is caught here
	   and not on a box. The build substitutes three directories into it, which
	   this does not read and which carry no separator of their own. */
	Restore back;
	Temp old("old"), out("out");

	const std::string shipped = readFile(NI_WEB_OLD_CONF);
	REQUIRE(shipped.find("mod_auth.password") != std::string::npos);

	writeFile(old.path(), shipped);
	REQUIRE(httpd::migrateFrom(old.path(), out.path()));
	REQUIRE(httpd::load(out.path()));

	REQUIRE(httpd::config().server.port == 80);
	REQUIRE(httpd::config().username == "root");
	/* Read out of the installed file rather than out of a copy of it here, so
	   that an image which one day ships a different value is answered by this
	   case rather than by a transcription of what the file used to say. */
	REQUIRE(httpd::verifySecret("ni", httpd::config().password_hash));
	REQUIRE(modeOf(out.path()) == 0600);
	REQUIRE_FALSE(anySettingMentions(readFile(out.path()), "authenticate"));
	REQUIRE(readFile(out.path()).find("=ni\n") == std::string::npos);
}

TEST_CASE("a line cut short by a number sign is not read at all", "[webconf]")
{
	/* The parser keeps whatever came before the number sign and says nothing
	   about the rest, so a value lost this way looks exactly like a value that
	   was meant. It is the one way this file loses part of a setting in
	   silence, and silence is what makes it worse than losing the whole
	   line. */
	Restore back;
	Temp conf("conf");

	writeFile(conf.path(),
		  "port=9000\n"
		  "docroot=/var/my#dir\n"
		  "lan_read=10.0.0.0/8\n");

	REQUIRE(httpd::load(conf.path()));

	REQUIRE(httpd::config().docroot.empty());
	REQUIRE(httpd::config().docroot != "/var/my");
	REQUIRE(anyProblemMentions("docroot"));
	REQUIRE(anyProblemMentions("line 2"));

	// The value itself is never repeated, so the half of a path that survived
	// the cut does not reach the log by way of the complaint about it.
	REQUIRE_FALSE(anyProblemMentions("/var/my"));

	// The lines around it still load.
	REQUIRE(httpd::config().server.port == 9000);
	REQUIRE(httpd::config().lan_read.size() == 1);
	REQUIRE(httpd::configProblems().size() == 2);
}

TEST_CASE("a comment carrying a separator is read as neither and is said out loud", "[webconf]")
{
	/* The parser looks for the separator first and the number sign second, so a
	   line meant as a comment about a setting becomes a setting under a key
	   spelled with a number sign in it. Nothing here reads such a key, and the
	   length it works out for the value goes round through zero on the way, so
	   what it stores is the whole tail of the line. */
	Restore back;
	Temp conf("conf");

	writeFile(conf.path(),
		  "# the default port is port=80\n"
		  "# a comment with no separator is read as nothing and says nothing\n"
		  "port=9000\n");

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::config().server.port == 9000);
	REQUIRE(httpd::configProblems().size() == 1);
	REQUIRE(anyProblemMentions("not read at all"));
	REQUIRE(anyProblemMentions("line 1"));

	/* The key the parse stored for that line is most of the line, so naming it
	   would name whatever the line held. Only a key this file knows is named,
	   and the line number identifies the rest. */
	REQUIRE_FALSE(anyProblemMentions("the default port is"));
}

TEST_CASE("a value the parser keeps whole is not disturbed", "[webconf]")
{
	// The guard above must not reach a line that carries no number sign at all,
	// or every file would load with the defaults and a list of complaints.
	Restore back;
	Temp conf("conf");

	writeFile(conf.path(),
		  "docroot=/var/tuxbox/httpd\n"
		  "username=admin\n"
		  "trusted_proxies=10.0.0.1/32,fc00::/7\n");

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::config().docroot == "/var/tuxbox/httpd");
	REQUIRE(httpd::config().username == "admin");
	REQUIRE(httpd::config().trusted_proxies.size() == 2);
	REQUIRE(httpd::configProblems().empty());
}

TEST_CASE("a file larger than a configuration is not read at all", "[webconf]")
{
	/* Both the parse and the scan beside it hold the whole file in memory, so a
	   file this size is refused rather than read in part: a scan that stopped
	   early would pass a line it never looked at, and the values before the cut
	   would install over what was in effect. */
	Restore back;
	Temp big("conf"), out("out");

	{
		std::ofstream f(big.c_str(), std::ios::out | std::ios::trunc);
		REQUIRE(f.good());
		f << "port=9000\n";
		const std::string filler(1024, 'x');
		for (int i = 0; i < 1030; ++i)
			f << "pad" << i << "=" << filler << "\n";
		f.close();
	}

	REQUIRE_FALSE(httpd::load(big.path()));
	REQUIRE(httpd::config().server.port == httpd::defaultConfig().port);
	REQUIRE(anyProblemMentions("larger than a configuration"));

	REQUIRE_FALSE(httpd::migrateFrom(big.path(), out.path()));

	struct stat st;
	REQUIRE(stat(out.c_str(), &st) != 0);
}

TEST_CASE("a network list cut short admits nobody rather than the seeded six", "[webconf]")
{
	/* The line an operator writes to narrow their own exemption is exactly the
	   line the parser cuts, and the seeded default is six private networks
	   including both halves of loopback. Falling back to it would answer a line
	   admitting one network by admitting six, which is the one drop here that
	   grants more than the file asks for. */
	Restore back;
	Temp conf("conf");

	writeFile(conf.path(), "lan_read=10.9.0.0/16 # only our own net\nport=8081\n");

	REQUIRE(httpd::load(conf.path()));

	REQUIRE(httpd::config().lan_read.empty());
	REQUIRE(httpd::config().lan_read.size() != 6);
	REQUIRE_FALSE(httpd::addressInAnyPrefix("192.168.1.1", httpd::config().lan_read));
	REQUIRE_FALSE(httpd::addressInAnyPrefix("127.0.0.1", httpd::config().lan_read));
	REQUIRE_FALSE(httpd::addressInAnyPrefix("::1", httpd::config().lan_read));
	REQUIRE_FALSE(httpd::addressInAnyPrefix("10.9.1.1", httpd::config().lan_read));
	REQUIRE(anyProblemMentions("lan_read"));
}

TEST_CASE("a proxy list cut short believes nobody", "[webconf]")
{
	Restore back;
	Temp conf("conf");

	writeFile(conf.path(), "trusted_proxies=10.0.0.1/32 # the reverse proxy\n");

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::config().trusted_proxies.empty());
	REQUIRE_FALSE(httpd::addressInAnyPrefix("10.0.0.1", httpd::config().trusted_proxies));
	REQUIRE(anyProblemMentions("trusted_proxies"));
}

TEST_CASE("a session lifetime cut short is the shortest and not the longest", "[webconf]")
{
	// The default is a day. A line asking for five minutes that cannot be read
	// must not become a day.
	Restore back;
	Temp conf("conf");

	writeFile(conf.path(), "session_lifetime_s=300 # five minutes\n");

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::config().session_lifetime_s == 60);
	REQUIRE(httpd::config().session_lifetime_s != 86400);
	REQUIRE(anyProblemMentions("session_lifetime_s"));
}

TEST_CASE("a stream count cut short is none", "[webconf]")
{
	Restore back;
	Temp conf("conf");

	writeFile(conf.path(), "sse_max_streams=4 # four at a time\n");

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::config().sse_max_streams == 0);
	REQUIRE(httpd::config().sse_max_streams != 8);
	REQUIRE(anyProblemMentions("sse_max_streams"));
}

TEST_CASE("a legacy switch cut short is off", "[webconf]")
{
	Restore back;
	Temp conf("conf");

	writeFile(conf.path(), "legacy_enabled=true # for the old clients\n");

	REQUIRE(httpd::load(conf.path()));
	REQUIRE_FALSE(httpd::config().legacy_enabled);
	REQUIRE(anyProblemMentions("legacy_enabled"));
}

TEST_CASE("a bind address cut short is one nothing can bind", "[webconf]")
{
	/* The default here is the wildcard, so this is the other shape of the same
	   rule: a line naming one interface that cannot be read must not become
	   every interface. */
	Restore back;
	Temp conf("conf");

	writeFile(conf.path(), "bind_address=127.0.0.1 # only from the box itself\n");

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::config().server.bind_address != "0.0.0.0");
	REQUIRE(anyProblemMentions("bind_address"));

	REQUIRE_FALSE(httpd::running());
	REQUIRE_FALSE(httpd::start(httpd::config().server));
	REQUIRE_FALSE(httpd::running());
}

TEST_CASE("a port cut short keeps the default, which is the one exception", "[webconf]")
{
	/* A port is where the box answers and not who may reach it. There is no
	   least port, and a box that refused to start over a mistyped one would be
	   off the network for a reason that grants nobody anything. */
	Restore back;
	Temp conf("conf");

	writeFile(conf.path(), "port=9090 # the one we chose\nusername=admin\n");

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::config().server.port == httpd::defaultConfig().port);
	REQUIRE(httpd::config().username == "admin");
	REQUIRE(anyProblemMentions("port"));
}

TEST_CASE("a stored form is never repeated in a complaint", "[webconf]")
{
	/* The list is public and both halves of it reach somewhere a person reads.
	   A stored form is what an offline attack is mounted against, so a message
	   about the line it is on names the line and not the line's content. */
	Restore back;
	Temp cut("conf"), shaped("conf");

	const std::string stored = httpd::hashSecret("hunter2", 1000);
	REQUIRE_FALSE(stored.empty());

	writeFile(cut.path(), "password_hash=" + stored + " # the admin password\n");
	REQUIRE(httpd::load(cut.path()));
	REQUIRE(httpd::config().password_hash.empty());
	REQUIRE_FALSE(anyProblemMentions(stored));
	REQUIRE_FALSE(anyProblemMentions(stored.substr(stored.size() - 12)));
	REQUIRE(anyProblemMentions("password_hash"));

	writeFile(shaped.path(), "password_hash=md5$deadbeefdeadbeefdeadbeef\n");
	REQUIRE(httpd::load(shaped.path()));
	REQUIRE_FALSE(anyProblemMentions("deadbeef"));
	REQUIRE(anyProblemMentions("password_hash"));
}

TEST_CASE("a value a message repeats cannot carry a byte that rewrites it", "[webconf]")
{
	// A message goes to a terminal and into a list something later renders, and
	// the value in it is bytes somebody else chose.
	Restore back;
	Temp conf("conf");

	writeFile(conf.path(), std::string("port=8\x1b[31m1\nlan_read=10.0.0.0/8\x07x\n"));

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::config().server.port == httpd::defaultConfig().port);

	const std::vector<std::string> &p = httpd::configProblems();
	REQUIRE(p.size() == 2);
	for (size_t i = 0; i < p.size(); ++i)
	{
		INFO(p[i]);
		REQUIRE(p[i].find('\x1b') == std::string::npos);
		REQUIRE(p[i].find('\x07') == std::string::npos);
	}
	REQUIRE(anyProblemMentions("8?[31m1"));
}

TEST_CASE("a value a message repeats is cut short of a whole line", "[webconf]")
{
	Restore back;
	Temp conf("conf");

	const std::string huge(400, 'z');
	writeFile(conf.path(), "port=" + huge + "\n");

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::config().server.port == httpd::defaultConfig().port);
	REQUIRE(httpd::configProblems().size() == 1);
	REQUIRE(httpd::configProblems()[0].find(huge) == std::string::npos);
	REQUIRE(httpd::configProblems()[0].size() < 200);
}

TEST_CASE("a bind address carrying a zero byte is refused by the binder too", "[webconf]")
{
	/* The value is compared as a C string where it is bound, and that reading
	   ends at the first zero byte, so half of it would be bound as though it
	   were the whole. This file refuses such a value, and the header says the
	   call that binds refuses it as well; this is what makes that true. */
	Restore back;
	Temp conf("conf");

	const char raw[] = "bind_address=127.0.0.1\0evil\nport=18081\n";
	writeFile(conf.path(), std::string(raw, sizeof(raw) - 1));

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::config().server.bind_address.size() == 14);
	REQUIRE(httpd::config().server.bind_address != "127.0.0.1");
	REQUIRE(anyProblemMentions("bind_address"));

	REQUIRE_FALSE(httpd::running());
	REQUIRE_FALSE(httpd::start(httpd::config().server));
	REQUIRE_FALSE(httpd::running());
	REQUIRE(httpd::boundPort() != 18081);
}

TEST_CASE("the binder refuses a zero byte whoever handed it over", "[webconf]")
{
	// Not only the caller that read a file. This drives the guard in the server
	// itself, with no file in the way.
	Restore back;

	httpd::WebConfig c = httpd::defaultWebConfig();
	c.server.bind_address = std::string("127.0.0.1\0evil", 14);
	c.server.port = 0;
	httpd::setConfigForTest(c);

	REQUIRE_FALSE(httpd::running());
	REQUIRE_FALSE(httpd::start(httpd::config().server));
	REQUIRE_FALSE(httpd::running());
}

TEST_CASE("an empty user name is said out loud and takes the password with it", "[webconf]")
{
	/* Whether an empty name answers a header field that decodes to an empty
	   user is decided by whatever compares them, which is not this file. What
	   this file can do is leave no pair of a name and a password that could be
	   answerable, and say so. */
	Restore back;
	Temp conf("conf");

	const std::string stored = httpd::hashSecret("hunter2", 1000);
	REQUIRE_FALSE(stored.empty());

	writeFile(conf.path(), "username=\npassword_hash=" + stored + "\n");

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::config().username.empty());
	REQUIRE(httpd::config().password_hash.empty());
	REQUIRE_FALSE(httpd::verifySecret("hunter2", httpd::config().password_hash));
	REQUIRE(anyProblemMentions("username"));
	REQUIRE_FALSE(httpd::configProblems().empty());
}

TEST_CASE("a name the file does not mention leaves the password alone", "[webconf]")
{
	// The rule above must not fire on a file that says nothing about the name,
	// or every configuration with a password and no name line would lose it.
	Restore back;
	Temp conf("conf");

	const std::string stored = httpd::hashSecret("hunter2", 1000);
	writeFile(conf.path(), "password_hash=" + stored + "\n");

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::config().username == "root");
	REQUIRE(httpd::config().password_hash == stored);
	REQUIRE(httpd::verifySecret("hunter2", httpd::config().password_hash));
	REQUIRE(httpd::configProblems().empty());
}

TEST_CASE("the list of complaints is bounded", "[webconf]")
{
	/* Every other quantity read out of a file here has a ceiling. A list of
	   messages about the file should not be the one thing that grows with
	   it. */
	Restore back;
	Temp conf("conf");

	std::string text;
	for (int i = 0; i < 400; ++i)
	{
		char line[32];
		std::snprintf(line, sizeof(line), "a%d=#\n", i);
		text += line;
	}
	writeFile(conf.path(), text);

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::configProblems().size() == 101);
	REQUIRE(httpd::configProblems().size() < 400);

	const std::string &last = httpd::configProblems().back();
	REQUIRE(last.find("not named here") != std::string::npos);
}

TEST_CASE("a migration whose value was cut short says which line did it", "[webconf]")
{
	/* The old server compared against the short value too, so what is carried
	   is right. What was missing was the reason: an operator shown a valid
	   looking address and told it is not one has nothing to go on. */
	Restore back;
	Temp old("old"), out("out"), good("old"), good_out("out");

	writeFile(old.path(), "WebsiteMain.host=192.168.1.1 # our LAN\nmod_auth.password=ni\n");
	REQUIRE_FALSE(httpd::migrateFrom(old.path(), out.path()));
	REQUIRE(anyProblemMentions("number sign"));
	REQUIRE(anyProblemMentions("WebsiteMain.host"));

	struct stat st;
	REQUIRE(stat(out.c_str(), &st) != 0);

	// And the same note on a line whose short value is still usable, because
	// that is where what is carried is visibly not what was typed.
	writeFile(good.path(), "mod_auth.username=root# the admin\nmod_auth.password=ni\n");
	REQUIRE(httpd::migrateFrom(good.path(), good_out.path()));
	REQUIRE(anyProblemMentions("mod_auth.username"));
	REQUIRE(anyProblemMentions("number sign"));
	REQUIRE(httpd::load(good_out.path()));
	REQUIRE(httpd::config().username == "root");
}

TEST_CASE("a write that stops half way leaves nothing behind", "[webconf]")
{
	/* Reached by capping the size a file this process may reach. Everything the
	   case does that could write to the harness log is kept outside the cap,
	   and the standard error is sent to the null device for the same reason. */
	Restore back;
	Temp old("old"), out("out");

	writeFile(old.path(), kShippedOldFile);

	struct rlimit saved;
	REQUIRE(getrlimit(RLIMIT_FSIZE, &saved) == 0);

	void (*prev)(int) = signal(SIGXFSZ, SIG_IGN);
	const int keep = dup(2);
	const int sink = open("/dev/null", O_WRONLY);
	REQUIRE(keep >= 0);
	REQUIRE(sink >= 0);
	dup2(sink, 2);

	struct rlimit small;
	small.rlim_cur = 128;
	small.rlim_max = saved.rlim_max;
	const int set = setrlimit(RLIMIT_FSIZE, &small);

	const bool done = (set == 0) ? httpd::migrateFrom(old.path(), out.path()) : true;

	setrlimit(RLIMIT_FSIZE, &saved);
	dup2(keep, 2);
	close(keep);
	close(sink);
	signal(SIGXFSZ, prev);

	REQUIRE(set == 0);
	REQUIRE_FALSE(done);

	struct stat st;
	REQUIRE(stat(out.c_str(), &st) != 0);
}

TEST_CASE("space around a name does not make the key absent", "[webconf]")
{
	/* The parser takes the name to be every byte in front of the separator, so
	   a line written the way most people write it hands its value over under a
	   name nothing here asks for. Read as absent, that line would seed six
	   private networks where it named one, which is the wide answer reached by
	   the most natural formatting there is. */
	Restore back;

	const char *const shapes[] =
	{
		"lan_read = 10.9.0.0/16\n",
		"lan_read =10.9.0.0/16\n",
		"  lan_read=10.9.0.0/16\n",
		"\tlan_read=10.9.0.0/16\n",
		"lan_read\t= 10.9.0.0/16\n"
	};

	for (size_t i = 0; i < sizeof(shapes) / sizeof(shapes[0]); ++i)
	{
		Temp conf("conf");
		writeFile(conf.path(), shapes[i]);

		REQUIRE(httpd::load(conf.path()));
		INFO("shape " << shapes[i]);

		REQUIRE(httpd::config().lan_read.empty());
		REQUIRE(httpd::config().lan_read.size() != 6);
		REQUIRE_FALSE(httpd::addressInAnyPrefix("127.0.0.1", httpd::config().lan_read));
		REQUIRE_FALSE(httpd::addressInAnyPrefix("192.168.1.1", httpd::config().lan_read));
		REQUIRE(anyProblemMentions("lan_read"));
		REQUIRE(anyProblemMentions("line 1"));
	}
}

TEST_CASE("space around a name reaches the other two keys the same way", "[webconf]")
{
	Restore back;
	Temp bind("conf"), life("conf");

	writeFile(bind.path(), "bind_address = 127.0.0.1\n");
	REQUIRE(httpd::load(bind.path()));
	REQUIRE(httpd::config().server.bind_address != "0.0.0.0");
	REQUIRE(anyProblemMentions("bind_address"));
	REQUIRE_FALSE(httpd::running());
	REQUIRE_FALSE(httpd::start(httpd::config().server));
	REQUIRE_FALSE(httpd::running());

	writeFile(life.path(), "session_lifetime_s = 300\n");
	REQUIRE(httpd::load(life.path()));
	REQUIRE(httpd::config().session_lifetime_s == 60);
	REQUIRE(httpd::config().session_lifetime_s != 86400);
	REQUIRE(anyProblemMentions("session_lifetime_s"));
}

TEST_CASE("space around a name and a number sign on the same line agree", "[webconf]")
{
	// Two of the three faults at once, and the key still has to be the one that
	// was meant rather than the one the parser stored.
	Restore back;
	Temp conf("conf");

	writeFile(conf.path(), "lan_read = 10.9.0.0/16 # only our own net\n");

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::config().lan_read.empty());
	REQUIRE(httpd::config().lan_read.size() != 6);
	REQUIRE(anyProblemMentions("lan_read"));
}

TEST_CASE("a name differing only in case is the key it can only mean", "[webconf]")
{
	/* Such a name can plausibly mean no other key, and on a box nobody reads
	   the standard error, so a report would be the only thing between a line
	   written to narrow the exemption and a policy of six seeded networks. Where
	   the intent is unmistakable the safe reading of it is the narrow one. */
	Restore back;
	Temp lan("conf"), bind("conf"), life("conf");

	writeFile(lan.path(), "LAN_READ=10.9.0.0/16\n");
	REQUIRE(httpd::load(lan.path()));
	REQUIRE(httpd::config().lan_read.empty());
	REQUIRE(httpd::config().lan_read.size() != 6);
	REQUIRE_FALSE(httpd::addressInAnyPrefix("127.0.0.1", httpd::config().lan_read));
	REQUIRE_FALSE(httpd::addressInAnyPrefix("192.168.1.1", httpd::config().lan_read));
	REQUIRE(anyProblemMentions("lan_read"));
	REQUIRE(anyProblemMentions("line 1"));

	// Case and space at once, which is the shape a person actually types.
	writeFile(bind.path(), "Bind_Address = 127.0.0.1\n");
	REQUIRE(httpd::load(bind.path()));
	REQUIRE(httpd::config().server.bind_address != "0.0.0.0");
	REQUIRE(anyProblemMentions("bind_address"));
	REQUIRE_FALSE(httpd::running());
	REQUIRE_FALSE(httpd::start(httpd::config().server));
	REQUIRE_FALSE(httpd::running());

	writeFile(life.path(), "SESSION_LIFETIME_S=300\n");
	REQUIRE(httpd::load(life.path()));
	REQUIRE(httpd::config().session_lifetime_s == 60);
	REQUIRE(httpd::config().session_lifetime_s != 86400);
	REQUIRE(anyProblemMentions("session_lifetime_s"));
}

TEST_CASE("a name this cannot take for any key is said and no more", "[webconf]")
{
	/* It may be a typo, it may be a key a later version removed, and neither is
	   dangerous by itself, while refusing the file for one would make a working
	   configuration unusable after an upgrade. Nothing here guesses at which
	   key a name it cannot place was meant to be. */
	Restore back;
	Temp conf("conf");

	writeFile(conf.path(), "listen_port=9090\nlan-read=10.9.0.0/16\nport=9000\n");

	// Read, not refused, and every line it does know still takes effect.
	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::config().server.port == 9000);

	REQUIRE(httpd::configProblems().size() == 2);
	REQUIRE(anyProblemMentions("listen_port"));
	REQUIRE(anyProblemMentions("lan-read"));
	REQUIRE(anyProblemMentions("not a key this reads"));

	// Nothing it could not place touched anything.
	REQUIRE(httpd::config().lan_read.size() == 6);
}

TEST_CASE("a name this does not know cannot rewrite the message about it", "[webconf]")
{
	Restore back;
	Temp conf("conf");

	writeFile(conf.path(), std::string("no\x1b[31msuch=1\n"));

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::configProblems().size() == 1);
	REQUIRE(httpd::configProblems()[0].find('\x1b') == std::string::npos);
	REQUIRE(anyProblemMentions("no?[31msuch"));
}

TEST_CASE("a name written exactly says nothing about itself", "[webconf]")
{
	// The two rules above must not fire on a file that spells every key the way
	// the parser reads it, or every configuration would load with complaints.
	Restore back;
	Temp conf("conf");

	writeFile(conf.path(),
		  "# a comment with no separator in it\n"
		  "\n"
		  "port=9000\n"
		  "bind_address=127.0.0.1\n"
		  "username=admin\n"
		  "lan_read=10.9.0.0/16\n"
		  "trusted_proxies=10.0.0.1/32\n"
		  "docroot=/var/tuxbox/httpd\n"
		  "session_lifetime_s=3600\n"
		  "sse_max_streams=4\n"
		  "legacy_enabled=true\n");

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::configProblems().empty());
	REQUIRE(httpd::config().server.port == 9000);
	REQUIRE(httpd::config().server.bind_address == "127.0.0.1");
	REQUIRE(httpd::config().username == "admin");
	REQUIRE(httpd::config().lan_read.size() == 1);
	REQUIRE(httpd::config().trusted_proxies.size() == 1);
	REQUIRE(httpd::config().docroot == "/var/tuxbox/httpd");
	REQUIRE(httpd::config().session_lifetime_s == 3600);
	REQUIRE(httpd::config().sse_max_streams == 4);
	REQUIRE(httpd::config().legacy_enabled);
}

TEST_CASE("the old server's keys are not called unknown", "[webconf]")
{
	// That file's keys are that server's. Calling a hundred of them unknown
	// would say a hundred wrong things about a file this takes four values out
	// of.
	Restore back;
	Temp old("old"), out("out");

	writeFile(old.path(), readFile(NI_WEB_OLD_CONF));

	REQUIRE(httpd::migrateFrom(old.path(), out.path()));
	REQUIRE_FALSE(anyProblemMentions("not a key this reads"));
	/* The switch that is not carried, and the one line about the password:
	   what is carried is the value the image ships with, which is also the
	   login this box takes over the network. Nothing else, and in particular
	   nothing about the port, which the shipped file names as a port this can
	   read and carry. */
	REQUIRE(httpd::configProblems().size() == 2);
	REQUIRE(anyProblemMentions("carried across as the password"));
}

/* The bearer path was built, checked and unreachable: nothing outside the
   suite ever put a token in the table it is answered out of, so a program with
   no browser to keep a cookie in had no way to talk to this server at all.
   These cases are about the file being that way in. */

namespace
{
/* A token of the length this server mints, written out of one repeated character so
   the value in a case is readable and still hexadecimal. Distinct tokens differ in the
   character, two records sharing a prefix being a different case. Never a token a real
   box holds: what these cases are about is the reading of a line, and a value drawn
   from the pool would leave a case that could not say which token it had written. */
std::string tokenOf(char c, size_t chars = 64)
{
	return std::string(chars, c);
}

// One of a series, each differing from the others in its first characters, so
// that a case holding several of them holds several records rather than
// several tokens found under one lookup key.
std::string tokenNumber(unsigned n)
{
	char head[16];
	std::snprintf(head, sizeof(head), "%08x", n);
	return std::string(head) + std::string(56, 'a');
}

// Puts the token table back where the next case finds it, for the reason every
// fixture in this file has a destructor: the table is one table for the whole
// program.
struct Tokens
{
	Tokens() { httpd::forgetApiTokens(); }
	~Tokens() { httpd::forgetApiTokens(); }

	private:
		Tokens(const Tokens &);
		Tokens &operator=(const Tokens &);
};

httpd::Credentials bearing(const std::string &token)
{
	httpd::Credentials c;
	// From outside, so the answer is the token's and not the exemption the
	// private ranges are given.
	c.peer = "8.8.8.8";
	c.bearer_token = token;
	return c;
}
}

TEST_CASE("a token the file names is one a request can present", "[webconf]")
{
	Restore back;
	Tokens held;
	Temp f("tokens");

	const std::string read = tokenOf('a');
	const std::string write = tokenOf('b');
	const std::string system = tokenOf('c');

	writeFile(f.path(), "api_tokens=read:" + read + ",write:" + write +
	                    ",system:" + system + "\n");
	REQUIRE(httpd::load(f.path()));
	REQUIRE(httpd::configProblems().empty());

	/* Through the gate every request goes through rather than through a reader
	   of the table, because what the key is for is a program presenting a
	   header and being answered at the level the line gave it. */
	REQUIRE(httpd::granted(bearing(read)) == httpd::AuthLevel::Read);
	REQUIRE(httpd::granted(bearing(write)) == httpd::AuthLevel::Write);
	REQUIRE(httpd::granted(bearing(system)) == httpd::AuthLevel::System);

	// And the level is the one beside that token and not the highest the file
	// named, or naming three would be naming one.
	REQUIRE(httpd::granted(bearing(tokenOf('d'))) == httpd::AuthLevel::Public);
}

TEST_CASE("the file holds the token and the table holds no token", "[webconf]")
{
	/* The key carries the token itself, because nothing in this product writes
	   a stored form out and a key demanding one would be a key nobody could
	   write. What that costs is the file; what it must not cost is the table,
	   so a record found by the prefix confirms against a derived form and the
	   token is not in it. */
	Restore back;
	Tokens held;
	Temp f("tokenstored");

	const std::string token = tokenOf('e');
	writeFile(f.path(), "api_tokens=write:" + token + "\n");
	REQUIRE(httpd::load(f.path()));

	// A token sharing the whole prefix and differing in its last character is
	// what a lookup by prefix has to survive.
	std::string nearly = token;
	nearly[nearly.size() - 1] = 'f';
	REQUIRE(httpd::tokenLookupPrefix(nearly) == httpd::tokenLookupPrefix(token));
	REQUIRE(httpd::granted(bearing(nearly)) == httpd::AuthLevel::Public);
	REQUIRE(httpd::granted(bearing(token)) == httpd::AuthLevel::Write);
}

TEST_CASE("an entry this cannot use is dropped and said out loud", "[webconf]")
{
	/* One good token beside each bad one, so that what a case reads is the bad
	   entry being dropped rather than the line being refused whole. */
	Restore back;

	struct Case
	{
		const char *what;
		const char *entry;
	};

	const Case cases[] = {
		{ "no colon at all",         "write" },
		{ "no level",                ":aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" },
		{ "a level nothing grants",  "admin:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" },
		{ "the level public",        "public:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" },
		{ "a space inside the token", "write:aaaaaaaaaaaaaaaa aaaaaaaaaaaaaaaa" },
		{ "written the wrong way",   "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:write" },
		{ "no token",                "write:" },
		{ "a token one character short", "write:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" },
		{ "a token in upper case",   "write:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" },
		{ "a token that is not hex", "write:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaag" },
	};

	const std::string good = tokenOf('b');

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
	{
		INFO(cases[i].what);
		Tokens held;
		Temp f("tokenbad");
		writeFile(f.path(), std::string("api_tokens=") + cases[i].entry +
		                    ",write:" + good + "\n");
		REQUIRE(httpd::load(f.path()));

		// Said, because a value dropped without a word looks exactly like a
		// value the file never carried.
		REQUIRE(anyProblemMentions("api_tokens: entry 1"));
		// And the good one beside it stands, so one mistyped entry does not
		// take the tokens the file got right with it.
		REQUIRE(httpd::granted(bearing(good)) == httpd::AuthLevel::Write);
	}
}

TEST_CASE("a space at either end of an entry is forgiven", "[webconf]")
{
	/* Written the way people write a list, which the two network lists above
	   answer by dropping the entry. An entry here is a word out of a closed
	   set and a run of hexadecimal, and neither can mean anything else for
	   having a space beside it, so the space costs nothing rather than costing
	   a credential. */
	Restore back;
	Tokens held;
	Temp f("tokenspace");

	const std::string first = tokenOf('a');
	const std::string second = tokenOf('b');
	writeFile(f.path(), "api_tokens= read:" + first + " ,  write:" + second + "  \n");
	REQUIRE(httpd::load(f.path()));

	REQUIRE(httpd::configProblems().empty());
	REQUIRE(httpd::granted(bearing(first)) == httpd::AuthLevel::Read);
	REQUIRE(httpd::granted(bearing(second)) == httpd::AuthLevel::Write);
}

TEST_CASE("nothing of an entry is repeated back", "[webconf]")
{
	/* Every other message in this file quotes what it could not use. These
	   cannot: half of an entry is a token, the list reaches a terminal and a
	   page, and an entry written the wrong way round puts the token where the
	   level belongs, so there is no half of one that is safe to show. They say
	   which entry they mean by counting it instead. */
	Restore back;
	Tokens held;
	Temp f("tokenquiet");

	const std::string secret = tokenOf('a');
	writeFile(f.path(), "api_tokens=" + secret + ":write,nonsense:" + secret + "\n");
	REQUIRE(httpd::load(f.path()));

	REQUIRE(anyProblemMentions("api_tokens: entry 1"));
	REQUIRE(anyProblemMentions("api_tokens: entry 2"));

	const std::vector<std::string> &said = httpd::configProblems();
	REQUIRE_FALSE(said.empty());
	for (size_t i = 0; i < said.size(); ++i)
	{
		INFO(said[i]);
		/* The whole of it, and the eight characters a record is found
		   under. The prefix is a lookup key rather than a secret, and it
		   still has no business in a list anybody can read: nothing of an
		   entry is repeated, and a check that allowed the front of one
		   would pass a message that named the token by its front. */
		REQUIRE(said[i].find(secret) == std::string::npos);
		REQUIRE(said[i].find(secret.substr(0, 8)) == std::string::npos);
	}
}

TEST_CASE("a line this cannot read leaves the box with no tokens", "[webconf]")
{
	/* The key is named and the parse cannot deliver it, which is not the same
	   box as one whose file never mentioned it. What they leave behind is the
	   same, no tokens being the least this key can mean and also its default;
	   what tells them apart is that one of them is said. */
	Restore back;
	Tokens held;
	Temp f("tokencut");

	const std::string token = tokenOf('a');
	writeFile(f.path(), "api_tokens=write:" + token + " # our build box\n");
	REQUIRE(httpd::load(f.path()));

	/* This key's own sentence and not the one every cut line gets. That other
	   one names the key too, so a case looking only for the name would be
	   satisfied by a message this key never said. */
	REQUIRE(anyProblemMentions("api_tokens: the line carries tokens this cannot read"));
	REQUIRE(httpd::granted(bearing(token)) == httpd::AuthLevel::Public);
}

TEST_CASE("a file that names no tokens says nothing about them", "[webconf]")
{
	Restore back;
	Tokens held;
	Temp f("tokennone");

	writeFile(f.path(), "port=8081\n");
	REQUIRE(httpd::load(f.path()));

	REQUIRE(httpd::configProblems().empty());
	REQUIRE(httpd::granted(bearing(tokenOf('a'))) == httpd::AuthLevel::Public);
}

TEST_CASE("reading the file twice leaves one record for each token", "[webconf]")
{
	/* The table is emptied before the file fills it. Without that a second
	   read would leave two records for every token, each of them a key
	   derivation on the way to answering a request that presents one, so the
	   cost of a bearer header would grow with the number of times the file had
	   been read. */
	Restore back;
	Tokens held;
	Temp f("tokentwice");

	const std::string first = tokenOf('a');
	writeFile(f.path(), "api_tokens=write:" + first + "\n");
	REQUIRE(httpd::load(f.path()));
	REQUIRE(httpd::granted(bearing(first)) == httpd::AuthLevel::Write);

	// A second file naming another token, which is what a reload is. The one
	// the first file named is gone, because the table is what the file says
	// and not what every file it ever read said.
	const std::string second = tokenOf('b');
	writeFile(f.path(), "api_tokens=read:" + second + "\n");
	REQUIRE(httpd::load(f.path()));
	REQUIRE(httpd::granted(bearing(second)) == httpd::AuthLevel::Read);
	REQUIRE(httpd::granted(bearing(first)) == httpd::AuthLevel::Public);
}

TEST_CASE("a file naming more tokens than this reads drops the rest", "[webconf]")
{
	/* Each one is a key derivation before the daemon starts, so a file naming
	   a thousand is a file that keeps the box off the network for as long as
	   it takes to read them. */
	Restore back;
	Tokens held;
	Temp f("tokenmany");

	std::string line = "api_tokens=";
	std::vector<std::string> written;
	for (unsigned i = 0; i < 20; ++i)
	{
		written.push_back(tokenNumber(i));
		if (i > 0)
			line += ",";
		line += "read:" + written[i];
	}
	writeFile(f.path(), line + "\n");
	REQUIRE(httpd::load(f.path()));

	REQUIRE(anyProblemMentions("api_tokens: the line names more tokens than this reads"));
	REQUIRE(httpd::granted(bearing(written[15])) == httpd::AuthLevel::Read);
	REQUIRE(httpd::granted(bearing(written[16])) == httpd::AuthLevel::Public);
	REQUIRE(httpd::granted(bearing(written[19])) == httpd::AuthLevel::Public);
}

namespace
{

/* A file of the shape a box really carries: the head the migration writes, the
   four keys a screen at the box owns, and two lines belonging to nobody the
   screen offers. What a save has to do with the last three is nothing. */
const char kFileOnABox[] =
	"# ni-web configuration.\n"
	"#\n"
	"# This box has no password, because the file this was written from carried\n"
	"# none.\n"
	"port=8081\n"
	"bind_address=0.0.0.0\n"
	"username=root\n"
	"password_hash=\n"
	"lan_read=10.0.0.0/8\n"
	"legacy_enabled=false\n";

httpd::WebSettings settingsOf(int port, const std::string &bind, const std::string &user)
{
	httpd::WebSettings s;
	s.port = port;
	s.bind_address = bind;
	s.username = user;
	s.change_password = false;
	return s;
}

bool fileHasLine(const std::string &text, const std::string &line)
{
	const std::string wanted = line + "\n";
	if (text.compare(0, wanted.size(), wanted) == 0)
		return true;
	return text.find("\n" + wanted) != std::string::npos;
}

size_t linesNaming(const std::string &text, const std::string &key)
{
	size_t found = 0;
	size_t at = 0;
	while (at <= text.size())
	{
		size_t end = text.find('\n', at);
		if (end == std::string::npos)
			end = text.size();
		const std::string line = text.substr(at, end - at);
		at = end + 1;
		if (line.compare(0, key.size() + 1, key + "=") == 0)
			++found;
	}
	return found;
}

// A port nothing else holds, found the way the server's own cases find one and
// given up again before it is written into a file.
int freePort()
{
	const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
	REQUIRE(fd >= 0);

	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = 0;
	REQUIRE(::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) == 1);
	REQUIRE(::bind(fd, (const struct sockaddr *) &addr, sizeof(addr)) == 0);

	struct sockaddr_in bound;
	socklen_t bound_len = sizeof(bound);
	std::memset(&bound, 0, sizeof(bound));
	REQUIRE(::getsockname(fd, (struct sockaddr *) &bound, &bound_len) == 0);
	const int port = (int) ntohs(bound.sin_port);
	::close(fd);

	REQUIRE(port > 0);
	return port;
}

// A daemon left running would refuse the next case's start for a reason that
// case is not about.
struct StopServer
{
	~StopServer() { httpd::stop(); }
};

} // namespace

TEST_CASE("a save writes the keys it owns and load reads them back", "[webconf]")
{
	Restore back;
	Temp conf("save");

	writeFile(conf.path(), kFileOnABox);
	REQUIRE(httpd::saveWebSettings(conf.path(), settingsOf(9099, "127.0.0.1", "somebody")));

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::config().server.port == 9099);
	REQUIRE(httpd::config().server.bind_address == "127.0.0.1");
	REQUIRE(httpd::config().username == "somebody");
}

TEST_CASE("a save leaves every line it does not own where it stands", "[webconf]")
{
	/* The lines a save does not own are the ones nobody can get back: a comment
	   saying why a box without a password can only be read from its own network
	   is written once, by the migration, and dropping it silently is a loss
	   nothing in this program would ever report. */
	Restore back;
	Temp conf("keep");

	writeFile(conf.path(), kFileOnABox);
	REQUIRE(httpd::saveWebSettings(conf.path(), settingsOf(9099, "127.0.0.1", "somebody")));

	const std::string after = readFile(conf.path());
	REQUIRE(fileHasLine(after, "# ni-web configuration."));
	REQUIRE(fileHasLine(after, "# This box has no password, because the file this was written from carried"));
	REQUIRE(fileHasLine(after, "# none."));
	REQUIRE(fileHasLine(after, "lan_read=10.0.0.0/8"));
	REQUIRE(fileHasLine(after, "legacy_enabled=false"));

	// And the settings that are not the screen's keep their meaning, which is
	// the reason the lines were kept rather than the look of the file.
	REQUIRE(httpd::load(conf.path()));
	REQUIRE_FALSE(httpd::config().legacy_enabled);
	REQUIRE(httpd::config().lan_read.size() == 1);
}

TEST_CASE("a save that was not asked about the password leaves the stored one", "[webconf]")
{
	/* Whoever came to change a port typed nothing in the password field,
	   because there is nothing to show in it. A save that read that field as a
	   password would take the password off the box. */
	Restore back;
	Temp conf("keeppass");

	const std::string stored = httpd::hashSecret("the-one-on-the-box");
	REQUIRE_FALSE(stored.empty());
	writeFile(conf.path(), "port=8081\npassword_hash=" + stored + "\n");

	REQUIRE(httpd::saveWebSettings(conf.path(), settingsOf(9099, "0.0.0.0", "root")));

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::config().password_hash == stored);
	REQUIRE(httpd::verifySecret("the-one-on-the-box", httpd::config().password_hash));
}

TEST_CASE("a password typed at the box is written as a stored form", "[webconf]")
{
	Restore back;
	Temp conf("newpass");

	writeFile(conf.path(), kFileOnABox);

	httpd::WebSettings s = settingsOf(8081, "0.0.0.0", "root");
	s.change_password = true;
	s.password = "not-the-shipped-one";
	REQUIRE(httpd::saveWebSettings(conf.path(), s));

	// The password itself is nowhere in the file, which is the whole point of
	// there being a stored form at all.
	const std::string after = readFile(conf.path());
	REQUIRE(after.find("not-the-shipped-one") == std::string::npos);

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::verifySecret("not-the-shipped-one", httpd::config().password_hash));
	REQUIRE_FALSE(httpd::verifySecret("the-shipped-one", httpd::config().password_hash));
}

TEST_CASE("a line naming a key another way does not survive a save", "[webconf]")
{
	/* Left standing, either of these would leave the file naming a port and the
	   server running on the default, because load counts a name it can take for
	   a key as that key being named and unusable whatever else the file says. */
	Restore back;
	Temp conf("otherspelling");

	writeFile(conf.path(),
		  "Port=80\n"
		  " username = somebody\n"
		  "bind_address=127.0.0.1 # only from the box itself\n");

	REQUIRE(httpd::saveWebSettings(conf.path(), settingsOf(9099, "0.0.0.0", "root")));

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::configProblems().empty());
	REQUIRE(httpd::config().server.port == 9099);
	REQUIRE(httpd::config().server.bind_address == "0.0.0.0");
	REQUIRE(httpd::config().username == "root");
}

TEST_CASE("a key the file named twice is one line after a save", "[webconf]")
{
	// Two lines naming one key are read as whichever came last, so a file left
	// saying one thing twice is a file that can be read two ways.
	Restore back;
	Temp conf("twice");

	writeFile(conf.path(), "port=8081\nlan_read=10.0.0.0/8\nport=8082\n");
	REQUIRE(httpd::saveWebSettings(conf.path(), settingsOf(9099, "0.0.0.0", "root")));

	const std::string after = readFile(conf.path());
	REQUIRE(linesNaming(after, "port") == 1);
	REQUIRE(fileHasLine(after, "port=9099"));
	REQUIRE(fileHasLine(after, "lan_read=10.0.0.0/8"));
}

TEST_CASE("a value that cannot be used leaves the file exactly as it was", "[webconf]")
{
	Restore back;

	SECTION("a port outside the range the protocol has")
	{
		Temp conf("badport");
		writeFile(conf.path(), kFileOnABox);

		REQUIRE_FALSE(httpd::saveWebSettings(conf.path(), settingsOf(0, "0.0.0.0", "root")));
		REQUIRE(readFile(conf.path()) == kFileOnABox);
		REQUIRE(anyProblemMentions("port"));
	}

	SECTION("an address that is not one")
	{
		Temp conf("badaddr");
		writeFile(conf.path(), kFileOnABox);

		REQUIRE_FALSE(httpd::saveWebSettings(conf.path(), settingsOf(8081, "localhost", "root")));
		REQUIRE(readFile(conf.path()) == kFileOnABox);
		REQUIRE(anyProblemMentions("bind_address"));
	}

	SECTION("a name nothing answers to")
	{
		Temp conf("badname");
		writeFile(conf.path(), kFileOnABox);

		REQUIRE_FALSE(httpd::saveWebSettings(conf.path(), settingsOf(8081, "0.0.0.0", "")));
		REQUIRE(readFile(conf.path()) == kFileOnABox);
		REQUIRE(anyProblemMentions("username"));
	}

	SECTION("a name carrying a byte that does not read back as it was written")
	{
		Temp conf("cutname");
		writeFile(conf.path(), kFileOnABox);

		REQUIRE_FALSE(httpd::saveWebSettings(conf.path(), settingsOf(8081, "0.0.0.0", "some#body")));
		REQUIRE(readFile(conf.path()) == kFileOnABox);
		REQUIRE(anyProblemMentions("username"));
	}
}

TEST_CASE("a box with no file gets one only its owner can read", "[webconf]")
{
	// The other way a box comes to have this file, the migration being the
	// first, and it carries a credential either way.
	Restore back;
	Temp conf("fresh");

	REQUIRE(httpd::saveWebSettings(conf.path(), settingsOf(9099, "0.0.0.0", "root")));
	REQUIRE(modeOf(conf.path()) == (S_IRUSR | S_IWUSR));

	REQUIRE(httpd::load(conf.path()));
	REQUIRE(httpd::config().server.port == 9099);
	REQUIRE(httpd::configProblems().empty());
}

TEST_CASE("a save is refused on a name that is a link", "[webconf]")
{
	/* What puts the new content in place puts a file at this name, so a link
	   here would have the link replaced and the file it names left exactly as
	   it was. */
	Restore back;
	Temp real("linktarget"), link("linkname");

	writeFile(real.path(), kFileOnABox);
	REQUIRE(::symlink(real.c_str(), link.c_str()) == 0);

	REQUIRE_FALSE(httpd::saveWebSettings(link.path(), settingsOf(9099, "0.0.0.0", "root")));
	REQUIRE(readFile(real.path()) == kFileOnABox);
	REQUIRE(anyProblemMentions("is a link"));

	::unlink(link.c_str());
}

TEST_CASE("the server comes back up on the port the file now names", "[webconf]")
{
	/* The property the screen at the box exists for. A save that reached the
	   file and a daemon still answering on the port before it is a box whose
	   file and whose socket say two different things. */
	Restore back;
	StopServer done;
	Temp conf("restart");

	const int first = freePort();
	writeFile(conf.path(), "bind_address=127.0.0.1\nlan_read=127.0.0.0/8\n");

	httpd::WebSettings s = settingsOf(first, "127.0.0.1", "root");
	REQUIRE(httpd::saveWebSettings(conf.path(), s));
	REQUIRE(httpd::reloadAndRestart(conf.path()));
	REQUIRE(httpd::running());
	REQUIRE(httpd::boundPort() == first);

	const int second = freePort();
	REQUIRE(second != first);
	s.port = second;
	REQUIRE(httpd::saveWebSettings(conf.path(), s));
	REQUIRE(httpd::reloadAndRestart(conf.path()));
	REQUIRE(httpd::running());
	REQUIRE(httpd::boundPort() == second);
	REQUIRE(httpd::config().server.port == second);
}

TEST_CASE("a reload that cannot read the file leaves the server on what it had", "[webconf]")
{
	/* A file somebody deleted is not a reason to take the box off the network,
	   and it is not a reason to say nothing either. */
	Restore back;
	StopServer done;
	Temp conf("reloadgone");

	const int port = freePort();
	writeFile(conf.path(), "bind_address=127.0.0.1\n");
	REQUIRE(httpd::saveWebSettings(conf.path(), settingsOf(port, "127.0.0.1", "root")));
	REQUIRE(httpd::reloadAndRestart(conf.path()));
	REQUIRE(httpd::boundPort() == port);

	::unlink(conf.c_str());
	REQUIRE_FALSE(httpd::reloadAndRestart(conf.path()));

	REQUIRE(httpd::running());
	REQUIRE(httpd::boundPort() == port);
	REQUIRE(anyProblemMentions("could not be read again"));
}
