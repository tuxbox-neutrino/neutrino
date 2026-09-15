/*
 * ep_storage.cpp - routes for mounts and files
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

#include "httpd/endpoints.h"

#include "httpd/auth.h"
#include "httpd/endpoint.h"
#include "httpd/http.h"
#include "httpd/json.h"
#include "httpd/schema.h"
#include "httpd/static.h"
#include "httpd/status.h"

#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"
#include "coreapi/storage.h"
#include "coreapi/base/types.h"

#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace httpd
{

namespace
{

/* THE ONE SCOPE THIS SERVER MINTS A TOKEN FOR, and the whole of what a token carrying it
   reaches.

   A NAME AND NOT A LIST OF DIRECTORIES. What a recording directory is called changes when
   a disk is added, when one is taken out and when somebody changes the setting, and a
   credential holding the answer to that would be a credential that is wrong from the next
   time it changes. The name is resolved again on every request, against the roots as they
   are at that moment.

   WHAT IT ADMITS. The media roots, which are where a box records and where the movie
   browser looks; and any root the live list holds that the shipped list does not, which is
   a directory this box was told to record into and nowhere else. What it therefore does
   not admit is the rest of the shipped list: the directory this server keeps its own
   configuration in, the one it writes temporary files to, and the ones a plugin is
   installed into. That is the difference the scope is for. A token good for those would
   be a token worth the box, and this one is meant to be carried in an address.

   The two spellings are compared as the elements they are written as, because a root
   beyond the shipped list is one an administrator put there and the answer for it is yes
   either way.

   The name itself is written once, below and outside this namespace, because the route
   that mints a token names the same scope and the document states it as the whole of what
   the member can be. */
const char kMediaRoots[] = "/media:/mnt:/hdd";

// One element of a colon separated list, compared whole. A comparison over a prefix would
// make /media name /mediaXYZ, which is the mistake the roots themselves are careful about.
bool namedIn(const std::string &one, const std::string &list)
{
	size_t i = 0;
	while (i <= list.size())
	{
		size_t e = list.find(':', i);
		if (e == std::string::npos)
			e = list.size();
		if (list.compare(i, e - i, one) == 0 && !one.empty())
			return true;
		i = e + 1;
	}
	return false;
}

/* Which of the roots this scope admits, worked out afresh for the request being answered.
   Empty for a scope this module does not know, which is every scope but the one above and
   is therefore a refusal: a credential naming a part of the box nothing here can resolve
   reaches no part of it. */
std::string scopeRoots(const std::string &scope)
{
	if (scope != mediaScopeName())
		return std::string();

	const std::string all = coreapi::storage::roots();
	const std::string shipped = coreapi::storage::defaultRoots();

	std::string admitted;
	size_t i = 0;
	while (i <= all.size())
	{
		size_t e = all.find(':', i);
		if (e == std::string::npos)
			e = all.size();
		const std::string one = all.substr(i, e - i);
		i = e + 1;
		if (one.empty())
			continue;
		if (!namedIn(one, kMediaRoots) && namedIn(one, shipped))
			continue;
		if (!admitted.empty())
			admitted += ":";
		admitted += one;
	}
	return admitted;
}

const char *fileKindName(coreapi::FileKind k)
{
	switch (k)
	{
		case coreapi::FileKind::Unknown:     return "unknown";
		case coreapi::FileKind::Fifo:        return "fifo";
		case coreapi::FileKind::CharDevice:  return "char-device";
		case coreapi::FileKind::Dir:         return "dir";
		case coreapi::FileKind::BlockDevice: return "block-device";
		case coreapi::FileKind::Regular:     return "regular";
		case coreapi::FileKind::Link:        return "link";
		case coreapi::FileKind::Socket:      return "socket";
		case coreapi::FileKind::Whiteout:    return "whiteout";
	}
	// Unreachable while the compiler holds the switch to the enumeration, an
	// unhandled enumerator being an error in this directory.
	return "unknown";
}

const FieldDesc kMountFields[] = {
	HTTPD_MEMBER("id", FieldType::String,
		"where the filesystem is mounted, which is what names it here"),
	HTTPD_MEMBER("device", FieldType::String, "what is mounted there"),
	HTTPD_MEMBER("fstype", FieldType::String, "what kind of filesystem it is"),
	HTTPD_MEMBER("total", FieldType::UInt,
		"how large it is in bytes, nought where the counters could not be read"),
	HTTPD_MEMBER("free", FieldType::UInt,
		"how much of it a recording could still use, which is less than what is unallocated because of the reserve"),
};

