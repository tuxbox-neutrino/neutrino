/*
 * archive.cpp - the finished recordings in the movie browser's directories
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

#include "coreapi/archive.h"
#include "coreapi/archive_internal.h"

#include "coreapi/channels.h"
#include "coreapi/library.h"
#include "coreapi/recordings.h"
#include "coreapi/base/deps.h"
#include "coreapi/base/errors.h"

#include <neutrinoMessages.h>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>
#include <openssl/sha.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>

#include <dirent.h>
#include <limits.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>

namespace coreapi
{
namespace archive
{

namespace
{

// Metadata the box writes is a few kilobytes; anything beyond this is not read.
const size_t kMaxMetadataBytes = 64 * 1024;

OpenThreads::Mutex &lock()
{
	static OpenThreads::Mutex m;
	return m;
}

std::string &playing()
{
	static std::string p;
	return p;
}

bool &playingShift()
{
	static bool b = false;
	return b;
}

// Both halves under one hold, so a playback ending between two reads is never half seen.
void playingNow(std::string &path, bool &timeshift)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	path = playing();
	timeshift = playingShift();
}

struct Found
{
	std::string rel;   // below the root
	std::string path;  // the resolved root and rel
	struct stat st;
};

bool endsWith(const std::string &s, const char *tail)
{
	const size_t n = std::strlen(tail);
	return s.size() > n && s.compare(s.size() - n, n, tail) == 0;
}

std::string stemOf(const std::string &path)
{
	return path.substr(0, path.size() - 3);
}

// Of the whole resolved name, so one file has one id whichever directory lists it.
std::string idOf(const std::string &path)
{
	unsigned char d[SHA256_DIGEST_LENGTH];
	SHA256((const unsigned char *) path.data(), path.size(), d);
	static const char kHex[] = "0123456789abcdef";
	std::string out;
	for (size_t i = 0; i < 8; ++i)
	{
		out += kHex[d[i] >> 4];
		out += kHex[d[i] & 15];
	}
	return out;
}

bool isPlainFile(const std::string &path, struct stat *st)
{
	return lstat(path.c_str(), st) == 0 && S_ISREG(st->st_mode);
}

// Same spelling, or the same file through a linked record directory.
bool sameFile(const std::string &a, const std::string &b)
{
	if (a == b)
		return true;
	struct stat sa, sb;
	if (a.empty() || b.empty())
		return false;
	if (stat(a.c_str(), &sa) != 0 || stat(b.c_str(), &sb) != 0)
		return false;
	return sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino;
}

void appendUtf8(std::string &out, unsigned long cp)
{
	if (cp < 0x80)
		out += (char) cp;
	else if (cp < 0x800)
	{
		out += (char) (0xc0 | (cp >> 6));
		out += (char) (0x80 | (cp & 0x3f));
	}
	else if (cp < 0x10000)
	{
		out += (char) (0xe0 | (cp >> 12));
		out += (char) (0x80 | ((cp >> 6) & 0x3f));
		out += (char) (0x80 | (cp & 0x3f));
	}
	else if (cp < 0x110000)
	{
		out += (char) (0xf0 | (cp >> 18));
		out += (char) (0x80 | ((cp >> 12) & 0x3f));
		out += (char) (0x80 | ((cp >> 6) & 0x3f));
		out += (char) (0x80 | (cp & 0x3f));
	}
}

std::string decoded(const std::string &s)
{
	std::string out;
	for (size_t i = 0; i < s.size(); ++i)
	{
		if (s[i] != '&')
		{
			out += s[i];
			continue;
		}
		const size_t semi = s.find(';', i);
		if (semi == std::string::npos || semi - i > 10)
		{
			out += s[i];
			continue;
		}
		const std::string name = s.substr(i + 1, semi - i - 1);
		if (name == "amp") out += '&';
		else if (name == "lt") out += '<';
		else if (name == "gt") out += '>';
		else if (name == "quot") out += '"';
		else if (name == "apos") out += '\'';
		else if (name.size() > 1 && name[0] == '#')
		{
			const bool hex = name[1] == 'x' || name[1] == 'X';
			const unsigned long cp = std::strtoul(name.c_str() + (hex ? 2 : 1), NULL, hex ? 16 : 10);
			// A NUL, a surrogate half or a value past the last code point is kept
			// as the entity rather than turned into a NUL or invalid UTF-8.
			if (cp == 0 || (cp >= 0xd800 && cp <= 0xdfff) || cp > 0x10ffff)
				out += s.substr(i, semi - i + 1);
			else
				appendUtf8(out, cp);
		}
		else
		{
			out += s[i];
			continue;
		}
		i = semi;
	}
	return out;
}

std::string tagText(const std::string &xml, const char *tag)
{
	const std::string open = std::string("<") + tag + ">";
	const std::string close = std::string("</") + tag + ">";
	const size_t a = xml.find(open);
	if (a == std::string::npos)
		return std::string();
	const size_t b = xml.find(close, a + open.size());
	if (b == std::string::npos)
		return std::string();
	return decoded(xml.substr(a + open.size(), b - a - open.size()));
}

// Cut at a character boundary, so a cut text is still whole UTF-8.
std::string bounded(const std::string &s, size_t max)
{
	if (s.size() <= max)
		return s;
	size_t n = max;
	while (n > 0 && ((unsigned char) s[n] & 0xc0) == 0x80)
		--n;
	return s.substr(0, n);
}

/* 0, which reads as not stated, for anything past max. A sign wraps round to
   past every max here, so a negative value goes the same way. */
