/*
 * netfs.cpp - the network filesystems this box mounts, as entries and not as file text
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

#include <config.h>

#include "netfs.h"

#include "coreapi/base/errors.h"
#include "shellcontract.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <utility>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

#include <system/helpers.h>

namespace coreapi
{
namespace netfs
{

namespace
{

/* Where the two files are on a box.

   THE FIRST ONE IS NOT /etc/fstab AND THE DIFFERENCE MATTERS. This box has two
   files of that name and they hold different things. /etc/fstab holds the
   filesystems of the box itself, its own disc among them, and is read by the
   start up script's first pass; /var/etc/fstab holds nothing but the network
   filesystems and is read by that script's second pass, which is the one the
   acts below drive. The image installs both, neither is a link to the other,
   and the screen at the box writes the second and never the first. A write that
   landed in /etc/fstab would be this server editing how the box mounts its own
   disc.

   The old web interface had a fallback from the second to the first, and on
   this image that fallback could only ever have opened the wrong file. */
const char kFstabPath[]     = "/var/etc/fstab";
const char kAutomountPath[] = "/var/etc/auto.net";

// Where the scripts that mount, unmount and restart the automounter live.
const char kInitRoot[] = "/etc/init.d";

/* What the screen at the box writes into a field nobody filled in, and what it
   reads back as nothing. Kept because that screen and this write each other's
   files: a slot left genuinely blank would give a line with columns missing,
   and the columns behind it would then be read as the wrong things. */
const char kNoHost[]  = "0.0.0.0";
const char kNoDir[]   = "netfsDir";
const char kNoMount[] = "mountPt";

// The one word a filesystem table has for an entry with no options. Written
// where there would otherwise be an empty column, which is a column the reader
// of such a file cannot see, and read back as no options at all.
const char kNoOptions[] = "defaults";

/* The most of a file that is read. These hold eight entries and a header, so
   anything approaching this is not one of them, and reading further would be
   this layer holding whatever somebody put at that name. */
const size_t kMaxFileBytes = 64 * 1024;

// What the scripts below are given and what is kept of what they write. The
// mount script starts its retries in the background and comes back, so what is
// waited for is the script and not the mounting.
const int    kScriptTimeoutMs = 20000;
const size_t kMaxOutput       = 4096;

OpenThreads::Mutex &pathMutex()
{
	static OpenThreads::Mutex m;
	return m;
}

std::string &fstabStore()
{
	static std::string s(kFstabPath);
	return s;
}

std::string &automountStore()
{
	static std::string s(kAutomountPath);
	return s;
}

std::string &initRootStore()
{
	static std::string s(kInitRoot);
	return s;
}

std::string trimmedDir(const std::string &dir)
{
	std::string out = dir;
	while (out.size() > 1 && out[out.size() - 1] == '/')
		out.erase(out.size() - 1);
	return out;
}

bool isSpace(char c)
{
	return c == ' ' || c == '\t' || c == '\r';
}

// The columns of one line, split on runs of whitespace, which is how every
// reader of a filesystem table splits one.
void columns(const std::string &line, std::vector<std::string> &out)
{
	out.clear();
	size_t at = 0;
	while (at < line.size())
	{
		while (at < line.size() && isSpace(line[at]))
			++at;
		if (at >= line.size())
			break;
		const size_t from = at;
		while (at < line.size() && !isSpace(line[at]))
			++at;
		out.push_back(line.substr(from, at - from));
	}
}

void splitOnCommas(const std::string &text, std::vector<std::string> &out)
{
	out.clear();
	size_t at = 0;
	while (at <= text.size())
	{
		const size_t comma = text.find(',', at);
		const size_t end = (comma == std::string::npos) ? text.size() : comma;
		if (end > at)
			out.push_back(text.substr(at, end - at));
		if (comma == std::string::npos)
			break;
		at = comma + 1;
	}
}

std::string joinOnCommas(const std::vector<std::string> &all)
{
	std::string out;
	for (size_t i = 0; i < all.size(); ++i)
	{
		if (i > 0)
			out += ',';
		out += all[i];
	}
	return out;
}

bool startsWith(const std::string &s, const char *prefix)
{
	const size_t n = std::strlen(prefix);
	return s.size() >= n && s.compare(0, n, prefix) == 0;
}

const char *kindName(Kind k)
{
	return (k == Kind::Cifs) ? "cifs" : "nfs";
}

bool kindFromName(const std::string &name, Kind &out)
{
	if (name == "nfs")
	{
		out = Kind::Nfs;
		return true;
	}
	if (name == "cifs")
	{
		out = Kind::Cifs;
		return true;
	}
	return false;
}

