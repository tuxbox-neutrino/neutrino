/*
 * webconfig.cpp - the webserver own configuration file
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (additions, rework, fixes)
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
#include "randomsource.h"
#include "coreapi/base/accessfiles.h"

#include <configfile.h>

#include <openssl/crypto.h>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include "coreapi/base/settingswrites.h"

namespace httpd
{

namespace
{

/* The keys this file knows, all of them flat. The old file grouped them under
   the name of a website and of a module, which was how one process served
   several sites; this one serves the box it runs on. */
const char kKeyUser[]            = "username";
const char kKeyHash[]            = "password_hash";
const char kKeyFirst[]           = "first_password";
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
	kKeyUser, kKeyHash, kKeyFirst, kKeyLanRead,
	kKeyTrustedProxies, kKeyDocroot, kKeySessionLifetime,
	kKeySseMaxStreams, kKeyLegacy, kKeyLogos, kKeyApiTokens
};

/* Two keys this file held once and holds no longer. Where the server answers
   is nhttpd.conf's: the menu at the box and nhttpd itself read and write it
   there, and a second copy here is how a port moved in one place came back
   from the other at the next restart. A line naming either is said, has no
   effect, and goes at the next save. */
const char *const kRetiredKeys[] = { "port", "bind_address" };

/* Where the server answers, in nhttpd's own file and under nhttpd's own names.
   Read the way nhttpd reads them (yhttpd.cpp, ReadConfig) and written the way
   the menu at the box writes them (gui/nhttpd_setup.cpp), so the three never
   disagree about what the file says. */
const char kServerPort[] = "WebsiteMain.port";
const char kServerHost[] = "WebsiteMain.host";

// The rest of what the settings page sets in that file (ServerOptions), under
// the names nhttpd and its modules read.
const char kServerAuth[]      = "mod_auth.authenticate";
const char kServerNoAuth[]    = "mod_auth.no_auth_client";
const char kServerThreading[] = "webserver.threading";
const char kServerOverride[]  = "WebsiteMain.override_directory";
const char kServerHosted[]    = "WebsiteMain.hosted_directory";
const char kServerSendAll[]   = "mod_sendfile.sendAll";

/* The longest prefix and the longest directory a save writes. A prefix is the
   start of an address of the first family, the only one nhttpd listens on, and
   no longer than the longest such address; a directory longer than this is not
   one anybody set up on a box. */
const size_t kMaxPrefixChars = 15;
const size_t kMaxDirectoryChars = 255;

// What the old file calls the account, where it has one at all.
const char kOldUser[]     = "mod_auth.username";
const char kOldPassword[] = "mod_auth.password";
const char kOldSwitch[]   = "mod_auth.authenticate";
const char kOldExempt[]   = "mod_auth.no_auth_client";

} // namespace

/* The password the images carry for the old server (configure:
   WEB_DEFAULT_PASSWORD), which is nobody's choice: it stands in the nhttpd.conf
   the image installs and is that server's value for a file that names none. The
   access file is never written with it; a box that would start on it starts on
   a first password instead. */
const char kShippedPassword[] = WEB_DEFAULT_PASSWORD;

/* Every password a box of this line has shipped with: this one, the old
   server's built-in default, the one Coolstream images carried and the one the
   first version of this layer wrote. A login still using one is asked for a
   password of its own as well. */
static const char *const kShippedPasswords[] = { WEB_DEFAULT_PASSWORD, "tuxbox", "coolstream", "ni" };

const char *const *shippedPasswords(size_t *count)
{
	if (count != NULL)
		*count = sizeof(kShippedPasswords) / sizeof(kShippedPasswords[0]);
	return kShippedPasswords;
}

bool isShippedPassword(const std::string &secret)
{
	for (size_t i = 0; i < sizeof(kShippedPasswords) / sizeof(kShippedPasswords[0]); ++i)
		if (secret == kShippedPasswords[i])
			return true;
	return false;
}

/* 31 characters, so a byte below 248, which is 8 times 31, maps onto every one
   of them equally often, and a byte from 248 up is drawn again: taking every
   byte modulo 31 would make the first eight characters likelier than the rest.
   Ten of them are about 49 bits. */
std::string drawFirstPassword()
{
	static const char kAlphabet[] = "abcdefghjkmnpqrstuvwxyz23456789";
	const unsigned kLetters = sizeof(kAlphabet) - 1;
	const unsigned kBelow = 256 - 256 % kLetters;

	std::string out;
	unsigned char drawn[32];
	while (out.size() < kFirstPasswordChars)
	{
		if (!randomBytes(drawn, sizeof(drawn)))
		{
			if (!out.empty())
				OPENSSL_cleanse(&out[0], out.size());
			return std::string();
		}
		for (size_t i = 0; i < sizeof(drawn) && out.size() < kFirstPasswordChars; ++i)
		{
			if (drawn[i] < kBelow)
				out += kAlphabet[drawn[i] % kLetters];
		}
		OPENSSL_cleanse(drawn, sizeof(drawn));
	}
	return out;
}

bool passwordAcceptable(const std::string &password, std::string *why)
{
	size_t chars = 0;
	for (size_t i = 0; i < password.size(); ++i)
	{
		const unsigned char c = (unsigned char) password[i];
		if (c < 0x20 || c == 0x7f)
		{
			if (why != NULL)
				*why = "the new password holds a control character";
			return false;
		}
		// a byte that does not continue a character starts one
		if ((c & 0xc0) != 0x80)
			++chars;
	}
	if (chars < kMinPasswordChars)
	{
		if (why != NULL)
		{
			char said[80];
			std::snprintf(said, sizeof(said), "the new password has fewer than %u characters",
			              (unsigned) kMinPasswordChars);
			*why = said;
		}
		return false;
	}
	if (isShippedPassword(password))
	{
		if (why != NULL)
			*why = "the new password is one these boxes have shipped with, which anybody can look up";
		return false;
	}
	return true;
}

namespace
{

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

/* Held only around the list above and never while taking another lock. Lines
   reach the list from threads holding the account's lock, from writers of
   nhttpd.conf holding only theirs, and from reads that hold none. */
OpenThreads::Mutex &problemLock()
{
	static OpenThreads::Mutex m;
	return m;
}

void clearProblems()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held_lock(problemLock());
	problemList().clear();
}

std::vector<std::string> problemsNow()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held_lock(problemLock());
	return problemList();
}

/* Asked for first by load, on the program's way up, so the mutex is built there
   and not on nhttpd's thread, which is built without exceptions and would
   otherwise make it at the first read of nhttpd.conf. */