unsigned long numberUpTo(const std::string &xml, const char *tag, unsigned long max)
{
	const unsigned long n = std::strtoul(tagText(xml, tag).c_str(), NULL, 10);
	return n > max ? 0 : n;
}

std::string attributeOf(const std::string &element, const char *name)
{
	const std::string open = std::string(" ") + name + "=\"";
	const size_t a = element.find(open);
	if (a == std::string::npos)
		return std::string();
	const size_t b = element.find('"', a + open.size());
	if (b == std::string::npos)
		return std::string();
	return decoded(element.substr(a + open.size(), b - a - open.size()));
}

std::vector<std::string> audioNames(const std::string &xml)
{
	std::vector<std::string> out;
	const size_t a = xml.find("<audiopids>");
	if (a == std::string::npos)
		return out;
	const size_t end = xml.find("</audiopids>", a);
	const std::string block = xml.substr(a, end == std::string::npos ? std::string::npos : end - a);
	size_t at = 0;
	while (out.size() < kMaxAudioTracks && (at = block.find("<audio ", at)) != std::string::npos)
	{
		const size_t close = block.find('>', at);
		const std::string element = block.substr(at, close == std::string::npos ? std::string::npos : close - at);
		const std::string name = bounded(attributeOf(element, "name"), kMaxShortText);
		if (!name.empty())
			out.push_back(name);
		at += element.size();
	}
	return out;
}

// The movie browser's order; a link or anything but a plain file inside its library directory is no cover.
std::string coverOf(const std::string &ts_path, const std::string &root)
{
	static const char *const kExts[] = { ".jpg", ".png", ".gif", ".jpeg", ".bmp" };
	if (root.empty())
		return std::string();
	const std::string stem = stemOf(ts_path);
	for (size_t i = 0; i < sizeof(kExts) / sizeof(kExts[0]); ++i)
	{
		const std::string path = stem + kExts[i];
		struct stat st;
		char real[PATH_MAX];
		// A second link is a second name for the same bytes, the way a symlink is.
		if (!isPlainFile(path, &st) || st.st_nlink > 1 || realpath(path.c_str(), real) == NULL)
			continue;
		if (std::string(real).compare(0, root.size() + 1, root + "/") == 0)
			return path;
	}
	return std::string();
}

std::string metadataOf(const std::string &path)
{
	std::string out;
	FILE *f = std::fopen(path.c_str(), "rb");
	if (f == NULL)
		return out;
	char buf[4096];
	size_t n = 0;
	while (out.size() < kMaxMetadataBytes && (n = std::fread(buf, 1, sizeof(buf), f)) > 0)
		out.append(buf, std::min(n, kMaxMetadataBytes - out.size()));
	std::fclose(f);
	return out;
}