const Schema kMountSchema = { "mount", HTTPD_FIELDS(kMountFields) };

const FieldDesc kMountListFields[] = {
	HTTPD_LIST_OF("items", &kMountSchema, "every filesystem the kernel says is mounted"),
};

const Schema kMountListSchema = { "mount-list", HTTPD_FIELDS(kMountListFields) };

/* Everything from mode down is absent for a name the filesystem would not
   answer for, a broken link above all, and that is the one place in this
   server where the shape of an item follows what is in it.

   Written out as zeroes they would be read as a real owner, a real size and a
   moment at the epoch, which is a listing stating things nobody measured.
   attributes_read is what a reader tests before it looks for any of them. */
const FieldDesc kFileFields[] = {
	HTTPD_MEMBER("id", FieldType::String, "the name, as the directory holds it"),
	HTTPD_MEMBER_OF_SET("kind", "unknown,fifo,char-device,dir,block-device,regular,link,socket,whiteout",
		"what the directory says the name is, which for a link is a link and not what it points at"),
	HTTPD_MEMBER("attributes_read", FieldType::Bool,
		"whether the filesystem answered for the name, and so whether the members below are there at all"),
	HTTPD_MEMBER_OPTIONAL("mode", FieldType::UInt,
		"the permission bits and the kind, as the kernel keeps them"),
	HTTPD_MEMBER_OPTIONAL("nlink", FieldType::UInt, "how many names the same content has"),
	HTTPD_MEMBER_OPTIONAL("size", FieldType::UInt,
		"how large it is in bytes, and for a link how large what it points at is"),
	HTTPD_MEMBER_OPTIONAL("mtime", FieldType::Time,
		"when it was last written, seconds since the epoch"),
	HTTPD_MEMBER_OPTIONAL("uid", FieldType::UInt,
		"the owner, as the number the kernel gave and not as a name"),
	HTTPD_MEMBER_OPTIONAL("gid", FieldType::UInt, "the group, likewise"),
};

const Schema kFileSchema = { "file", HTTPD_FIELDS(kFileFields) };

const FieldDesc kFileListFields[] = {
	HTTPD_LIST_OF("items", &kFileSchema,
		"one entry per name the directory holds, in the order it gave them"),
};

const Schema kFileListSchema = { "file-list", HTTPD_FIELDS(kFileListFields) };

Response listMounts(const Request &)
{
	coreapi::Result<std::vector<coreapi::MountInfo> > got = coreapi::storage::mounts();
	if (!got.ok())
		return problemFor(got.error());

	const std::vector<coreapi::MountInfo> all = std::move(got).value();

	Response out = okJson();
	Json j(out.body, 32 + 128 * all.size());
	j.beginObject();
	j.key("items");
	j.beginArray();
	for (size_t i = 0; i < all.size(); ++i)
	{
		const coreapi::MountInfo &m = all[i];
		j.beginObject();
		j.key("id");
		j.value(m.mountpoint);
		j.key("device");
		j.value(m.device);
		j.key("fstype");
		j.value(m.fstype);
		j.key("total");
		j.value((unsigned long long) m.total);
		j.key("free");
		j.value((unsigned long long) m.free);
		j.endObject();
	}
	j.endArray();
	j.endObject();
	return out;
}

/* Where this box records and where its movie browser was pointed are settings and can
   name anywhere, so the roots are put together from them again before every one of the
   five routes below that reaches a name.

   Here rather than once when the server starts, because a directory entered in a menu has
   to work without the box being restarted. And on each of the five rather than in one
   place they share, because there is no such place: a handler is what the router calls. */
void confine()
{
	coreapi::storage::refreshRoots();
}

