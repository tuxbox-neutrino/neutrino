/*
 * archive.h - the finished recordings on the record disk
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

#ifndef __coreapi_archive_h__
#define __coreapi_archive_h__

#include "coreapi/base/result.h"
#include "coreapi/base/types.h"

#include <stdint.h>
#include <time.h>

#include <string>
#include <vector>

namespace coreapi
{
namespace archive
{

// One finished recording: a .ts in the record directory with its .xml beside it.
struct Entry
{
	std::string id;          // 16 lowercase hex digits
	std::string title;
	std::string channel;
	ChannelId   channel_id;  // 0 when the metadata names none
	time_t      start;
	long        duration;    // seconds, 0 when unknown
	uint64_t    size;
	bool        playing;
	std::string path;        // whole name of the .ts, never sent by a list route
};

struct Page
{
	std::vector<Entry> items;
	size_t             total;
};

const size_t kDefaultLimit = 15;
const size_t kMaxLimit = 50;

std::string recordDirectory();

enum class SortKey
{
	Start,
	Title,
	Channel,
	Duration,
	Size
};

// Equal keys fall back to the id, ascending, in either order.
struct Sort
{
	SortKey key;
	bool    descending;

	Sort() : key(SortKey::Start), descending(true) {}
	Sort(SortKey k, bool d) : key(k), descending(d) {}
};

// Start, duration and size descending; title and channel ascending.
bool descendingByDefault(SortKey key);

Result<Page> list(const std::string &title_part, size_t offset, size_t limit, const Sort &sort = Sort());

Result<Entry> find(const std::string &id);

// The entry one path names, held to the rules a list keeps, without a scan.
Result<Entry> entryAt(const std::string &path);

// Texts past these are cut at a character boundary.
const size_t kMaxShortText = 1024;
const size_t kMaxLongText = 16 * 1024;
const size_t kMaxAudioTracks = 16;
// A recording longer than this is treated as a metadata fault rather than a duration.
const unsigned long kMaxDurationMinutes = 7 * 24 * 60;
// The movie browser's age for a recording never to be shown without the PIN.
const unsigned long kAlwaysLocked = 99;

// What the metadata says beyond the list; an empty text or a 0 is not stated.
struct Details
{
	Entry                    entry;
	std::string              description;       // info1
	std::string              long_description;  // info2
	unsigned long            genre;             // genremajor, the guide's content byte
	unsigned long            genre_minor;
	std::string              series;
	std::string              country;
	unsigned long            year;
	unsigned long            rating;            // tenths, 81 for 8.1
	unsigned long            quality;           // stars, 0 to 3
	unsigned long            age;               // up to 18, or kAlwaysLocked
	std::vector<std::string> audio;             // track names
	bool                     cover;
};

Result<Details> details(const std::string &id);

// The picture of the same name beside the stream, in the order the movie browser looks.
Result<std::string> coverPath(const std::string &id);
Result<void> remove(const std::string &id);
// Order: no-such-recording, playback-running unless stop_playback, recording-playing
// during a timeshift, mode-unavailable while the box starts, then box-in-standby
// unless wake.
Result<void> play(const std::string &id, bool wake, bool stop_playback);

// What a play message carries: the path, and whether it may end a file playing.
struct PlayAsked
{
	std::string path;
	bool        stop_playback;
};
PlayAsked playAsked(const char *payload);

// The movie player's own note; timeshift marks the shift the box keeps rather than a file.
void notePlaying(const std::string &path, bool timeshift = false);
std::string playingPath();

// Conflict while the movie player plays a file and stop_playback is not set. A web
// channel plays in another player, which never notes, and a timeshift is no file.
Result<void> playbackAllows(bool stop_playback);

} // namespace archive
} // namespace coreapi

#endif