/* The server and what it offers, out of the one column both files write them
   in. The two kinds spell it differently and neither spelling is the other's:
   an export is named after a colon and a share after a pair of separators.

   A leading "://" is taken off first. Nothing writes that on purpose; the
   screen at the box writes it into the automounter's file for a share, where it
   is a fault of that screen. Reading past it is what lets a file that screen
   wrote be read here at all. */
bool splitServer(const std::string &text, Kind kind, std::string &host, std::string &dir)
{
	std::string rest = text;
	if (startsWith(rest, "://"))
		rest.erase(0, 3);

	if (kind == Kind::Cifs)
	{
		if (startsWith(rest, "//"))
			rest.erase(0, 2);
		const size_t slash = rest.find('/');
		if (slash == std::string::npos)
		{
			host = rest;
			dir.clear();
		}
		else
		{
			host = rest.substr(0, slash);
			dir = rest.substr(slash);
		}
		return !host.empty();
	}

	const size_t colon = rest.find(':');
	if (colon == std::string::npos)
		return false;
	host = rest.substr(0, colon);
	dir = rest.substr(colon + 1);
	return !host.empty();
}

std::string serverText(const Entry &e)
{
	const std::string host = e.host.empty() ? kNoHost : e.host;
	std::string dir = e.remote_dir.empty() ? kNoDir : e.remote_dir;

	if (e.kind != Kind::Cifs)
		return host + ":" + dir;

	if (dir[0] != '/')
		dir = "/" + dir;
	return "//" + host + dir;
}

// What the screen at the box writes for a field nobody filled in reads back as
// the nothing it stands for, so a slot that was empty there is empty here.
std::string realOrEmpty(const std::string &value, const char *placeholder)
{
	if (value == placeholder)
		return std::string();
	// The directory placeholder reaches this with and without a leading
	// separator, the two files writing it differently.
	if (value.size() > 1 && value[0] == '/' && value.compare(1, std::string::npos, placeholder) == 0)
		return std::string();
	return value;
}

/* The options column apart: the word that says a mount is not automatic, the
   two that carry the credential, and everything else.

   noauto is read for the first file only. In the second a switched off entry is
   a line commented out, so the word there would be an option somebody wrote and
   is left where it stands. */
void readOptions(const std::string &text, bool honour_noauto, Entry &e)
{
	std::vector<std::string> all;
	splitOnCommas(text, all);

	std::vector<std::string> rest;
	for (size_t i = 0; i < all.size(); ++i)
	{
		const std::string &one = all[i];
		if (startsWith(one, "username="))
		{
			e.username = one.substr(9);
			continue;
		}
		if (startsWith(one, "user="))
		{
			e.username = one.substr(5);
			continue;
		}
		if (startsWith(one, "password="))
		{
			e.password = one.substr(9);
			e.has_password = !e.password.empty();
			continue;
		}
		if (honour_noauto && one == "noauto")
		{
			e.active = false;
			continue;
		}
		rest.push_back(one);
	}

	// The one word that stands for no options is read back as none, so that
	// what this writes for an entry with none comes back as none.
	if (rest.size() == 1 && rest[0] == kNoOptions)
		rest.clear();

	e.options = joinOnCommas(rest);
}

// The options column as it goes out: what this layer owns first, then what the
// caller kept, and the one word that stands for none where there would be an
// empty column.
std::string optionsText(const Entry &e, bool write_noauto, bool pad_when_empty)
{
	std::vector<std::string> all;
	if (write_noauto && !e.active)
		all.push_back("noauto");
	if (e.kind == Kind::Cifs)
	{
		all.push_back("username=" + e.username);
		all.push_back("password=" + e.password);
	}

	std::vector<std::string> kept;
	splitOnCommas(e.options, kept);
	for (size_t i = 0; i < kept.size(); ++i)
		all.push_back(kept[i]);

	if (all.empty() && pad_when_empty)
		all.push_back(kNoOptions);
	return joinOnCommas(all);
}

// Whether the line begins with a number sign, which in the automounter's file
// is both what a comment begins with and how an entry is switched off.
bool commentedOut(const std::string &line)
{
	size_t at = 0;
	while (at < line.size() && isSpace(line[at]))
		++at;
	return at < line.size() && line[at] == '#';
}

enum LineKind
{
	LineComment,   // a comment, or nothing at all
	LineEntry,     // an entry this read
	LineOther,     // a line of a kind this file may hold and this does not own
	LineUnreadable // a line meant to be an entry that nothing could be made of
};

