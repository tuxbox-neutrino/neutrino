/*
 * daemons.h - which of the box daemons are up
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

#ifndef __coreapi_daemons_h__
#define __coreapi_daemons_h__

#include "coreapi/base/result.h"

#include <string>
#include <vector>

namespace coreapi
{
namespace daemons
{

/* What sort of thing a name in the list is.

   Two sorts, because the box drives them through two different scripts and they
   answer two different questions. A softcam is part of what the box is: the
   television has its own screen for these seven and starts them through the same
   init script this does. A service is something an image was built to carry and
   a box may not have installed at all.

   The sort is stated once in the table beside the two things that already differ
   per sort, which are the script that drives it and the place its presence is
   read from. */
enum class Kind
{
	Softcam,
	Service
};

/* One name the box knows, everything a caller can be told about it before it
   asks for anything else. */
struct Entry
{
	std::string name;
	Kind        kind;
	// Whether a process of its name is in the process table.
	bool        running;
	// Whether this box carries the thing at all.
	bool        installed;

	Entry() : kind(Kind::Service), running(false), installed(false) {}
};

// Every daemon this layer can drive, in the order it knows them.
//
// Running is read from the process table and not from the flag files the scripts
// keep beside it, because a flag says a daemon was asked for and not that it is
// there: one that was started and then died leaves its flag behind. A daemon
// started through a wrapper carries the wrapper's name and is missed, which is
// the same hole every reader of the process table has.
//
// Installed is read the way the two things that start these read it. A softcam
// is there when its program is under /var/bin, which is what the television's
// own softcam screen looks at; a service is there when an executable init script
// of its name is in one of the two directories the box's own service command
// searches. Neither answers whether the thing works, only whether it is on the
// box.
Result<std::vector<Entry> > list();

// NotFound for a name this box has no daemon of, NotSupported for one that has
// no branch for the verb, and whatever the runner answers for the rest, so a
// script that failed is Internal and one that outran its timeout is Busy.
Result<void> start(const std::string &name);
Result<void> stop(const std::string &name);
Result<void> restart(const std::string &name);

// Where the process table is read. Named here so the suite can hand it a
// directory it filled itself, which is the only way to drive the reading without
// a daemon to start.
void setProcRoot(const std::string &dir);
std::string procRoot();

// What is put in front of the absolute paths the presence of a daemon is read
// from, empty on a box. Here for the reason above and for one more: a machine
// running the suite has an /etc/init.d of its own, and a check that read it
// would answer differently on every machine.
void setSystemRoot(const std::string &dir);
std::string systemRoot();

} // namespace daemons
} // namespace coreapi

#endif
