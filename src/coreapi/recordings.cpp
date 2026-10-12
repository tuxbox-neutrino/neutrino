/*
 * recordings.cpp - recordings, their files, and what is being written
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

#include "recordings.h"

#include "coreapi/base/deps.h"
#include "coreapi/base/errors.h"
#include "coreapi/channels.h"
#include "coreapi/library.h"
#include "coreapi/recordings_internal.h"

#include <timerdclient/timerdtypes.h>

#include <algorithm>
#include <cstring>
#include <utility>

#include <dirent.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>

namespace coreapi
{
namespace recordings
{

namespace
{

// As in every other read here, the status comes from the source so that one
// cause cannot become two answers on the way out. Only the code and the
// wording are this layer's.
Failure listUnreadable(Status s)
{
	return fail(s, ErrorCode::RecordingListUnavailable,
		    "what the box is recording could not be read");
}

/* Left unstated rather than guessed at when the file cannot be measured, because
   nought is a real size: a recording begun a moment ago has written nothing yet,
   and a reader handed nought for a disc that went away would be told that
   instead of being told nothing.

   Only a plain file is measured. A path that has become a directory or a device
   is not the recording it was. */
void measure(RecordingInfo &r)
{
	r.size = 0;
	r.size_known = false;
	if (r.path.empty())
		return;

	struct stat st;
	if (stat(r.path.c_str(), &st) != 0)
		return;
	if (!S_ISREG(st.st_mode))
		return;

	r.size = (uint64_t) st.st_size;
	r.size_known = true;
}

/* The running recordings, for the three calls that each have to know something
   about them before they act. Shared so that a list that could not be read is
   never mistaken for a box recording nothing, which is the one reading that
   would turn every refusal below into a success. Gone through whole because two
   of the three ask about the set and not about a member. */
Status running(RecordingList &out)
{
	return recordingSource().list(out);
}

// Whether one of them is the shift the box keeps of what it is showing. There
// is at most one: the box starts the next only once the last has ended.
bool anyTimeshift(const RecordingList &all)
{
	for (size_t i = 0; i < all.size(); i++)
	{
		if (all[i].timeshift)
			return true;
	}
	return false;
}

bool holds(const RecordingList &all, uint32_t id)
{
	for (size_t i = 0; i < all.size(); i++)
	{
		if (all[i].id == id)
			return true;
	}
	return false;
}

} // anonymous namespace

Result<RecordingList> list()
{
	RecordingList out;
	Status s = running(out);
	if (s != Status::Ok)
		return listUnreadable(s);

	for (size_t i = 0; i < out.size(); i++)
		measure(out[i]);

	return ok(std::move(out));
}

Result<void> stop(uint32_t id)
{
	RecordingList all;
	Status s = running(all);
	if (s != Status::Ok)
		return listUnreadable(s);
	if (!holds(all, id))
		return fail(Status::NotFound, ErrorCode::NoSuchRecording,
			    "the box is recording nothing under that number");

	s = recordingSource().stop(id);
	if (s != Status::Ok)
		return fail(s, ErrorCode::RecordingNotStopped,
			    "the box did not take the request to stop recording");
	return ok();
}

Result<void> startTimeshift()
{
	RecordingList all;
	Status s = running(all);
	if (s != Status::Ok)
		return listUnreadable(s);
	/* Asked here as well as on the loop, although only the loop's answer can be
	   the last word. Without it a caller asking twice is told twice that the box
	   was asked, and learns nothing about why the second one changed nothing. */
	if (anyTimeshift(all))
		return fail(Status::Conflict, ErrorCode::TimeshiftRunning,
			    "the box is already keeping a shift");

	return postEvent(BoxEvent::StartTimeshift);
}

Result<void> stopTimeshift()
{
	RecordingList all;
	Status s = running(all);
	if (s != Status::Ok)
		return listUnreadable(s);
	if (!anyTimeshift(all))
		return fail(Status::NotFound, ErrorCode::NoTimeshift,
			    "the box is keeping no shift");

	return postEvent(BoxEvent::StopTimeshift);
}

Result<void> startInstant(bool wake)
{
	Result<void> allowed = channels::standbyAllows(wake);
	if (!allowed.ok())
		return allowed;

	Result<ChannelInfo> live = channels::current();
	if (!live.ok())
		return fail(live.error());

	RecordingList all;
	Status s = running(all);
	if (s != Status::Ok)
		return listUnreadable(s);
	/* Asked here so a caller learns why nothing happened: the loop starts no
	   second recording of a channel it is recording already. The shift does not
	   count: the loop starts the recording beside it, as a file of its own. */
	for (size_t i = 0; i < all.size(); ++i)
		if (!all[i].timeshift && all[i].channel_id == live.value().id)
			return fail(Status::Conflict, ErrorCode::RecordingRunning,
				    "the channel the box is showing is being recorded already");

	/* No event behind it, which is what tells the loop to record the channel it
	   is showing rather than a timer's (src/neutrino.cpp, RECORD_START). */
	CTimerd::RecordingInfo info;
	memset(&info, 0, sizeof(info));
	info.eventID = 0;
	info.channel_id = live.value().id;
	return postEvent(BoxEvent::StartRecording, &info, sizeof(info));
}