Response listFiles(const Request &r)
{
	confine();

	/* Not paged, unlike the channel list, and for the reason that list is: a
	   page needs a cursor that survives to the next call, and the only thing
	   here to write one over is the order the directory gave, which is not an
	   order the directory promises to give again. A cursor over it would put a
	   caller back somewhere it had already been, or past something it had not,
	   with nothing to see that it had. */
	coreapi::Result<std::vector<coreapi::FileEntry> > got =
		coreapi::storage::list(r.asString("path"));
	if (!got.ok())
		return problemFor(got.error());

	const std::vector<coreapi::FileEntry> all = std::move(got).value();

	Response out = okJson();
	Json j(out.body, 32 + 160 * all.size());
	j.beginObject();
	j.key("items");
	j.beginArray();
	for (size_t i = 0; i < all.size(); ++i)
	{
		const coreapi::FileEntry &e = all[i];
		j.beginObject();
		j.key("id");
		j.value(e.name);
		j.key("kind");
		j.value(fileKindName(e.kind));
		j.key("attributes_read");
		j.value(e.attributes_read);
		if (e.attributes_read)
		{
			j.key("mode");
			j.value((unsigned long) e.mode);
			j.key("nlink");
			j.value((unsigned long long) e.nlink);
			j.key("size");
			j.value((unsigned long long) e.size);
			j.key("mtime");
			j.value((long long) e.mtime);
			j.key("uid");
			j.value((unsigned long) e.uid);
			j.key("gid");
			j.value((unsigned long) e.gid);
		}
		j.endObject();
	}
	j.endArray();
	j.endObject();
	return out;
}

Response createDirectory(const Request &r)
{
	confine();

	/* Where the name is allowed to be is the layer below's question and not
	   asked twice: it decides containment before it decides existence, so a
	   name outside the roots and a name inside them whose parent is missing
	   come back as two different refusals and neither says what is on the box
	   elsewhere. */
	coreapi::Result<void> done = coreapi::storage::createDirectory(r.asString("path"));
	if (!done.ok())
		return problemFor(done.error());

	/* Made, and not merely asked for. This one is a call to the filesystem that
	   answered before this line, unlike the commands that go to the box's own
	   loop, so it says the directory is there rather than that something was
	   asked to make it. */
	return created();
}

Response removePath(const Request &r)
{
	confine();

	// One regular file, or one directory with nothing in it. A directory that
	// still holds something is a conflict below rather than something emptied
	// here, and a link is removed itself rather than what it points at.
	coreapi::Result<void> done = coreapi::storage::removePath(r.asString("path"));
	if (!done.ok())
		return problemFor(done.error());
	return noContent();
}


/* The last part of a name, which is what the file is called.

   The caller's own spelling and not what the name resolved to. What it asked
   for is what it expects back, and the resolved one is where a link inside the
   roots leads, which is nothing a caller is owed and nothing it can act on. */
std::string leafOf(const std::string &path)
{
	const size_t slash = path.rfind('/');
	return (slash == std::string::npos) ? path : path.substr(slash + 1);
}

bool endsWithNoCase(const std::string &name, const char *suffix)
{
	const size_t n = std::strlen(suffix);
	if (name.size() < n)
		return false;
	for (size_t i = 0; i < n; ++i)
	{
		char c = name[name.size() - n + i];
		if (c >= 'A' && c <= 'Z')
			c = (char) (c - 'A' + 'a');
		if (c != suffix[i])
			return false;
	}
	return true;
}

/* What a file on the box's disks goes out as.

   THE STORED FORM IS ANSWERED FIRST AND THAT IS THE WHOLE REASON THIS IS NOT THE TABLE
   THE PAGES DIRECTORY USES. That one takes a trailing .gz off the name before it reads
   the extension, because a page kept compressed is served as what it holds with a header
   saying how it was packed. A file on a disk is handed over as the bytes it is, and a
   name ending in .gz answered as what is inside it would state a type for bytes the
   caller cannot read that way.

   What follows is what a box's own disks hold and the pages directory does not:
   recordings, the music and pictures a player reads, and the plain text of a
   configuration file. Everything else falls through to the one table this server already
   has. */
