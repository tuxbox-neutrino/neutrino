/*
 * websources.h - where the box takes its web channels and its XMLTV guide from
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#ifndef __coreapi_websources_h__
#define __coreapi_websources_h__

#include "coreapi/base/result.h"

#include <stddef.h>

#include <string>
#include <vector>

namespace coreapi
{
namespace websources
{

/* The three lists the box reads web content from, each in the order the box
   reads it: the web TV channel lists and the web radio channel lists (XML or
   M3U files), and the XMLTV guides. An entry is an address on the web or a file
   on the box. */
enum Kind
{
	WebTv,
	WebRadio,
	XmlTv
};

struct Lists
{
	std::vector<std::string> webtv;
	std::vector<std::string> webradio;
	std::vector<std::string> xmltv;
};

// More than any box reads; a longer list or a longer entry is refused.
const size_t kMaxSources = 32;
const size_t kMaxSourceBytes = 1024;
// The longest guide address the guide daemon takes; it drops a longer one unread.
const size_t kMaxGuideBytes = 100;

/* What the box reads now, and where a write it has not carried yet stands, what
   that write named: a reader after a write sees the write. NotSupported where the
   program installed no source. */
Result<Lists> read();

/* Replaces one list. Each entry is an http:// or https:// address or an absolute
   path, holds no control byte, no number sign (the settings file's reader cuts
   a value off there) and no more than kMaxSourceBytes (kMaxGuideBytes for a
   guide), and a list holds no more than kMaxSources. InvalidArgument BadString naming the entry for
   anything else, and nothing is written.

   Carried by the program's own loop, which saves the settings with it; reload
   then reads the channel lists again, and the guide where the XMLTV list was
   written, which is what the box's own screen does after such a change.
   Internal where the loop could not be asked. */
Result<void> write(Kind kind, const std::vector<std::string> &entries, bool reload);

/* The judgement write makes before it writes anything, for a caller replacing
   more than one list that has to know all of them pass first. */
Result<void> check(Kind kind, const std::vector<std::string> &entries);

// Where the box keeps channel lists of its own, which an upload is put in.
std::string webtvDir();
std::string webradioDir();

/* What holds the lists: the program's settings on a box, something else in a
   case. Read and written from request threads. */
struct Source
{
	virtual ~Source() {}
	virtual Status read(Lists &out) const = 0;
	virtual Status write(Kind kind, const std::vector<std::string> &entries, bool reload) = 0;
	// On the loop's thread, for a source that holds writes until then.
	virtual void carry() {}
	// On the loop's thread, after the program changed the lists itself.
	virtual void refresh() {}
};

// NULL takes it away again; nothing installed answers NotSupported.
void setSource(Source *s);

/* Binds the lists to the program's settings and the save beside them, and
   carries what was written on the loop's own thread (applyPending). Defined in
   the part of this layer that reaches the program (box/websources_real.cpp). */
void installRealSource(bool (*save)());

/* On the loop's thread: what was written since the last time, then the save,
   then the reloads asked for. A source that holds nothing does nothing. */
void applyPending();

/* On the loop's thread, wherever the program changed the lists itself: its
   screens and a reload of its settings. What a request reads is a copy the loop
   publishes, never the lists themselves, which the loop rebuilds without a lock
   a request could take. */
void refresh();

} // namespace websources
} // namespace coreapi

#endif