OpenThreads::Mutex &serverFileLock()
{
	static OpenThreads::Mutex m;
	return m;
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

std::string &heldServerPath()
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
	OpenThreads::ScopedLock<OpenThreads::Mutex> held_lock(problemLock());
	const size_t held_now = problemList().size();
	if (held_now > kMaxProblems)
		return;

	if (held_now == kMaxProblems)
	{
		const std::string last = "the file has more lines this cannot use than are worth listing, and "
					 "the rest are not named here";
		problemList().push_back(last);
		std::fprintf(stderr, "[nhttpd access] %s\n", last.c_str());
		return;
	}

	problemList().push_back(line);
	std::fprintf(stderr, "[nhttpd access] %s\n", line.c_str());
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

/* An address nhttpd listens on as it is written, which is one of the first
   family and nothing else: its socket is AF_INET (ysocket.cpp), and what it
   cannot use there, a name it fails to resolve or an address of the second
   family, it answers by listening on every interface. A save writing one of
   those would be asking for one interface and getting all of them.

   An address and not a name, too: a name would have to be resolved to be
   bound, and what it resolves to is not what the file says. */
bool isListenAddress(const std::string &text)
{
	if (text.empty() || text.find('\0') != std::string::npos)
		return false;

	struct in_addr v4;
	return inet_pton(AF_INET, text.c_str(), &v4) == 1;
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

// The same reading for the keys this file no longer holds, so that a line a
// person spelled Port goes the way port does.
std::string retiredKeyFor(const std::string &key)
{
	const std::string folded = foldedAscii(key);
	const size_t count = sizeof(kRetiredKeys) / sizeof(kRetiredKeys[0]);
	for (size_t i = 0; i < count; ++i)
	{
		if (folded == kRetiredKeys[i])
			return std::string(kRetiredKeys[i]);
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

		/* Before the number sign, which would make it unreadable: a line
		   this file no longer reads is not one whose least meaning stands in
		   for it, whatever it carries. */
		const std::string retired = judge_names ? retiredKeyFor(trimmed(key)) : std::string();
		if (!retired.empty())
		{
			out.notes.push_back("line " + decimal(lineno) + " (" + retired + "): where the server "
					    "answers is read from nhttpd.conf (" + kServerPort + ", " + kServerHost +
					    ") and no longer from this file, so the line has no effect and goes at "
					    "the next save");
			continue;
		}

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
	forgetConfiguredApiTokens();

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

/* A rename that refuses a name that is taken: errno ENOSYS or EINVAL where the
   kernel or the filesystem cannot do that. The system call and not the C
   library's wrapper, which an older library does not have even on a kernel that
   does. */
#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1 << 0)
#endif
int renameNoReplace(const char *from, const char *to)
{
#ifdef SYS_renameat2
	return (int) ::syscall(SYS_renameat2, AT_FDCWD, from, AT_FDCWD, to, RENAME_NOREPLACE);
#else
	(void) from;
	(void) to;
	errno = ENOSYS;
	return -1;
#endif
}

void (*afterCreate)(const std::string &working) = NULL;
void (*beforeReplace)(const std::string &path) = NULL;

/* Every file the two writers below make is one of those that hold who may reach
   the box, and is kept as one from the moment it is made, before a byte of it is
   written (coreapi::accessfiles): a descriptor opened on it while it is written,
   or under the name it gets afterwards, is never handed out. Made while no gate
   asks about a descriptor (Making), so none opened on it before it was kept is let
   through either. The descriptor, or -1 with the file removed and the reason said:
   a file that could not be kept is not written at all. */
int makeAccessFile(const std::string &working, mode_t mode)
{
	int fd;
	{
		coreapi::accessfiles::Making making;
		fd = ::open(working.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, mode);
		if (fd < 0)
		{
			complain(std::string("cannot create ") + working + ": " + std::strerror(errno));
			return -1;
		}
		if (!coreapi::accessfiles::created(fd))
		{
			::close(fd);
			::unlink(working.c_str());
			complain("cannot keep " + working + " as a file that holds who may reach the box, "
				 "so nothing is written");
			return -1;
		}
	}
	if (afterCreate != NULL)
		afterCreate(working);
	return fd;
}

/* A working file a write that did not end left behind is one of those files as
   well, and may have a second name elsewhere: kept before its name goes, so that
   name stays held back. One that cannot be kept stays, and nothing is written. */
bool discardWorking(const std::string &working)
{
	if (!coreapi::accessfiles::named(working))
	{
		complain("cannot keep " + working + ", left by a write that did not end, as a file that "
			 "holds who may reach the box, so it stays and nothing is written");
		return false;
	}
	::unlink(working.c_str());
	return true;
}

/* The content goes to a working name first and is committed there, and only then
   is path made a second name of it: a link, which like the exclusive creation
   before it refuses a name that is taken, and which is one step, so a power cut
   leaves either no file at path or the whole of it. Written straight to path, a cut
   in the middle would leave half a file there, which every later start takes for
   the configuration it is, and the start after a takeover would remove the whole
   one it came from as a leftover. */
bool writeNewFile(const std::string &path, const std::string &text)
{
	{
		struct stat st;
		if (::lstat(path.c_str(), &st) == 0)
		{
			complain(std::string("cannot create ") + path + ": " + std::strerror(EEXIST));
			return false;
		}
	}

	const std::string working = workingFile(path);
	if (!discardWorking(working))
		return false;

	const int fd = makeAccessFile(working, S_IRUSR | S_IWUSR);
	if (fd < 0)
		return false;

	bool ok = writeWholeFile(fd, text);

	if (ok && ::fsync(fd) != 0)
		ok = false;
	if (::close(fd) != 0)
		ok = false;
	if (!ok)
	{
		complain(std::string("cannot write ") + working + ", and what was written of it is removed");
		::unlink(working.c_str());
		return false;
	}

	/* A filesystem without hard links (EPERM, EOPNOTSUPP) is asked for a rename
	   that refuses a name that is taken, which is the same one step; one that
	   cannot do that either (ENOSYS, EINVAL) gets the rename after a look, the
	   look and the rename being as close as a start running alone needs. */
	if (::link(working.c_str(), path.c_str()) == 0)
		::unlink(working.c_str());
	else if (errno != EPERM && errno != EOPNOTSUPP)
	{
		complain(std::string("cannot create ") + path + ": " + std::strerror(errno));
		::unlink(working.c_str());
		return false;
	}
	else if (renameNoReplace(working.c_str(), path.c_str()) != 0)
	{
		int why = errno;
		struct stat st;
		bool placed = false;
		if (why == ENOSYS || why == EINVAL)
		{
			if (::lstat(path.c_str(), &st) == 0)
				why = EEXIST;
			else if (::rename(working.c_str(), path.c_str()) == 0)
				placed = true;
			else
				why = errno;
		}
		if (!placed)
		{
			complain(std::string("cannot create ") + path + ": " + std::strerror(why));
			::unlink(working.c_str());
			return false;
		}
	}

	if (!syncDirectoryOf(path))
	{
		complain(std::string("cannot commit ") + path + ", and it is removed again");
		::unlink(path.c_str());
		return false;
	}
	return true;
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
	FirstLogin,
	CarriedLogin,
	OwnLogin
};

/* What this file says about logging in, the paragraphs that open it under its
   title.

   THE READER THIS IS FOR IS THE ONE WHO HAS JUST BEEN REFUSED A LOGIN and has
   opened the file over a shell to find out why. What they need is three
   sentences: what the login is, where to change it, and how to get back to a
   known one. Everything else in the file is under them for that reason.

   old_path is the old server's file a start would write this one out of again,
   and empty where there is none to name. */
void appendLoginLines(std::string &text, LoginSource from, const std::string &user,
		      bool has_password, const std::string &old_path)
{
	if (from == FirstLogin)
		appendWrapped(text, "",
			      std::string("LOGIN: user \"") + user + "\", with a first password the box drew "
			      "for itself, which the screen at the box shows when the box starts and in the "
			      "web server menu (System settings, Network, Web server (nhttpd)). The first login "
			      "asks for a password of your own and can do nothing else until it is set; "
			      "until then the first password also stands in plain on the " + kKeyFirst +
			      " line at the end of this file.");
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

	/* The reset at the box is the way back to a known login. Deleting this file
	   is not: a start writes it again out of the old server's file, which no
	   longer names a password once a start has carried it, so the new file would
	   hold none. */
	std::string over = "TO START OVER: the web server menu of the box (System settings, Network, "
			   "Web server (nhttpd)) resets the login to user \"root\" and a new first "
			   "password, which the screen at the box shows.";
	if (!old_path.empty())
		over += " Deleting this file is no way back: the next start writes it again out of " +
			old_path + ", which names no password any more once a start has carried it, and "
			"the login is then left with none.";
	appendWrapped(text, "", over);
}

// The title of the file and the paragraphs about the login under it.
void appendLoginHead(std::string &text, LoginSource from, const std::string &user,
		     bool has_password, const std::string &old_path)
{
	appendCommentLine(text, "nhttpd access configuration.");
	appendCommentLine(text, "");
	appendLoginLines(text, from, user, has_password, old_path);
}

/* text with the paragraphs about the login rewritten for a password a save has
   just set, so a file does not go on naming a login that is no longer in force:
   a shipped password after the box replaced it, a first password after the owner
   did. The one exception to a save leaving every line it does not own where it
   stands, and a narrow one: those paragraphs are what this layer wrote about the
   lines it owns.

   What is rewritten runs from a comment line opening with "LOGIN:" in the head
   of the file, the comment lines before any setting, to the first comment line
   with nothing on it after it. A file without such a line, or one whose head has
   no end, is handed back as it was. */
std::string withLoginLines(const std::string &text, LoginSource from, const std::string &user,
			   const std::string &old_path)
{
	const std::string opening = "# LOGIN:";
	size_t start = std::string::npos;
	size_t at = 0;
	while (at < text.size())
	{
		const size_t end = text.find('\n', at);
		if (end == std::string::npos)
			return text;
		const std::string line = text.substr(at, end - at);
		if (line.empty() || line[0] != '#')
			return text;
		if (start == std::string::npos && line.compare(0, opening.size(), opening) == 0)
			start = at;
		else if (start != std::string::npos && line == "#")
		{
			std::string lines;
			appendLoginLines(lines, from, user, true, old_path);
			return text.substr(0, start) + lines + text.substr(at);
		}
		at = end + 1;
	}
	return text;
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
	appendWrapped(text, "", std::string("WHERE THE SERVER ANSWERS is not in this file. The port and the "
		      "interface are nhttpd.conf's, ") + kServerPort + " and " + kServerHost + ", where the "
		      "menu at the box and the web interface both set them.");

	appendCommentLine(text, "");
	appendComment(text,
		      "THE LINES AT THE END OF THIS FILE.");
	appendKeyDoc(text, kKeyUser, "the name a login is made under", "");
	appendKeyDoc(text, kKeyHash, "the password, kept as a hash. An empty one is no password, and a "
		     "box with none answers nothing that changes anything", "");
	appendKeyDoc(text, kKeyFirst, "the first password in plain, which the box drew for itself and "
		     "shows on its screen. It counts only while the hash above is that password, the "
		     "first login asks to replace it, and it goes with the first change", "");

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
	appendKeyDoc(text, kKeyLegacy, "nothing acts on it: the old /control/ addresses answer whatever "
		     "it says, true or false",
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

/* The head of a file written out of the old server's, where first says the
   password found there was one these boxes shipped with and a first password
   takes its place. */
std::string migratedHead(const std::string &user, bool has_password, const std::string &old_path,
			 const std::vector<std::string> &notes, bool first)
{
	std::string text;

	appendLoginHead(text, first ? FirstLogin : CarriedLogin, user, has_password, old_path);

	appendCommentLine(text, "");
	appendWrapped(text, "", "Written from " + old_path + ", the file of the web interface this one "
		      "replaces, which is left where it is and keeps saying where the server answers.");

	appendNotes(text, "NOTES ON WHAT WAS AND WAS NOT CARRIED OVER:", notes);
	appendKeyDocs(text);

	text += '\n';
	return text;
}

// The head of a file written because there was nothing to carry over or
// because carrying it over was refused.
std::string firstHead(const std::string &user, const std::string &old_path, bool tried_old,
		      const std::vector<std::string> &why)
{
	std::string text;

	appendLoginHead(text, FirstLogin, user, true, old_path);

	appendCommentLine(text, "");
	if (tried_old)
	{
		appendWrapped(text, "", "Written by the box itself. There is a file of the web interface "
			      "this one replaces at " + old_path + ", and this is what stopped it from "
			      "being carried over:");
		for (size_t i = 0; i < why.size(); ++i)
			appendWrapped(text, "  - ", "    ", why[i]);
		appendWrapped(text, "", "So the login above is a first password and not the one that file "
			      "holds.");
	}
	else
		appendWrapped(text, "", "Written by the box itself, there having been nothing of the web "
			      "interface this one replaces to carry over.");

	appendKeyDocs(text);

	text += '\n';
	return text;
}

// The head of a file a save at the box writes where there was none.
std::string ownHead(const std::string &user, bool has_password, bool first, const std::string &old_path)
{
	std::string text;

	appendLoginHead(text, first ? FirstLogin : OwnLogin, user, has_password, old_path);

	appendCommentLine(text, "");
	appendWrapped(text, "", "Written at the box.");

	appendKeyDocs(text);

	text += '\n';
	return text;
}

// Says that this run wrote a first password, for the screen at the box to show.
void noteFirstWritten();

/* The file a box gets when there was nothing to carry over, or when carrying it over was
   refused.

   WRITTEN RATHER THAN LEFT OUT, WHICH IS THE WHOLE OF THIS. Every refusal above used to
   end at a box whose web interface had no credential at all: it answers reads from the
   private networks, refuses everything that changes anything from anywhere, and says so
   nowhere anybody looks, so its owner is locked out by a line in a boot log that is gone
   by morning. The login written here is user root and a first password, which the screen
   at the box shows and the first login asks to replace. Besides a name that cannot be
   written, the random source failing is the one way to write nothing, and that source is
   what a hash needs as well. */
bool writeFirstFile(const std::string &out_path, const std::string &old_path, bool tried_old)
{
	// Taken before the write below, which complains into the same list.
	const std::vector<std::string> why = problemsNow();

	std::string first = drawFirstPassword();
	const std::string hash = first.empty() ? std::string() : hashSecret(first);
	if (hash.empty())
	{
		forget(first);
		complain("no first password could be drawn and hashed, the random source of the system "
			 "failing, so nothing is written and this box answers no request that changes "
			 "anything until a password is set at the box itself");
		return false;
	}

	const WebConfig d = defaultWebConfig();

	std::string text = firstHead(d.username, old_path, tried_old, why);
	appendPair(text, kKeyUser, d.username);
	appendPair(text, kKeyHash, hash);
	appendPair(text, kKeyFirst, first);
	forget(first);

	const bool written = writeNewFile(out_path, text);
	forget(text);
	if (written)
		noteFirstWritten();
	return written;
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

   NOT THE WRITER IN system/helpers.h, which also writes beside the file, commits it, puts
   it in place and syncs the directory. What sets this one apart: the working name is
   removed and then created exclusively, where that one opens a side file left behind
   again; a directory sync that fails once the file is in place is said here and answered
   true, where that one answers false, and a directory the filesystem cannot sync at all
   (EINVAL) is such a failure here, where that one counts it as synced and says nothing;
   and a failure is said into the list beside this and not only onto the standard error.
   That list is what a screen at the box shows somebody who has just been refused, and a
   line on the standard error of a program drawing a television picture reaches nobody.
   Used here, that one would also have to be handed the owner-only mode and told to
   replace a link: left to its defaults it makes the file readable by everybody and
   follows a link at path.

   PATH ITSELF IS NEVER FOLLOWED EITHER, and the caller checks that before calling this:
   the step below puts a file at a name, so a name that is a link would have the link
   replaced and the file it points at left alone.

   False and the working file removed for anything up to and including the step that puts
   it in place. After that step there is no old file to go back to, so a failure past it is
   said and answered true. */
bool replaceFile(const std::string &path, const std::string &text, mode_t mode = S_IRUSR | S_IWUSR)
{
	const std::string working = workingFile(path);
	if (!discardWorking(working))
		return false;

	const int fd = makeAccessFile(working, mode);
	if (fd < 0)
		return false;

	bool ok = writeWholeFile(fd, text);
	if (ok && ::fsync(fd) != 0)
		ok = false;
	if (::close(fd) != 0)
		ok = false;

	/* The file this one replaces is kept before it loses its name, where the start
	   could not keep it: a second name of it, or a descriptor on it, stays held
	   back. Not kept, it is not replaced. */
	if (ok && !coreapi::accessfiles::named(path))
	{
		complain("cannot keep " + path + " as a file that holds who may reach the box, "
			 "so it is not replaced");
		ok = false;
	}
	if (ok && beforeReplace != NULL)
		beforeReplace(path);
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

/* What a save writes for each key it owns: the name, the stored form and the
   first password when a new password was typed, and the logo switch unless the
   save is the account's alone. Every other line of that file belongs to whoever
   wrote it and is carried across untouched, but for the paragraphs about the
   login at its head, which a new password rewrites (withLoginLines). A key owned
   to be dropped has every line naming it taken out and none written in its
   place. */
struct OwnedLine
{
	const char *key;
	std::string value;
	bool        written;
	bool        drop;
};

/* Which of the keys a save is writing this line names, and nothing for a line that names
   none of them.

   The name is taken the way scanLines takes it and not byte for byte, for the same
   reason: a line spelled Username, a line with a space in front of the separator and a
   line carrying a number sign are all lines load counts as that key being named and
   unusable. Left standing beside the line a save writes, any one of them would leave the
   file naming one account and the server checking against none. */
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

/* What nhttpd.conf says about where the server answers, read the way nhttpd reads
   it when it starts (yhttpd.cpp, ReadConfig): its own reader, its own defaults, and
   an empty address meaning every interface. Read any other way, a caller would be
   told a port nhttpd is not going to bind.

   A file that is not there is nhttpd's defaults, which is what nhttpd starts on
   before it writes the file out itself. False for a file that is there and cannot
   be read, and server is then left as it was handed in. */
bool readServerFile(const std::string &path, ServerConfig &server)
{
	const ServerConfig d = defaultConfig();
	struct stat st;
	if (::stat(path.c_str(), &st) != 0 && errno == ENOENT)
	{
		server.port = d.port;
		server.bind_address = d.bind_address;
		return true;
	}
	if (!isReadableRegularFile(path, "server configuration"))
		return false;

	CConfigFile file(',');
	if (!file.loadConfig(path))
	{
		complain("cannot parse " + path);
		return false;
	}

	server.port = file.getInt32(kServerPort, d.port);
	server.bind_address = file.getString(kServerHost, d.bind_address);
	if (server.bind_address.empty())
		server.bind_address = d.bind_address;
	return true;
}

/* The options as nhttpd reads them: the switches through the same reading its
   modules make (a page login is on for anything but false or 0, every file type
   is sent only for true), and every line the file does not carry at nhttpd's
   default. False for a file that is there and cannot be read, and options are
   then left as they were handed in. */
bool readServerOptions(const std::string &path, ServerOptions &options)
{
	struct stat st;
	if (::stat(path.c_str(), &st) != 0 && errno == ENOENT)
	{
		options = ServerOptions();
		return true;
	}
	if (!isReadableRegularFile(path, "server configuration"))
		return false;

	CConfigFile file(',');
	if (!file.loadConfig(path))
	{
		complain("cannot parse " + path);
		return false;
	}

	const ServerOptions d;
	options.page_login = file.getBool(kServerAuth, d.page_login);
	options.no_login_prefix = file.getString(kServerNoAuth, d.no_login_prefix);
	options.threading = file.getBool(kServerThreading, d.threading);
	options.override_directory = file.getString(kServerOverride, d.override_directory);
	options.hosted_directory = file.getString(kServerHosted, d.hosted_directory);
	options.send_all_types = file.getString(kServerSendAll, d.send_all_types ? "true" : "false") == "true";
	return true;
}

// One line of nhttpd.conf a save writes, and the value it is to read back as.
struct ServerLine
{
	const char *key;
	std::string value;
};

/* Writes lines of nhttpd.conf as the menu at the box changes them
   (gui/nhttpd_setup.cpp): read through nhttpd's own reader, so every other line
   of that file stays as nhttpd and its pages wrote it, and written through
   saveServerFile, which checks what nhttpd's own writer does not. The caller
   holds ServerFileLock.

   A file that is not there is refused rather than created: nhttpd writes it out
   when it starts, and a file holding only a few of its lines is one nhttpd takes
   for the format before its own and rebuilds without them. */
bool writeServerLines(const std::string &path, const std::vector<ServerLine> &lines)
{
	if (!isReadableRegularFile(path, "server configuration"))
		return false;

	CConfigFile file(',', false);
	if (!file.loadConfig(path))
	{
		complain("cannot parse " + path + ", and nothing is written rather than a file holding a "
			 "few lines of it");
		return false;
	}

	for (size_t i = 0; i < lines.size(); ++i)
		file.setString(lines[i].key, lines[i].value);

	return saveServerFile(path, file.getConfigDataMap());
}

std::vector<ServerLine> serverLinesOf(int port, const std::string &host)
{
	std::vector<ServerLine> lines;
	ServerLine one;
	one.key = kServerPort;
	one.value = decimal(static_cast<unsigned long>(port));
	lines.push_back(one);
	one.key = kServerHost;
	one.value = host;
	lines.push_back(one);
	return lines;
}

/* The start of an address as nhttpd writes its callers' down before it compares
   them, byte for byte, with this: dotted decimal of the first family, the only one
   it listens on, without leading zeros. So up to four groups of up to three
   digits, none above 255 and none but a lone 0 starting with 0, with a dot
   between them and at most one after the last of fewer than four. Empty is
   nobody. Anything else matches no caller, which a page could not tell from a
   prefix that works. */
bool isAddressPrefix(const std::string &v)
{
	if (v.size() > kMaxPrefixChars)
		return false;

	size_t groups = 0;
	size_t at = 0;
	while (at < v.size())
	{
		const size_t dot = v.find('.', at);
		const std::string group = v.substr(at, (dot == std::string::npos) ? std::string::npos : dot - at);
		if (group.empty() || group.size() > 3 || (group.size() > 1 && group[0] == '0'))
			return false;
		unsigned value = 0;
		for (size_t i = 0; i < group.size(); ++i)
		{
			if (group[i] < '0' || group[i] > '9')
				return false;
			value = value * 10 + static_cast<unsigned>(group[i] - '0');
		}
		if (value > 255)
			return false;
		++groups;
		if (dot == std::string::npos)
			break;
		if (groups == 4)
			return false;
		at = dot + 1;
	}
	return true;
}

/* A directory nhttpd serves files out of: an absolute path, without a step up,
   that reads back out of the file as it was written. Whether it is there is not
   asked: nhttpd looks for each file as it is asked for, and a directory made later
   is one it serves from then on. */
bool isServedDirectory(const std::string &v)
{
	if (v.empty() || v.size() > kMaxDirectoryChars || v[0] != '/' || !survivesTheFile(v))
		return false;
	const std::string wrapped = v + "/";
	return wrapped.find("/../") == std::string::npos;
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
	   well, which nobody asked this server to do. */
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

namespace
{

// Built on first use, for the reason held() is.
Login &heldLogin()
{
	static Login l;
	return l;
}

// Beside heldLogin and under the same lock.
std::string &heldFirst()
{
	static std::string first;
	return first;
}

OpenThreads::Mutex &loginLock()
{
	static OpenThreads::Mutex m;
	return m;
}

// Whether this run wrote a first password, under loginLock.
bool &firstWritten()
{
	static bool written = false;
	return written;
}

void noteFirstWritten()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held_lock(loginLock());
	firstWritten() = true;
}

OpenThreads::Mutex &accountLock()
{
	static OpenThreads::Mutex m;
	return m;
}


/* What the last save wrote into the file the server reads, from that save until a
   load reads the file back. A REST save is put in effect by a restart that runs
   once its answer is gone; a writer in between that started from what is in effect
   would write the values that save replaced back over it. Held under loginLock. */
struct Pending
{
	bool        held;
	WebSettings s;
	bool        new_password;
	bool        restart_due;

	// the session keepSessionAtLoad named, carried across the change the load makes
	std::string keep_session;

	Pending() : held(false), new_password(false), restart_due(false) {}
};

Pending &pending()
{
	static Pending p;
	return p;
}

void forgetPending()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held_lock(loginLock());
	forget(pending().keep_session);
	pending() = Pending();
}

// The session keepSessionAtLoad named, and empty where none is named.
std::string pendingKeep()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held_lock(loginLock());
	return pending().keep_session;
}

// A restart ran, whether or not its read went through: none is due any more.
void restartRan()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held_lock(loginLock());
	pending().restart_due = false;
}

} // namespace

AccountLock::AccountLock()
{
	accountLock().lock();
}

AccountLock::~AccountLock()
{
	accountLock().unlock();
}

ServerFileLock::ServerFileLock()
{
	serverFileLock().lock();
}

ServerFileLock::~ServerFileLock()
{
	serverFileLock().unlock();
}

Login loginInEffect()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held_lock(loginLock());
	return heldLogin();
}

unsigned long long accountRevision()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held_lock(loginLock());
	return heldLogin().revision;
}

std::string firstPasswordInEffect()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held_lock(loginLock());
	return heldFirst();
}

bool firstPasswordStands()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held_lock(loginLock());
	return !heldFirst().empty();
}

unsigned long long publishLogin(const Login &l, const std::string &first_password)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held_lock(loginLock());
	Login &in_effect = heldLogin();
	/* A first password standing or gone alone is no new account: it goes with a stored
	   form, and a stored form that changed is counted here already. */
	const bool same = in_effect.username == l.username && in_effect.password_hash == l.password_hash;
	const unsigned long long revision = same ? in_effect.revision : in_effect.revision + 1;
	in_effect = l;
	in_effect.revision = revision;
	forget(heldFirst());
	heldFirst() = first_password;
	return revision;
}