LineKind readFstabLine(const std::string &line, Entry &e)
{
	std::vector<std::string> col;
	columns(line, col);
	if (col.empty())
		return LineComment;
	if (col[0][0] == '#')
		return LineComment;
	if (col.size() < 3)
		return LineUnreadable;

	if (!kindFromName(col[2], e.kind))
		return LineOther;
	if (!splitServer(col[0], e.kind, e.host, e.remote_dir))
		return LineUnreadable;

	e.host = realOrEmpty(e.host, kNoHost);
	e.remote_dir = realOrEmpty(e.remote_dir, kNoDir);
	e.local_dir = realOrEmpty(col[1], kNoMount);
	e.active = true;
	readOptions(col.size() > 3 ? col[3] : std::string(), true, e);
	e.dump = (col.size() > 4) ? col[4] : "0";
	e.pass = (col.size() > 5) ? col[5] : "0";
	return LineEntry;
}

LineKind readAutoLine(const std::string &line, Entry &e)
{
	std::string body = line;
	bool active = true;

	/* A line commented out is how that file switches an entry off, so the
	   number sign alone does not say which of the two a line is. What says it is
	   the word every entry there carries and no comment does, which is the same
	   reading the screen at the box makes.

	   WHICH IS NOT ENOUGH ON ITS OWN, because the head the file ships with
	   carries that word too: the line describing the columns spells one out. So
	   a line that began with a number sign and then turns out not to be an
	   entry is a comment and not a line nothing could be made of. Counting a
	   file's own head as broken would say a fresh box has a damaged file. */
	const bool commented = commentedOut(body);
	if (commented)
	{
		if (body.find("-fstype=") == std::string::npos)
			return LineComment;
		body.erase(body.find('#'), 1);
		active = false;
	}

	std::vector<std::string> col;
	columns(body, col);
	if (col.empty())
		return LineComment;
	if (col.size() < 3)
		return commented ? LineComment : LineUnreadable;
	if (!startsWith(col[1], "-fstype="))
		return commented ? LineComment : LineUnreadable;

	const std::string spec = col[1].substr(8);
	const size_t comma = spec.find(',');
	const std::string type = (comma == std::string::npos) ? spec : spec.substr(0, comma);
	if (!kindFromName(type, e.kind))
		return LineOther;
	if (!splitServer(col[2], e.kind, e.host, e.remote_dir))
		return commented ? LineComment : LineUnreadable;

	e.host = realOrEmpty(e.host, kNoHost);
	e.remote_dir = realOrEmpty(e.remote_dir, kNoDir);
	e.local_dir = realOrEmpty(col[0], kNoMount);
	e.active = active;
	readOptions((comma == std::string::npos) ? std::string() : spec.substr(comma + 1), false, e);
	// That file has no such columns, so there is nothing to carry across.
	e.dump.clear();
	e.pass.clear();
	return LineEntry;
}

std::string fstabLine(const Entry &e)
{
	std::string out = serverText(e);
	out += '\t';
	out += e.local_dir.empty() ? kNoMount : e.local_dir;
	out += "\t\t";
	out += kindName(e.kind);
	out += '\t';
	out += optionsText(e, true, true);
	out += "\t\t";
	out += e.dump.empty() ? "0" : e.dump;
	out += '\t';
	out += e.pass.empty() ? "0" : e.pass;
	out += '\n';
	return out;
}

std::string autoLine(const Entry &e)
{
	std::string out;
	if (!e.active)
		out += '#';
	out += e.local_dir.empty() ? kNoMount : e.local_dir;
	out += "\t-fstype=";
	out += kindName(e.kind);

	// No padding word here and no trailing separator either: the kind and the
	// options are one field, so an entry with no options is the kind alone.
	const std::string opts = optionsText(e, false, false);
	if (!opts.empty())
	{
		out += ',';
		out += opts;
	}
	out += "\t\t";
	out += serverText(e);
	out += '\n';
	return out;
}

// The head of each file, word for word what the screen at the box writes, so
// that a file is the same file whichever of the two wrote it last.
std::string headerFor(Table t, const std::string &path)
{
	if (t == Table::Fstab)
	{
		return "# " + path + ": static file system information (generated from NetFS setup).\n"
		       "#\n"
		       "# <file system> <mount point> <type> <options> <dump> <pass>\n";
	}
	return "# " + path + ": dynamic file system information (generated from NetFS setup).\n"
	       "#\n"
	       "# Every single mount point below will be available in /mnt/autofs during start\n"
	       "# or possibly even later if you request it.\n"
	       "#\n"
	       "# <mount point> -fstype=<type>,<options> <file system>\n";
}

