/*
 * archive.cpp - the finished recordings on the record disk
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

#include "coreapi/channels.h"
#include "coreapi/recordings.h"
#include "coreapi/settings/settings.h"
#include "coreapi/base/deps.h"
#include "coreapi/base/errors.h"

#include <neutrinoMessages.h>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>
#include <openssl/sha.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
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
	std::string rel;
	std::string path;
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

std::string idOf(const std::string &rel)
{
	unsigned char d[SHA256_DIGEST_LENGTH];
	SHA256((const unsigned char *) rel.data(), rel.size(), d);
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

// The movie browser's order; a link or anything but a plain file inside the record directory is no cover.
std::string coverOf(const std::string &ts_path)
{
	static const char *const kExts[] = { ".jpg", ".png", ".gif", ".jpeg", ".bmp" };
	const std::string root = recordDirectory();
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

// Depth 0 is the record directory, depth 1 one folder below it; links are never followed.
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

std::vector<Found> scan()
{
	std::vector<Found> out;
	const std::string root = recordDirectory();
	if (!root.empty())
		scanDir(root, std::string(), 0, out);
	return out;
}

Entry entryOf(const Found &f)
{
	const std::string xml = metadataOf(stemOf(f.path) + ".xml");
	Entry e;
	e.id = idOf(f.rel);
	e.title = bounded(tagText(xml, "epgtitle"), kMaxShortText);
	e.channel = bounded(tagText(xml, "channelname"), kMaxShortText);
	e.channel_id = (ChannelId) std::strtoull(tagText(xml, "id").c_str(), NULL, 10);
	const unsigned long minutes = numberUpTo(xml, "length", kMaxDurationMinutes);
	e.duration = (long) (minutes * 60);
	e.start = f.st.st_mtime - e.duration;
	e.size = (uint64_t) f.st.st_size;
	e.path = f.path;
	e.playing = sameFile(e.path, playingPath());
	return e;
}

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

std::string recordDirectory()
{
	Result<std::string> d = settings::get("network_nfs_recordingdir");
	if (!d.ok() || d.value().empty() || d.value()[0] != '/')
		return std::string();
	char real[PATH_MAX];
	if (realpath(d.value().c_str(), real) == NULL)
		return std::string();
	return std::string(real);
}

bool descendingByDefault(SortKey key)
{
	return key == SortKey::Start || key == SortKey::Duration || key == SortKey::Size;
}

Result<Page> list(const std::string &title_part, size_t offset, size_t limit, const Sort &sort)
{
	if (limit > kMaxLimit)
		limit = kMaxLimit;
	const std::string want = folded(title_part);
	const std::vector<Found> all = scan();
	std::vector<Entry> kept;
	for (size_t i = 0; i < all.size(); ++i)
	{
		Entry e = entryOf(all[i]);
		if (want.empty() || folded(e.title).find(want) != std::string::npos)
			kept.push_back(e);
	}
	std::sort(kept.begin(), kept.end(), Before(sort));
	Page p;
	p.total = kept.size();
	for (size_t i = offset; i < kept.size() && p.items.size() < limit; ++i)
		p.items.push_back(kept[i]);
	return ok(p);
}

Result<Entry> entryAt(const std::string &path)
{
	const Result<Entry> none = fail<Entry>(Status::NotFound, ErrorCode::NoSuchRecording, "no recording has that path");
	const std::string root = recordDirectory();
	const size_t cut = path.rfind('/');
	if (root.empty() || cut == std::string::npos || cut == 0)
		return none;
	// The root may be spelled through a link; the leaf and its one folder may not, as in scanDir.
	struct stat st;
	const std::string folder = path.substr(0, cut);
	char real[PATH_MAX];
	if (lstat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode) || realpath(folder.c_str(), real) == NULL)
		return none;
	Found f;
	f.rel = path.substr(cut + 1);
	if (std::string(real) != root)
	{
		struct stat dir;
		const size_t up = folder.rfind('/');
		if (up == std::string::npos || up == 0 || lstat(folder.c_str(), &dir) != 0 || !S_ISDIR(dir.st_mode) ||
		    realpath(folder.substr(0, up).c_str(), real) == NULL || std::string(real) != root)
			return none;
		const std::string name = folder.substr(up + 1);
		if (name.empty() || name[0] == '.')
			return none;
		f.rel = name + "/" + f.rel;
	}
	struct stat meta;
	const std::string leaf = path.substr(cut + 1);
	f.path = root + "/" + f.rel;
	f.st = st;
	if (leaf.empty() || leaf[0] == '.' || !endsWith(leaf, ".ts") || !isPlainFile(stemOf(f.path) + ".xml", &meta))
		return none;
	return ok(entryOf(f));
}

Result<Entry> find(const std::string &id)
{
	if (id.size() != 16 || id.find_first_not_of("0123456789abcdef") != std::string::npos)
		return fail<Entry>(Status::NotFound, ErrorCode::NoSuchRecording, "no recording has that id");
	const std::vector<Found> all = scan();
	for (size_t i = 0; i < all.size(); ++i)
	{
		if (idOf(all[i].rel) == id)
			return ok(entryOf(all[i]));
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

	if (unlink(path.c_str()) != 0)
		return fail(Status::Internal, ErrorCode::ChangeRefused, "the recording could not be removed");
	const std::string stem = stemOf(path);
	unlink((stem + ".xml").c_str());
	unlink((stem + ".jpg").c_str());
	unlink((stem + ".png").c_str());
	unlink((stem + ".gif").c_str());
	unlink((stem + ".jpeg").c_str());
	unlink((stem + ".bmp").c_str());
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
	d.cover = !coverOf(d.entry.path).empty();
	return ok(d);
}

Result<std::string> coverPath(const std::string &id)
{
	Result<Entry> e = find(id);
	if (!e.ok())
		return fail(e.error());
	const std::string cover = coverOf(e.value().path);
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

} // namespace archive
} // namespace coreapi