void keepSessionAtLoad(const std::string &token)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held_lock(loginLock());
	if (!pending().held)
		return;
	forget(pending().keep_session);
	pending().keep_session = token;
}

bool firstPasswordWrittenThisRun()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held_lock(loginLock());
	return firstWritten();
}

void setConfigForTest(const WebConfig &c)
{
	forgetPending();
	held() = c;
	heldPath().clear();
	heldServerPath().clear();
	Login l;
	l.username = c.username;
	l.password_hash = c.password_hash;
	publishLogin(l);
}

std::vector<std::string> configProblems()
{
	return problemsNow();
}

const std::string &configPath()
{
	return heldPath();
}

const std::string &serverPath()
{
	return heldServerPath();
}

bool load(const std::string &path, const std::string &server_path)
{
	(void) serverFileLock();
	AccountLock account;
	clearProblems();

	/* Before the file is looked at, and kept whatever the look comes to. What
	   this records is where the box keeps its configuration, which is a
	   different question from whether there is one there to read: a box being
	   started for the first time has no file and still has the one name a save
	   is to write it at. */
	heldPath() = path;
	heldServerPath() = server_path;

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

	/* Where the server answers, out of nhttpd's file and not this one, so that
	   what is in effect here is what nhttpd binds when it starts. A file there
	   that cannot be read leaves nhttpd's defaults, which is what nhttpd itself
	   starts on without one. */
	if (!server_path.empty() && !readServerFile(server_path, c.server))
		complain("where the server answers could not be read out of " + server_path + ", and "
			 "what stands is nhttpd's default, port " + decimal(c.server.port) + " on every "
			 "interface");

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

	/* The first password counts only while the stored form is that password,
	   which is asked here once and not taken on the line's word: a line that
	   outlived a change made by hand would otherwise be shown on the screen as
	   the login and ask the owner to replace a password the box no longer
	   takes. Its value is never repeated, being a password. */
	std::string first;
	{
		const Reading r = reading(m, cut, kKeyFirst, raw);
		if (r == KeyUnreadable)
			complain(std::string(kKeyFirst) + ": the line carries a value this cannot read, so no "
				 "first password stands");
		else if (r == KeyPresent && !raw.empty())
		{
			if (!c.password_hash.empty() && verifySecret(raw, c.password_hash))
				first = raw;
			else
				complain(std::string(kKeyFirst) + ": the password on the line is not the one "
					 "stored, so it is neither shown nor asked to be replaced");
		}
		forget(raw);
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
	/* What changed the account since the last read is a save waiting for this one,
	   and the session of the caller that asked for it stays signed in: carried from
	   the account it was opened under to this one, while every other session ends.
	   Under the account's lock, so nothing publishes in between. */
	std::string keep = pendingKeep();
	forgetPending();
	Login l;
	l.username = c.username;
	l.password_hash = c.password_hash;
	const unsigned long long was = accountRevision();
	const unsigned long long now = publishLogin(l, first);
	if (now != was && !keep.empty())
		(void) carrySession(keep, was, now);
	forget(keep);
	forget(first);
	return true;
}

WebSettings currentWebSettings()
{
	WebSettings s;
	bool waiting = false;
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> held_lock(loginLock());
		waiting = pending().held;
		if (waiting)
			s = pending().s;
	}
	if (!waiting)
		s = settingsInEffect();

	/* Where the server answers is what nhttpd.conf says now, whatever is in effect
	   and whatever a save here wrote: the menu at the box writes that file too, and
	   a writer starting from anything older would put back the port it moved. */
	if (!heldServerPath().empty())
	{
		ServerConfig server = defaultConfig();
		if (readServerFile(heldServerPath(), server))
		{
			s.port = server.port;
			s.bind_address = server.bind_address;
		}
	}
	s.options = currentServerOptions();
	return s;
}

