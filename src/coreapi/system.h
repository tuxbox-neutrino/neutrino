/*
 * system.h - box facts, standby, reboot and restart
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (additions)
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

#ifndef __coreapi_system_h__
#define __coreapi_system_h__

#include "coreapi/base/result.h"
#include "coreapi/base/types.h"

#include <string>
#include <vector>

namespace coreapi
{
namespace system
{

Result<BoxInfo> info();

/* One process as /proc tells of it: its number and its parent's, the user it
   runs as (numeric), the state letter, the memory it holds and has mapped, the
   name the kernel keeps and the command line it was started with (a kernel
   thread, which has none, shows its name in brackets). */
struct ProcessInfo
{
	int                pid;
	int                ppid;
	unsigned           uid;
	char               state;
	unsigned long long rss_bytes;
	unsigned long long vsize_bytes;
	std::string        name;
	std::string        command;
};

/* Every process running now, by number. Read out of /proc and not by starting
   ps, so nothing runs to answer. One that ends while the list is read is left
   out. NotSupported ProcessesUnreadable where there is no /proc. */
Result<std::vector<ProcessInfo> > processes();

/* One line of the kernel's log: the level it was written at (0 the most
   urgent, 7 debug, -1 where the line carries none) and the text. */
struct KernelLogLine
{
	int         level;
	std::string text;
};

/* The kernel's log as it stands, what dmesg prints, read and not cleared.
   NotSupported KernelLogUnreadable where the kernel will not hand it to this
   program (kernel.dmesg_restrict for a program not running as root). */
Result<std::vector<KernelLogLine> > kernelLog();

// The four below are commands, sent and forgotten, so an ok says the command
// reached the loop's socket and nothing about what the box did with it: standby
// on while already in standby and standby off while not in it are both accepted
// and both change nothing.
//
// At most COMMAND_SEND_TIMEOUT_MS of the calling thread, which is the
// transport's ceiling and not a wait: a loop too busy to take the command inside
// that loses it and the call answers Internal rather than holding the caller.
Result<void> standby(bool on);

// Said once the box has gone into standby or come out of it. Asking is not
// going: a request can be refused, and one can be dropped half way.
void announceStandby(bool on);
Result<void> reboot();
Result<void> shutdown();
Result<void> restart();

// What the box tells the television over the cable beside a standby change.
// Separate from standby rather than folded into it, because whether it is
// wanted at all is a setting this layer does not read, and the two are sent as
// two events even where both are wanted.
Result<void> hdmiCec(bool view_on);

/* A command like the ones above: what comes back says the message reached the
   loop's socket and nothing about what the box made of it. */
Result<void> reloadSetup();

} // namespace system

namespace config
{

/* The whole of what may be asked for. Three names and not a name a caller
   writes: a call that took a file name would read any file the box can read the
   moment that name carries a separator, and nothing here could tell the two
   apart. */
enum class Document
{
	Services,
	Bouquets,
	UserBouquets
};

/* A path and not its bytes: the documents run to megabytes on a satellite box,
   and whoever answers with one sends it out of the file rather than through this
   process.

   Whether there is a file at that name is not asked here. The answer would be
   out of date by the time a caller opened it, and the caller has to answer for
   the open failing anyway, so there is one place that finds out and not two. */
Result<std::string> pathOf(Document d);

/* Where the three are looked for; a trailing separator is dropped. Settable for
   the reason the script root is: the name the image installs to is on no host
   but a box, so a caller fixed to it could only ever be answered by the absence
   of a file. */
void setConfigRoot(const std::string &dir);
std::string configRoot();

} // namespace config
} // namespace coreapi

#endif