// Depth 0 is the library directory, depth 1 one folder below it; links are never followed.
void scanDir(const std::string &root, const std::string &rel_dir, int depth, std::vector<Found> &out)
{
	const std::string dir = rel_dir.empty() ? root : root + "/" + rel_dir;
	DIR *d = opendir(dir.c_str());
	if (d == NULL)
		return;
	struct dirent *e;
	while ((e = readdir(d)) != NULL)
	{
		const std::string name = e->d_name;
		// Covers "." and ".." too, and the timeshift buffer under its dot-named folder.
		if (!name.empty() && name[0] == '.')
			continue;
		const std::string rel = rel_dir.empty() ? name : rel_dir + "/" + name;
		const std::string path = root + "/" + rel;
		struct stat st;
		if (lstat(path.c_str(), &st) != 0 || S_ISLNK(st.st_mode))
			continue;
		if (S_ISDIR(st.st_mode))
		{
			if (depth == 0)
				scanDir(root, rel, 1, out);
			continue;
		}
		struct stat meta;
		if (!S_ISREG(st.st_mode) || !endsWith(name, ".ts") || !isPlainFile(stemOf(path) + ".xml", &meta))
			continue;
		Found f;
		f.rel = rel;
		f.path = path;
		f.st = st;
		out.push_back(f);
	}
	closedir(d);
}

Entry entryOf(const Found &f, const std::string &source, const std::string &root)
{
	const std::string xml = metadataOf(stemOf(f.path) + ".xml");
	Entry e;
	e.id = idOf(f.path);
	e.title = bounded(tagText(xml, "epgtitle"), kMaxShortText);
	e.channel = bounded(tagText(xml, "channelname"), kMaxShortText);
	e.channel_id = (ChannelId) std::strtoull(tagText(xml, "id").c_str(), NULL, 10);
	const unsigned long minutes = numberUpTo(xml, "length", kMaxDurationMinutes);
	e.duration = (long) (minutes * 60);
	e.start = f.st.st_mtime - e.duration;
	e.size = (uint64_t) f.st.st_size;
	e.path = f.path;
	e.playing = false;
	e.source = source;
	e.root = root;
	return e;
}

/* What a scan of one library directory found. A scan runs on a thread of its
   own, because a share that went away blocks whoever touches it for as long as
   its mount waits, and no request may wait that long. */
struct Item
{
	Entry entry;
	dev_t dev;
	ino_t ino;
};

struct RootState
{
	std::string        resolved;
	bool               probed;        // the running or last scan reached the directory
	bool               scanned;       // a scan has finished
	bool               busy;
	unsigned long      epoch;         // bumped to make what was scanned stale
	unsigned long      scanned_epoch;
	int64_t            scanned_ms;    // when the scan that finished began
	int64_t            started_ms;    // when the running scan began
	std::vector<Item>  items;

	RootState() : probed(false), scanned(false), busy(false), epoch(0), scanned_epoch(0), scanned_ms(0), started_ms(0) {}
};

// A scan running this long has hung on a share that went away after it answered.
const int64_t kGiveUpMs = 10 * 60 * 1000;

long wait_ms = internal::kWaitMs;
long max_age_ms = internal::kMaxAgeMs;
internal::RootProbe root_probe = NULL;
internal::RootProbe reading_probe = NULL;
internal::Writable writable_check = NULL;
internal::Unlink unlink_call = NULL;

struct Shelf
{
	pthread_mutex_t                  mutex;
	pthread_cond_t                   changed;
	std::map<std::string, RootState> roots;

	Shelf()
	{
		pthread_mutex_init(&mutex, NULL);
		pthread_condattr_t attr;
		pthread_condattr_init(&attr);
		pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
		pthread_cond_init(&changed, &attr);
		pthread_condattr_destroy(&attr);
	}
};

// Never destroyed: a worker held up by a share may still write into it at exit.
Shelf &shelf()
{
	static Shelf *s = new Shelf;
	return *s;
}

