/*
 * plugins.cpp - the plugins the box knows, and starting one
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

#include "plugins.h"

#include "coreapi/base/deps.h"
#include "coreapi/base/errors.h"
#include "shellcontract.h"

#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include <limits.h>
#include <stdlib.h>

#include <system/helpers.h>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

namespace coreapi
{
namespace plugins
{

namespace
{

// The box gives every plugin script the same extension, so a caller names the
// plugin and this names the file.
const char kScriptSuffix[] = ".sh";

/* Ten seconds: a caller holds a connection open for the whole of it. A script
   wanting longer is one to be started and then asked about. The ceiling on the
   output keeps a script that writes without end from being carried up in full. */
const int kTimeoutMs = 10000;
const size_t kMaxOutput = 65536;

OpenThreads::Mutex &rootMutex()
{
	static OpenThreads::Mutex m;
	return m;
}

std::string &rootStore()
{
	/* The one plugin directory the image makes for the running box to write
	   into. What the image installed itself sits in the read only share beside
	   it. */
	static std::string s(PLUGINDIR_VAR);
	return s;
}

/* A positive set rather than a list of what is refused: the list of what a
   filesystem and a shell each read specially is not one this can be sure it has
   finished writing. The separator is outside it, which keeps a name from ever
   being a path, and so is the zero byte, which keeps the name that was checked
   and the name that is joined below from being two different strings. */
bool isNameByte(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
	       (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_';
}

// The position a piece of the arguments arrives at, as the declared name it is
// handed over under.
std::string positionName(size_t n)
{
	char buf[24];
	std::snprintf(buf, sizeof(buf), "%lu", (unsigned long) n);
	return std::string(buf);
}

/* Split where the copied runner splits, at whitespace and nowhere else. Quotes
   and separators of every other kind reach the script as the characters they
   are, so a file name holding an apostrophe survives. */
void splitArgs(const std::string &args, std::vector<std::string> &out)
{
	size_t at = 0;
	while (at < args.size())
	{
		at = args.find_first_not_of(" \t", at);
		if (at == std::string::npos)
			break;
		size_t end = args.find_first_of(" \t", at);
		if (end == std::string::npos)
			end = args.size();
		out.push_back(args.substr(at, end - at));
		at = end;
	}
}

/* The file as lines, without whatever ends them. Read whole and written whole:
   the one line this writes sits among lines that say what starts the plugin and
   what it is called, and rewriting the file from what this layer knows would
   drop every one of them. */
bool readLines(const std::string &path, std::vector<std::string> &out)
{
	FILE *f = std::fopen(path.c_str(), "r");
	if (f == NULL)
		return false;

	std::string held;
	for (;;)
	{
		char buf[512];
		if (std::fgets(buf, sizeof(buf), f) == NULL)
			break;
		held += buf;
		// A line longer than the buffer arrives in pieces, and a piece is not
		// a line: only the one carrying the break ends one.
		if (held.empty() || held[held.size() - 1] != '\n')
			continue;
		held.erase(held.size() - 1);
		out.push_back(held);
		held.clear();
	}
	const bool whole = (std::ferror(f) == 0);
	std::fclose(f);
	// A file whose last line has no break behind it, which is what the surface
	// this replaces had to patch up before it could append anything.
	if (!held.empty())
		out.push_back(held);
	return whole;
}

bool writeLines(const std::string &path, const std::vector<std::string> &lines)
{
	CAtomicFileWriter writer(path);
	FILE *f = writer.file();
	if (f == NULL)
		return false;
	for (size_t i = 0; i < lines.size(); i++)
	{
		if (std::fprintf(f, "%s\n", lines[i].c_str()) < 0)
			return false;
	}
	return writer.commit();
}

} // namespace

Result<PluginList> list()
{
	PluginList out;
	Status s = pluginSource().list(out);
	if (s != Status::Ok)
		return fail(s, ErrorCode::PluginListUnavailable,
			    "the plugins of this box could not be read");
	return ok(std::move(out));
}

Result<void> start(const std::string &name)
{
	if (name.empty())
		return fail(Status::InvalidArgument, ErrorCode::BadName, "no plugin was named");
	/* The name and the byte after it. What arrives at the loop is an address
	   and a length, and what reads it there reads up to the end of a string,
	   so the end has to be among the bytes that travel. */
	return postEvent(BoxEvent::StartPlugin, name.c_str(), name.size() + 1);
}

Result<void> reload()
{
	return postEvent(BoxEvent::ReloadPlugins);
}

Result<void> setHidden(const std::string &name, bool hidden)
{
	if (name.empty())
		return fail(Status::InvalidArgument, ErrorCode::BadName, "no plugin was named");

	Result<PluginList> got = list();
	if (!got.ok())
		return fail(got.error());

	const PluginList all = std::move(got).value();
	const PluginInfo *found = NULL;
	for (size_t i = 0; i < all.size() && found == NULL; i++)
	{
		if (all[i].name == name)
			found = &all[i];
	}
	if (found == NULL)
		return fail(Status::NotFound, ErrorCode::NoSuchName,
			    "this box has no plugin called " + name);
	if (found->config.empty())
		return fail(Status::NotSupported, ErrorCode::PluginNotChanged,
			    "this box cannot say which file the plugin " + name + " came from");

	std::vector<std::string> lines;
	if (!readLines(found->config, lines))
		return fail(Status::Internal, ErrorCode::PluginNotChanged,
			    "the file the plugin " + name + " came from could not be read");

	/* The last one wins, that being what the box's own parser is left holding
	   after the whole file. A file carrying the key twice is one somebody edited
	   by hand, and rewriting only the deciding line leaves the rest where it
	   is. */
	const std::string wanted = std::string("hide=") + (hidden ? "1" : "0");
	size_t at = lines.size();
	for (size_t i = 0; i < lines.size(); i++)
	{
		if (lines[i].compare(0, 5, "hide=") == 0)
			at = i;
	}
	if (at == lines.size())
		lines.push_back(wanted);
	else
		lines[at] = wanted;

	if (!writeLines(found->config, lines))
		return fail(Status::Internal, ErrorCode::PluginNotChanged,
			    "the file the plugin " + name + " came from did not take the change");
	return ok();
}

