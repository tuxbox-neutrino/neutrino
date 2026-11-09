/*
 * captureguard.cpp - no picture of the screen while it shows a password
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
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
 */

#include "captureguard.h"

#include <coreapi/base/accessfiles.h>

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

namespace captureguard
{

namespace
{

// Initialised statically, so nothing runs before main and a capture taken
// during start-up finds the lock ready.
pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
// Exclusions standing now.
unsigned g_standing = 0;
// Exclusions ever begun; a Capture compares it with what it saw at admission.
unsigned long g_begun = 0;
// Side names handed out.
unsigned long g_sides = 0;

class Locked
{
	public:
		Locked() { pthread_mutex_lock(&g_lock); }
		~Locked() { pthread_mutex_unlock(&g_lock); }

	private:
		Locked(const Locked &);
		Locked &operator=(const Locked &);
};

} // anonymous namespace

Exclusion::Exclusion()
{
	Locked hold;
	++g_standing;
	++g_begun;
}

Exclusion::~Exclusion()
{
	Locked hold;
	--g_standing;
}

Capture::Capture()
{
	Locked hold;
	admitted_ = g_standing == 0;
	begun_ = g_begun;
}

bool Capture::clean() const
{
	Locked hold;
	return admitted_ && g_begun == begun_;
}

std::string sideName(const std::string &path)
{
	unsigned long n;
	{
		Locked hold;
		n = ++g_sides;
	}
	char own[64];
	snprintf(own, sizeof(own), ".capture-%ld-%lu.taking", (long) getpid(), n);
	const size_t slash = path.rfind('/');
	return (slash == std::string::npos ? std::string() : path.substr(0, slash + 1)) + own;
}

namespace
{
void (*stagingMade)(const std::string &name) = NULL;
bool stagingNamed = false;
} // anonymous namespace

void setStagingMadeForTest(void (*made)(const std::string &name), bool with_a_name)
{
	stagingMade = made;
	stagingNamed = with_a_name;
}

Staging::Staging() : fd_(-1)
{
	/* No gate of the web server asks about a descriptor while this is made
	   (coreapi::accessfiles::Making). Made without a name at all where /tmp
	   can (O_TMPFILE: Linux 3.11 on, tmpfs among the filesystems that take
	   it); elsewhere made under a name and the name removed before the
	   lock is given back, so a descriptor opened by that name meanwhile is
	   asked about once it has none, and refused for that. */
	coreapi::accessfiles::Making making;
#ifdef O_TMPFILE
	if (!stagingNamed)
		fd_ = open("/tmp", O_TMPFILE | O_RDWR | O_CLOEXEC, 0600);
	if (fd_ >= 0 && stagingMade != NULL)
		stagingMade(std::string());
#endif
	if (fd_ < 0)
	{
		char name[] = "/tmp/.capture-XXXXXX";
		fd_ = mkostemp(name, O_CLOEXEC);
		if (fd_ < 0)
			return;
		if (stagingMade != NULL)
			stagingMade(name);
		// no name from here on: what is written is reachable only through fd_
		if (unlink(name) != 0)
		{
			close(fd_);
			fd_ = -1;
			return;
		}
	}
	char own[64];
	snprintf(own, sizeof(own), "/proc/%ld/fd/%d", (long) getpid(), fd_);
	path_ = own;
}

Staging::~Staging()
{
	if (fd_ >= 0)
		close(fd_);
}

bool Staging::written() const
{
	struct stat st;
	return fd_ >= 0 && fstat(fd_, &st) == 0 && st.st_size > 0;
}

bool Staging::publish(const std::string &target) const
{
	if (fd_ < 0)
		return false;
	const std::string side = sideName(target);
	const int out = open(side.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
	if (out < 0)
		return false;
	bool ok = true;
	char buf[16384];
	off_t at = 0;
	for (;;)
	{
		const ssize_t n = pread(fd_, buf, sizeof(buf), at);
		if (n < 0)
		{
			ok = false;
			break;
		}
		if (n == 0)
			break;
		for (ssize_t done = 0; done < n; )
		{
			const ssize_t w = write(out, buf + done, (size_t) (n - done));
			if (w <= 0)
			{
				ok = false;
				break;
			}
			done += w;
		}
		if (!ok)
			break;
		at += n;
	}
	if (close(out) != 0)
		ok = false;
	if (!ok || rename(side.c_str(), target.c_str()) != 0)
	{
		unlink(side.c_str());
		return false;
	}
	return true;
}

} // namespace captureguard