class Held
{
	public:
		Held() { pthread_mutex_lock(&shelf().mutex); }
		~Held() { pthread_mutex_unlock(&shelf().mutex); }
};

int64_t nowMs()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

struct Job
{
	std::string   source;
	unsigned long epoch;
	int64_t       started_ms;
};

void *scanRoot(void *arg)
{
	const Job job = *(Job *) arg;
	delete (Job *) arg;
	internal::RootProbe probe = NULL;
	internal::RootProbe reading = NULL;
	{
		Held held;
		probe = root_probe;
		reading = reading_probe;
	}
	if (probe != NULL)
		probe(job.source);

	std::string resolved;
	char real[PATH_MAX];
	struct stat st;
	if (realpath(job.source.c_str(), real) != NULL && stat(real, &st) == 0 && S_ISDIR(st.st_mode))
		resolved = real;
	{
		Held held;
		RootState &r = shelf().roots[job.source];
		r.resolved = resolved;
		r.probed = true;
		pthread_cond_broadcast(&shelf().changed);
	}

	std::vector<Item> items;
	if (reading != NULL)
		reading(job.source);
	if (!resolved.empty())
	{
		std::vector<Found> found;
		scanDir(resolved, std::string(), 0, found);
		for (size_t i = 0; i < found.size(); ++i)
		{
			Item it;
			it.entry = entryOf(found[i], job.source, resolved);
			it.dev = found[i].st.st_dev;
			it.ino = found[i].st.st_ino;
			items.push_back(it);
		}
	}

	Held held;
	RootState &r = shelf().roots[job.source];
	r.items.swap(items);
	r.scanned = true;
	r.scanned_epoch = job.epoch;
	r.scanned_ms = job.started_ms;
	r.busy = false;
	pthread_cond_broadcast(&shelf().changed);
	return NULL;
}

// Under the shelf's lock.
void startScan(const std::string &source, RootState &r, int64_t now)
{
	Job *job = new Job;
	job->source = source;
	job->epoch = r.epoch;
	job->started_ms = now;
	r.busy = true;
	r.probed = false;
	r.started_ms = now;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_t t;
	if (pthread_create(&t, &attr, &scanRoot, job) != 0)
	{
		delete job;
		r.busy = false;
	}
	pthread_attr_destroy(&attr);
}

bool fresh(const RootState &r, int64_t now)
{
	return r.scanned && r.scanned_epoch == r.epoch && now - r.scanned_ms < max_age_ms;
}

/* Every running scan is waited for up to its deadline, also one that only
   renews an aged one, so a file copied in by other means is seen by the next
   request. A scan still reading past it is answered with what the one before
   it found. */
bool settled(const RootState &r)
{
	return !r.busy;
}

/* A directory that has not answered the running scan within the wait is left
   out, as is one whose scan has hung: either is a share that went away. */
bool answering(const RootState &r, int64_t now)
{
	if (!r.busy)
		return true;
	if (r.probed)
		return now - r.started_ms < kGiveUpMs;
	return now - r.started_ms < wait_ms;
}

struct Library
{
	std::vector<Item>        items;    // each file once, the first directory listing it wins
	std::vector<std::string> sources;  // read whole, so their recordings are in items
	std::vector<std::string> roots;    // resolved, beside sources
	bool                     partial;  // a directory that answered is still being read the first time
	Library() : partial(false) {}
};

/* The library as the movie browser names it, each directory scanned again once
   what was scanned has aged. Waits for a scan until it has run wait_ms, so a
   share that never answers costs the first request that long and no other.
   With only set, its items are taken whole, also those another directory
   lists first. */