const char *storedContentType(const std::string &name)
{
	if (endsWithNoCase(name, ".gz") || endsWithNoCase(name, ".tgz"))
		return "application/gzip";

	struct Row
	{
		const char *suffix;
		const char *type;
	};
	static const Row table[] =
	{
		{ ".ts",    "video/mp2t" },
		{ ".m2ts",  "video/mp2t" },
		{ ".mpg",   "video/mpeg" },
		{ ".mpeg",  "video/mpeg" },
		{ ".vob",   "video/mpeg" },
		{ ".mp4",   "video/mp4" },
		{ ".m4v",   "video/mp4" },
		{ ".mkv",   "video/x-matroska" },
		{ ".avi",   "video/x-msvideo" },
		{ ".webm",  "video/webm" },
		{ ".mov",   "video/quicktime" },
		{ ".mp3",   "audio/mpeg" },
		{ ".m4a",   "audio/mp4" },
		{ ".aac",   "audio/aac" },
		{ ".flac",  "audio/flac" },
		{ ".ogg",   "audio/ogg" },
		{ ".wav",   "audio/wav" },
		{ ".m3u",   "audio/x-mpegurl" },
		{ ".m3u8",  "audio/x-mpegurl" },
		{ ".pdf",   "application/pdf" },
		{ ".zip",   "application/zip" },
		{ ".tar",   "application/x-tar" },
		{ ".conf",  "text/plain" },
		{ ".cfg",   "text/plain" },
		{ ".log",   "text/plain" },
		{ ".list",  "text/plain" }
	};

	for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); ++i)
	{
		if (endsWithNoCase(name, table[i].suffix))
			return table[i].type;
	}
	return contentTypeFor(name);
}

/* The bytes of one file, sent out of the descriptor the layer below opened and
   never read into this process on the way. A recording runs to gigabytes and a
   copy of one in a response body would be that many bytes of a box that has a
   hundred and twenty eight megabytes.

   Nothing here says anything about ranges. The transport answers one stretch of
   any file backed answer where the caller asked for one, and it does that off
   the descriptor's own length, which it has and this does not. */
Response readFile(const Request &r)
{
	confine();

	const std::string &path = r.asString("path");

	/* A CREDENTIAL THAT STANDS FOR PART OF THIS BOX IS HELD TO THAT PART HERE, and this
	   is the only route that is asked to. A caller whose credential stands for the whole
	   of what its level reaches carries no scope and this says nothing about it, which is
	   every caller that arrived on a session or on a token a configuration names.

	   Before the file is opened rather than after, so a name the credential does not
	   reach is turned down without this process having opened it.

	   The sentence is the one the gate uses and not one of its own. It names no
	   directory, so a caller cannot read the box's recording directories out of a
	   refusal, and it does not say whether the name is there, so it cannot be used to
	   ask what exists elsewhere either. */
	if (!r.scope().empty() && !coreapi::storage::inside(path, scopeRoots(r.scope())))
		return problemResponse(StatusForbidden, coreapi::ErrorCode::NotPermitted,
				       notPermittedDetail());

	coreapi::Result<int> opened = coreapi::storage::openFile(path);
	if (!opened.ok())
		return problemFor(opened.error());

	Response out;
	out.code = StatusOk;
	out.content_type = storedContentType(path);
	// The descriptor is taken over whichever way this answers, so there is
	// nothing left here to give back.
	if (!answerFromDescriptor(out, opened.value()))
		return problemResponse(StatusNotFound, coreapi::ErrorCode::NoSuchName,
				       "there is nothing to read under that name");

	out.headers.push_back(std::make_pair(std::string("Content-Disposition"),
					     dispositionFor(leafOf(path))));
	return out;
}

/* One file, put where the caller named. The body is the file and nothing else: no form,
   no wrapper and no encoding, so the bytes that arrive are the bytes that are written.

   HOW LARGE ONE MAY BE IS THE TRANSPORT'S CEILING ON A BODY AND NOT A NUMBER OF ITS OWN.
   A body reaches a handler whole, so the whole of it is in this process before this line
   runs, and a second, larger ceiling here would be this route asking the box to hold more
   memory than the box was configured to hold. What that buys is a route for the files a
   box is actually given through a browser, which are its configuration, its lists and its
   pictures; a firmware image and a recording go the other way, and the route above sends
   those without holding any of one.

   WHAT A TRANSFER THAT BREAKS OFF LEAVES. Nothing at all, in either of the two places it
   can break. A body that stops early never reaches this line, because the request is not
   whole and no handler is entered; and a write that fails partway leaves the file that
   was there, because the layer below writes beside it and puts what it wrote in its place
   in one step. */
