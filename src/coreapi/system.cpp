/*
 * system.cpp - box facts, standby, reboot and restart
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

#include "system.h"
#include "coreapi/base/errors.h"

#include "coreapi/base/deps.h"

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

namespace coreapi
{
namespace system
{

Result<BoxInfo> info()
{
	BoxInfo out;
	Status s = systemSource().boxInfo(out);
	if (s != Status::Ok)
		return fail(s, ErrorCode::BoxUnreadable,
			    "the box could not be identified");
	return ok(std::move(out));
}

Result<void> standby(bool on)
{
	return postEvent(on ? BoxEvent::StandbyOn : BoxEvent::StandbyOff);
}

Result<void> reboot() { return postEvent(BoxEvent::Reboot); }

Result<void> shutdown() { return postEvent(BoxEvent::Shutdown); }

Result<void> restart() { return postEvent(BoxEvent::Restart); }

Result<void> hdmiCec(bool view_on)
{
	return postEvent(view_on ? BoxEvent::HdmiCecViewOn : BoxEvent::HdmiCecStandby);
}

Result<void> reloadSetup() { return postEvent(BoxEvent::ReloadSetup); }

} // namespace system

namespace config
{

namespace
{

/* The tail each document is kept under, joined to the root per call rather than
   written out absolute. An absolute name here would be the one the image
   installs to, which nothing but a box has, so every caller away from a box
   would be answered by the absence of a file whatever it asked for. */
const char *fileNameOf(Document d)
{
	switch (d)
	{
		case Document::Services:     return "services.xml";
		case Document::Bouquets:     return "bouquets.xml";
		case Document::UserBouquets: return "ubouquets.xml";
	}
	/* A value cast into the enumeration from outside it, which is the only way
	   past the three above. Answered as no document rather than as the first
	   one, so a number nobody wrote does not hand out a file. */
	return NULL;
}

OpenThreads::Mutex &rootMutex()
{
	static OpenThreads::Mutex m;
	return m;
}

std::string &rootStore()
{
	static std::string s(ZAPITDIR);
	return s;
}

} // namespace

void setConfigRoot(const std::string &dir)
{
	std::string trimmed = dir;
	while (trimmed.size() > 1 && trimmed[trimmed.size() - 1] == '/')
		trimmed.erase(trimmed.size() - 1);

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(rootMutex());
	rootStore() = trimmed;
}

std::string configRoot()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(rootMutex());
	return rootStore();
}

Result<std::string> pathOf(Document d)
{
	const char *name = fileNameOf(d);
	if (name == NULL)
		return fail(Status::InvalidArgument, ErrorCode::NoSuchName,
			    "that is not a document the box keeps its configuration in");

	return ok(configRoot() + "/" + name);
}

} // namespace config
} // namespace coreapi