namespace
{

// An absolute directory without its trailing slash; empty for a name that names
// none, which the movie browser also refuses: empty, relative, or / itself.
std::string folderName(std::string dir)
{
	while (dir.size() > 1 && dir[dir.size() - 1] == '/')
		dir.erase(dir.size() - 1);
	if (dir.empty() || dir[0] != '/' || dir == "/")
		return std::string();
	return dir;
}

bool listed(const FolderList &list, const std::string &path)
{
	for (size_t i = 0; i < list.items.size(); ++i)
		if (list.items[i].path == path)
			return true;
	return false;
}

/* One walk at a time, on a thread of its own: a disc that stops answering, a
   network share whose server went away, holds that thread and no request. A
   request waits for the walk until its deadline and then answers what the walk
   has found so far. Never destroyed, because a walk held up by a share may
   still write into it at exit. */
struct Walk
{
	pthread_mutex_t        mutex;
	pthread_cond_t         changed;
	bool                   busy;
	unsigned long          started;   // walks begun
	unsigned long          finished;  // walks ended
	FolderList             found;     // what the running walk has found so far
	/* What the last walk to end found, whole. A request whose walk has ended
	   reads this and never found: by the time it wakes, another request may
	   have begun the next walk and emptied found. */
	FolderList             last;
	long                   wait_ms;
	internal::FolderProbe  probe;

	Walk() : busy(false), started(0), finished(0), wait_ms(internal::kFolderWaitMs), probe(NULL)
	{
		found.partial = false;
		last.partial = false;
		pthread_mutex_init(&mutex, NULL);
		pthread_condattr_t attr;
		pthread_condattr_init(&attr);
		pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
		pthread_cond_init(&changed, &attr);
		pthread_condattr_destroy(&attr);
	}
};

Walk &walk()
{
	static Walk *w = new Walk;
	return *w;
}

class Held
{
	public:
		Held() { pthread_mutex_lock(&walk().mutex); }
		~Held() { pthread_mutex_unlock(&walk().mutex); }
};

int64_t nowMs()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// Under the lock; false once the list is full, which ends the walk.
bool addFolder(const std::string &path, bool root, bool present)
{
	FolderList &list = walk().found;
	if (listed(list, path))
		return true;
	if (list.items.size() >= kMaxFolders)
	{
		list.partial = true;
		return false;
	}
	Folder f;
	f.path = path;
	f.root = root;
	f.present = present;
	list.items.push_back(f);
	return true;
}

void cut()
{
	Held held;
	walk().found.partial = true;
}

/* The directories below dir, by name, each followed by its own; false once the
   walk is to end. Names are read whole before going down, so one directory is
   open at a time, but no more of them than the list still has room for, and no
   more entries in all than the budget: a directory of a million files costs the
   budget and not a million calls. */
bool descend(const std::string &dir, int depth, size_t &budget, internal::FolderProbe probe)
{
	if (depth > kFolderDepth)
		return true;
	if (budget == 0)
	{
		cut();
		return false;
	}
	size_t room;
	{
		Held held;
		if (walk().found.items.size() >= kMaxFolders)
		{
			walk().found.partial = true;
			return false;
		}
		room = kMaxFolders - walk().found.items.size();
	}
	if (probe != NULL)
		probe(dir);
	DIR *d = opendir(dir.c_str());
	if (d == NULL)
		return true;
	std::vector<std::string> names;
	bool more = false;
	struct dirent *e;
	while ((e = readdir(d)) != NULL)
	{
		if (budget == 0)
		{
			more = true;
			break;
		}
		--budget;
		// Covers "." and ".." too, and the timeshift buffer under its dot-named folder.
		if (e->d_name[0] == '.')
			continue;
		bool is_dir = e->d_type == DT_DIR;
		// Only an entry the filesystem does not type is asked about; a link is typed as one.
		if (e->d_type == DT_UNKNOWN)
		{
			struct stat st;
			is_dir = lstat((dir + "/" + e->d_name).c_str(), &st) == 0 && S_ISDIR(st.st_mode);
		}
		if (!is_dir)
			continue;
		if (names.size() == room)
		{
			more = true;
			break;
		}
		names.push_back(e->d_name);
	}
	closedir(d);
	if (more)
		cut();
	std::sort(names.begin(), names.end());
	for (size_t i = 0; i < names.size(); ++i)
	{
		const std::string path = dir + "/" + names[i];
		{
			Held held;
			if (!addFolder(path, false, true))
				return false;
		}
		if (!descend(path, depth + 1, budget, probe))
			return false;
	}
	return true;
}

struct WalkJob
{
	std::vector<std::string> roots;
};

void *walkFolders(void *arg)
{
	const std::vector<std::string> roots = ((WalkJob *) arg)->roots;
	delete (WalkJob *) arg;
	internal::FolderProbe probe;
	{
		Held held;
		probe = walk().probe;
	}

	// The roots first, so a list cut short below them still holds them all.
	std::vector<std::string> present;
	for (size_t i = 0; i < roots.size(); ++i)
	{
		if (probe != NULL)
			probe(roots[i]);
		// A root may be a link to where the disc is mounted, so it is followed.
		struct stat st;
		const bool there = stat(roots[i].c_str(), &st) == 0 && S_ISDIR(st.st_mode);
		{
			Held held;
			addFolder(roots[i], true, there);
			pthread_cond_broadcast(&walk().changed);
		}
		if (there)
			present.push_back(roots[i]);
	}
	size_t budget = internal::kFolderEntryBudget;
	for (size_t i = 0; i < present.size(); ++i)
		if (!descend(present[i], 1, budget, probe))
			break;

	Held held;
	walk().last = walk().found;
	walk().busy = false;
	++walk().finished;
	pthread_cond_broadcast(&walk().changed);
	return NULL;
}

// Under the lock; false when no thread could be started.
bool startWalk(const std::vector<std::string> &roots)
{
	Walk &w = walk();
	w.found = FolderList();
	w.found.partial = false;
	WalkJob *job = new WalkJob;
	job->roots = roots;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_t t;
	const bool ok = pthread_create(&t, &attr, &walkFolders, job) == 0;
	pthread_attr_destroy(&attr);
	if (!ok)
	{
		delete job;
		return false;
	}
	w.busy = true;
	++w.started;
	return true;
}

} // namespace