/* Whether a value reaches the file as itself.

   The columns are split on whitespace and the options on commas, a number sign
   ends a line, and a control byte ends it somewhere nobody can see. A value
   carrying any of those would be read back as something else or would take the
   columns behind it with it. */
bool survivesTheFile(const std::string &value, bool commas_allowed)
{
	for (size_t i = 0; i < value.size(); ++i)
	{
		const unsigned char c = (unsigned char) value[i];
		if (c < 0x20 || c == 0x7f)
			return false;
		if (c == ' ' || c == '#')
			return false;
		if (c == ',' && !commas_allowed)
			return false;
	}
	return true;
}

Result<void> allowed(const Entry &e)
{
	if (!survivesTheFile(e.host, false) || !survivesTheFile(e.remote_dir, false) ||
	    !survivesTheFile(e.local_dir, false) || !survivesTheFile(e.username, false) ||
	    !survivesTheFile(e.password, false))
	{
		return fail(Status::InvalidArgument, ErrorCode::BadString,
			    "a value carries a space, a number sign, a comma or a control byte, and none "
			    "of those reaches the file as itself");
	}
	if (!survivesTheFile(e.options, true))
	{
		return fail(Status::InvalidArgument, ErrorCode::BadString,
			    "the options carry a space, a number sign or a control byte");
	}

	if (e.kind == Kind::Nfs && e.host.find(':') != std::string::npos)
	{
		return fail(Status::InvalidArgument, ErrorCode::BadString,
			    "the colon is what separates the server from what it exports, so the server "
			    "cannot carry one");
	}
	if (e.kind == Kind::Cifs && e.host.find('/') != std::string::npos)
	{
		return fail(Status::InvalidArgument, ErrorCode::BadString,
			    "the separator is what ends the server name, so the server cannot carry one");
	}

	/* Three options this layer writes itself, out of the three members that say
	   what they say. Offered here they would put a second value for one of them
	   on the line, and which of the two a mount then used would be the reader's
	   business rather than the caller's. */
	std::vector<std::string> kept;
	splitOnCommas(e.options, kept);
	for (size_t i = 0; i < kept.size(); ++i)
	{
		if (kept[i] == "noauto" || startsWith(kept[i], "user=") ||
		    startsWith(kept[i], "username=") || startsWith(kept[i], "password="))
		{
			return fail(Status::InvalidArgument, ErrorCode::BadString,
				    "the options name something this writes itself: whether the mount is "
				    "automatic, the user name and the password each have a value of their "
				    "own here");
		}
	}
	return ok();
}

// Reads the file the slots are built from and hands back every password with
// them, which is what a rewrite needs and what nothing above this may have.
Result<Slots> readWithPasswords(Table t)
{
	const std::string path = pathOf(t);

	Slots out;
	out.items.resize(kSlots);

	FILE *f = std::fopen(path.c_str(), "r");
	if (f == NULL)
	{
		// A box that has not been given the file has eight empty slots, which
		// is what a box nobody configured has. Anything else went wrong.
		if (errno == ENOENT)
			return ok(std::move(out));
		return fail(Status::Internal, ErrorCode::NetfsTableUnreadable,
			    "the file naming the network filesystems could not be read");
	}

	size_t filled = 0;
	size_t read_bytes = 0;
	char buf[1024];
	while (filled < kSlots && std::fgets(buf, sizeof(buf), f) != NULL)
	{
		std::string line(buf);
		read_bytes += line.size();
		if (read_bytes > kMaxFileBytes)
			break;

		while (!line.empty() && (line[line.size() - 1] == '\n' || line[line.size() - 1] == '\r'))
			line.erase(line.size() - 1);

		Entry e;
		const LineKind kind = (t == Table::Fstab) ? readFstabLine(line, e) : readAutoLine(line, e);
		if (kind == LineEntry)
			out.items[filled++] = e;
		else if (kind == LineUnreadable)
			++out.unreadable_lines;
	}
	std::fclose(f);
	return ok(std::move(out));
}