Library gather(const std::string &only)
{
	const std::vector<std::string> names = library::roots();
	Library out;
	Held held;
	Shelf &sh = shelf();
	int64_t now = nowMs();

	for (std::map<std::string, RootState>::iterator it = sh.roots.begin(); it != sh.roots.end();)
	{
		if (!it->second.busy && std::find(names.begin(), names.end(), it->first) == names.end())
			sh.roots.erase(it++);
		else
			++it;
	}
	for (size_t i = 0; i < names.size(); ++i)
	{
		RootState &r = sh.roots[names[i]];
		if (!r.busy && !fresh(r, now))
			startScan(names[i], r, now);
	}

	for (;;)
	{
		now = nowMs();
		int64_t deadline = now;
		for (size_t i = 0; i < names.size(); ++i)
		{
			const RootState &r = sh.roots[names[i]];
			if (!settled(r))
				deadline = std::max(deadline, r.started_ms + wait_ms);
		}
		if (deadline <= now)
			break;
		struct timespec until;
		until.tv_sec = (time_t) (deadline / 1000);
		until.tv_nsec = (long) (deadline % 1000) * 1000000;
		pthread_cond_timedwait(&sh.changed, &sh.mutex, &until);
	}

	std::set<std::string> ids;
	for (size_t i = 0; i < names.size(); ++i)
	{
		const RootState &r = sh.roots[names[i]];
		if (!answering(r, now))
			continue;
		// Answered but not read through yet, or a worker that could not start: ask again.
		if (!r.scanned)
		{
			// Only a directory the answer draws on makes it partial.
			if ((only.empty() || names[i] == only) && (!r.probed || !r.resolved.empty()))
				out.partial = true;
			continue;
		}
		if (r.resolved.empty())
			continue;
		out.sources.push_back(names[i]);
		out.roots.push_back(r.resolved);
		if (!only.empty() && names[i] != only)
			continue;
		for (size_t k = 0; k < r.items.size(); ++k)
		{
			if (ids.insert(r.items[k].entry.id).second)
				out.items.push_back(r.items[k]);
		}
	}
	return out;
}

// Every directory listing this file reads again, also one another directory lies in.
void outdate(const std::string &path)
{
	Held held;
	for (std::map<std::string, RootState>::iterator it = shelf().roots.begin(); it != shelf().roots.end(); ++it)
	{
		const std::string &root = it->second.resolved;
		if (!root.empty() && path.compare(0, root.size() + 1, root + "/") == 0)
			++it->second.epoch;
	}
}

int unlinked(const std::string &path)
{
	return unlink_call != NULL ? unlink_call(path.c_str()) : unlink(path.c_str());
}

bool liesIn(const std::string &path, const std::string &dir)
{
	return path.compare(0, dir.size() + 1, dir + "/") == 0;
}

// A medium mounted read-only, or a directory this process may not write.
bool writable(const std::string &dir)
{
	if (writable_check != NULL)
		return writable_check(dir);
	struct statvfs vfs;
	if (statvfs(dir.c_str(), &vfs) == 0 && (vfs.f_flag & ST_RDONLY))
		return false;
	return access(dir.c_str(), W_OK) == 0;
}

/* The playing file, looked at once per request rather than once per entry, and
   only inside a directory that answered: a share that went away would hold the
   request for as long as its mount waits. */
class Playing
{
	public:
		explicit Playing(const Library &lib) : path_(playingPath()), known_(false)
		{
			bool reachable = false;
			for (size_t i = 0; i < lib.sources.size() && !reachable; ++i)
				reachable = liesIn(path_, lib.sources[i]) || liesIn(path_, lib.roots[i]);
			known_ = reachable && stat(path_.c_str(), &st_) == 0;
		}

		bool is(const Item &it) const
		{
			if (path_.empty())
				return false;
			return it.entry.path == path_ || (known_ && st_.st_dev == it.dev && st_.st_ino == it.ino);
		}

	private:
		std::string path_;
		bool        known_;
		struct stat st_;
};

std::string folded(const std::string &s)
{
	std::string out(s);
	for (size_t i = 0; i < out.size(); ++i)
	{
		if (out[i] >= 'A' && out[i] <= 'Z')
			out[i] = (char) (out[i] - 'A' + 'a');
	}
	return out;
}

// Title and channel compare folded the way the title filter matches.
class Before
{
	public:
		explicit Before(const Sort &sort) : sort_(sort) {}

		bool operator()(const Entry &a, const Entry &b) const
		{
			const int c = compared(a, b);
			if (c != 0)
				return sort_.descending ? c > 0 : c < 0;
			return a.id < b.id;
		}