ServerOptions::ServerOptions()
	: page_login(false),
	  threading(true),
	  override_directory(PUBLIC_HTTPDDIR),
	  hosted_directory(HOSTED_HTTPDDIR),
	  send_all_types(false)
{
}

bool sameServerOptions(const ServerOptions &a, const ServerOptions &b)
{
	return a.page_login == b.page_login && a.no_login_prefix == b.no_login_prefix &&
	       a.threading == b.threading && a.override_directory == b.override_directory &&
	       a.hosted_directory == b.hosted_directory && a.send_all_types == b.send_all_types;
}

void setAfterCreateForTest(void (*hook)(const std::string &path))
{
	afterCreate = hook;
}

void setBeforeReplaceForTest(void (*hook)(const std::string &path))
{
	beforeReplace = hook;
}

// nhttpd, built without exceptions, calls this directly: nothing leaves it
std::string workingFile(const std::string &path)
{
	try
	{
		return path + ".new";
	}
	catch (...)
	{
		return std::string();
	}
}

/* nhttpd calls this directly and is built without exceptions, so nothing may
   leave it: an allocation that fails is a file left as it was. */
bool saveServerFile(const std::string &path, const std::map<std::string, std::string> &lines)
{
	try
	{
		// settings put back from an archive wait for the restart that reads them
		coreapi::SettingsWrite may;
		if (!may.allowed())
		{
			complain(path + ": settings are being put back, and nothing is written until the restart");
			return false;
		}
		std::string text;
		for (std::map<std::string, std::string>::const_iterator it = lines.begin(); it != lines.end(); ++it)
			text += it->first + "=" + it->second + "\n";

		// What nhttpd's writer gives a file it makes (libconfigfile, saveConfig).
		mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
		struct stat st;
		if (::stat(path.c_str(), &st) == 0)
			mode = st.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);

		if (!replaceFile(path, text, mode))
		{
			complain("cannot write " + path + ", which is left as it was");
			return false;
		}
		return true;
	}
	catch (...)
	{
		return false;
	}
}

