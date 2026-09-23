/*
 * webconfig.cpp - the webserver own configuration file
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

#include "webconfig.h"

/* Where the shipped docs page is installed, for the default below: read here
   and not carried in as a string of its own, so the two cannot name different
   places. */
#include <config.h>

#include "auth.h"
#include "credentials.h"

#include <configfile.h>

#include <openssl/crypto.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace httpd
{

namespace
{

/* The keys this file knows, all of them flat. The old file grouped them under
   the name of a website and of a module, which was how one process served
   several sites; this one serves the box it runs on. */
const char kKeyPort[]            = "port";
const char kKeyBind[]            = "bind_address";
const char kKeyUser[]            = "username";
const char kKeyHash[]            = "password_hash";
const char kKeyLanRead[]         = "lan_read";
const char kKeyTrustedProxies[]  = "trusted_proxies";
const char kKeyDocroot[]         = "docroot";
const char kKeySessionLifetime[] = "session_lifetime_s";
const char kKeySseMaxStreams[]   = "sse_max_streams";
const char kKeyLegacy[]          = "legacy_enabled";
const char kKeyLogos[]           = "channel_logos";
const char kKeyApiTokens[]       = "api_tokens";

// Read where a message would otherwise have to name a key it found in a file.
// A key out of a file is text somebody else wrote; one of these is not.
const char *const kKnownKeys[] =
{
	kKeyPort, kKeyBind, kKeyUser, kKeyHash, kKeyLanRead,
	kKeyTrustedProxies, kKeyDocroot, kKeySessionLifetime,
	kKeySseMaxStreams, kKeyLegacy, kKeyLogos, kKeyApiTokens
};

// What the old file calls the same things, where it has them at all.
const char kOldHost[]     = "WebsiteMain.host";
const char kOldPort[]     = "WebsiteMain.port";
const char kOldUser[]     = "mod_auth.username";
const char kOldPassword[] = "mod_auth.password";
const char kOldSwitch[]   = "mod_auth.authenticate";
const char kOldExempt[]   = "mod_auth.no_auth_client";

/* The password the other server ships with, which is not a password anybody
   chose. It stands in that server's own configuration file two lines under the
   switch that turns its checking off, and it is written into that server a
   second time as the value used when the file names none, so it is published
   twice in the source of every image. Carrying it would hand the level that
   shuts the box down to whoever has read either place. */
const char kOldShippedPassword[] = "ni";

/* How long a session may stand. A day by default. The bounds are here because
   this is a security parameter and both ends of it are a way to be wrong: a
   lifetime of seconds is a box nobody can stay logged in to, and one of years
   is a stolen token that never stops working. The shortest of them is also
   what a value this cannot use falls back to, for the reason the header
   states. */
const unsigned kSessionLifetimeDefault = 86400u;
const unsigned kSessionLifetimeMin     = 60u;
const unsigned kSessionLifetimeMax     = 2592000u;

/* How many event streams may be open at once. Zero is a real answer and means
   none: a box configured that way offers no live updates rather than offering
   an unbounded number of them, which is also why zero is what an unusable
   value falls back to. The ceiling is what the connection limit can carry
   without every slot being an idle stream. */
const unsigned kSseMaxStreamsDefault = 8u;
const unsigned kSseMaxStreamsMax     = 64u;

/* The shortest token this accepts, in characters of the hexadecimal a token is written
   as, which is half as many bytes.

   The lookup prefix is the first eight of them and is a key rather than a secret, so a
   token only a little longer than the prefix is one that can be worked through. This is
   a hundred and twenty eight bits, which cannot be, and what this server mints for a
   session is twice as long again. The floor is here and not in the reader that recovers
   the prefix, because that reader answers about the shape of a token and this is a
   policy about a credential somebody is about to write down by hand.

   No ceiling beside it. A token longer than this costs one key derivation either way. */
const size_t kMinTokenChars = 32;

/* How many of them one file may name. Each one is a key derivation before the
   daemon starts, of the same thousands of rounds a login costs, so a file
   naming a thousand would be a file that keeps the box off the network for as
   long as it takes to read them. A box has a handful of programs talking to
   it, and the ones past this are dropped and named rather than read. */
const size_t kMaxApiTokens = 16;

/* What a bind address becomes when the file names one this cannot recover.

   Not an address in either family, so the call that binds refuses it and the
   server does not come up. That is the answer here rather than the default,
   because the default is the wildcard and every line naming an interface is
   narrower than it. It is also visibly not something anybody typed, which is
   what distinguishes it from a value carried through as it stands. */
const char kNoAddress[] = "";

/* What credentials.h writes in front of a stored form. Named here only to say
   that a value which does not begin with it was probably mistyped. It is not a
   second judge of what a stored form may be: the value is kept whatever this
   says about it and verifySecret decides, so that a scheme added there is not
   refused here by a copy of the rule that never heard of it. */
const char kHashMarker[] = "pbkdf2-sha256$";

/* Nine digits and not ten. The long on the boxes this runs on is four bytes,
   so ten digits reach past what one holds and would be accumulated into a
   value smaller than the one written. Nine covers every number this file has a
   key for with four orders of magnitude to spare. */
const size_t kMaxDigits = 9;

/* The most of a file this reads. A configuration is a handful of lines, and
   both the parse and the scan beside it hold the whole of one in memory at
   once, so a file past this is either not a configuration or is there to make
   the daemon allocate on somebody else's word. Refused whole rather than read
   in part, because a scan that stopped early would pass a line it never
   looked at. */
const off_t kMaxFileBytes = 1 << 20;

/* The most complaints one load keeps. A file inside the ceiling above can hold
   a quarter of a million lines this cannot use, and every other quantity here
   is bounded; a list of messages about a file should not be the one thing that
   grows with the file. Past this one last message says so and the rest are
   counted by nobody. */
const size_t kMaxProblems = 100;

/* The most of a value a message repeats. Long enough to recognise a network or
   a number and short enough that no one line of a file becomes one message the
   size of the line. */
const size_t kEchoBytes = 64;

std::vector<std::string> &problemList()
{
	static std::vector<std::string> held;
	return held;
}

WebConfig &held()
{
	static WebConfig c = defaultWebConfig();
	return c;
}

std::string &heldPath()
{
	static std::string p;
	return p;
}

std::string decimal(unsigned long v)
{
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%lu", v);
	return std::string(buf);
}

/* What a message may repeat of a value.

   A message goes to a terminal and into a list something later renders, and a
   value out of a file is bytes somebody else chose. A byte that moves a cursor,
   ends a line or ends a C string is replaced rather than passed on, so one
   value cannot rewrite the message around it and cannot cut it short where the
   printing stops at a zero byte. */
std::string printable(const std::string &v)
{
	const size_t show = (v.size() < kEchoBytes) ? v.size() : kEchoBytes;

	std::string out;
	out.reserve(show + 3);
	for (size_t i = 0; i < show; ++i)
	{
		const unsigned char c = static_cast<unsigned char>(v[i]);
		out += (c >= 0x20 && c < 0x7f) ? static_cast<char>(c) : '?';
	}
	if (v.size() > show)
		out += "...";
	return out;
}

std::string quoted(const std::string &v)
{
	return "\"" + printable(v) + "\"";
}

/* Said once to the operator and once to whoever asks the list. The standard error is
   where a box that never runs a test shows it, and the list is where a case can see that
   it was said at all: a value dropped without a word looks exactly like a value the file
   never carried.

   Nothing a caller of this hands over carries a value whose key makes it a credential,
   and nothing carries a raw line of a file. */
void complain(const std::string &line)
{
	const size_t held_now = problemList().size();
	if (held_now > kMaxProblems)
		return;

	if (held_now == kMaxProblems)
	{
		const std::string last = "the file has more lines this cannot use than are worth listing, and "
					 "the rest are not named here";
		problemList().push_back(last);
		std::fprintf(stderr, "[ni-web config] %s\n", last.c_str());
		return;
	}

	problemList().push_back(line);
	std::fprintf(stderr, "[ni-web config] %s\n", line.c_str());
}

/* Whether a value survives being written to this file and read back out of it.

   The parser under this splits a line at the first separator and cuts the rest off at
   the first number sign, and neither side escapes anything. So a value carrying the byte
   that ends a line is read back as a value and then as a setting of its own under
   whatever key the rest of it names; a value carrying a number sign is read back shorter
   than it was written; and a space at either end is kept while nothing that shows the
   value can show it. The other control bytes go with the first.

   The separator needs no rule: the split is at the first one, which is the one this
   wrote.

   The number sign is a rule about what may be written and not about what arrives: a
   value that reached this out of a file has already had everything from the first one
   cut off it. What it catches on that side is what the parse does keep, which is the
   space at either end and every control byte but the one that ended the line. */
bool survivesTheFile(const std::string &v)
{
	for (size_t i = 0; i < v.size(); ++i)
	{
		const unsigned char c = static_cast<unsigned char>(v[i]);
		if (c < 0x20 || c == 0x7f)
			return false;
		if (c == '#')
			return false;
	}

	return v.empty() || (v[0] != ' ' && v[v.size() - 1] != ' ');
}

// Digits and nothing else. The library's own reader is atoi, which answers
// zero for a line that is not a number at all, and zero is a port the kernel
// chooses and a stream count that turns the feature off.
bool readUnsigned(const std::string &text, unsigned long &out)
{
	if (text.empty() || text.size() > kMaxDigits)
		return false;

	unsigned long v = 0;
	for (size_t i = 0; i < text.size(); ++i)
	{
		const char c = text[i];
		if (c < '0' || c > '9')
			return false;
		v = (v * 10) + static_cast<unsigned long>(c - '0');
	}

	out = v;
	return true;
}

// An address and not a name. A name would have to be resolved to be bound, and
// what it resolves to is not what the file says.
bool isAddressLiteral(const std::string &text)
{
	if (text.empty() || text.find('\0') != std::string::npos)
		return false;

	struct in_addr v4;
	if (inet_pton(AF_INET, text.c_str(), &v4) == 1)
		return true;

	struct in6_addr v6;
	return inet_pton(AF_INET6, text.c_str(), &v6) == 1;
}

/* A list as the file carries one. An empty value is a list of nothing and not
   a list holding one empty entry, so a key written with nothing after it means
   nobody rather than one unreadable somebody. */
std::vector<std::string> splitOnComma(const std::string &text)
{
	std::vector<std::string> out;
	if (text.empty())
		return out;

	size_t at = 0;
	for (;;)
	{
		const size_t comma = text.find(',', at);
		if (comma == std::string::npos)
		{
			out.push_back(text.substr(at));
			return out;
		}
		out.push_back(text.substr(at, comma - at));
		at = comma + 1;
	}
}

typedef std::map<std::string, std::string> Pairs;

// A key the file names on a line this cannot recover, and the line it was on.
typedef std::map<std::string, unsigned long> CutShort;

/* The file as it stands, for the one question the parser's answer cannot be asked: what
   a line held before the parse cut it. This is the second full read of the same file in
   one load, and it exists only to recover what the parse threw away.

   Bounded by the same ceiling the caller checked before reading anything, so the bound
   does not depend on a stat taken earlier still being true. Empty for a file that could
   not be opened, which the caller has already refused. */
std::string wholeFile(const std::string &path)
{
	std::string text;
	const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return text;

	char buf[4096];
	while (text.size() < static_cast<size_t>(kMaxFileBytes))
	{
		const ssize_t n = ::read(fd, buf, sizeof(buf));
		if (n < 0)
		{
			if (errno == EINTR)
				continue;
			break;
		}
		if (n == 0)
			break;
		text.append(buf, static_cast<size_t>(n));
	}

	::close(fd);
	return text;
}

bool isSpace(char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f';
}

std::string trimmed(const std::string &v)
{
	size_t first = 0;
	while (first < v.size() && isSpace(v[first]))
		++first;

	size_t last = v.size();
	while (last > first && isSpace(v[last - 1]))
		--last;

	return v.substr(first, last - first);
}

bool isKnownKey(const std::string &key)
{
	const size_t count = sizeof(kKnownKeys) / sizeof(kKnownKeys[0]);
	for (size_t i = 0; i < count; ++i)
	{
		if (key == kKnownKeys[i])
			return true;
	}
	return false;
}

/* Its own fold and not the library's, which reads the process locale. A key
   here is ASCII and the locale a daemon inherits is nobody's decision in
   particular, so a fold that changed with it would make the same file mean two
   things on two boxes. */
std::string foldedAscii(const std::string &v)
{
	std::string out(v);
	for (size_t i = 0; i < out.size(); ++i)
	{
		if (out[i] >= 'A' && out[i] <= 'Z')
			out[i] = static_cast<char>(out[i] - 'A' + 'a');
	}
	return out;
}

/* The known key a name can be taken for, or nothing.

   Matched without regard to case. A name differing from one of these only in case cannot
   plausibly mean any other key, and where the intent is unmistakable the safe reading of
   it is the narrow one: the alternative is a line an operator wrote to narrow something
   being read as a key nobody wrote, which is the default, which for two of these keys is
   wider than any line a person would write there.

   The answer is the key as this file spells it. */
std::string knownKeyFor(const std::string &key)
{
	const std::string folded = foldedAscii(key);
	const size_t count = sizeof(kKnownKeys) / sizeof(kKnownKeys[0]);
	for (size_t i = 0; i < count; ++i)
	{
		if (folded == kKnownKeys[i])
			return std::string(kKnownKeys[i]);
	}
	return std::string();
}

/* A key of a file named back to the operator, or nothing when it is not one of
   ours.

   A key is the half of a line before the separator and a value is the half
   after, but a line the parse cut short in front of the separator makes a key
   of most of the line, so naming an arbitrary key would name whatever that line
   held. The line number identifies it either way. */
std::string namedIfKnown(const std::string &key)
{
	return isKnownKey(key) ? (" (" + key + ")") : std::string();
}

/* What is wrong with each line of a file, which the parse cannot be asked
   because it has already thrown the evidence away. */
struct LineFaults
{
	/* Every known key the file names on a line that will not hand a value over
	   under that name, and the line it was on. This is what the reading below
	   consults, so such a key is present and unreadable rather than absent,
	   which is the difference between the least the key can mean and whatever
	   the default happens to be. */
	CutShort unreadable;

	/* Every key a number sign cut short, whatever the key. The migration reads
	   this one and nothing else: it carries the short value on purpose, because
	   that is the value the old server compared against, and only needs to say
	   which line made it short. */
	CutShort cut_short;

	// What to say about each faulty line, in the order the lines appear.
	std::vector<std::string> notes;
};

/* Three ways a line names a key and does not deliver a value under it.

   THE NUMBER SIGN. The parse cuts a line off at the first one, so the line reaches the
   map holding a value shorter than the one that was written, and nothing about the short
   value says it was ever longer. A line whose number sign comes before the separator is
   one of these too: the parse looks for the separator first, so a line somebody wrote as
   a comment about a setting becomes a setting under a key spelled with the number sign
   in it, and the length it works out for the value goes round through zero on the way
   and takes the whole tail of the line.

   THE NAME AS IT IS SPELLED. The parse takes the name to be every byte in front of the
   separator, untrimmed and case sensitive, so

       lan_read = 10.9.0.0/16
       LAN_READ=10.9.0.0/16

   both hand the value over under a name nothing here ever asks for. Left alone the key
   is genuinely absent, which means the default, which for that key is six private
   networks where the line named one. That is the same outcome the rule in the header
   exists to prevent, reached by the most ordinary things a person does when writing a
   file, so a line whose name can be taken for one of ours counts as that key being
   present and unreadable.

   Taken for, and not read as: the value is not used under the right name either. What
   the name settles is which key was meant, and therefore which least value stands in its
   place.

   A NAME THIS CANNOT BE TAKEN FOR ANY KEY. Said and no more: refusing the file for one
   would make a working configuration unusable after an upgrade.

   judge_names is off for the old server's file. Its keys are that server's, and calling
   a hundred of them unknown would say a hundred wrong things about a file this takes
   four values out of. */
LineFaults scanLines(const std::string &path, bool judge_names)
{
	LineFaults out;
	const std::string text = wholeFile(path);

	size_t at = 0;
	unsigned long lineno = 0;
	while (at <= text.size())
	{
		size_t end = text.find('\n', at);
		if (end == std::string::npos)
			end = text.size();

		const std::string line = text.substr(at, end - at);
		at = end + 1;
		++lineno;

		// A line with no separator is a comment or a blank, and the parse
		// passes over it as this does.
		const size_t sep = line.find('=');
		if (sep == std::string::npos)
			continue;

		const std::string key = line.substr(0, sep);
		const std::string taken_for = knownKeyFor(trimmed(key));

		if (line.find('#') != std::string::npos)
		{
			// First wins, so that a message names the line that took the key
			// out rather than the last line to mention it.
			out.cut_short.insert(std::make_pair(key, lineno));
			if (!judge_names)
				continue;

			out.unreadable.insert(std::make_pair(taken_for.empty() ? key : taken_for, lineno));
			out.notes.push_back("line " + decimal(lineno) + namedIfKnown(taken_for) +
					    ": a number sign is where a value is cut off when it is read "
					    "back, so this line is not read at all");
			continue;
		}

		if (!judge_names || key == taken_for)
			continue;

		if (!taken_for.empty())
		{
			out.unreadable.insert(std::make_pair(taken_for, lineno));
			out.notes.push_back("line " + decimal(lineno) + " (" + taken_for + "): the name is read "
					    "byte for byte and this one is not written as it is read, so the "
					    "value never reaches " + taken_for + ". It can be taken for no "
					    "other key, so what stands in its place is the least " + taken_for +
					    " can mean rather than the default");
			continue;
		}

		out.notes.push_back("line " + decimal(lineno) + ": " + quoted(key) +
				    " is not a key this reads, and the line has no effect");
	}

	return out;
}

/* What the file said about a key, which is three answers and not two.

   A key nobody wrote keeps the default without a word. A key written wrong is
   dropped with one, and what it is dropped to is not the default: see the rule
   in the header. Collapsing those two is how a line meant to narrow something
   ends up widening it. */
enum Reading
{
	KeyAbsent,
	KeyUnreadable,
	KeyPresent
};

// Whether the old server's file said anything at all under this key. Two
// answers and not the three below: a line the parse cut short is used there as
// the parse left it, because that is the value the old server used too.
bool present(const Pairs &m, const char *key, std::string &out)
{
	const Pairs::const_iterator it = m.find(key);
	if (it == m.end())
		return false;
	out = it->second;
	return true;
}

Reading reading(const Pairs &m, const CutShort &cut, const char *key, std::string &out)
{
	if (cut.find(key) != cut.end())
		return KeyUnreadable;

	const Pairs::const_iterator it = m.find(key);
	if (it == m.end())
		return KeyAbsent;

	out = it->second;
	return KeyPresent;
}

/* Reads one list of networks and answers what the file said about the key.

   An entry that is not one is dropped and named, and the rest of the list still stands:
   dropping narrows what is admitted, which is the direction to be wrong in. A list that
   cannot be recovered at all becomes empty rather than the seeded default, because the
   seeded default is six private networks and every line an operator writes here is
   narrower than that.

   The answer is here because an empty list does not mean the same thing under both keys
   that use this, and the caller is the only place that knows which. Empty is the least
   lan_read can mean and it is not the least trusted_proxies can mean: what an empty
   proxy list does is turn the read exemption back on, so the key's caller reads whether
   the key was named at all rather than reading the list it left behind. */
Reading readPrefixList(const Pairs &m, const CutShort &cut, const char *key,
		       const char *closed_says, std::vector<NetPrefix> &out)
{
	std::string raw;
	const Reading r = reading(m, cut, key, raw);

	if (r == KeyAbsent)
		return r;

	if (r == KeyUnreadable)
	{
		complain(std::string(key) + ": " + closed_says);
		out.clear();
		return r;
	}

	std::vector<NetPrefix> read;
	const std::vector<std::string> parts = splitOnComma(raw);
	for (size_t i = 0; i < parts.size(); ++i)
	{
		NetPrefix p;
		if (parsePrefix(parts[i], &p))
			read.push_back(p);
		else
			complain(std::string(key) + ": " + quoted(parts[i]) + " is not a network, and it is dropped");
	}

	out = read;
	return r;
}

// closed is what the key means least, which is what a value this cannot use
// falls back to. The default is not that, and is only what a key nobody wrote
// falls back to.
void readUnsignedField(const Pairs &m, const CutShort &cut, const char *key,
		       unsigned lo, unsigned hi, unsigned closed, unsigned &out)
{
	std::string raw;
	const Reading r = reading(m, cut, key, raw);

	if (r == KeyAbsent)
		return;

	if (r == KeyUnreadable)
	{
		complain(std::string(key) + ": the line carries a number this cannot read, so it stands at " +
			 decimal(closed) + ", which is the least it can mean");
		out = closed;
		return;
	}

	unsigned long v = 0;
	if (!readUnsigned(raw, v) || v < lo || v > hi)
	{
		complain(std::string(key) + ": " + quoted(raw) + " is not a number from " + decimal(lo) +
			 " to " + decimal(hi) + ", so it stands at " + decimal(closed) +
			 ", which is the least it can mean");
		out = closed;
		return;
	}

	out = static_cast<unsigned>(v);
}

/* Reads a value that has to survive the file to mean anything.

   Never repeats the value. Two of the three keys read through this are a name
   and a stored form, and the third is a path; none of them is worth showing to
   whoever is reading the message and the first two are what an attack is aimed
   at. */
void readTextField(const Pairs &m, const CutShort &cut, const char *key,
		   const std::string &closed, std::string &out)
{
	std::string raw;
	const Reading r = reading(m, cut, key, raw);

	if (r == KeyAbsent)
		return;

	if (r == KeyUnreadable)
	{
		complain(std::string(key) + ": the line carries a value this cannot read, and what stands in "
			 "its place is the least the key can mean");
		out = closed;
		return;
	}

	if (!survivesTheFile(raw))
	{
		complain(std::string(key) + ": the value carries a control byte or a space at one of its ends, "
			 "neither of which reads back as it was written, and what stands in its place is the "
			 "least the key can mean");
		out = closed;
		return;
	}

	out = raw;
}

/* Wipes a value that was somebody's password before it was anything else.

   What this reaches is the copy this file made. The parser under it holds its
   own and hands out copies of that, and none of those can be reached from
   here, so this narrows the window rather than closing it. What it does close
   is the part that matters: nothing this writes carries the cleartext, and no
   file is created holding it. */
void forget(std::string &secret)
{
	if (!secret.empty())
		OPENSSL_cleanse(&secret[0], secret.size());
	secret.clear();
}

/* One of the three levels a token may be given, or false.

   Asked of this server's own names for them rather than of a list written out
   here, so the word in a file and the word an answer carries cannot drift
   apart. Public is not among them on purpose: a token granting it grants what
   a caller presenting nothing already has, so a line asking for one is a line
   whose author meant one of the other three. */
bool grantableLevel(const std::string &word, AuthLevel &out)
{
	static const AuthLevel kGrantable[] = { AuthLevel::Read, AuthLevel::Write, AuthLevel::System };

	for (size_t i = 0; i < sizeof(kGrantable) / sizeof(kGrantable[0]); ++i)
	{
		if (word == authLevelName(kGrantable[i]))
		{
			out = kGrantable[i];
			return true;
		}
	}
	return false;
}

/* One entry of that key, recorded or dropped.

   NO PART OF AN ENTRY IS EVER REPEATED IN A MESSAGE, which is why these say which entry
   they mean by counting rather than by quoting it as every other message in this file
   does. Half of an entry is a token, the messages reach a terminal and a page, and an
   entry written the wrong way round would put the token where the level belongs.

   The token is wiped once it has been written down. What that is worth is what the wipe
   under the migration is worth and no more: the parser this was read through holds
   copies of its own that nothing here can reach, and the file itself still holds the
   token. */
void takeApiToken(const std::string &entry, size_t at)
{
	const std::string said = std::string(kKeyApiTokens) + ": entry " + decimal(at) + " ";

	const size_t colon = entry.find(':');
	if (colon == std::string::npos)
	{
		complain(said + "is not a level and a token with a colon between them, and it is dropped");
		return;
	}

	AuthLevel level = AuthLevel::Public;
	if (!grantableLevel(entry.substr(0, colon), level))
	{
		complain(said + "does not begin with read, write or system, and it is dropped. A token "
			 "carries the level it is worth, or every token would be worth the most there is");
		return;
	}

	std::string token = entry.substr(colon + 1);
	/* The shape of a token and the prefix a record is found under come off one
	   reader, so a token this stores can be found again by the header that
	   presents it. It answers nothing for anything outside lower case
	   hexadecimal, which is every hand written line this should refuse. */
	const std::string prefix = tokenLookupPrefix(token);
	const bool usable = !prefix.empty() && token.size() >= kMinTokenChars;
	const std::string hashed = usable ? hashSecret(token) : std::string();
	forget(token);

	if (!usable)
	{
		complain(said + "does not carry a token of at least " + decimal(kMinTokenChars) +
			 " characters of lower case hexadecimal, and it is dropped");
		return;
	}

	if (hashed.empty())
	{
		complain(said + "carries a token this box could not write down, and it is dropped");
		return;
	}

	addApiToken(prefix, hashed, level);
}

/* The tokens a program presents in place of a cookie, read into the table a bearer
   header is answered out of.

   THE VALUE IS THE TOKENS THEMSELVES AND NOT THE STORED FORM OF THEM, which is the
   opposite of the password above. Nothing in this product writes a stored form out: the
   derivation is behind this server and no tool of the box reaches it, so a key that
   demanded one would be a key nobody could write, and the whole path a program
   authenticates by would stay unreachable. What an operator has in hand is the token
   itself, because they have to give it to the program that will present it. This file is
   already where the box keeps its credentials, it is created readable by its owner
   alone, and whoever can read it can log in with the password beside these in any case.

   What is kept is the lookup prefix and the stored form, so nothing answers a request out
   of a table holding a token.

   Absent and unreadable come to the same thing here, which no other key in this file can
   say. The line is still said when it is named and unreadable, because a file whose only
   line about this was mistyped and a file that says nothing about it are the same box
   from outside and are not the same mistake. */
void readApiTokens(const Pairs &m, const CutShort &cut)
{
	forgetApiTokens();

	std::string raw;
	const Reading r = reading(m, cut, kKeyApiTokens, raw);
	if (r == KeyAbsent)
		return;

	if (r == KeyUnreadable)
	{
		complain(std::string(kKeyApiTokens) + ": the line carries tokens this cannot read, and the box "
			 "is left with none of them, which is the least the key can mean");
		return;
	}

	std::vector<std::string> parts = splitOnComma(raw);
	forget(raw);

	size_t taken = parts.size();
	if (taken > kMaxApiTokens)
	{
		taken = kMaxApiTokens;
		complain(std::string(kKeyApiTokens) + ": the line names more tokens than this reads, and "
			 "everything past the first " + decimal(kMaxApiTokens) + " of them is dropped");
	}

	for (size_t i = 0; i < parts.size(); ++i)
	{
		if (i < taken)
		{
			/* A space at either end of an entry is forgiven, which the two
			   network lists above do not do for theirs. What an entry holds is
			   a word out of a closed set of three and a run of hexadecimal,
			   and neither of those can mean anything else for having a space
			   beside it, so forgiving one cannot admit anybody the line did
			   not name. What not forgiving one costs is a credential dropped
			   over the way people write a list. */
			std::string entry = trimmed(parts[i]);
			takeApiToken(entry, i + 1);
			forget(entry);
		}
		forget(parts[i]);
	}
}

/* Commits the directory entry that names a file, which the file's own sync
   does not.

   Without this a migration that answered true can be followed by a power cut
   and a box that comes up with no file at all. That matters once the
   switchover removes the old server's file, at which point neither exists. */
bool syncDirectoryOf(const std::string &path)
{
	const size_t slash = path.rfind('/');
	const std::string dir = (slash == std::string::npos) ? std::string(".")
			      : (slash == 0 ? std::string("/") : path.substr(0, slash));

	const int fd = ::open(dir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (fd < 0)
		return false;

	const bool synced = (::fsync(fd) == 0);
	return (::close(fd) == 0) && synced;
}

/* All of it or none of it, at the mode it keeps.

   Created rather than opened: the exclusive flag refuses a file that is already there and
   refuses a symbolic link pointing at one, so this never writes a credential through a
   name somebody else chose the target of, and never widens a file it did not create. The
   mode is given to the call that creates it, so there is no moment between the creation
   and a narrowing at which the file is readable by anyone.

   Anything that stops short of a file that is there and will still be there takes the
   file with it, the directory entry included. What would be left of a partial write is a
   truncated stored form, which verifies against nothing and would be a box nobody can log
   in to that looks configured. */
bool writeWholeFile(int fd, const std::string &text)
{
	size_t done = 0;
	while (done < text.size())
	{
		const ssize_t n = ::write(fd, text.data() + done, text.size() - done);
		if (n < 0)
		{
			if (errno == EINTR)
				continue;
			return false;
		}
		// Not something a regular file does, and a loop that treated it as one
		// would spin rather than end.
		if (n == 0)
			return false;
		done += static_cast<size_t>(n);
	}

	return true;
}

bool writeNewFile(const std::string &path, const std::string &text)
{
	const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, S_IRUSR | S_IWUSR);
	if (fd < 0)
	{
		complain(std::string("cannot create ") + path + ": " + std::strerror(errno));
		return false;
	}

	bool ok = writeWholeFile(fd, text);

	if (ok && ::fsync(fd) != 0)
		ok = false;
	if (::close(fd) != 0)
		ok = false;
	if (ok && !syncDirectoryOf(path))
		ok = false;

	if (!ok)
	{
		complain(std::string("cannot write ") + path + ", and what was written of it is removed");
		::unlink(path.c_str());
	}

	return ok;
}

void appendPair(std::string &text, const char *key, const std::string &value)
{
	text += key;
	text += '=';
	text += value;
	text += '\n';
}

// How wide a comment of this file is folded. What has to fit is a terminal
// somebody opened over a shell after being refused a login.
const size_t kCommentWidth = 76;

/* One line of prose as a comment of this file.

   THE SEPARATOR IS TAKEN OUT HERE RATHER THAN TRUSTED NOT TO BE THERE. The parser reads
   any line carrying one as a key and a value, whatever else is on the line, so a number
   sign in front buys nothing. What comes through here can be a user name or a complaint
   repeating a value out of somebody else's file, and one separator in either would turn a
   comment into a setting and then into a line every later load reports as unreadable. */
void appendCommentLine(std::string &text, const std::string &line)
{
	text += '#';
	if (!line.empty())
		text += ' ';
	for (size_t i = 0; i < line.size(); ++i)
		text += (line[i] == '=') ? ' ' : line[i];
	text += '\n';
}

// A paragraph written out here as it stands, one comment to a line.
void appendComment(std::string &text, const std::string &block)
{
	size_t at = 0;
	while (at < block.size())
	{
		const size_t end = block.find('\n', at);
		const bool last = (end == std::string::npos);
		appendCommentLine(text, last ? block.substr(at) : block.substr(at, end - at));
		at = last ? block.size() : end + 1;
	}
}

/* A sentence nobody could wrap where it was written, folded to the width
   above. What comes through here is built at the moment of writing out of a
   path, a name or a complaint, so its length is not known anywhere it could
   have been broken by hand, and unfolded it is one line as long as the
   message. */
void appendWrapped(std::string &text, const std::string &lead, const std::string &indent,
		   const std::string &body)
{
	std::string line = lead;
	bool empty = true;
	size_t at = 0;

	while (at < body.size())
	{
		const size_t end = body.find(' ', at);
		const bool last = (end == std::string::npos);
		const std::string word = last ? body.substr(at) : body.substr(at, end - at);
		at = last ? body.size() : end + 1;

		if (word.empty())
			continue;

		if (!empty && line.size() + 1 + word.size() > kCommentWidth)
		{
			appendCommentLine(text, line);
			line = indent;
			empty = true;
		}

		if (!empty)
			line += ' ';
		line += word;
		empty = false;
	}

	if (!empty)
		appendCommentLine(text, line);
}

// The common case, where the first line of a paragraph is set like the rest.
void appendWrapped(std::string &text, const std::string &indent, const std::string &body)
{
	appendWrapped(text, indent, indent, body);
}

// Where the login in a written file came from, which is the one thing the
// first lines of it have to say differently.
enum LoginSource
{
	ShippedLogin,
	CarriedLogin,
	OwnLogin
};

/* What this file says about logging in, and the first thing in it.

   THE READER THIS IS FOR IS THE ONE WHO HAS JUST BEEN REFUSED A LOGIN and has
   opened the file over a shell to find out why. What they need is three
   sentences: what the login is, where to change it, and how to get back to a
   known one. Everything else in the file is under them for that reason. */
void appendLoginHead(std::string &text, LoginSource from, const std::string &user,
		     bool has_password, const std::string &old_path)
{
	appendCommentLine(text, "ni-web configuration.");
	appendCommentLine(text, "");

	if (from == ShippedLogin)
		appendWrapped(text, "",
			      std::string("LOGIN: user \"") + user + "\", password \"" + kOldShippedPassword +
			      "\". That is also the login this box takes over the network for a shell, "
			      "so it is one password to change and not two.");
	else if (has_password)
		appendWrapped(text, "",
			      std::string("LOGIN: user \"") + user + "\", with the password " +
			      ((from == CarriedLogin) ? "the old web interface used" : "last set at the box") +
			      ". It is kept at the end of this file as a hash and cannot be read back out "
			      "of it.");
	else
		appendWrapped(text, "",
			      std::string("LOGIN: user \"") + user + "\" and no password" +
			      ((from == CarriedLogin) ? ", the old web interface having carried none" : "") +
			      ". A box with no password can be read from the private networks this server "
			      "admits and answers nothing that changes anything, from anywhere.");

	if (has_password)
		appendWrapped(text, "", "TO CHANGE IT: the web interface under Settings, or the settings "
			      "screen at the box itself.");
	else
		appendWrapped(text, "", "TO SET ONE: the settings screen at the box itself. The web "
			      "interface cannot, no request that changes anything being answered while "
			      "there is none.");

	std::string over = "TO START OVER: delete this file";
	if (from == CarriedLogin)
		over += " and " + old_path;
	over += std::string(", then restart. This file is written again with user \"root\" and password \"") +
		kOldShippedPassword + "\"";
	over += (from == ShippedLogin) ? ", which is what stands above."
				       : ", which is also the login this box takes over the network for a shell.";
	appendWrapped(text, "", over);
}

// One key this file does not write, what it decides, and the value a box
// follows while no line names it.
void appendKeyDoc(std::string &text, const char *key, const std::string &what, const std::string &value)
{
	appendCommentLine(text, std::string("  ") + key);
	appendWrapped(text, "      ", value.empty() ? (what + ".") : (what + ". Default " + value));
}

std::string joined(const std::vector<std::string> &parts)
{
	std::string out;
	for (size_t i = 0; i < parts.size(); ++i)
	{
		if (i != 0)
			out += ", ";
		out += parts[i];
	}
	return out;
}

/* What the lines at the end of this file say, and what else may be put into it.

   THE KEYS THIS DOES NOT WRITE ARE NAMED WITH THEIR DEFAULTS AND STILL LEFT OUT, which
   is a decision between two ways to be wrong. Writing them out freezes the defaults of
   the day into a box that will never see tomorrow's, and three of them are worse than
   stale: the docroot is where the image puts its pages, the network list is the policy
   for who may read without a password, and a proxy list that is merely present turns that
   reading off. Not writing them leaves a box following a default it cannot be told by
   reading its own configuration.

   A line commented out with a number sign would settle it and this format has no such
   line: the parser reads anything carrying the separator as a setting. What stands here
   instead is the key, what it decides, and the value in force the moment the file was
   written, read out of the program rather than copied. */
void appendKeyDocs(std::string &text)
{
	const WebConfig d = defaultWebConfig();

	appendCommentLine(text, "");
	appendComment(text,
		      "HOW THIS FILE IS READ. One key and one value to a line, split at the first\n"
		      "equals sign. A number sign ends a line early, so no value may carry one, and\n"
		      "a space at either end of a value is kept and invisible, so no value may carry\n"
		      "one of those either. A line carrying an equals sign is read as a setting\n"
		      "whether or not a number sign comes first, so no comment here carries one and\n"
		      "no key can be turned off by writing a number sign in front of it.");

	appendCommentLine(text, "");
	appendComment(text,
		      "THE LINES AT THE END OF THIS FILE.");
	appendKeyDoc(text, kKeyPort, "the port this box answers on", "");
	appendKeyDoc(text, kKeyBind, "the interface it answers on, \"0.0.0.0\" being every one of them", "");
	appendKeyDoc(text, kKeyUser, "the name a login is made under", "");
	appendKeyDoc(text, kKeyHash, "the password, kept as a hash. An empty one is no password, and a "
		     "box with none answers nothing that changes anything", "");

	appendCommentLine(text, "");
	appendComment(text,
		      "WHAT ELSE MAY BE WRITTEN HERE. None of these is written out, so a box follows\n"
		      "the default of the version it is running rather than the one it was written\n"
		      "by. What stands with each is the default in force when this file was written.");
	appendKeyDoc(text, kKeyLanRead, "which networks may read without a password, separated by commas",
		     joined(defaultLanPrefixTexts()));
	appendKeyDoc(text, kKeyTrustedProxies, "which networks a forwarded caller address is believed "
		     "from, separated by commas. Naming any of them turns the reading above off",
		     "none");
	appendKeyDoc(text, kKeyDocroot, "where the pages are read from", d.docroot);
	appendKeyDoc(text, kKeySessionLifetime, "how long a login stands, in seconds, from " +
		     decimal(kSessionLifetimeMin) + " to " + decimal(kSessionLifetimeMax),
		     decimal(d.session_lifetime_s));
	appendKeyDoc(text, kKeySseMaxStreams, "how many live update streams may be open at once, up to " +
		     decimal(kSseMaxStreamsMax), decimal(d.sse_max_streams));
	appendKeyDoc(text, kKeyLegacy, "whether the old /control/ addresses answer, true or false",
		     d.legacy_enabled ? "true" : "false");
	appendKeyDoc(text, kKeyLogos, "whether the pages show a picture beside a channel, true or false",
		     d.channel_logos ? "true" : "false");
	appendKeyDoc(text, kKeyApiTokens, "tokens a program presents in place of a password, separated by "
		     "commas, each one a level and a token with a colon between them, the level being "
		     "read, write or system", "none");
}

/* What went wrong, in the file rather than only on the standard error.

   The standard error of a program drawing a television picture reaches
   nobody, and the boot log holding it is not kept. This is the copy that is
   still there when somebody comes looking, which is usually days later and
   usually because they cannot log in. */
void appendNotes(std::string &text, const std::string &heading, const std::vector<std::string> &notes)
{
	if (notes.empty())
		return;

	appendCommentLine(text, "");
	appendWrapped(text, "", heading);
	for (size_t i = 0; i < notes.size(); ++i)
		appendWrapped(text, "  - ", "    ", notes[i]);
}

// The head of a file written out of the old server's.
std::string migratedHead(const std::string &user, bool has_password, const std::string &old_path,
			 const std::vector<std::string> &notes)
{
	std::string text;

	appendLoginHead(text, CarriedLogin, user, has_password, old_path);

	appendCommentLine(text, "");
	appendWrapped(text, "", "Written from " + old_path + ", the file of the web interface this one "
		      "replaces, which is left where it is. The port below is the one that file named: "
		      "nothing of that server is on it any more, and it is where this box was reached "
		      "before.");

	appendNotes(text, "NOTES ON WHAT WAS AND WAS NOT CARRIED OVER:", notes);
	appendKeyDocs(text);

	text += '\n';
	return text;
}

// The head of a file written because there was nothing to carry over or
// because carrying it over was refused.
std::string shippedHead(const std::string &user, const std::string &old_path, bool tried_old,
			const std::vector<std::string> &why)
{
	std::string text;

	appendLoginHead(text, ShippedLogin, user, true, old_path);

	appendCommentLine(text, "");
	if (tried_old)
	{
		appendWrapped(text, "", "Written by the box itself. There is a file of the web interface "
			      "this one replaces at " + old_path + ", and this is what stopped it from "
			      "being carried over:");
		for (size_t i = 0; i < why.size(); ++i)
			appendWrapped(text, "  - ", "    ", why[i]);
		appendWrapped(text, "", "So the login above is the one the image ships with and not the one "
			      "that file holds.");
	}
	else
		appendWrapped(text, "", "Written by the box itself, there having been nothing of the web "
			      "interface this one replaces to carry over.");

	appendKeyDocs(text);

	text += '\n';
	return text;
}

// The head of a file a save at the box writes where there was none.
std::string ownHead(const std::string &user, bool has_password)
{
	std::string text;

	appendLoginHead(text, OwnLogin, user, has_password, std::string());

	appendCommentLine(text, "");
	appendWrapped(text, "", "Written at the box.");

	appendKeyDocs(text);

	text += '\n';
	return text;
}

/* The file a box gets when there was nothing to carry over, or when carrying it over was
   refused.

   WRITTEN RATHER THAN LEFT OUT, WHICH IS THE WHOLE OF THIS. Every refusal above used to
   end at a box whose web interface had no credential at all: it answers reads from the
   private networks, refuses everything that changes anything from anywhere, and says so
   nowhere anybody looks, so its owner is locked out by a line in a boot log that is gone
   by morning. The login written here is the one the image already carries in the password
   file of the box and is the shell login of the same box over the network. */
bool writeShippedFile(const std::string &out_path, const std::string &old_path, bool tried_old)
{
	// Taken before the write below, which complains into the same list.
	const std::vector<std::string> why = problemList();

	const std::string hash = hashSecret(kOldShippedPassword);
	if (hash.empty())
	{
		complain("the password this image ships with could not be hashed, so nothing is written and "
			 "this box answers no request that changes anything until a password is set at the "
			 "box itself");
		return false;
	}

	const WebConfig d = defaultWebConfig();

	std::string text = shippedHead(d.username, old_path, tried_old, why);
	appendPair(text, kKeyPort, decimal(static_cast<unsigned long>(d.server.port)));
	appendPair(text, kKeyBind, d.server.bind_address);
	appendPair(text, kKeyUser, d.username);
	appendPair(text, kKeyHash, hash);

	return writeNewFile(out_path, text);
}

/* All of it or none of it, over a name that may already be taken.

   Not writeNewFile above, which refuses an existing name on purpose: what that one guards
   is the credential the box is running on, and this is the call whose whole job is to
   change one. What it keeps of that guard is the rest. The content is written to a name
   of this call's own, committed there, and put in the place of path in a single step, so
   nothing ever reads half a file and a power cut in the middle leaves the whole of the old
   one.

   The working name is fixed rather than drawn, so a save the box did not survive leaves
   one file behind and not one for every attempt. It is removed before it is created and
   then created exclusively, so this never writes through a name somebody else decides the
   target of.

   NOT THE WRITER IN system/helpers.h, which does the same three steps. Two things here
   are not that one's: the directory entry naming the file is committed as well, which is
   the difference between answering true and a box that comes up after a power cut with no
   file at all, and a failure is said into the list beside this rather than onto the
   standard error. That list is what a screen at the box shows somebody who has just been
   refused, and a line on the standard error of a program drawing a television picture
   reaches nobody.

   PATH ITSELF IS NEVER FOLLOWED EITHER, and the caller checks that before calling this:
   the step below puts a file at a name, so a name that is a link would have the link
   replaced and the file it points at left alone.

   False and the working file removed for anything up to and including the step that puts
   it in place. After that step there is no old file to go back to, so a failure past it is
   said and answered true. */
bool replaceFile(const std::string &path, const std::string &text)
{
	const std::string working = path + ".new";
	::unlink(working.c_str());

	const int fd = ::open(working.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, S_IRUSR | S_IWUSR);
	if (fd < 0)
	{
		complain(std::string("cannot create ") + working + ": " + std::strerror(errno));
		return false;
	}

	bool ok = writeWholeFile(fd, text);
	if (ok && ::fsync(fd) != 0)
		ok = false;
	if (::close(fd) != 0)
		ok = false;

	if (ok && ::rename(working.c_str(), path.c_str()) != 0)
	{
		complain(std::string("cannot put ") + working + " in the place of " + path + ": " +
			 std::strerror(errno));
		ok = false;
	}

	if (!ok)
	{
		::unlink(working.c_str());
		return false;
	}

	/* The content is in place by now and cannot be taken back, so this is said
	   and not answered. Answering false here would tell a caller the old file
	   is still there, which is the one thing that is no longer true. */
	if (!syncDirectoryOf(path))
		complain(std::string("the new ") + path + " is in place but the directory naming it was not "
			 "committed, so a power cut before the next one could leave neither file");

	return true;
}

/* The keys a save at the box owns. Every other line of that file belongs to
   whoever wrote it and is carried across untouched. */
const char *const kOwnedKeys[] = { kKeyPort, kKeyBind, kKeyUser, kKeyHash };
const size_t kOwnedKeyCount = sizeof(kOwnedKeys) / sizeof(kOwnedKeys[0]);

// What a save writes for each key it owns, which is fewer than the four above
// when the password is not being changed.
struct OwnedLine
{
	const char *key;
	std::string value;
	bool        written;
};

/* Which of the keys a save is writing this line names, and nothing for a line that names
   none of them.

   The name is taken the way scanLines takes it and not byte for byte, for the same
   reason: a line spelled Port, a line with a space in front of the separator and a line
   carrying a number sign are all lines load counts as that key being named and unusable.
   Left standing beside the line a save writes, any one of them would leave the file
   naming a port and the server running on something else. */
size_t ownedLineFor(const std::string &line, const std::vector<OwnedLine> &owned)
{
	const size_t sep = line.find('=');
	if (sep == std::string::npos)
		return owned.size();

	const std::string taken_for = knownKeyFor(trimmed(line.substr(0, sep)));
	if (taken_for.empty())
		return owned.size();

	for (size_t i = 0; i < owned.size(); ++i)
	{
		if (taken_for == owned[i].key)
			return i;
	}
	return owned.size();
}

// Reads a file this layer is about to take values out of. A directory opens
// for reading and gives back no lines, so without this a load of one would
// answer that it read a file holding nothing.
bool isReadableRegularFile(const std::string &path, const char *what)
{
	struct stat st;
	if (stat(path.c_str(), &st) != 0)
	{
		complain(std::string("cannot read the ") + what + " " + path + ": " + std::strerror(errno));
		return false;
	}
	if (!S_ISREG(st.st_mode))
	{
		complain(std::string("the ") + what + " " + path + " is not a regular file");
		return false;
	}
	if (st.st_size < 0 || st.st_size > kMaxFileBytes)
	{
		complain(std::string("the ") + what + " " + path + " is larger than a configuration is, and it "
			 "is not read");
		return false;
	}
	return true;
}

} // namespace

WebConfig defaultWebConfig()
{
	WebConfig c;
	c.server = defaultConfig();
	c.username = "root";
	c.password_hash = "";
	c.lan_read = defaultLanPrefixes();
	c.trusted_proxies.clear();
	c.trusted_proxies_named = false;
	/* This server's own directory, under its own name rather than at
	   PRIVATE_HTTPDDIR itself: that directory is also where the old server's
	   pages land, and naming it here would serve those out of this one as
	   well, which nobody asked this server to do. The docs page installs
	   into "swagger" under it and answers at /swagger/, not at the root: the
	   root is where the WebIf this docroot is also meant for lands later,
	   and a docs page sitting there would have to move the day it does. */
	c.docroot = PRIVATE_HTTPDDIR "/ni-web";
	c.session_lifetime_s = kSessionLifetimeDefault;
	c.sse_max_streams = kSseMaxStreamsDefault;
	c.legacy_enabled = true;
	c.channel_logos = true;
	return c;
}

bool proxiesConfigured(const WebConfig &c)
{
	return c.trusted_proxies_named || !c.trusted_proxies.empty();
}

bool isLoopbackPeer(const std::string &peer)
{
	// Built on first use and not at namespace scope, for the reason every
	// other table in this tree gives: nothing here may need a constructor to
	// have run before main.
	static NetPrefix v4;
	static NetPrefix v6;
	static const bool ready = parsePrefix("127.0.0.0/8", &v4) && parsePrefix("::1/128", &v6);

	if (!ready)
		return false;
	return addressInPrefix(peer, v4) || addressInPrefix(peer, v6);
}

const WebConfig &config()
{
	return held();
}

void setConfigForTest(const WebConfig &c)
{
	held() = c;
}

const std::vector<std::string> &configProblems()
{
	return problemList();
}

const std::string &configPath()
{
	return heldPath();
}

bool load(const std::string &path)
{
	problemList().clear();

	/* Before the file is looked at, and kept whatever the look comes to. What
	   this records is where the box keeps its configuration, which is a
	   different question from whether there is one there to read: a box being
	   started for the first time has no file and still has the one name a save
	   is to write it at. */
	heldPath() = path;

	if (!isReadableRegularFile(path, "configuration"))
		return false;

	// Nothing is stored back through this object and no default is asked of
	// it, so it is told not to keep any: its readers write the default they
	// were handed into the map, which would turn a key the file does not carry
	// into one it does the next time anything saved it.
	CConfigFile file(',', false);
	if (!file.loadConfig(path))
	{
		complain("cannot parse " + path);
		return false;
	}

	const Pairs m = file.getConfigDataMap();
	const LineFaults faults = scanLines(path, true);
	const CutShort &cut = faults.unreadable;

	for (size_t i = 0; i < faults.notes.size(); ++i)
		complain(faults.notes[i]);

	WebConfig c = defaultWebConfig();
	std::string raw;

	{
		const Reading r = reading(m, cut, kKeyPort, raw);
		if (r == KeyUnreadable)
		{
			/* The one key whose fallback is the default and not the least it
			   can mean. A port is where the box answers and not who may reach
			   it, there is no least port, and a box refusing to start over a
			   mistyped one would be off the network for a reason that grants
			   nobody anything. */
			complain(std::string(kKeyPort) + ": the line carries a port this cannot read, and the "
				 "default stands, a port being where the box answers rather than who may "
				 "reach it");
		}
		else if (r == KeyPresent)
		{
			unsigned long v = 0;
			// Zero is refused where it is allowed at the call that binds. There
			// it means a port the kernel picks, which is what a case wants and
			// what nobody running a box wants.
			if (!readUnsigned(raw, v) || v < 1 || v > 65535)
				complain(std::string(kKeyPort) + ": " + quoted(raw) +
					 " is not a port from 1 to 65535, and the default stands");
			else
				c.server.port = static_cast<int>(v);
		}
	}

	{
		const Reading r = reading(m, cut, kKeyBind, raw);
		if (r == KeyUnreadable)
		{
			complain(std::string(kKeyBind) + ": the line carries an address this cannot read, and "
				 "what stands in its place is one nothing can bind, so the server will not "
				 "start. The default is the wildcard and would listen more widely than any "
				 "line naming an interface");
			c.server.bind_address = kNoAddress;
		}
		else if (r == KeyPresent)
		{
			/* Carried through even when it is unusable, unlike everything else
			   here. The default is the wildcard, so dropping a line naming one
			   interface would answer it by listening on all of them. Kept, the
			   call that binds refuses it, and that call refuses everything this
			   refuses: a value carrying a zero byte included, which is the one
			   shape where the two could have disagreed. */
			if (!isAddressLiteral(raw))
				complain(std::string(kKeyBind) + ": the value is not an address, and the server "
					 "will refuse to start on it");
			c.server.bind_address = raw;
		}
	}

	readTextField(m, cut, kKeyDocroot, "", c.docroot);

	{
		const Reading r = reading(m, cut, kKeyHash, raw);
		if (r == KeyUnreadable)
		{
			complain(std::string(kKeyHash) + ": the line carries a stored form this cannot read, and "
				 "what stands in its place verifies against nothing");
			c.password_hash.clear();
		}
		else if (r == KeyPresent)
		{
			if (!survivesTheFile(raw))
			{
				complain(std::string(kKeyHash) + ": the stored form carries a byte that does not "
					 "read back as it was written, and it is dropped");
				c.password_hash.clear();
			}
			else
			{
				if (!raw.empty() && raw.compare(0, std::strlen(kHashMarker), kHashMarker) != 0)
					complain(std::string(kKeyHash) + ": the stored form does not begin with " +
						 quoted(kHashMarker) + " and will verify against nothing");
				c.password_hash = raw;
			}
		}
	}

	readTextField(m, cut, kKeyUser, "", c.username);

	/* A name nothing answers to takes the password with it, so that no pair of
	   a name and a password is answerable rather than leaving the question to
	   whatever compares them. An empty name matching an empty header field is a
	   way in, and this is the only place that can close it without knowing what
	   the comparison will be. Reached only when the file says so: the default
	   name is not empty. */
	if (c.username.empty())
	{
		complain(std::string(kKeyUser) + " is empty, which is not a name anything answers to, so the "
			 "password goes with it and no request needing one is answered");
		c.password_hash.clear();
	}

	readPrefixList(m, cut, kKeyLanRead,
		       "the line names networks this cannot read, so the list is empty and admits nobody. "
		       "The seeded list is wider than any line naming a network, so falling back to it "
		       "would admit more than the file asks for",
		       c.lan_read);

	/* The one key whose least meaning is not its least value. An empty proxy
	   list is how a box with nothing in front of it is described, and that is
	   the state in which the read exemption is on, so a line naming a proxy
	   that cannot be read would turn the exemption back on if only the list
	   were looked at. What is remembered instead is that the key was named,
	   and a named key keeps the exemption off however little of it survived. */
	{
		const Reading r =
			readPrefixList(m, cut, kKeyTrustedProxies,
				       "the line names networks this cannot read, so no forwarded address is "
				       "believed. A list this cannot read is still a list, so the exemption "
				       "that lets a caller read without a password stays off, which is what a "
				       "line naming a proxy asks for",
				       c.trusted_proxies);
		c.trusted_proxies_named = (r != KeyAbsent);
	}

	readUnsignedField(m, cut, kKeySessionLifetime, kSessionLifetimeMin, kSessionLifetimeMax,
			  kSessionLifetimeMin, c.session_lifetime_s);
	readUnsignedField(m, cut, kKeySseMaxStreams, 0u, kSseMaxStreamsMax, 0u, c.sse_max_streams);

	{
		const Reading r = reading(m, cut, kKeyLegacy, raw);
		if (r == KeyUnreadable)
		{
			complain(std::string(kKeyLegacy) + ": the line carries a word this cannot read, and the "
				 "switch is off, which is the least it can mean");
			c.legacy_enabled = false;
		}
		else if (r == KeyPresent)
		{
			if (raw == "true" || raw == "1")
				c.legacy_enabled = true;
			else if (raw == "false" || raw == "0")
				c.legacy_enabled = false;
			else
			{
				complain(std::string(kKeyLegacy) + ": " + quoted(raw) +
					 " is neither true nor false, and the switch is off");
				c.legacy_enabled = false;
			}
		}
	}

	/* The same two words read the same way, and the same rule for a line that
	   says neither of them. What stands in for one this cannot read is the
	   switch off, because a page drawing nothing extra is the least a switch
	   that shows something can mean, and because a key that was named and fell
	   back to its default would be a line the box quietly ignored. */
	{
		const Reading r = reading(m, cut, kKeyLogos, raw);
		if (r == KeyUnreadable)
		{
			complain(std::string(kKeyLogos) + ": the line carries a word this cannot read, and the "
				 "switch is off, which is the least it can mean");
			c.channel_logos = false;
		}
		else if (r == KeyPresent)
		{
			if (raw == "true" || raw == "1")
				c.channel_logos = true;
			else if (raw == "false" || raw == "0")
				c.channel_logos = false;
			else
			{
				complain(std::string(kKeyLogos) + ": " + quoted(raw) +
					 " is neither true nor false, and the switch is off");
				c.channel_logos = false;
			}
		}
	}

	/* Last, and not into the structure above: a token is a credential and
	   goes where this server keeps the other ones, which is the table a bearer
	   header is answered out of. Everything above describes the box; this
	   names who may talk to it. */
	readApiTokens(m, cut);

	held() = c;
	return true;
}

WebSettings currentWebSettings()
{
	const WebConfig &c = config();

	WebSettings s;
	s.port = c.server.port;
	s.bind_address = c.server.bind_address;
	s.username = c.username;
	// Nothing to show: what is held is a stored form, and the switch beside it
	// is what makes an untouched field mean the stored form stays.
	s.password.clear();
	s.change_password = false;
	s.channel_logos = c.channel_logos;
	return s;
}

bool saveWebSettings(const std::string &path, const WebSettings &s)
{
	problemList().clear();

	/* Every value is judged before the file is opened, so a value that cannot
	   be used leaves the file as it was rather than half of it rewritten. The
	   value is not repeated back where the key makes it a credential. */
	if (s.port < 1 || s.port > 65535)
	{
		complain(std::string(kKeyPort) + ": the port asked for is not one from 1 to 65535, and "
			 "nothing is written");
		return false;
	}

	if (!survivesTheFile(s.bind_address) || !isAddressLiteral(s.bind_address))
	{
		complain(std::string(kKeyBind) + ": " + quoted(s.bind_address) + " is not an address this "
			 "can write down and bind, and nothing is written. Every interface is written as "
			 "\"0.0.0.0\", which is what a box answers on when nothing narrower is asked for");
		return false;
	}

	if (s.username.empty() || !survivesTheFile(s.username))
	{
		complain(std::string(kKeyUser) + ": the name is empty, or carries a control byte, a number "
			 "sign or a space at one of its ends, and nothing is written. An empty name is not "
			 "a name anything answers to");
		return false;
	}

	std::string hash;
	if (s.change_password)
	{
		if (s.password.empty())
		{
			complain(std::string(kKeyHash) + ": an empty password verifies against nothing, and "
				 "nothing is written rather than leaving a box that looks configured and "
				 "answers no request that changes anything");
			return false;
		}

		hash = hashSecret(s.password);
		if (hash.empty())
		{
			complain(std::string(kKeyHash) + ": the password could not be hashed, and nothing is "
				 "written");
			return false;
		}
	}

	std::vector<OwnedLine> owned;
	{
		OwnedLine one;
		one.written = false;

		one.key = kKeyPort;
		one.value = decimal(static_cast<unsigned long>(s.port));
		owned.push_back(one);

		one.key = kKeyBind;
		one.value = s.bind_address;
		owned.push_back(one);

		one.key = kKeyUser;
		one.value = s.username;
		owned.push_back(one);

		/* Written as a word and not as a digit, because that is how the one
		   switch this file already had is written wherever a person sets it,
		   and a file where one switch reads true and the next reads 1 is a file
		   somebody has to look up the reader for. Both are read back. */
		one.key = kKeyLogos;
		one.value = s.channel_logos ? "true" : "false";
		owned.push_back(one);

		/* The stored form is a line this save owns only when a new password was
		   typed. Left out of the list otherwise, whatever the file holds under
		   that key is carried across like any other line nobody asked about,
		   which is what an untouched field has to mean. */
		if (s.change_password)
		{
			one.key = kKeyHash;
			one.value = hash;
			owned.push_back(one);
		}
	}

	std::string text;
	struct stat st;

	if (::lstat(path.c_str(), &st) == 0)
	{
		/* A link is refused and not followed. What puts the new content in
		   place puts a file at this name, so a link here would have the link
		   replaced and the file it names left exactly as it was, which is the
		   opposite of what a save means. */
		if (S_ISLNK(st.st_mode))
		{
			complain(path + " is a link, and a save would replace the link rather than the file "
				 "it names, so nothing is written");
			return false;
		}

		if (!isReadableRegularFile(path, "configuration"))
			return false;

		text = wholeFile(path);
	}
	else if (errno != ENOENT)
	{
		complain(std::string("cannot look at ") + path + ": " + std::strerror(errno));
		return false;
	}
	else
	{
		/* The other way a box comes to have this file, the one that runs on
		   every start being the first. This one is reached by a save at a box
		   whose file was taken away under it. */
		text = ownHead(s.username, s.change_password);
	}

	std::string out;
	out.reserve(text.size() + 128);

	size_t at = 0;
	while (at < text.size())
	{
		const size_t end = text.find('\n', at);
		const bool last = (end == std::string::npos);
		const std::string line = last ? text.substr(at) : text.substr(at, end - at);
		at = last ? text.size() : end + 1;

		const size_t which = ownedLineFor(line, owned);
		if (which == owned.size())
		{
			out += line;
			out += '\n';
			continue;
		}

		/* The first line naming a key is where the new value goes, so a key
		   somebody put at the top of their file stays at the top of it. Every
		   later line naming the same key is dropped rather than rewritten: two
		   lines naming one key are read as whichever came last, and a file
		   saying one thing twice is a file that can be read two ways. */
		if (!owned[which].written)
		{
			appendPair(out, owned[which].key, owned[which].value);
			owned[which].written = true;
		}
	}

	for (size_t i = 0; i < owned.size(); ++i)
	{
		if (!owned[i].written)
			appendPair(out, owned[i].key, owned[i].value);
	}

	return replaceFile(path, out);
}

bool reloadAndRestart(const std::string &path)
{
	/* First, and the read below is inside the window this opens. What is in
	   effect is read by request threads without a lock, so replacing it under a
	   running daemon would be replacing something one of them holds, and a
	   listening socket has nowhere to put a new port in any case. */
	stop();

	const bool read = load(path);
	if (!read)
		complain("the configuration could not be read again, so what was already in effect stays in "
			 "effect and the server comes back up on that");

	const bool up = start(config().server);
	if (!up)
		complain("the server did not come back up, so this box answers nothing over the network until "
			 "the port and the address it was given are ones it can bind");

	return read && up;
}

bool migrateFrom(const std::string &nhttpd_conf, const std::string &out_path)
{
	problemList().clear();

	if (!isReadableRegularFile(nhttpd_conf, "old configuration"))
		return false;

	CConfigFile file(',', false);
	if (!file.loadConfig(nhttpd_conf))
	{
		complain("cannot parse " + nhttpd_conf);
		return false;
	}

	const Pairs m = file.getConfigDataMap();

	/* Named rather than refused, because what the old server compared against
	   was the short value too and carrying the long one would change who can
	   log in. This is the one place where the value carried is visibly not the
	   value somebody typed, so it is said. */
	const CutShort cut = scanLines(nhttpd_conf, false).cut_short;
	const char *const carried[] = { kOldHost, kOldUser, kOldPassword };
	for (size_t i = 0; i < sizeof(carried) / sizeof(carried[0]); ++i)
	{
		const CutShort::const_iterator at = cut.find(carried[i]);
		if (at == cut.end())
			continue;
		complain(std::string(carried[i]) + " is on line " + decimal(at->second) +
			 ", which carries a number sign, so everything from it was cut off before either "
			 "server ever saw the value. What is carried is the part in front of it, which is "
			 "what the old server compared against");
	}

	std::string bind = defaultConfig().bind_address;
	std::string user = "root";
	int listen_port = defaultConfig().port;
	std::string raw;

	if (present(m, kOldHost, raw) && !raw.empty())
	{
		/* Refused rather than dropped, and the whole migration with it. The
		   default is the wildcard, so taking it for a line naming something
		   else would answer a narrower instruction with a wider one, and a box
		   that ends up listening on every interface its owner named one of is
		   worse off than a box that says on the next start why it wrote
		   nothing. */
		if (!isAddressLiteral(raw))
		{
			complain(std::string(kOldHost) + ": " + quoted(raw) + " is not an address, and nothing "
				 "is written rather than listening more widely than it says");
			return false;
		}
		bind = raw;
	}

	if (present(m, kOldUser, raw))
	{
		/* What is refused is a name this file cannot carry unchanged, and an
		   empty one, which is a name nothing can be typed to. The value is not
		   repeated: a name is closer to a credential than to a setting. */
		if (raw.empty() || !survivesTheFile(raw))
		{
			complain(std::string(kOldUser) + ": the name carries a control byte or a space at one "
				 "of its ends, or is empty, and nothing is written");
			return false;
		}
		user = raw;
	}

	std::string hash;
	bool has_password = false;
	{
		std::string secret;
		const bool has_secret = present(m, kOldPassword, secret) && !secret.empty();
		const bool shipped = has_secret && secret == kOldShippedPassword;

		has_password = has_secret;
		if (has_password)
			hash = hashSecret(secret);
		forget(secret);

		if (has_password && hash.empty())
		{
			// The box would look configured and answer nothing that needs the
			// password. Saying so and writing nothing is the loud half of the
			// same outcome.
			complain("the password could not be hashed, and nothing is written");
			return false;
		}

		if (!has_secret)
			complain(std::string(kOldPassword) +
				 " is empty, so the new file carries no password and every request needing one is refused");
		else if (shipped)
		{
			/* Said once, at the first start, and not a refusal.

			   The value is named. It is this file's own constant and not something read
			   out of the file being migrated, and it is the root login these boxes take
			   over the network as well. What it is for is the owner who changed that
			   login and never opened this file: without this they have a web password
			   they did not choose and no reason to look for one. */
			complain(std::string(kOldPassword) + " is \"" + kOldShippedPassword +
				 "\", the value that server ships with, and it is carried across as the "
				 "password for this one. It is also the login this box takes over the network, "
				 "so changing it is one thing and not two");
		}
	}

	if (present(m, kOldSwitch, raw) && (raw == "false" || raw == "0"))
		complain(std::string(kOldSwitch) +
			 " turned authentication off, and that is the one thing this file does not carry across");

	if (present(m, kOldExempt, raw) && !raw.empty())
		complain(std::string(kOldExempt) +
			 " named callers that needed no password, and this server exempts nobody by name");

	if (present(m, kOldPort, raw))
	{
		/* Carried, which it was not while both servers ran. The one that held this
		   number goes out with this work, so the number is free, and it is the one this
		   box was reached at. A box whose owner moved the old server off eighty moved it
		   for a reason this cannot see.

		   A value that is not a port leaves the default standing rather than refusing the
		   migration, the way the read of this server's own file treats such a line: a
		   port is where the box answers and not who may reach it, so there is nothing to
		   be granted by refusing, and the refusal would cost the credential beside it. */
		unsigned long v = 0;
		if (!readUnsigned(raw, v) || v < 1 || v > 65535)
			complain(std::string(kOldPort) + ": " + quoted(raw) +
				 " is not a port from 1 to 65535, and the default stands");
		else
			listen_port = static_cast<int>(v);
	}

	/* Taken before the write below, which complains into the same list. Every
	   one of these is said in the written file as well as on the standard
	   error: the list is read by whoever is watching a boot, and the file is
	   read by whoever comes looking for the password weeks later. */
	const std::vector<std::string> notes = problemList();

	std::string text = migratedHead(user, has_password, nhttpd_conf, notes);

	appendPair(text, kKeyPort, decimal(static_cast<unsigned long>(listen_port)));
	appendPair(text, kKeyBind, bind);
	appendPair(text, kKeyUser, user);
	appendPair(text, kKeyHash, hash);

	return writeNewFile(out_path, text);
}

bool ensureConfigFile(const std::string &nhttpd_conf, const std::string &out_path)
{
	struct stat st;

	/* lstat and not stat, so that a link pointing nowhere counts as the name
	   being taken rather than as nothing being there. The creations below are
	   exclusive and would refuse such a name anyway; what this keeps away is
	   the attempt and a line about it on every start for as long as the name
	   stays that way. */
	if (::lstat(out_path.c_str(), &st) == 0)
		return false;

	/* stat and not lstat for the old file, which is only read: a link to the
	   old server's file is a perfectly ordinary thing for a box to have, and
	   the call below judges what it finds through the link the same way. */
	const bool have_old = (::stat(nhttpd_conf.c_str(), &st) == 0);

	if (have_old && migrateFrom(nhttpd_conf, out_path))
		return true;

	/* Whatever the list holds now is why the migration was refused, and the
	   file below repeats it. Nothing was refused where there was nothing to
	   migrate, so what an earlier read left in the list goes rather than being
	   written into a file it says nothing about. */
	if (!have_old)
		problemList().clear();

	return writeShippedFile(out_path, nhttpd_conf, have_old);
}

} // namespace httpd