FolderList folders()
{
	const library::Dirs d = library::read();
	std::vector<std::string> roots;
	std::vector<std::string> named;
	named.push_back(d.record);
	for (size_t i = 0; i < d.own.size(); ++i)
		named.push_back(d.own[i].path);
	for (size_t i = 0; i < named.size(); ++i)
	{
		const std::string dir = folderName(named[i]);
		if (!dir.empty() && std::find(roots.begin(), roots.end(), dir) == roots.end())
			roots.push_back(dir);
	}

	Walk &w = walk();
	FolderList out;
	bool done = false;
	{
		Held held;
		/* A walk already running is waited for rather than a second one begun,
		   so a share that hangs holds one thread however often this is asked. */
		if (w.busy || startWalk(roots))
		{
			const unsigned long mine = w.started;
			const int64_t deadline = nowMs() + w.wait_ms;
			while (w.finished < mine)
			{
				const int64_t now = nowMs();
				if (now >= deadline)
					break;
				struct timespec until;
				until.tv_sec = (time_t) (deadline / 1000);
				until.tv_nsec = (long) (deadline % 1000) * 1000000;
				pthread_cond_timedwait(&w.changed, &w.mutex, &until);
			}
			done = w.finished >= mine;
			// Ended: the walk waited for, or one begun and ended after it.
			out = done ? w.last : w.found;
		}
	}
	if (done)
		return out;

	/* Cut short: what the walk found, with the roots it has not reached, or
	   that another walk was begun for, as not there. */
	FolderList cut_short;
	cut_short.partial = true;
	for (size_t i = 0; i < roots.size(); ++i)
	{
		Folder f;
		f.path = roots[i];
		f.root = true;
		f.present = false;
		for (size_t j = 0; j < out.items.size(); ++j)
			if (out.items[j].root && out.items[j].path == roots[i])
				f.present = out.items[j].present;
		cut_short.items.push_back(f);
	}
	for (size_t j = 0; j < out.items.size(); ++j)
		if (!out.items[j].root && !listed(cut_short, out.items[j].path))
			cut_short.items.push_back(out.items[j]);
	return cut_short;
}

namespace internal
{

void setFolderWaitForTest(long wait_ms)
{
	Held held;
	walk().wait_ms = wait_ms < 0 ? kFolderWaitMs : wait_ms;
}

void setFolderProbeForTest(FolderProbe p)
{
	Held held;
	walk().probe = p;
}

} // namespace internal


} // namespace recordings
} // namespace coreapi