ServerOptions currentServerOptions()
{
	ServerOptions o;
	if (!heldServerPath().empty())
		readServerOptions(heldServerPath(), o);
	return o;
}

namespace
{

/* Every language the pages have texts for (data/web-ui/languages): its code, the
   name of the old pages' file, which is what nhttpd.conf names, and what it calls
   itself. A language not here is one this server has no code for. */
struct LanguageRow
{
	const char *code;
	const char *file;
	const char *name;
};

const LanguageRow kLanguages[] = {
	{ "en", "English",    "English" },
	{ "de", "Deutsch",    "Deutsch" },
	{ "cs", "Czech",      "\xc4\x8c" "e\xc5\xa1tina" },
	{ "pl", "Polski",     "Polski" },
	{ "pt", "Portuguese", "Portugu\xc3\xaas" },
	{ "sk", "Slovak",     "Sloven\xc4\x8dina" },
};

const char kServerLanguage[]  = "Language.selected";
const char kServerDirectory[] = "WebsiteMain.directory";

// nhttpd's own default (yconfig.h, HTTPD_DEFAULT_LANGUAGE), for a file that
// names none.
const char kDefaultLanguage[] = "English";

// nhttpd.conf through nhttpd's own reader, empty where there is no such file,
// none was named or it does not parse, so every default applies.
void readServerFileInto(CConfigFile &file)
{
	if (!heldServerPath().empty())
		file.loadConfig(heldServerPath());
}

bool regularFile(const std::string &path)
{
	struct stat st;
	return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

/* The interface's texts in a language, as the pages ask for them:
   /languages/<code>.json, found the way the server finds every file it hands
   out (ystatic::resolveName), file by file in the override directory, then in
   the pages' own, each also compressed. Not Language.directory, which names
   the old pages' files and is no part of the address a page asks for. */
bool hasInterfaceTexts(CConfigFile &file, const char *code)
{
	const std::string name = std::string("/languages/") + code + ".json";
	const std::string over = file.getString(kServerOverride, PUBLIC_HTTPDDIR) + name;
	const std::string core = file.getString(kServerDirectory, PRIVATE_HTTPDDIR) + name;
	return regularFile(over) || regularFile(over + ".gz") || regularFile(core) || regularFile(core + ".gz");
}

} // namespace

std::vector<WebLanguage> webLanguages()
{
	CConfigFile file(',');
	readServerFileInto(file);

	std::vector<WebLanguage> offered;
	for (size_t i = 0; i < sizeof(kLanguages) / sizeof(kLanguages[0]); ++i)
	{
		if (!hasInterfaceTexts(file, kLanguages[i].code))
			continue;
		WebLanguage l;
		l.code = kLanguages[i].code;
		l.name = kLanguages[i].name;
		offered.push_back(l);
	}
	return offered;
}

std::string webLanguage()
{
	CConfigFile file(',');
	readServerFileInto(file);
	const std::string named = file.getString(kServerLanguage, kDefaultLanguage);
	for (size_t i = 0; i < sizeof(kLanguages) / sizeof(kLanguages[0]); ++i)
		if (named == kLanguages[i].file)
			return kLanguages[i].code;
	return "en";
}

bool setWebLanguage(const std::string &code)
{
	bool offered = false;
	const std::vector<WebLanguage> all = webLanguages();
	for (size_t i = 0; i < all.size(); ++i)
		offered = offered || all[i].code == code;
	if (!offered || heldServerPath().empty())
		return false;

	for (size_t i = 0; i < sizeof(kLanguages) / sizeof(kLanguages[0]); ++i)
	{
		if (code != kLanguages[i].code)
			continue;
		std::vector<ServerLine> lines(1);
		lines[0].key = kServerLanguage;
		lines[0].value = kLanguages[i].file;
		ServerFileLock hold;
		return writeServerLines(heldServerPath(), lines);
	}
	return false;
}

bool newPasswordAwaitsLoad()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held_lock(loginLock());
	return pending().held && pending().new_password;
}