Result<void> writeSlots(Table t, const Slots &s)
{
	const std::string path = pathOf(t);

	std::string text = headerFor(t, path);
	for (size_t i = 0; i < s.items.size(); ++i)
		text += (t == Table::Fstab) ? fstabLine(s.items[i]) : autoLine(s.items[i]);

	/* Beside the file and put in its place at the end, so that the start up
	   script reading it while this runs reads either the whole of what was
	   there or the whole of what arrived.

	   Readable by everybody, which is the mode both of these files already
	   carry: the scripts that read them do not run as this does, and a password
	   in them is readable to anyone on the box either way. */
	CAtomicFileWriter writer(path);
	if (writer.file() == NULL)
	{
		return fail(Status::Internal, ErrorCode::NetfsTableNotWritten,
			    "the file naming the network filesystems could not be opened for writing");
	}
	if (std::fwrite(text.data(), 1, text.size(), writer.file()) != text.size())
	{
		return fail(Status::Internal, ErrorCode::NetfsTableNotWritten,
			    "the file naming the network filesystems was not written");
	}
	if (!writer.commit())
	{
		return fail(Status::Internal, ErrorCode::NetfsTableNotWritten,
			    "the file naming the network filesystems was not put in place");
	}
	return ok();
}

// One of the box's own start up scripts, asked for one thing. No shell is
// involved and no value a caller wrote reaches it: what is passed is the word
// written here.
Result<std::string> runInitScript(const char *name, const char *verb)
{
	const std::string script = initRoot() + "/" + name;

	ShellCommand cmd;
	cmd.script = script.c_str();
	cmd.verb = verb;
	cmd.params = NULL;
	cmd.param_count = 0;
	// What the script wrote, as it wrote it. Nothing here reads it.
	cmd.output = OutputFormat::Raw;
	cmd.timeout_ms = kScriptTimeoutMs;
	cmd.max_output = kMaxOutput;
	// Not asked for: the one argument is a word this file wrote and no program
	// reading options would read it as one.
	cmd.end_options = false;

	std::vector<std::pair<std::string, std::string> > none;
	return shell::run(cmd, none);
}

} // namespace

Entry::Entry()
	: active(false), kind(Kind::Nfs), change_password(false), has_password(false)
{
}

void setPath(Table t, const std::string &path)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(pathMutex());
	if (t == Table::Fstab)
		fstabStore() = path;
	else
		automountStore() = path;
}

std::string pathOf(Table t)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(pathMutex());
	return (t == Table::Fstab) ? fstabStore() : automountStore();
}

void setInitRoot(const std::string &dir)
{
	const std::string trimmed = trimmedDir(dir);
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(pathMutex());
	initRootStore() = trimmed;
}

std::string initRoot()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(pathMutex());
	return initRootStore();
}

Result<Slots> read(Table t)
{
	Result<Slots> got = readWithPasswords(t);
	if (!got.ok())
		return got;

	Slots out = std::move(got).value();
	for (size_t i = 0; i < out.items.size(); ++i)
	{
		// The one place the value is taken out, so that nothing above this has
		// to remember to leave it behind.
		out.items[i].password.clear();
		out.items[i].change_password = false;
	}
	return ok(std::move(out));
}

Result<void> write(Table t, size_t slot, const Entry &e)
{
	if (slot >= kSlots)
	{
		return fail(Status::NotFound, ErrorCode::NoSuchName,
			    "there is no such slot in the file naming the network filesystems");
	}

	Result<void> shape = allowed(e);
	if (!shape.ok())
		return fail(shape.error());

	Result<Slots> got = readWithPasswords(t);
	if (!got.ok())
		return fail(got.error());
	Slots all = std::move(got).value();

	const Entry before = all.items[slot];

	Entry put = e;
	if (!put.change_password)
		put.password = before.password;
	put.change_password = false;
	put.has_password = !put.password.empty();
	// Neither is offered to anybody, so what the file already said is what it
	// goes on saying.
	put.dump = before.dump;
	put.pass = before.pass;

	all.items[slot] = put;
	return writeSlots(t, all);
}

Result<void> clear(Table t, size_t slot)
{
	if (slot >= kSlots)
	{
		return fail(Status::NotFound, ErrorCode::NoSuchName,
			    "there is no such slot in the file naming the network filesystems");
	}

	Result<Slots> got = readWithPasswords(t);
	if (!got.ok())
		return fail(got.error());
	Slots all = std::move(got).value();

	all.items[slot] = Entry();
	return writeSlots(t, all);
}

Result<std::string> mountAll()
{
	return runInitScript("fstab", "start_netfs");
}

Result<std::string> unmountAll()
{
	return runInitScript("fstab", "stop_netfs");
}

Result<std::string> reloadAutomounter()
{
	return runInitScript("autofs", "restart");
}

} // namespace netfs
} // namespace coreapi
