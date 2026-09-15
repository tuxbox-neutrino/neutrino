/*
 * inputdevice_real.cpp - remote keys written to the box input device
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

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <driver/rcinput.h>

extern CRCInput *g_RCInput;

namespace coreapi
{

namespace
{

bool writeEvent(int fd, unsigned short type, unsigned short code, int value)
{
	struct input_event ev;
	memset(&ev, 0, sizeof(ev));

	/* Stamped, because what reads these tells one press from the repeat of the
	   last by how far apart the two arrived, and a pair carrying no moment at
	   all is a pair that arrived in the same one. */
	struct timeval now;
	gettimeofday(&now, NULL);
#ifdef input_event_sec
	ev.input_event_sec = now.tv_sec;
	ev.input_event_usec = now.tv_usec;
#else
	ev.time = now;
#endif

	ev.type = type;
	ev.code = code;
	ev.value = value;
	return write(fd, &ev, sizeof(ev)) == (ssize_t) sizeof(ev);
}

// Its own translation unit, so that the input layer is pulled in only by a
// binary that installs this.
class RealInputDevice : public InputDevice
{
	public:
		Status sendKey(unsigned long code)
		{
			/* Asked of the input layer rather than answered again here: what
			   that layer opens is what it reads, and a key written to a node
			   nobody is reading is a key nothing acts on. */
			const std::vector<std::string> nodes = injectedKeyNodes();

			int fd = -1;
			bool denied = false;
			for (size_t i = 0; i < nodes.size() && fd < 0; i++)
			{
				fd = open(nodes[i].c_str(), O_RDWR | O_CLOEXEC);
				if (fd < 0 && (errno == EACCES || errno == EPERM))
					denied = true;
			}
			if (fd < 0)
			{
				// Which of the two it is decides what the caller does about
				// it: a node this process may not write to is a box set up
				// that way, and one that is not there at all is a box that
				// does not do this. One node of the list refusing is enough
				// for the first answer, because a node that is there to refuse
				// is a node this box has.
				return denied ? Status::Denied : Status::NotSupported;
			}

			/* The number the table carries is the one the input layer maps a
			   press onto, so what goes on the wire is what that mapping would
			   have started from. The layer is asked for it rather than a
			   second copy of its answer being kept here. */
			const int wire = (g_RCInput != NULL) ? g_RCInput->translateRevert((int) code)
			                                     : (int) code;

			/* Down and up, each followed by the report that says the batch is
			   whole. A press with no release leaves the key held for whoever
			   reads the node next, which is a box that repeats it until
			   something else is sent. */
			bool sent = writeEvent(fd, EV_KEY, (unsigned short) wire, 1);
			sent = writeEvent(fd, EV_SYN, SYN_REPORT, 0) && sent;
			sent = writeEvent(fd, EV_KEY, (unsigned short) wire, 0) && sent;
			sent = writeEvent(fd, EV_SYN, SYN_REPORT, 0) && sent;
			close(fd);
			return sent ? Status::Ok : Status::Internal;
		}

		/* Ok whatever it answers. What holds the flag is made while the box
		   starts and reading a flag cannot fail, so there is nothing here to
		   report; the status is the shape every read in this layer has, and it
		   is what lets a box that holds no such flag answer rather than abort.
		   */
		Status locked(bool &out) const
		{
			out = remoteControlLocked();
			return Status::Ok;
		}
};

RealInputDevice g_real_input;

} // anonymous namespace

void installRealInputDevice() { setInputDevice(&g_real_input); }

} // namespace coreapi