bool restartAwaitsLoad()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held_lock(loginLock());
	return pending().held && pending().restart_due;
}

void armRestart()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held_lock(loginLock());
	if (pending().held)
		pending().restart_due = true;
}

WebSettings settingsInEffect()
{
	const WebConfig &c = config();

	WebSettings s;
	s.port = c.server.port;
	s.bind_address = c.server.bind_address;
	s.username = loginInEffect().username;
	// Nothing to show: what is held is a stored form, and the switch beside it
	// is what makes an untouched field mean the stored form stays.
	s.password.clear();
	s.change_password = false;
	s.channel_logos = c.channel_logos;
	return s;
}

namespace
{

// ServerFileLock where a save writes nhttpd.conf, and nothing where it cannot.
struct ServerFileHold
{
	explicit ServerFileHold(bool take) : held(take)
	{
		if (held)
			serverFileLock().lock();
	}

	~ServerFileHold()
	{
		if (held)
			serverFileLock().unlock();
	}

	const bool held;

private:
	ServerFileHold(const ServerFileHold &);
	ServerFileHold &operator=(const ServerFileHold &);
};

} // namespace

bool usernameAcceptable(const std::string &name, std::string *why)
{
	if (!name.empty() && survivesTheFile(name))
		return true;
	if (why != NULL)
		*why = "the new name is empty, or carries a control byte, a number sign or a space at one of its ends";
	return false;
}