Result<std::string> run(const std::string &name, const std::string &args)
{
	const std::string root = scriptRoot();
	/* Three roots that are no root. An empty one joins to a bare separator and
	   turns the search into a walk of the filesystem root. A relative one is
	   resolved against a working directory nothing here declares. The filesystem
	   root itself puts every program on the box behind this one name. */
	if (root.empty() || root[0] != '/' || root == "/")
		return fail(Status::Internal, ErrorCode::NoRoot,
			    "no directory is set for the scripts");

	if (name.empty())
		return fail(Status::InvalidArgument, ErrorCode::BadName, "no script was named");
	for (size_t i = 0; i < name.size(); i++)
	{
		if (!isNameByte(name[i]))
			return fail(Status::InvalidArgument, ErrorCode::BadName,
				    "that is not a name a script of this box carries");
	}
	// Both are made of bytes the rule above accepts, and neither is a file.
	if (name == "." || name == "..")
		return fail(Status::InvalidArgument, ErrorCode::BadName,
			    "that names a directory and not a script");
	// A program that reads options reads them wherever they sit.
	if (name[0] == '-')
		return fail(Status::InvalidArgument, ErrorCode::BadName,
			    "a name is not an option");

	char rootbuf[PATH_MAX];
	if (realpath(root.c_str(), rootbuf) == NULL)
		return fail(Status::Internal, ErrorCode::NoRoot,
			    "the directory set for the scripts is not there");
	const std::string resolved_root(rootbuf);

	const std::string joined = resolved_root + "/" + name + kScriptSuffix;
	char scriptbuf[PATH_MAX];
	if (realpath(joined.c_str(), scriptbuf) == NULL)
		/* The same code the runner answers for a file that is there and
		   cannot be started: either way there is no script of that name this
		   can run. */
		return fail(Status::NotFound, ErrorCode::ScriptMissing,
			    "there is no script of that name");
	const std::string script(scriptbuf);

	/* Directly inside the root and nowhere else. The name carries no separator
	   of its own, so the only way out of the root is a link, and a link is
	   followed by whatever opens the file rather than refused by it. One that
	   still lands inside the root is left alone. */
	if (script.size() <= resolved_root.size() ||
	    script.compare(0, resolved_root.size(), resolved_root) != 0 ||
	    script[resolved_root.size()] != '/' ||
	    script.find('/', resolved_root.size() + 1) != std::string::npos)
		return fail(Status::InvalidArgument, ErrorCode::OutsideRoots,
			    "that name reaches nothing inside the directory this runs out of");

	std::vector<std::string> pieces;
	splitArgs(args, pieces);

	/* One declared position per piece, in the order the pieces were written, so
	   the script reads them in the order the caller wrote them.

	   Built per call rather than as a table, because a table would have to state
	   a highest position and nothing could derive that number. The names are kept
	   alive here because what the runner is given is an address of each of them,
	   and the room is taken in advance so that taking one address cannot be
	   undone by the next. */
	std::vector<std::string> names;
	names.reserve(pieces.size());
	std::vector<Param> params;
	params.reserve(pieces.size());
	std::vector<std::pair<std::string, std::string> > given;
	given.reserve(pieces.size());

	for (size_t i = 0; i < pieces.size(); i++)
	{
		names.push_back(positionName(i + 1));

		Param p;
		p.name = names[i].c_str();
		p.type = ParamType::String;
		p.required = false;
		p.doc = "";
		// No bound of its own: what a value may weigh at all is the runner's,
		// and a bound here would be a second number saying the same thing.
		p.min = 0;
		p.max = 0;
		p.values = NULL;
		p.root = NULL;
		params.push_back(p);

		given.push_back(std::make_pair(names[i], pieces[i]));
	}

	ShellCommand cmd;
	cmd.script = script.c_str();
	cmd.verb = NULL;
	cmd.params = params.empty() ? NULL : &params[0];
	cmd.param_count = params.size();
	// What the script wrote, as it wrote it. Nothing here reads it.
	cmd.output = OutputFormat::Raw;
	cmd.timeout_ms = kTimeoutMs;
	cmd.max_output = kMaxOutput;
	/* Not asked for, so a piece that would be read as an option is refused
	   rather than let through. What runs here is whatever was put in the
	   directory, and none of it can say it meant to take one. */
	cmd.end_options = false;

	return shell::run(cmd, given);
}

void setScriptRoot(const std::string &dir)
{
	std::string trimmed = dir;
	while (trimmed.size() > 1 && trimmed[trimmed.size() - 1] == '/')
		trimmed.erase(trimmed.size() - 1);

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(rootMutex());
	rootStore() = trimmed;
}

std::string scriptRoot()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(rootMutex());
	return rootStore();
}

} // namespace plugins
} // namespace coreapi
