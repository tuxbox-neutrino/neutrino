/*
 * systemsource_real.cpp - system facts read from the running box
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

#include "coreapi/base/deps.h"

#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <configfile.h>
#include <global.h>
#include <hardware/ca.h>
#include <hardware_caps.h>
#include <system/helpers.h>
#include <zapit/capmt.h>
#include <zapit/femanager.h>
#include <zapit/zapit.h>

namespace coreapi
{

namespace
{

/* Read once and kept. The image cannot be replaced under a running process:
   writing this file is part of an update and an update restarts the box. A file
   edited by hand is picked up at the next start. */
std::string readImageVersion()
{
	CConfigFile version('\t');
	if (!version.loadConfig(IMAGE_VERSION_FILE))
		return std::string();
	return version.getString("imageversion", "");
}

const std::string &imageVersion()
{
	static const std::string held = readImageVersion();
	return held;
}

// Its own translation unit, so that the hardware library and the channel stack
// are only pulled in by a binary that installs this.
class RealSystemSource : public SystemSource
{
	public:
		Status boxInfo(BoxInfo &out) const
		{
			hw_caps_t *caps = get_hwcaps();
			if (!caps)
				return Status::Internal;

			/* The vendor of one family is reported under the abbreviation
			   rather than the name it was sold under. Kept here rather than at
			   the answer, so every reader of the box name gets the same one;
			   the web pages match on the short form. */
			out.vendor = caps->boxvendor;
			if (out.vendor == "Coolstream")
				out.vendor = "CST";

			out.model = caps->boxname;
			// One model was sold in a single and a twin tuner version under one
			// name, and only the number of tuners tells them apart. Counted per
			// call rather than once, because the count is a map size and the
			// tuners are found after this layer is installed.
			if (out.model == "Neo" &&
			    CFEManager::getInstance()->getFrontendCount() > 1)
				out.model = "Neo Twin";

			out.chipset = caps->boxarch;
			out.image_version = imageVersion();

			// The three below are one system call each. Uptime has to be read
			// every time by what it means, and a box can be renamed while it
			// runs, so neither of those is worth keeping.
			struct utsname sys;
			if (uname(&sys) == 0)
				out.kernel = sys.release;

			// The name is truncated rather than terminated when it does not
			// fit, so the end is put there rather than trusted.
			char host[256];
			if (gethostname(host, sizeof(host)) == 0)
			{
				host[sizeof(host) - 1] = 0;
				out.hostname = host;
			}

			struct sysinfo si;
			if (::sysinfo(&si) == 0)
				out.uptime = (time_t) si.uptime;

			/* Both read through the program's own two readers rather than
			   through a second pair written here, so the numbers this answers
			   are the numbers the television's own box screen shows. Free
			   memory there counts what the buffers and the cache are holding,
			   which is memory a program asking for some would get.

			   Multiplied out into bytes here. The units are the readers' own,
			   kilobytes for the one and filesystem blocks for the other. */
			unsigned long ram_total = 0;
			unsigned long ram_free = 0;
			if (get_mem_usage(ram_total, ram_free))
			{
				out.memory_total = (unsigned long long) ram_total * 1024;
				out.memory_free = (unsigned long long) ram_free * 1024;
			}

			/* The root filesystem, which on the smallest box is the whole of
			   what an image has to fit in and is the first thing an image runs
			   out of. */
			uint64_t blocks = 0;
			uint64_t used = 0;
			long block_size = 0;
			if (get_fs_usage("/", blocks, used, &block_size) && block_size > 0)
			{
				out.root_total = (unsigned long long) blocks * (unsigned long long) block_size;
				out.root_free = (unsigned long long)(blocks - used) * (unsigned long long) block_size;
			}

			return Status::Ok;
		}

		/* Read in process, as the channel source beside this one does, and not
		   over the channel daemon's socket. It is the same value the daemon
		   answers a request with, read out of the same place, so the socket
		   would only add a round trip that has to be given a ceiling and an
		   answer that cannot say whether it failed. */
		Status volume(int &out) const
		{
			CZapit *zapit = CZapit::getInstance();
			if (!zapit)
				return Status::Internal;
			out = zapit->GetVolume();
			return Status::Ok;
		}

		/* The flag the mute command leaves behind, and not the decoder's. The
		   two part on a box muting over the television link and on a build
		   whose decoder has no flag to write, and there the decoder answers
		   sound that is on while the box draws the mute icon. */
		Status muted(bool &out) const
		{
			out = audioMuted();
			return Status::Ok;
		}

		/* The three below cannot fail. Both of the things they ask answer out
		   of memory they already hold, with no device opened and nothing
		   waited on, which is what lets a display that redraws every second
		   ask for them. They still report, because a seam whose answer is a
		   bare value leaves an implementation that cannot read with nothing to
		   say but a plausible number. */
		Status ciSlotCount(unsigned &out) const
		{
			out = (unsigned) cCA::GetInstance()->GetNumberCISlots();
			return Status::Ok;
		}

		/* Asked of the module's own ready flag, which the layer holding the
		   slot keeps up to date. A slot number the box does not have finds no
		   slot and answers no module. */
		Status ciModulePresent(unsigned slot, bool &out) const
		{
			out = cCA::GetInstance()->ModulePresent(CA_SLOT_TYPE_CI, slot);
			return Status::Ok;
		}

		/* What the channel stack decided when it last tuned, and not a setting:
		   the box may be set to use a module and still be descrambling some
		   other way on a channel the module carries nothing for. */
		Status ciInUse(bool &out) const
		{
			out = CCamManager::getInstance()->getUseCI();
			return Status::Ok;
		}
};

RealSystemSource g_real_system;

} // anonymous namespace

void installRealSystemSource() { setSystemSource(&g_real_system); }

} // namespace coreapi