bool saveWebSettings(const std::string &path, const WebSettings &s, std::string *stored_hash,
                     const WebSettings *from)
{
	clearProblems();
	// settings put back from an archive wait for the restart that reads them
	coreapi::SettingsWrite may;
	if (!may.allowed())
	{
		complain(path + ": settings are being put back, and nothing is written until the restart");
		return false;
	}
	if (stored_hash != NULL)
		stored_hash->clear();

	/* Where the server answers goes into nhttpd.conf, and only where it changes:
	   a save leaving the port and the address as that file has them neither
	   touches the file, which is nhttpd's, nor judges what it finds there. Each
	   of the two is judged only when it changes, so a value somebody wrote there
	   by hand that nhttpd reads and this would not write stays where it is. A
	   save of the account alone never looks at the file. */
	const std::string &server_path = heldServerPath();

	/* From the read of nhttpd.conf below to the write of it and, where the access
	   file then cannot be written, to putting it back: every other writer of that
	   file in this program waits meanwhile, so none of them writes over this. */
	ServerFileHold server_hold(!s.account_only && !server_path.empty());

	ServerConfig was = config().server;
	ServerOptions was_options;
	if (!s.account_only && !server_path.empty() &&
	    (!readServerFile(server_path, was) || !readServerOptions(server_path, was_options)))
		return false;

	/* What the caller changes is what it asks for against what it started from,
	   and only that is written: a value the menu at the box wrote since the caller
	   read the file stays, unless the caller changed that same value. Nor is a
	   value written that the file already says. */
	ServerConfig base = was;
	ServerOptions base_options = was_options;
	if (from != NULL)
	{
		base.port = from->port;
		base.bind_address = from->bind_address;
		base_options = from->options;
	}
	bool port_changes = false;
	bool host_changes = false;
	if (!s.account_only)
	{
		port_changes = s.port != base.port && s.port != was.port;
		host_changes = s.bind_address != base.bind_address && s.bind_address != was.bind_address;
	}

	/* The lines of nhttpd.conf this save writes, and what each of them said
	   before, which is what goes back when the access file then cannot be
	   written. */
	std::vector<ServerLine> server_lines;
	std::vector<ServerLine> server_before;
	if (!s.account_only)
	{
		const ServerOptions &o = s.options;
		const ServerOptions &w = was_options;
		const ServerOptions &b = base_options;
		const struct
		{
			const char *key;
			bool        changes;
			std::string value;
			std::string before;
		} all[] =
		{
			{ kServerPort, port_changes, decimal(static_cast<unsigned long>(s.port)),
			  decimal(static_cast<unsigned long>(was.port)) },
			{ kServerHost, host_changes, s.bind_address, was.bind_address },
			{ kServerAuth, o.page_login != b.page_login && o.page_login != w.page_login,
			  o.page_login ? "true" : "false", w.page_login ? "true" : "false" },
			{ kServerNoAuth, o.no_login_prefix != b.no_login_prefix && o.no_login_prefix != w.no_login_prefix,
			  o.no_login_prefix, w.no_login_prefix },
			{ kServerThreading, o.threading != b.threading && o.threading != w.threading,
			  o.threading ? "true" : "false", w.threading ? "true" : "false" },
			{ kServerOverride, o.override_directory != b.override_directory &&
			  o.override_directory != w.override_directory, o.override_directory, w.override_directory },
			{ kServerHosted, o.hosted_directory != b.hosted_directory &&
			  o.hosted_directory != w.hosted_directory, o.hosted_directory, w.hosted_directory },
			{ kServerSendAll, o.send_all_types != b.send_all_types && o.send_all_types != w.send_all_types,
			  o.send_all_types ? "true" : "false", w.send_all_types ? "true" : "false" },
		};
		for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); ++i)
		{
			if (!all[i].changes)
				continue;
			ServerLine line;
			line.key = all[i].key;
			line.value = all[i].value;
			server_lines.push_back(line);
			line.value = all[i].before;
			server_before.push_back(line);
		}
	}
	const bool move = !server_lines.empty();

	/* Every value is judged before either file is opened, so a value that cannot
	   be used leaves both as they were rather than half of it rewritten. The
	   value is not repeated back where the key makes it a credential. */
	if (move && server_path.empty())
	{
		complain("this server was never told which file says where it answers, so the port, the "
			 "address and the rest of nhttpd.conf cannot be written, and nothing is written");
		return false;
	}

	if (port_changes && (s.port < 1 || s.port > 65535))
	{
		complain(std::string(kServerPort) + ": the port asked for is not one from 1 to 65535, and "
			 "nothing is written");
		return false;
	}

	if (host_changes && (!survivesTheFile(s.bind_address) || !isListenAddress(s.bind_address)))
	{
		complain(std::string(kServerHost) + ": " + quoted(s.bind_address) + " is not an address this "
			 "server can listen on, and nothing is written. It listens on addresses of the first "
			 "family only, and every interface is written as \"0.0.0.0\"");
		return false;
	}

	if (s.options.no_login_prefix != base_options.no_login_prefix &&
	    s.options.no_login_prefix != was_options.no_login_prefix && !s.account_only &&
	    !isAddressPrefix(s.options.no_login_prefix))
	{
		complain(std::string(kServerNoAuth) + ": " + quoted(s.options.no_login_prefix) + " is not the "
			 "start of an address of the first family as nhttpd writes them, such as \"192.168.1.\", "
			 "and nothing is written");
		return false;
	}

	const struct
	{
		const char        *key;
		const std::string *now;
		const std::string *base;
		const std::string *was;
	} directories[] =
	{
		{ kServerOverride, &s.options.override_directory, &base_options.override_directory,
		  &was_options.override_directory },
		{ kServerHosted, &s.options.hosted_directory, &base_options.hosted_directory,
		  &was_options.hosted_directory },
	};
	for (size_t i = 0; i < sizeof(directories) / sizeof(directories[0]); ++i)
	{
		if (s.account_only || *directories[i].now == *directories[i].base ||
		    *directories[i].now == *directories[i].was || isServedDirectory(*directories[i].now))
			continue;
		complain(std::string(directories[i].key) + ": " + quoted(*directories[i].now) + " is not an "
			 "absolute path without a step up, and nothing is written");
		return false;
	}

	if (!usernameAcceptable(s.username, NULL))
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

		// a first password is written as it stands, so it has to read back as it stands
		if (s.first_password && !survivesTheFile(s.password))
		{
			complain(std::string(kKeyFirst) + ": the password carries a byte that does not read back "
				 "as it was written, and nothing is written");
			return false;
		}

		hash = hashSecret(s.password);
		if (hash.empty())
		{
			complain(std::string(kKeyHash) + ": the password could not be hashed, and nothing is "
				 "written");
			return false;
		}
		if (stored_hash != NULL)
			*stored_hash = hash;
	}

	std::vector<OwnedLine> owned;
	{
		OwnedLine one;
		one.written = false;
		one.drop = false;

		one.key = kKeyUser;
		one.value = s.username;
		owned.push_back(one);

		/* Written as a word and not as a digit, because that is how the one
		   switch this file already had is written wherever a person sets it,
		   and a file where one switch reads true and the next reads 1 is a file
		   somebody has to look up the reader for. Both are read back. */
		if (!s.account_only)
		{
			one.key = kKeyLogos;
			one.value = s.channel_logos ? "true" : "false";
			owned.push_back(one);
		}

		/* The stored form is a line this save owns only when a new password was
		   typed. Left out of the list otherwise, whatever the file holds under
		   that key is carried across like any other line nobody asked about,
		   which is what an untouched field has to mean. */
		if (s.change_password)
		{
			one.key = kKeyHash;
			one.value = hash;
			owned.push_back(one);

			/* The first password goes with the stored form it is, so it is written
			   with it or taken out with it, and carried across with the rest where
			   no password was typed: a name changed alone leaves the box asking
			   for a password of the owner's own, as it did. */
			one.key = kKeyFirst;
			one.value = s.first_password ? s.password : std::string();
			one.drop = !s.first_password;
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
		if (s.change_password)
			text = withLoginLines(text, s.first_password ? FirstLogin : OwnLogin, s.username,
					      server_path);
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
		text = ownHead(s.username, s.change_password, s.change_password && s.first_password,
			       server_path);
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

		// A line this file no longer reads goes, so the file stops saying a
		// port nothing here is answering on.
		const size_t sep = line.find('=');
		if (sep != std::string::npos && !retiredKeyFor(trimmed(line.substr(0, sep))).empty())
			continue;

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
			if (!owned[which].drop)
				appendPair(out, owned[which].key, owned[which].value);
			owned[which].written = true;
		}
	}

	for (size_t i = 0; i < owned.size(); ++i)
	{
		if (!owned[i].written && !owned[i].drop)
			appendPair(out, owned[i].key, owned[i].value);
	}
	for (size_t i = 0; i < owned.size(); ++i)
		forget(owned[i].value);

	/* nhttpd's file first and this one after, and the first put back when the
	   second cannot be written, so a save answered false has not moved the port
	   that the next start binds. Two files are never written as one step; what
	   this rules out is the half of a save that would outlast a refusal. */
	if (move && !writeServerLines(server_path, server_lines))
		return false;

	const bool replaced = replaceFile(path, out);
	forget(out);
	if (!replaced && move && !writeServerLines(server_path, server_before))
		complain(server_path + " could not be put back, and says what this save was refused for");

	/* Written into the file the server reads: what it now says is where the next
	   writer starts until a load reads it back. An account-only save is in effect
	   when it returns (login_bridge publishes it), so it leaves nothing waiting and
	   only carries its name into a save that already waits. */
	if (replaced && path == heldPath())
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> held_lock(loginLock());
		if (s.change_password && s.first_password)
			firstWritten() = true;
		if (s.account_only)
		{
			if (pending().held)
				pending().s.username = s.username;
		}
		else
		{
			Pending p;
			p.held = true;
			p.s = s;
			p.s.password.clear();
			p.s.change_password = false;
			p.s.first_password = false;
			p.new_password = s.change_password || (pending().held && pending().new_password);
			p.restart_due = pending().held && pending().restart_due;
			if (pending().held)
				p.keep_session = pending().keep_session;
			forget(pending().keep_session);
			pending() = p;
		}
	}
	return replaced;
}

