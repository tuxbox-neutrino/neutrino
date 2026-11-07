/*
 * test_rc.cpp - tests for remote keys
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

#include "support/catch.hpp"
#include "support/rcfixture.h"

#include "coreapi/base/deps.h"
#include "coreapi/osd.h"
#include "coreapi/base/result.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <string>
#include <vector>

#include <linux/input.h>

/* The application owns both of these and this binary holds neither. The
   pointer is defined by the case that drives the message queue; the lock is a
   flag held by a screen no test links, and false is what a build holding no
   such flag answers. Nothing here reads the lock through the device below. */
extern CRCInput *g_RCInput;
bool coreapi::remoteControlLocked() { return false; }

using namespace coreapi;

namespace
{

/* The node the shipped build writes a key to, read off the list itself rather
   than written out again here: a second copy of the path is the very thing
   this file is about. Held for the life of the program, because what moves it
   below hands out a pointer and keeps it. */
const char *shippedKeyNode()
{
	static const std::string held = injectedKeyNodes().back();
	return held.c_str();
}

/* Moves that node off the one a box uses, so that a run of this does not put
   keys into a box running beside it. A fifo and not a file: the loop selects
   on what it opened, and a file is always readable, so a file would have it
   spinning over bytes nobody wrote. */
struct InjectedNode
{
	std::string path;

	InjectedNode() : path(std::string("/tmp/coreapi-key.") + std::to_string(getpid()))
	{
		// Read before it is moved, and not empty, or the way back is a path
		// the next case would write keys to.
		REQUIRE_FALSE(std::string(shippedKeyNode()).empty());
		unlink(path.c_str());
		REQUIRE(mkfifo(path.c_str(), 0600) == 0);
		setInjectedKeyNodeForTest(path.c_str());
	}

	~InjectedNode()
	{
		setInjectedKeyNodeForTest(shippedKeyNode());
		unlink(path.c_str());
	}

	private:
		InjectedNode(const InjectedNode &);
		InjectedNode &operator=(const InjectedNode &);
};

/* The device the server installs, pointed at the loop under test. Both globals
   are put back by a destructor rather than by the last lines of a case,
   because a failed assertion unwinds out of one. */
struct InstalledRealInput
{
	explicit InstalledRealInput(CRCInput *rc)
	{
		g_RCInput = rc;
		installRealInputDevice();
	}

	~InstalledRealInput()
	{
		setInputDevice(0);
		g_RCInput = 0;
	}

	private:
		InstalledRealInput(const InstalledRealInput &);
		InstalledRealInput &operator=(const InstalledRealInput &);
};

} // namespace

/* The reading of the bits an event node answers with. Built here by hand in
   the layout the kernel uses, a long at a time, so the cases hold on a box of
   either byte order. */
TEST_CASE("a key is in the bits only where its own bit is set", "[rc]")
{
	const size_t per_long = sizeof(unsigned long) * 8;
	unsigned long bits[KEY_MAX / (sizeof(unsigned long) * 8) + 1];
	memset(bits, 0, sizeof(bits));
	const size_t longs = sizeof(bits) / sizeof(bits[0]);

	bits[KEY_MUTE / per_long] |= 1UL << (KEY_MUTE % per_long);

	REQUIRE(keyInBits(bits, longs, KEY_MUTE));
	// The neighbours of a set bit are not set with it.
	REQUIRE_FALSE(keyInBits(bits, longs, KEY_MUTE - 1));
	REQUIRE_FALSE(keyInBits(bits, longs, KEY_MUTE + 1));
	// The key this is about, which no handset has.
	REQUIRE_FALSE(keyInBits(bits, longs, KEY_MUTEON));
	// Past the end is a key the node does not have, and not a read past it.
	REQUIRE_FALSE(keyInBits(bits, 1, KEY_MUTEON));
	REQUIRE_FALSE(keyInBits(bits, 0, KEY_MUTE));
}

/* A node that is not an event node hands back what was written to it, so it
   is asked about no key: the fifo a build with no input hardware reads has to
   carry the keys no handset has as well as the ones every handset has. */