	private:
		template <typename T>
		static int order(const T &a, const T &b)
		{
			return a < b ? -1 : (b < a ? 1 : 0);
		}

		int compared(const Entry &a, const Entry &b) const
		{
			switch (sort_.key)
			{
				case SortKey::Title:    return order(folded(a.title), folded(b.title));
				case SortKey::Channel:  return order(folded(a.channel), folded(b.channel));
				case SortKey::Duration: return order(a.duration, b.duration);
				case SortKey::Size:     return order(a.size, b.size);
				case SortKey::Start:    break;
			}
			return order(a.start, b.start);
		}

		Sort sort_;
};

} // namespace

bool descendingByDefault(SortKey key)
{
	return key == SortKey::Start || key == SortKey::Duration || key == SortKey::Size;
}

Result<Page> list(const std::string &title_part, size_t offset, size_t limit, const Sort &sort,
                  const std::string &source)
{
	if (limit > kMaxLimit)
		limit = kMaxLimit;
	const std::string want = folded(title_part);
	const Library lib = gather(source);
	const Playing now(lib);
	std::vector<Entry> kept;
	for (size_t i = 0; i < lib.items.size(); ++i)
	{
		Entry e = lib.items[i].entry;
		e.playing = now.is(lib.items[i]);
		if (want.empty() || folded(e.title).find(want) != std::string::npos)
			kept.push_back(e);
	}
	std::sort(kept.begin(), kept.end(), Before(sort));
	Page p;
	p.total = kept.size();
	p.sources = lib.sources;
	p.partial = lib.partial;
	for (size_t i = offset; i < kept.size() && p.items.size() < limit; ++i)
		p.items.push_back(kept[i]);
	return ok(p);
}

void refresh()
{
	Held held;
	for (std::map<std::string, RootState>::iterator it = shelf().roots.begin(); it != shelf().roots.end(); ++it)
		++it->second.epoch;
}

namespace
{

/* The rules a scan keeps, for one name below the first of the resolved library
   directories that holds it, in their order; -1 for none. Each name is looked
   at once, however many directories there are. */
int foundIn(const std::string &path, const std::vector<std::string> &roots, Found &f)
{
	const size_t cut = path.rfind('/');
	if (cut == std::string::npos || cut == 0)
		return -1;
	const std::string leaf = path.substr(cut + 1);
	if (leaf.empty() || leaf[0] == '.' || !endsWith(leaf, ".ts"))
		return -1;
	struct stat st;
	const std::string folder = path.substr(0, cut);
	char real[PATH_MAX];
	if (lstat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode) || realpath(folder.c_str(), real) == NULL)
		return -1;
	const std::string at(real);
	// The root may be spelled through a link; the leaf and its one folder may not, as in scanDir.
	bool asked_above = false;
	std::string above;
	const size_t up = folder.rfind('/');
	const std::string name = up == std::string::npos ? std::string() : folder.substr(up + 1);
	for (size_t i = 0; i < roots.size(); ++i)
	{
		if (roots[i].empty())
			continue;
		if (at == roots[i])
		{
			f.rel = leaf;
		}
		else
		{
			if (!asked_above)
			{
				asked_above = true;
				struct stat dir;
				if (up != std::string::npos && up != 0 && !name.empty() && name[0] != '.' &&
				    lstat(folder.c_str(), &dir) == 0 && S_ISDIR(dir.st_mode) &&
				    realpath(folder.substr(0, up).c_str(), real) != NULL)
					above = real;
			}
			if (above.empty() || above != roots[i])
				continue;
			f.rel = name + "/" + leaf;
		}
		struct stat meta;
		f.path = roots[i] + "/" + f.rel;
		f.st = st;
		return isPlainFile(stemOf(f.path) + ".xml", &meta) ? (int) i : -1;
	}
	return -1;
}

} // namespace

/* Called as a file starts on the screen's thread, so it neither starts a scan
   nor waits for one: it takes the directories as the last scan resolved them,
   and resolves one not scanned yet only when the path names it, which touches
   no share but the one the file lies on. */