bool reloadAndRestart(const std::string &path)
{
	/* First, and the read below is inside the window this opens. What is in
	   effect is read by request threads without a lock, so replacing it under a
	   running daemon would be replacing something one of them holds, and a
	   listening socket has nowhere to put a new port in any case. */
	const ServerConfig before = config().server;
	stop();

	const bool read = load(path, heldServerPath());
	if (!read)
	{
		complain("the configuration could not be read again, so what was already in effect stays in "
			 "effect and the server comes back up on that");
		/* Except where it answers. nhttpd binds what its own file says whatever
		   was read here, so that is read again on its own: the going back below
		   compares against it, and without it a new port nothing can bind would
		   look like the old one and the server would stay down. */
		if (!heldServerPath().empty())
			readServerFile(heldServerPath(), held().server);
	}

	bool up = start(config().server);

	/* Nothing on the new port or address, which is usually something else already
	   holding it. Once, back to where the server answered before, and nhttpd.conf
	   back with it so the next start does not repeat the failure: a box answering
	   where it used to beats one answering nowhere until somebody edits a file. */
	bool went_back = false;
	const ServerConfig failed = config().server;
	if (!up && !heldServerPath().empty() &&
	    (failed.port != before.port || failed.bind_address != before.bind_address))
	{
		complain("nothing answers on port " + decimal(static_cast<unsigned long>(failed.port)) + " at " +
			 quoted(failed.bind_address) + ", so the server goes back to port " +
			 decimal(static_cast<unsigned long>(before.port)) + " at " + quoted(before.bind_address) +
			 " and " + heldServerPath() + " says so again");
		bool put_back = false;
		{
			ServerFileLock hold;
			put_back = writeServerLines(heldServerPath(), serverLinesOf(before.port, before.bind_address));
		}
		if (put_back)
		{
			held().server = before;
			up = start(before);
			went_back = up;
		}
	}

	if (!up)
		complain("the server did not come back up, so this box answers nothing over the network until "
			 "the port and the address it was given are ones it can bind");

	/* A read that failed leaves the saves it was for waiting, and a writer still
	   starts from them; the restart that was armed for them has run all the same. */
	restartRan();
	return read && up && !went_back;
}

bool migrateFrom(const std::string &nhttpd_conf, const std::string &out_path)
{
	clearProblems();

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
	const char *const carried[] = { kOldUser, kOldPassword };
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

	/* Only the account. Where the server answers stays in that file, which
	   nhttpd goes on reading and which this layer reads it out of as well. */
	std::string user = "root";
	std::string raw;

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
	std::string first;
	bool has_password = false;
	bool shipped = false;
	{
		std::string secret;
		const bool has_secret = present(m, kOldPassword, secret) && !secret.empty();
		shipped = has_secret && isShippedPassword(secret);

		has_password = has_secret;
		if (shipped)
		{
			first = drawFirstPassword();
			if (!first.empty())
				hash = hashSecret(first);
		}
		else if (has_password)
			hash = hashSecret(secret);
		forget(secret);

		if (has_password && hash.empty())
		{
			// The box would look configured and answer nothing that needs the
			// password. Saying so and writing nothing is the loud half of the
			// same outcome.
			forget(first);
			complain(shipped ? "no first password could be drawn and hashed, and nothing is written"
					 : "the password could not be hashed, and nothing is written");
			return false;
		}

		if (!has_secret)
			complain(std::string(kOldPassword) +
				 " is empty, so the new file carries no password and every request needing one is refused");
		else if (shipped)
		{
			/* Said once, at the first start, and not a refusal: what stood there is a
			   password every box has shipped with, which is not carried, a first
			   password taking its place. The value is not repeated here, being one of
			   a list. */
			complain(std::string(kOldPassword) + " is a password these boxes have shipped "
				 "with, which anybody can look up, so it is not carried across: the box "
				 "drew a first password of its own, which its screen shows, and the first "
				 "login asks for a password of your own");
		}
	}

	if (present(m, kOldSwitch, raw) && (raw == "false" || raw == "0"))
		complain(std::string(kOldSwitch) +
			 " turned authentication off, and that is the one thing this file does not carry across");

	if (present(m, kOldExempt, raw) && !raw.empty())
		complain(std::string(kOldExempt) +
			 " named callers that needed no password, and this server exempts nobody by name");

	/* Taken before the write below, which complains into the same list. Every
	   one of these is said in the written file as well as on the standard
	   error: the list is read by whoever is watching a boot, and the file is
	   read by whoever comes looking for the password weeks later. */
	const std::vector<std::string> notes = problemsNow();

	std::string text = migratedHead(user, has_password, nhttpd_conf, notes, shipped);

	appendPair(text, kKeyUser, user);
	appendPair(text, kKeyHash, hash);
	if (shipped)
		appendPair(text, kKeyFirst, first);
	forget(first);

	const bool written = writeNewFile(out_path, text);
	forget(text);
	if (written && shipped)
		noteFirstWritten();
	return written;
}

/* The file under the name it had before, taken over as it stands: it is this
   file in every line, so nothing is carried or judged, only the first line of
   the head is given the file's new name. Created exclusively like every other
   file this writes. */
bool takeOverFormer(const std::string &former_path, const std::string &out_path)
{
	if (!isReadableRegularFile(former_path, "former configuration"))
		return false;

	/* Read to its end or not at all, because what this writes is the box's
	   login from now on and the file it came from goes: a read that stopped
	   early would be a shorter login the next start no longer has the whole
	   of. */
	std::string text;
	{
		const int fd = ::open(former_path.c_str(), O_RDONLY | O_CLOEXEC);
		bool ok = fd >= 0;
		char buf[4096];
		while (ok)
		{
			const ssize_t n = ::read(fd, buf, sizeof(buf));
			if (n < 0 && errno == EINTR)
				continue;
			if (n <= 0)
			{
				ok = (n == 0);
				break;
			}
			text.append(buf, static_cast<size_t>(n));
			ok = text.size() <= static_cast<size_t>(kMaxFileBytes);
		}
		if (fd >= 0)
			::close(fd);
		if (!ok)
		{
			complain("cannot read the former configuration " + former_path + " to its end, so it is "
				 "left where it is and taken over at the next start");
			return false;
		}
	}
	const std::string old_head = "# ni-web configuration.";
	if (text.compare(0, old_head.size(), old_head) == 0)
		text.replace(0, old_head.size(), "# nhttpd access configuration.");
	return writeNewFile(out_path, text);
}

bool ensureConfigFile(const std::string &nhttpd_conf, const std::string &out_path,
		      const std::string &former_path)
{
	/* Nothing below removes one of the files, or the working file one is written
	   through, before it is kept by what it is (the former name here, a working
	   file by the writers, a file replaced by replaceFile): a second name one has
	   elsewhere stays held back once its name here is gone, and a file that
	   cannot be kept is left where it is. */
	/* A link to a file counts as the file: a box that kept its login there
	   through a link is a box with a login, and building one out of nhttpd's
	   file instead would hand it the one it shipped with. It is read through
	   the link, and what goes afterwards is the link and not what it points
	   at. */
	struct stat st;
	const bool have_former = !former_path.empty() && ::lstat(former_path.c_str(), &st) == 0 &&
				 (S_ISREG(st.st_mode) ||
				  (S_ISLNK(st.st_mode) && ::stat(former_path.c_str(), &st) == 0 && S_ISREG(st.st_mode)));

	/* lstat and not stat, so that a link pointing nowhere counts as the name
	   being taken rather than as nothing being there. The creations below are
	   exclusive and would refuse such a name anyway; what this keeps away is
	   the attempt and a line about it on every start for as long as the name
	   stays that way. A file under the former name beside it is a leftover
	   holding an old password hash and tokens, which nothing reads any more,
	   so it goes; the storage layer goes on refusing that name for a box
	   where it cannot. */
	if (::lstat(out_path.c_str(), &st) == 0)
	{
		if (have_former && coreapi::accessfiles::named(former_path) &&
		    ::unlink(former_path.c_str()) == 0)
			complain(former_path + " was left beside " + out_path + " and is removed; " +
				 out_path + " is the one in effect");
		return false;
	}

	/* The file under its former name is this box's login already: taken over
	   whole, and not built again out of nhttpd's file, which no longer holds a
	   password once the account was carried. Where it cannot be taken over
	   both stay as they are and the next start tries again, rather than a
	   shipped login taking the place of the one the box has. */
	if (have_former)
	{
		if (!takeOverFormer(former_path, out_path))
			return false;
		if (!coreapi::accessfiles::named(former_path) || ::unlink(former_path.c_str()) != 0)
			complain(std::string("cannot remove ") + former_path + " after taking it over: " +
				 std::strerror(errno));
		return true;
	}

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
		clearProblems();

	return writeFirstFile(out_path, nhttpd_conf, have_old);
}

} // namespace httpd
