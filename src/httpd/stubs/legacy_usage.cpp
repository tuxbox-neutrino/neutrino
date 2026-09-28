/*
 * legacy_usage.cpp - the counting seam behind /api/v1/system/legacy-usage
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * The nhttpd half of what src/httpd/compat/mount.h states. The shape of
 * the counting is the source's (compat/mount.cpp): everything under one
 * lock, a vector per side, the unknown side bounded and both scanned
 * linearly - the known table holds a few dozen names and the unknown
 * side cannot outgrow its ceiling, short enough that a scan costs
 * nothing next to building an answer. The known table is not compiled
 * in here: CControlAPI seeds it name by name from its own dispatch
 * table, so the two cannot go out of step.
 */

#include "httpd/compat/mount.h"

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

namespace httpd
{
namespace compat
{

// 200: generous next to the few dozen names the dispatch table holds,
// and still a hard ceiling on what asking for a name nobody registered
// can cost this program, at no charge to whoever asks.
const std::size_t kMaxUnknownLegacyNames = 200;

namespace
{

// Built on first use rather than at namespace scope so nothing here
// needs to have run before main.
OpenThreads::Mutex &usageLock()
{
	static OpenThreads::Mutex m;
	return m;
}

// The known table, in the order it was seeded, each row carrying its
// count so the answer is one walk.
std::vector<Usage> &knownUsage()
{
	static std::vector<Usage> rows;
	return rows;
}

// Names outside that table, in the order each was first seen.
std::vector<Usage> &unknownUsage()
{
	static std::vector<Usage> rows;
	return rows;
}

} // anonymous namespace

void knowName(const char *name)
{
	if (name == NULL || *name == '\0')
		return;

	OpenThreads::ScopedLock<OpenThreads::Mutex> held(usageLock());

	std::vector<Usage> &k = knownUsage();
	for (size_t i = 0; i < k.size(); ++i)
	{
		if (k[i].name == name)
			return;
	}

	Usage row;
	row.name = name;
	row.calls = 0;
	k.push_back(row);
}

void record(const std::string &name)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(usageLock());

	std::vector<Usage> &k = knownUsage();
	for (size_t i = 0; i < k.size(); ++i)
	{
		if (k[i].name == name)
		{
			++k[i].calls;
			return;
		}
	}

	std::vector<Usage> &u = unknownUsage();
	for (size_t i = 0; i < u.size(); ++i)
	{
		if (u[i].name == name)
		{
			++u[i].calls;
			return;
		}
	}
	if (u.size() < kMaxUnknownLegacyNames)
	{
		Usage next;
		next.name = name;
		next.calls = 1;
		u.push_back(next);
	}
}

std::vector<Usage> usage()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(usageLock());

	std::vector<Usage> out;
	out.reserve(knownUsage().size() + unknownUsage().size());
	out.insert(out.end(), knownUsage().begin(), knownUsage().end());
	out.insert(out.end(), unknownUsage().begin(), unknownUsage().end());
	return out;
}

void resetForTest()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(usageLock());
	knownUsage().clear();
	unknownUsage().clear();
}

} // namespace compat
} // namespace httpd
