/*
 * daemons.h - which of the box daemons are up
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (fixes)
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

   One sort for now: a service, something an image was built to carry and a box
   may not have installed at all. Softcams are left out of this list until they
   are offered the way any extension from another source is; the sort stays, so
   a second one can come back without the list changing its shape. */
enum class Kind
{
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
// Installed is read the way the thing that starts it reads it. On a box run by
// systemd a service is there when its unit file is in one of the unit
// directories, or an executable init script of its name is in /etc/init.d,
// which systemd turns into a unit of that name; on a box without it, when an
// executable init script of its name is in one of the two init directories.
// Neither answers whether the thing works, only whether it is on the box.
Result<std::vector<Entry> > list();

// Runs systemctl with the verb and the unit, or the init script with the verb,
// whichever list() found the service by, and no shell. NotFound NoSuchDaemon
// for a name this box has no daemon of, NotFound ScriptMissing for one it does
// not carry, and whatever the runner answers for the rest, so a command that
// failed is Internal and one that outran its timeout is Busy.
Result<void> start(const std::string &name);
Result<void> stop(const std::string &name);
Result<void> restart(const std::string &name);

// Where the process table is read. Named here so the suite can hand it a
// directory it filled itself, which is the only way to drive the reading without
// a daemon to start.
void setProcRoot(const std::string &dir);
std::string procRoot();

// What is put in front of the absolute paths the presence of a daemon is read
// from and the command that drives it is run from, empty on a box. Here for the
// reason above and for one more: a machine running the suite has an
// /etc/init.d and a systemctl of its own, and a check that read or ran them
// would answer differently on every machine.
void setSystemRoot(const std::string &dir);
std::string systemRoot();

} // namespace daemons
} // namespace coreapi

#endif