TEST_CASE("a node that is not an event node carries every key", "[rc]")
{
	InjectedNode node;

	const int fd = open(node.path.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
	REQUIRE(fd >= 0);
	const bool mute = nodeCarriesKey(fd, KEY_MUTE);
	const bool mute_on = nodeCarriesKey(fd, KEY_MUTEON);
	close(fd);

	REQUIRE(mute);
	REQUIRE(mute_on);
}

namespace
{

bool carriesNoKey(int, unsigned int) { return false; }

/* Stands in for an event node whose driver has none of the keys asked about,
   which is what a box's remote control node is to mute on. Put back by a
   destructor, for the reason the globals above are. */
struct NodeLackingKeys
{
	NodeLackingKeys() { setNodeCarriesKeyForTest(carriesNoKey); }
	~NodeLackingKeys() { setNodeCarriesKeyForTest(NULL); }

	private:
		NodeLackingKeys(const NodeLackingKeys &);
		NodeLackingKeys &operator=(const NodeLackingKeys &);
};

} // namespace

/* Mute on and mute off, all the way through a node that carries them. */
TEST_CASE("mute on and mute off come back off the loop", "[rc]")
{
	InjectedNode node;
	requireFreeEventSocket();

	CRCInput rc;
	InstalledRealInput installed(&rc);

	REQUIRE(osd::sendKey("KEY_MUTEON").ok());
	neutrino_msg_t got = 0;
	neutrino_msg_data_t data = 0;
	rc.getMsg_ms(&got, &data, 2000);
	REQUIRE(got == (neutrino_msg_t) CRCInput::RC_mute_on);

	REQUIRE(osd::sendKey("KEY_MUTEOFF").ok());
	rc.getMsg_ms(&got, &data, 2000);
	REQUIRE(got == (neutrino_msg_t) CRCInput::RC_mute_off);
}

/* What broke: a box's node dropped mute on unread and the server still said
   it went. A key the node lacks reaches the loop anyway, once and as a press,
   and none of it is written to the node, or a box whose node does have it
   after all would act on it twice. */
TEST_CASE("a key the node lacks still reaches the loop, once", "[rc]")
{
	InjectedNode node;
	requireFreeEventSocket();

	CRCInput rc;
	InstalledRealInput installed(&rc);
	NodeLackingKeys lacking;

	REQUIRE(osd::sendKey("KEY_MUTEON").ok());

	neutrino_msg_t got = 0;
	neutrino_msg_data_t data = 0;
	rc.getMsg_ms(&got, &data, 2000);
	INFO("the loop answered " << got << " and the key was " << (neutrino_msg_t) CRCInput::RC_mute_on);
	REQUIRE(got == (neutrino_msg_t) CRCInput::RC_mute_on);
	REQUIRE(data == 0);

	// Nothing more: not the same key again off the node.
	rc.getMsg_ms(&got, &data, 300);
	REQUIRE(got == (neutrino_msg_t) CRCInput::RC_timeout);
}

namespace
{

/* What this process writes to stdout while one of these is alive, kept in a
   file beside the others this file makes. Put back by the destructor, so a
   failed assertion does not leave the rest of the run writing into it. */
struct CapturedStdout
{
	std::string path;
	int saved;

	CapturedStdout() : path(std::string("/tmp/coreapi-stdout.") + std::to_string(getpid())), saved(-1)
	{
		fflush(stdout);
		saved = dup(STDOUT_FILENO);
		REQUIRE(saved >= 0);
		const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
		REQUIRE(fd >= 0);
		REQUIRE(dup2(fd, STDOUT_FILENO) == STDOUT_FILENO);
		close(fd);
	}

	// Everything written so far, with stdout put back first.
	std::string text()
	{
		restore();
		std::string out;
		FILE *f = fopen(path.c_str(), "r");
		if (f == NULL)
			return out;
		char buf[512];
		size_t n;
		while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
			out.append(buf, n);
		fclose(f);
		return out;
	}

	~CapturedStdout()
	{
		restore();
		unlink(path.c_str());
	}

	private:
		void restore()
		{
			if (saved < 0)
				return;
			fflush(stdout);
			dup2(saved, STDOUT_FILENO);
			close(saved);
			saved = -1;
		}

		CapturedStdout(const CapturedStdout &);
		CapturedStdout &operator=(const CapturedStdout &);
};

} // namespace

namespace
{

// The line the read loop writes for a press of code, translated to key.
std::string pressLine(unsigned code, neutrino_msg_t key)
{
	char line[128];
	snprintf(line, sizeof(line), "key: %04x value 1, translate: %04x -%s-\n",
		 code, (unsigned) key, CRCInput::getKeyName(key).c_str());
	return line;
}

} // namespace

/* A key read off a node is on the console as one line in a build that debugs
   the input, and in no other, so an ordinary build does not put every key
   into the journal. A key handed to the loop past the node is on it exactly
   where such a key is, in that same line, as a press. */
TEST_CASE("a key the node lacks is on the console like any other", "[rc]")
{
	InjectedNode node;
	requireFreeEventSocket();

	CRCInput rc;
	InstalledRealInput installed(&rc);

	std::string said;
	neutrino_msg_t got = 0;
	neutrino_msg_data_t data = 0;
	{
		CapturedStdout out;
		// One the node carries, read off it by the loop.
		REQUIRE(osd::sendKey("KEY_MUTEON").ok());
		rc.getMsg_ms(&got, &data, 2000);
		REQUIRE(got == (neutrino_msg_t) CRCInput::RC_mute_on);
		// One it lacks, handed to the loop past it.
		NodeLackingKeys lacking;
		REQUIRE(osd::sendKey("KEY_STANDBYON").ok());
		rc.getMsg_ms(&got, &data, 2000);
		said = out.text();
	}
	// And the key went where the line says it did.
	REQUIRE(got == (neutrino_msg_t) CRCInput::RC_standby_on);

	const bool read_line = said.find(pressLine(KEY_MUTEON, CRCInput::RC_mute_on)) != std::string::npos;
	const bool past_line = said.find(pressLine(KEY_STANDBYON, CRCInput::RC_standby_on)) != std::string::npos;
	INFO("the console said: " << said);
	REQUIRE(past_line == read_line);
}