Result<Entry> entryAt(const std::string &path)
{
	if (path.empty() || path[0] != '/')
		return fail<Entry>(Status::NotFound, ErrorCode::NoSuchRecording, "no recording has that path");
	const std::vector<std::string> names = library::roots();
	std::vector<std::string> roots(names.size());
	std::vector<bool> left_out(names.size(), false);
	{
		Held held;
		const int64_t now = nowMs();
		for (size_t i = 0; i < names.size(); ++i)
		{
			std::map<std::string, RootState>::const_iterator it = shelf().roots.find(names[i]);
			if (it == shelf().roots.end())
				continue;
			left_out[i] = !answering(it->second, now);
			if (!left_out[i])
				roots[i] = it->second.resolved;
		}
	}
	for (size_t i = 0; i < names.size(); ++i)
	{
		char real[PATH_MAX];
		if (roots[i].empty() && !left_out[i] && liesIn(path, names[i]) && realpath(names[i].c_str(), real) != NULL)
			roots[i] = real;
	}
	Found f;
	const int at = foundIn(path, roots, f);
	if (at >= 0)
	{
		Entry e = entryOf(f, names[at], roots[at]);
		e.playing = sameFile(e.path, playingPath());
		return ok(e);
	}
	return fail<Entry>(Status::NotFound, ErrorCode::NoSuchRecording, "no recording has that path");
}

Result<Entry> find(const std::string &id)
{
	if (id.size() != 16 || id.find_first_not_of("0123456789abcdef") != std::string::npos)
		return fail<Entry>(Status::NotFound, ErrorCode::NoSuchRecording, "no recording has that id");
	const Library lib = gather(std::string());
	for (size_t i = 0; i < lib.items.size(); ++i)
	{
		if (lib.items[i].entry.id != id)
			continue;
		Entry e = lib.items[i].entry;
		// The scan may be seconds old; its rules are kept again right before the path is used.
		Found f;
		if (foundIn(e.path, std::vector<std::string>(1, e.root), f) != 0 || f.path != e.path)
		{
			outdate(e.path);
			break;
		}
		e.playing = Playing(lib).is(lib.items[i]);
		return ok(e);
	}
	return fail<Entry>(Status::NotFound, ErrorCode::NoSuchRecording, "no recording has that id");
}

Result<void> remove(const std::string &id)
{
	Result<Entry> e = find(id);
	if (!e.ok())
		return fail(e.error());
	const std::string path = e.value().path;

	Result<RecordingList> running = recordings::list();
	if (!running.ok())
		return fail(running.error());
	const RecordingList rl = running.value();
	for (size_t i = 0; i < rl.size(); ++i)
	{
		if (sameFile(rl[i].path, path) || sameFile(rl[i].path + ".ts", path))
			return fail(Status::Conflict, ErrorCode::RecordingRunning, "the box is still writing this recording");
	}
	if (sameFile(playingPath(), path))
		return fail(Status::Conflict, ErrorCode::RecordingPlaying, "the box is playing this recording");

	// Asked before the first unlink, so a read-only share never loses half a recording.
	const Result<void> read_only = fail(Status::Conflict, ErrorCode::MediumReadOnly,
		"the box may not remove files in the directory this recording lies in; nothing was removed");
	if (!writable(path.substr(0, path.rfind('/'))))
		return read_only;
	if (unlinked(path) != 0)
	{
		const int why = errno;
		if (why == ENOENT)
		{
			outdate(path);
			return fail(Status::NotFound, ErrorCode::NoSuchRecording, "no recording has that id");
		}
		if (why == EROFS || why == EACCES || why == EPERM)
			return read_only;
		return fail(Status::Internal, ErrorCode::ChangeRefused, "the recording could not be removed");
	}
	const std::string stem = stemOf(path);
	static const char *const kBeside[] = { ".xml", ".jpg", ".png", ".gif", ".jpeg", ".bmp" };
	for (size_t i = 0; i < sizeof(kBeside) / sizeof(kBeside[0]); ++i)
		unlinked(stem + kBeside[i]);
	outdate(path);
	return ok();
}