Response writeFile(const Request &r)
{
	confine();

	coreapi::Result<coreapi::storage::Wrote> done =
		coreapi::storage::writeFile(r.asString("path"), r.body(),
					    r.has("overwrite") && r.asBool("overwrite"));
	if (!done.ok())
		return problemFor(done.error());

	/* Made, and not merely asked for, like the directory beside it: this is a
	   call to the filesystem that answered before this line. The two answers
	   are told apart because a caller replacing a file it knew about and a
	   caller putting one where nothing was are doing different things, and an
	   answer that said the same to both would tell the second nothing about
	   whether the name was free. */
	if (std::move(done).value() == coreapi::storage::Wrote::Created)
		return created();
	return noContent();
}

const Param kFileParams[] = {
	/* No ceiling of its own on the length. What one is depends on the system the name is
	   opened on, so it is asked where the name is opened. The server's own ceiling on any
	   value still applies.

	   Where the name is allowed to be is likewise not asked here. The layer below decides
	   containment before it decides existence, so a name outside the roots and a name
	   inside them that is not there come back as two different refusals and neither says
	   what is on the box elsewhere. */
	HTTPD_QUERY_REQUIRED("path", ParamType::String,
		"the directory to list, which has to be inside the roots"),
};

const Param kMakeParams[] = {
	// No ceiling of its own on the length, for the reason the listing's own
	// name has none: what one is depends on the system the name is made on.
	HTTPD_BODY_REQUIRED("path", ParamType::String,
		"the directory to make, whose parent has to exist inside the roots"),
};

/* The name is in the query and not in a body, which is where the check over
   these tables puts it: what sits between a caller and this box drops or
   rewrites a body on a removal often enough that a route depending on one would
   work from some networks and not from others. */
const Param kRemoveParams[] = {
	HTTPD_QUERY_REQUIRED("path", ParamType::String,
		"the file or the empty directory to remove, inside the roots"),
};

const Param kReadParams[] = {
	// No ceiling of its own on the length, for the reason the listing's name
	// has none: what one is depends on the system the name is opened on.
	HTTPD_QUERY_REQUIRED("path", ParamType::String,
		"the file to read, which has to be inside the roots"),
};

/* The two named values are in the query and neither is in the body, because
   the body is the file. A route whose body carries the bytes has nowhere else
   to put what it is told about them.

   The body is declared all the same, as the third row here. Without it the
   document said this route takes no body, which is not what the handler does
   and left a caller with nothing to write against for the one thing the route
   exists to carry. */
const Param kWriteParams[] = {
	HTTPD_QUERY_REQUIRED("path", ParamType::String,
		"where to put the file, whose directory has to exist inside the roots"),
	/* Absent reads as no, so a caller that says nothing does not replace
	   anything. The direction to be wrong in: a caller that meant to replace a
	   file says so and asks again, and one that did not is not the reason a
	   file went away. */
	HTTPD_QUERY("overwrite", ParamType::Bool, "whether a name that is already taken may be replaced"),
	/* No floor and no ceiling of its own. A file of no bytes is a file, and
	   how large one may be is the filesystem's answer and this server's, not
	   a number this table could state for every box. */
	HTTPD_BODY_IS_BYTES("file", "the bytes of the file, sent as they are and not wrapped in anything", 0, 0),
};

