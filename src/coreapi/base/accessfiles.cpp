/*
 * accessfiles.cpp - the files that hold who may reach the box, by what they are
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

#include "accessfiles.h"

#include <cerrno>
#include <map>
#include <new>
#include <utility>

#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

namespace coreapi
{
namespace accessfiles
{

namespace
{

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/* Writers first: a gate that is asked again and again must not keep a file
   that is being made waiting for good. */
pthread_once_t makingOnce = PTHREAD_ONCE_INIT;
pthread_rwlock_t making;

void initMaking()
{
	pthread_rwlockattr_t attr;
	pthread_rwlockattr_init(&attr);
	pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
	pthread_rwlock_init(&making, &attr);
	pthread_rwlockattr_destroy(&attr);
}

pthread_rwlock_t &makingLock()
{
	pthread_once(&makingOnce, initMaking);
	return making;
}

Fail failing = FailNone;
unsigned failingAfter = 0;
void (*askingHook)() = NULL;

// whether the failure set up for the cases strikes this call
bool fails(Fail f)
{
	if (failing != f)
		return false;
	if (failingAfter > 0)
	{
		--failingAfter;
		return false;
	}
	return true;
}

typedef std::pair<dev_t, ino_t> Key;
typedef std::map<Key, int> Kept;

// what is kept, each with the descriptor held on it
Kept &kept()
{
	static Kept k;
	return k;
}

// the registry's mutex for a scope, given back however the scope is left
class Locked
{
	public:
		Locked() { pthread_mutex_lock(&lock); }
		~Locked() { pthread_mutex_unlock(&lock); }

	private:
		Locked(const Locked &);
		Locked &operator=(const Locked &);
};

// a descriptor closed with its scope unless it was handed on
class Owned
{
	public:
		explicit Owned(int fd) : fd_(fd) {}
		~Owned() { if (fd_ >= 0) close(fd_); }
		int get() const { return fd_; }
		int release() { const int fd = fd_; fd_ = -1; return fd; }

	private:
		int fd_;
		Owned(const Owned &);
		Owned &operator=(const Owned &);
};

// under lock: lets go of every file no name reaches any more
void sweep()
{
	Kept &k = kept();
	for (Kept::iterator it = k.begin(); it != k.end(); )
	{
		struct stat st;
		if (fstat(it->second, &st) != 0 || st.st_nlink == 0)
		{
			close(it->second);
			k.erase(it++);
		}
		else
			++it;
	}
}

// keeps the file own is open on; own is closed here unless it is kept
bool keep(int fd)
{
	Owned own(fd);
	if (own.get() < 0)
		return false;
	struct stat st;
	if (fstat(own.get(), &st) != 0)
		return false;
	const Key key(st.st_dev, st.st_ino);
	try
	{
		Locked hold;
		if (fails(FailAlloc))
			throw std::bad_alloc();
		sweep();
		Kept &k = kept();
		if (k.find(key) != k.end())
			return true;
		k[key] = own.get();
		own.release();
		return true;
	}
	catch (...)
	{
		return false;
	}
}

} // anonymous namespace

Making::Making()
{
	pthread_rwlock_wrlock(&makingLock());
}

Making::~Making()
{
	pthread_rwlock_unlock(&makingLock());
}

Asking::Asking()
{
	if (askingHook != NULL)
		askingHook();
	pthread_rwlock_rdlock(&makingLock());
}

Asking::~Asking()
{
	pthread_rwlock_unlock(&makingLock());
}

bool created(int fd)
{
	if (fd < 0)
		return false;
	const int own = fails(FailDup) ? -1 : fcntl(fd, F_DUPFD_CLOEXEC, 0);
	return keep(own);
}

bool named(const std::string &path)
{
	const int own = open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK | O_NOCTTY);
	if (own < 0)
		return errno == ENOENT || errno == ENOTDIR;
	return keep(own);
}

bool held(dev_t dev, ino_t ino)
{
	Locked hold;
	return kept().find(Key(dev, ino)) != kept().end();
}

void forgetAll()
{
	Locked hold;
	Kept &k = kept();
	for (Kept::iterator it = k.begin(); it != k.end(); ++it)
		close(it->second);
	k.clear();
}

size_t count()
{
	Locked hold;
	return kept().size();
}

void setFailForTest(Fail f, unsigned after)
{
	failing = f;
	failingAfter = after;
}

bool unlockedForTest()
{
	if (pthread_mutex_trylock(&lock) != 0)
		return false;
	pthread_mutex_unlock(&lock);
	return true;
}

void setAskingForTest(void (*asking)())
{
	askingHook = asking;
}

} // namespace accessfiles
} // namespace coreapi