Result<void> play(const std::string &id, bool wake, bool stop_playback)
{
	Result<Entry> e = find(id);
	if (!e.ok())
		return fail(e.error());
	Result<void> free = playbackAllows(stop_playback);
	if (!free.ok())
		return free;
	std::string now;
	bool timeshift = false;
	playingNow(now, timeshift);
	if (!now.empty() && timeshift)
		return fail(Status::Conflict, ErrorCode::RecordingPlaying, "the box is playing back its timeshift");
	// Unlike a zap: the player draws on screens the box builds only once it has a mode.
	int mode = 0;
	if (channelSource().currentMode(mode) == Status::NotFound)
		return fail(Status::Conflict, ErrorCode::ModeUnavailable, "the box has not finished starting");
	Result<void> allowed = channels::standbyAllows(wake);
	if (!allowed.ok())
		return allowed;
	// The loop wakes a sleeping box on this message before it plays, as on a zap.
	// The leave travels along: a file may start between this check and the loop.
	const std::string payload = (stop_playback ? "1" : "0") + e.value().path;
	return postPayload(NeutrinoMessages::EVT_PLAY_RECORDING, payload.c_str(), payload.size() + 1);
}

PlayAsked playAsked(const char *payload)
{
	PlayAsked asked;
	asked.stop_playback = payload && payload[0] == '1';
	asked.path = payload && payload[0] ? payload + 1 : "";
	return asked;
}

Result<Details> details(const std::string &id)
{
	Result<Entry> e = find(id);
	if (!e.ok())
		return fail(e.error());
	Details d;
	d.entry = e.value();
	const std::string xml = metadataOf(stemOf(d.entry.path) + ".xml");
	d.description = bounded(tagText(xml, "info1"), kMaxShortText);
	d.long_description = bounded(tagText(xml, "info2"), kMaxLongText);
	d.genre = numberUpTo(xml, "genremajor", 255);
	d.genre_minor = numberUpTo(xml, "genreminor", 255);
	d.series = bounded(tagText(xml, "seriename"), kMaxShortText);
	d.country = bounded(tagText(xml, "productioncountry"), kMaxShortText);
	d.year = numberUpTo(xml, "productiondate", 9999);
	d.rating = numberUpTo(xml, "rating", 100);
	d.quality = numberUpTo(xml, "quality", 3);
	if (d.quality == 0)
		d.quality = numberUpTo(xml, "qualitiy", 3);
	d.age = numberUpTo(xml, "parentallockage", 99);
	if (d.age > 18 && d.age != kAlwaysLocked)
		d.age = 0;
	d.audio = audioNames(xml);
	d.cover = !coverOf(d.entry.path, d.entry.root).empty();
	return ok(d);
}

Result<std::string> coverPath(const std::string &id)
{
	Result<Entry> e = find(id);
	if (!e.ok())
		return fail(e.error());
	const std::string cover = coverOf(e.value().path, e.value().root);
	if (cover.empty())
		return fail<std::string>(Status::NotFound, ErrorCode::NoSuchRecording, "this recording has no cover");
	return ok(cover);
}

void notePlaying(const std::string &path, bool timeshift)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	playing() = path;
	playingShift() = timeshift;
}

std::string playingPath()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	return playing();
}

Result<void> playbackAllows(bool stop_playback)
{
	std::string now;
	bool timeshift = false;
	playingNow(now, timeshift);
	if (!now.empty() && !timeshift && !stop_playback)
		return fail(Status::Conflict, ErrorCode::PlaybackRunning, "something is playing in the movie player");
	return ok();
}

namespace internal
{

void setTimesForTest(long wait, long max_age)
{
	Held held;
	wait_ms = wait < 0 ? kWaitMs : wait;
	max_age_ms = max_age < 0 ? kMaxAgeMs : max_age;
}

void setRootProbeForTest(RootProbe p)
{
	Held held;
	root_probe = p;
}

void setReadingForTest(RootProbe p)
{
	Held held;
	reading_probe = p;
}

void setWritableForTest(Writable w)
{
	writable_check = w;
}

void setUnlinkForTest(Unlink u)
{
	unlink_call = u;
}

} // namespace internal

} // namespace archive
} // namespace coreapi
