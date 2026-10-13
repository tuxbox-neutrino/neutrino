/*
 * websources.cpp - where the box takes its web channels and its XMLTV guide from
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include <config.h>

#include "coreapi/websources.h"
#include "coreapi/base/errors.h"

#include <cstdio>
#include <string>
#include <vector>

namespace coreapi
{
namespace websources
{

namespace
{

Source *g_source = NULL;

const char *nameOf(Kind kind)
{
	switch (kind)
	{
		case WebTv:    return "webtv";
		case WebRadio: return "webradio";
		case XmlTv:    return "xmltv";
	}
	return "?";
}

bool startsWith(const std::string &s, const char *head)
{
	const std::string h(head);
	return s.compare(0, h.size(), h) == 0;
}

/* What the reader of such a list takes: an address it fetches or a file it
   opens. A control byte is refused because the settings file holds one entry to
   a line and the old route split its text at line ends; a number sign because
   that file's reader cuts a value off there, so the entry would come back
   shorter after the next start; a relative path because what it is relative to
   is whatever directory the program happens to stand in. */
bool usable(const std::string &entry, Kind kind)
{
	const size_t most = (kind == XmlTv) ? kMaxGuideBytes : kMaxSourceBytes;
	if (entry.empty() || entry.size() > most)
		return false;
	for (size_t i = 0; i < entry.size(); ++i)
	{
		const unsigned char c = static_cast<unsigned char>(entry[i]);
		if (c < 0x20 || c == 0x7f || c == '#')
			return false;
	}
	return startsWith(entry, "http://") || startsWith(entry, "https://") || entry[0] == '/';
}

} // namespace

Result<Lists> read()
{
	if (g_source == NULL)
		return fail<Lists>(Status::NotSupported, ErrorCode::CommandNotSent,
				   "this box keeps no web channel or guide lists");
	Lists out;
	const Status s = g_source->read(out);
	if (s != Status::Ok)
		return fail<Lists>(s, ErrorCode::CommandNotSent, "the lists could not be read");
	return Result<Lists>::success(out);
}

Result<void> check(Kind kind, const std::vector<std::string> &entries)
{
	if (entries.size() > kMaxSources)
	{
		char most[32];
		std::snprintf(most, sizeof(most), "%lu", (unsigned long) kMaxSources);
		return fail(Status::InvalidArgument, ErrorCode::BadString,
			    std::string("the ") + nameOf(kind) + " list holds at most " + most + " entries");
	}
	for (size_t i = 0; i < entries.size(); ++i)
	{
		if (usable(entries[i], kind))
			continue;
		char at[32];
		std::snprintf(at, sizeof(at), "%lu", (unsigned long) (i + 1));
		char most[32];
		std::snprintf(most, sizeof(most), "%lu",
			      (unsigned long) ((kind == XmlTv) ? kMaxGuideBytes : kMaxSourceBytes));
		return fail(Status::InvalidArgument, ErrorCode::BadString,
			    std::string("entry ") + at + " of the " + nameOf(kind) + " list is not an http or "
			    "https address or an absolute path of up to " + most + " bytes without control "
			    "characters or a number sign");
	}

	return ok();
}

Result<void> write(Kind kind, const std::vector<std::string> &entries, bool reload)
{
	const Result<void> passes = check(kind, entries);
	if (!passes.ok())
		return passes;

	if (g_source == NULL)
		return fail(Status::NotSupported, ErrorCode::CommandNotSent,
			    "this box keeps no web channel or guide lists");
	const Status s = g_source->write(kind, entries, reload);
	if (s != Status::Ok)
		return fail(s, ErrorCode::CommandNotPosted,
			    "the box could not be asked to take the list over");
	return ok();
}

std::string webtvDir()
{
	return WEBTVDIR_VAR;
}

std::string webradioDir()
{
	return WEBRADIODIR_VAR;
}

void setSource(Source *s)
{
	g_source = s;
}

void applyPending()
{
	if (g_source != NULL)
		g_source->carry();
}

void refresh()
{
	if (g_source != NULL)
		g_source->refresh();
}

} // namespace websources
} // namespace coreapi