const Endpoint kStorageEndpoints[] = {
	{ Method::Get, "/api/v1/storage/mounts", AuthLevel::Read,
	  "the filesystems the box has mounted and how much room each has",
	  NULL, 0, &kMountListSchema, &listMounts, false },
	/* System and not Read, which is where the level table puts the filesystem and is a
	   level higher than the two reads above it.

	   What it hands back is the names, the modes, the owners and the sizes of whatever is
	   under the roots, and the roots include the directory this server's own configuration
	   lives in. A caller granted a read by its address alone is granted that read for the
	   channel list and the guide, and the difference between knowing what is on television
	   and knowing what is on the box's disks is the difference this level exists to
	   draw. */
	{ Method::Get, "/api/v1/storage/files", AuthLevel::System,
	  "what one directory inside the roots holds",
	  HTTPD_PARAMS(kFileParams), &kFileListSchema, &listFiles, false },
	/* Both of the two below are System and not Write. What they reach is the
	   filesystem the box records to, a removal there is not something a later
	   request can undo, and the roots the layer below confines them to are only
	   worth what the account this server runs as cannot reach around them. */
	{ Method::Post, "/api/v1/storage/directory", AuthLevel::System,
	  "makes one directory inside the roots",
	  HTTPD_PARAMS(kMakeParams), NULL, &createDirectory, false },
	{ Method::Delete, "/api/v1/storage/path", AuthLevel::System,
	  "removes one file, or one directory with nothing in it",
	  HTTPD_PARAMS(kRemoveParams), NULL, &removePath, false },
	/* System like the listing above, and for more reason than that one has.
	   What the listing hands back is the names, the modes and the sizes of
	   what is under the roots; this one hands back the bytes, and the roots
	   hold the directory this server keeps its own configuration in. The
	   server the new one replaces could be asked for any file on the box by
	   anybody who could reach the port, which is the defect this must not
	   carry forward, and a level is the half of not carrying it that a caller
	   meets first. */
	/* THE ONE ROUTE ON THIS SERVER THAT TAKES A CREDENTIAL OUT OF AN ADDRESS, which is
	   the last field of the row.

	   What it is for: a recording is watched in a media player, a media player is given
	   an address and nothing else, and every other way of presenting a credential here is
	   a header or a cookie such a program has no way of setting. Without this the address
	   this box shows beside a recording is an address that answers 403 to everybody who
	   pastes it.

	   What it costs, and why it is this route and no other. A token in an address outlives
	   the request: it is in a history list, in whatever the address was pasted into, and
	   in the log of anything the request passed through. So the one that may travel that
	   way is worth a part of this box and not the whole of it, readFile above holds it to
	   that part, and it runs out on its own. The route beside this one that writes a file
	   and the one that removes a name are not written to read a scope and do not get the
	   flag: nothing a leaked address can do should be a thing this box cannot undo. */
	{ Method::Get, "/api/v1/storage/file", AuthLevel::System,
	  "the bytes of one file inside the roots",
	  HTTPD_PARAMS(kReadParams), NULL, &readFile, true },
	/* The other half of the old defect, and the worse half: that server could
	   be made to write any file on the box as well. Same level, same roots,
	   and the body is the file's own bytes rather than a document naming
	   anything. */
	{ Method::Put, "/api/v1/storage/file", AuthLevel::System,
	  "puts one file inside the roots, as the bytes of the body",
	  HTTPD_PARAMS(kWriteParams), NULL, &writeFile, false },
};

} // namespace

/* What the file is called, as a header a file name cannot write into.

   A NAME IS BYTES AND A HEADER IS A LINE. A file name may hold a carriage return, a
   quotation mark and a byte of any value, and every one of those written into a header
   straight would end the line early and let whoever named the file write headers of their
   own into an answer this server sends. So nothing from the name reaches the header as
   itself: the bytes that are allowed through are named one by one, and every other byte
   becomes three characters that cannot be any of them. What is refused is worked out from
   what is permitted, so a byte nobody thought of is encoded rather than passed.

   Only the encoded spelling, and not the plain one beside it that the older convention
   has. Two spellings of one name in one header are two things that can disagree, and this
   one is understood by everything that has read a header in the last fifteen years.

   Attachment and never inline, whatever the type says. What a caller may write into these
   directories is anything at all, a page that would run in this server's own origin among
   it, and a browser told to display such a file displays it as this box. A player reading
   the file through a media element is not stopped by this: that is not display.

   Outside the anonymous namespace, and declared in endpoints.h, because the routes that
   hand a playlist over name their file the same way. Two writers of one header is how a
   name escaped through one of them and not the other. */
std::string dispositionFor(const std::string &name)
{
	static const char kHex[] = "0123456789ABCDEF";

	std::string out = "attachment; filename*=UTF-8''";
	for (size_t i = 0; i < name.size(); ++i)
	{
		const unsigned char c = (unsigned char) name[i];
		const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
				   (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_';
		if (plain)
		{
			out += (char) c;
			continue;
		}
		out += '%';
		out += kHex[c >> 4];
		out += kHex[c & 0x0f];
	}
	return out;
}

/* The one scope a token may be minted for, written out here once.

   Outside the anonymous namespace, and declared in endpoints.h, because three places say
   it: this module resolves it against the roots, the route that mints a token stamps it,
   and the document states it as the whole of what that answer's member can be. Three
   spellings of one name is a token minted for a scope nothing resolves. */
const char *mediaScopeName()
{
	return "media";
}

extern const RouteTable storageTable = {
	HTTPD_TABLE("storage", kStorageEndpoints)
};

} // namespace httpd
