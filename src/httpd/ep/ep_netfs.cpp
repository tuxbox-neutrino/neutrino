/*
 * ep_netfs.cpp - routes for the network filesystems the box mounts
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

#include "httpd/endpoint.h"
#include "httpd/http.h"
#include "httpd/json.h"
#include "httpd/schema.h"
#include "httpd/status.h"

#include "coreapi/base/errors.h"
#include "coreapi/netfs.h"
#include "coreapi/base/result.h"

#include <cstddef>
#include <string>
#include <utility>

/* The two files the box keeps its network filesystems in, as slots and not as
   file text.

   WHY THESE ARE ROUTES AT ALL, when this server already offers a route that
   reads any file under the roots and one that writes any of them. Because what
   is in those two files is a table with a shape, and a page editing raw text
   would put the whole of that shape in the page: where a password goes on a
   line, what makes an entry not mount at start up, which of the two ways of
   naming a server belongs to which kind. Every one of those is written down
   twice the moment it is written in a page, and the copy in the page is the one
   nobody holds to the file. The file also holds a password in clear, which is
   the one thing the file routes must not hand over and cannot avoid handing
   over when what they hand over is the file.

   ALL OF THEM System, the read included. What a read answers is the address of
   somebody's storage on their own network, the share names on it and the name
   they log in to it under, which is the shape of a private network and not the
   kind of thing the read exemption is for. The routes that list and read a file
   on this box's discs are System for the same reason, and these two files are
   under those roots.

   THE ACTS ARE UNDER THE TABLE THEY ACT ON. Mounting everything is a thing the
   first file has, because those are the mounts made at start up and kept, and
   reloading is a thing the second has, because that file is read by a daemon
   that only reads it when it starts. Neither act means anything for the other
   table, so neither is offered under a name that would take one. */

namespace httpd
{

namespace
{

// The two names the segment takes and the one reading of them, so that the set
// written in the table and the branch that reads it cannot come apart.
const char kTableNames[] = "fstab,automount";

const char kTableValueDocs[] =
	"fstab: the file mounted at start up and kept, /var/etc/fstab\n"
	"automount: the file the automounter reads when it starts, /var/etc/auto.net";

const char kNetfsTypeDocs[] =
	"nfs: a Network File System export\n"
	"cifs: a Windows or Samba share";

coreapi::netfs::Table tableOf(const Request &r)
{
	return (r.asString("table") == "automount") ? coreapi::netfs::Table::Automount
						    : coreapi::netfs::Table::Fstab;
}

const char *tableName(coreapi::netfs::Table t)
{
	return (t == coreapi::netfs::Table::Automount) ? "automount" : "fstab";
}

const char *kindName(coreapi::netfs::Kind k)
{
	return (k == coreapi::netfs::Kind::Cifs) ? "cifs" : "nfs";
}

const FieldDesc kEntryFields[] = {
	HTTPD_MEMBER("slot", FieldType::UInt,
		"which of the 8 the entry is, counted from 0; it is what names it and what a write addresses, and it does not move when another entry is changed"),
	HTTPD_MEMBER("active", FieldType::Bool,
		"whether the box mounts it: at start up for the first table, and when somebody looks at it for the second"),
	HTTPD_MEMBER_OF_SET("type", "nfs,cifs", "what filesystem protocol the other end speaks", kNetfsTypeDocs),
	HTTPD_MEMBER("host", FieldType::String,
		"the remote server this entry mounts from, as an address or a host name"),
	HTTPD_MEMBER("remote_dir", FieldType::String,
		"what that server offers: the export for one kind and the share for the other, with the separator it is written with"),
	HTTPD_MEMBER("local_dir", FieldType::String,
		"where it lands: a path on this box for the first table, and a name the automounter offers under /mnt/autofs for the second"),
	HTTPD_MEMBER("user", FieldType::String, "the name it logs in under, and empty where it needs none"),
	HTTPD_MEMBER("has_password", FieldType::Bool,
		"whether a password is set at all, which is the whole of what is said about it: the file holds it in clear because the kernel takes it no other way, and it never leaves the box through this"),
	HTTPD_MEMBER("options", FieldType::String,
		"everything else the mount is given, comma separated; whether it is automatic and the credential are the members above and are not in here"),
};

const Schema kEntrySchema = { "netfs-entry", HTTPD_FIELDS(kEntryFields) };

const FieldDesc kSlotsFields[] = {
	HTTPD_MEMBER_OF_SET("table", kTableNames,
		"which of the 2 network filesystem files this table reflects", kTableValueDocs),
	HTTPD_MEMBER("path", FieldType::String,
		"the absolute path of that file on this box's filesystem"),
	HTTPD_LIST_OF("items", &kEntrySchema,
		"all 8 slots in order, the empty ones among them, because what a write addresses is a slot and an empty slot is one to fill in"),
	HTTPD_MEMBER("unreadable_lines", FieldType::UInt,
		"how many lines of the file were meant to be entries and could not be read; the entries beside them are still here, and a write keeps those and not these"),
};

const Schema kSlotsSchema = { "netfs-table", HTTPD_FIELDS(kSlotsFields) };

const FieldDesc kRanFields[] = {
	HTTPD_MEMBER("output", FieldType::String,
		"what the box's own script wrote while it ran, as it wrote it"),
};

const Schema kRanSchema = { "netfs-run", HTTPD_FIELDS(kRanFields) };

void writeSlots(Json &j, coreapi::netfs::Table t, const coreapi::netfs::Slots &s)
{
	j.beginObject();
	j.key("table");
	j.value(tableName(t));
	j.key("path");
	j.value(coreapi::netfs::pathOf(t));
	j.key("items");
	j.beginArray();
	for (size_t i = 0; i < s.items.size(); ++i)
	{
		const coreapi::netfs::Entry &e = s.items[i];
		j.beginObject();
		j.key("slot");
		j.value((unsigned long) i);
		j.key("active");
		j.value(e.active);
		j.key("type");
		j.value(kindName(e.kind));
		j.key("host");
		j.value(e.host);
		j.key("remote_dir");
		j.value(e.remote_dir);
		j.key("local_dir");
		j.value(e.local_dir);
		j.key("user");
		j.value(e.username);
		/* Whether one is set and never which one. The layer below hands this
		   answer up with the value already taken out, so there is nothing here
		   to forget to leave behind. */
		j.key("has_password");
		j.value(e.has_password);
		j.key("options");
		j.value(e.options);
		j.endObject();
	}
	j.endArray();
	j.key("unreadable_lines");
	j.value((unsigned long) s.unreadable_lines);
	j.endObject();
}

// The whole table, which is what both the read and either write answer with: a
// write rewrites the file, so what a caller has to be able to read afterwards
// is the file and not the one slot it named.
Response answerWith(coreapi::netfs::Table t)
{
	coreapi::Result<coreapi::netfs::Slots> got = coreapi::netfs::read(t);
	if (!got.ok())
		return problemFor(got.error());

	const coreapi::netfs::Slots all = std::move(got).value();

	Response out = okJson();
	Json j(out.body, 128 + 224 * all.items.size());
	writeSlots(j, t, all);
	return out;
}

Response listEntries(const Request &r)
{
	return answerWith(tableOf(r));
}

Response writeEntry(const Request &r)
{
	const coreapi::netfs::Table t = tableOf(r);

	coreapi::netfs::Entry e;
	e.active = r.asBool("active");
	e.kind = (r.asString("type") == "cifs") ? coreapi::netfs::Kind::Cifs
						: coreapi::netfs::Kind::Nfs;
	e.host = r.asString("host");
	e.remote_dir = r.asString("remote_dir");
	e.local_dir = r.asString("local_dir");
	if (r.has("user"))
		e.username = r.asString("user");
	if (r.has("options"))
		e.options = r.asString("options");
	/* A member left out means the password stays, and there is no other reading
	   available: a read answers nothing for it, so a form that redraws itself
	   from what it read has nothing to send back, and taking that nothing would
	   wipe the value the read protected. Clearing one is done by emptying the
	   whole slot, which is the act beside this. */
	e.change_password = r.has("password");
	if (e.change_password)
		e.password = r.asString("password");

	coreapi::Result<void> done = coreapi::netfs::write(t, (size_t) r.asUInt("slot"), e);
	if (!done.ok())
		return problemFor(done.error());
	return answerWith(t);
}

Response clearEntry(const Request &r)
{
	const coreapi::netfs::Table t = tableOf(r);

	coreapi::Result<void> done = coreapi::netfs::clear(t, (size_t) r.asUInt("slot"));
	if (!done.ok())
		return problemFor(done.error());
	return answerWith(t);
}

// What the box's own script wrote, handed back whole. It is the only thing
// there is to say: what came of the mounting is read off what is mounted, which
// is the route beside these, and the script itself answers in lines meant for a
// person.
Response answerWithOutput(coreapi::Result<std::string> ran)
{
	if (!ran.ok())
		return problemFor(ran.error());

	const std::string said = std::move(ran).value();

	Response out = okJson();
	Json j(out.body, 64 + said.size() + said.size() / 4);
	j.beginObject();
	j.key("output");
	j.value(said);
	j.endObject();
	return out;
}

Response mountAll(const Request &)
{
	return answerWithOutput(coreapi::netfs::mountAll());
}

Response unmountAll(const Request &)
{
	return answerWithOutput(coreapi::netfs::unmountAll());
}

Response reloadAutomounter(const Request &)
{
	return answerWithOutput(coreapi::netfs::reloadAutomounter());
}

const Param kTableParams[] = {
	HTTPD_SEGMENT_FROM_SET("table", "which of the 2 files: `fstab`, mounted at start up, or `automount`, which the automounter reads", kTableNames, kTableValueDocs),
};

const Param kSlotParams[] = {
	HTTPD_SEGMENT_FROM_SET("table", "which of the 2 files: `fstab`, mounted at start up, or `automount`, which the automounter reads", kTableNames, kTableValueDocs),
	/* Both ends written out, so a slot outside the eight is refused where every
	   other parameter of this server is refused rather than one floor down. The
	   layer below still refuses it, for the reason every rule said twice here
	   is said twice: the one that decides is the one that owns the file. */
	HTTPD_SEGMENT_IN("slot", ParamType::UInt, "which of the 8 slots this addresses, counted from 0 to 7", 0, 7),
};

const Param kEntryParams[] = {
	HTTPD_SEGMENT_FROM_SET("table", "which of the 2 files: `fstab`, mounted at start up, or `automount`, which the automounter reads", kTableNames, kTableValueDocs),
	HTTPD_SEGMENT_IN("slot", ParamType::UInt, "which of the 8 slots this addresses, counted from 0 to 7", 0, 7),
	/* The whole entry and not the parts of it that changed, unlike the route
	   that sets this server up. What is behind that one is four values a caller
	   has no business restating to change a fifth; what is behind this one is
	   one form with one entry on it, and a caller sending half of one would be
	   sending an entry that is half what it read and half what somebody typed.

	   The password is the one member that may be left out, and leaving it out
	   means it stays. That is not a shortening of the rule above: it is the one
	   value a read cannot answer with, so it is the one value a form cannot
	   send back. */
	HTTPD_BODY_REQUIRED("active", ParamType::Bool,
		"whether the box is to mount this entry: true to enable it, false to keep it defined but unmounted"),
	HTTPD_BODY_REQUIRED_FROM_SET("type", "what filesystem protocol the other end speaks", "nfs,cifs", kNetfsTypeDocs),
	HTTPD_BODY_REQUIRED_TEXT("host", "the remote server this entry mounts from, as an address or a host name", 255),
	HTTPD_BODY_REQUIRED_TEXT("remote_dir", "what that server offers, with the separator it is written with", 255),
	HTTPD_BODY_REQUIRED_TEXT("local_dir", "where it lands: a path on this box, or a name under /mnt/autofs for the automounter table", 255),
	HTTPD_BODY_TEXT("user", "the name it logs in under, and left out where it needs none", 64),
	HTTPD_BODY_TEXT("password", "the password, as it is typed, and left out where it is to stay as it is", 255),
	HTTPD_BODY_TEXT("options", "everything else the mount is given, comma separated", 255),
};

const Endpoint kNetfsEndpoints[] = {
	{ Method::Get, "/api/v1/storage/netfs/{table}", AuthLevel::System,
	  "all 8 entries of 1 of the 2 files, with the passwords withheld", "Lists all 8 slots "
	  "of one of the 2 network filesystem files this box keeps, `fstab` or `automount`, read "
	  "fresh from the file for each request. Every slot is carried even when it is empty, "
	  "because what a write addresses is a slot number and an empty slot is one to fill in. No "
	  "password is ever in the answer, only whether one is set.\n\n"
	  "**Related:** `PUT /api/v1/storage/netfs/{table}/{slot}`, "
	  "`DELETE /api/v1/storage/netfs/{table}/{slot}`.",
	  HTTPD_PARAMS(kTableParams), &kSlotsSchema, &listEntries, false,
	  Answers200, HTTPD_NO_REFUSALS },
	{ Method::Put, "/api/v1/storage/netfs/{table}/{slot}", AuthLevel::System,
	  "puts one entry in one slot and answers the whole table as it now stands", "Replaces the "
	  "whole entry at one slot of 1 of the 2 files and answers the whole table as it now "
	  "stands, which is `200` and not `204` because a caller has to be able to read the result "
	  "of a write that rewrites the whole underlying file. Sending the whole entry is required: "
	  "a caller that wants to change one field has to send every other field as it already "
	  "reads, except `password`, which is the one field a read never carries back, so leaving "
	  "it out keeps the password that was set rather than clearing it.\n\n"
	  "**Side effects:** rewrites the file naming the network filesystems; this changes what "
	  "gets mounted at the box's next start up for `fstab`, or what the automounter offers the "
	  "next time it is reloaded or restarted for `automount`. It does not itself mount or "
	  "unmount anything.\n\n"
	  "**Related:** `GET /api/v1/storage/netfs/{table}`, "
	  "`POST /api/v1/storage/netfs/fstab/mount`, `POST /api/v1/storage/netfs/automount/reload`.",
	  HTTPD_PARAMS(kEntryParams), &kSlotsSchema, &writeEntry, false,
	  Answers200, HTTPD_NO_REFUSALS_AND_BODY(
		"{\"active\":true,\"type\":\"nfs\",\"host\":\"192.168.0.10\",\"remote_dir\":\"/nas/video\","
		"\"local_dir\":\"/media/net\",\"options\":\"soft,nolock\"}") },
	{ Method::Delete, "/api/v1/storage/netfs/{table}/{slot}", AuthLevel::System,
	  "empties one slot, which is also the one way a password is taken off a mount", "Empties "
	  "one slot of 1 of the 2 files and answers the whole table as it now stands. This is "
	  "the only way to take a password off a mount: `PUT` leaves a password alone when it is "
	  "left out of the body, so clearing one means emptying the whole slot and writing it "
	  "again.\n\n"
	  "**Side effects:** rewrites the file naming the network filesystems, the same way a "
	  "`PUT` to this slot would; it does not unmount anything already mounted under the slot's "
	  "old directory.\n\n"
	  "**Related:** `GET /api/v1/storage/netfs/{table}`, "
	  "`PUT /api/v1/storage/netfs/{table}/{slot}`.",
	  HTTPD_PARAMS(kSlotParams), &kSlotsSchema, &clearEntry, false,
	  Answers200, HTTPD_NO_REFUSALS },
	/* The two below drive the box's own start up script, which is what mounts
	   these at start up, so what a caller asks for here and what the box does
	   on its own are one piece of work and not two that have to agree. */
	{ Method::Post, "/api/v1/storage/netfs/fstab/mount", AuthLevel::System,
	  "mounts everything the first file names that is not mounted already", "Runs the box's own "
	  "start up script to mount everything `/var/etc/fstab` names that is not mounted already. "
	  "The answer carries the script's own output, as it wrote it, which is meant for a person "
	  "reading it rather than for a program to parse; what actually came of mounting is read "
	  "back with `GET /api/v1/storage/mounts`. The script starts its retries in the background "
	  "and returns, so a mount that needs retrying may still be pending once this answers.\n\n"
	  "**Related:** `GET /api/v1/storage/netfs/{table}`, `GET /api/v1/storage/mounts`, "
	  "`POST /api/v1/storage/netfs/fstab/unmount`.",
	  NULL, 0, &kRanSchema, &mountAll, false,
	  Answers200, HTTPD_NO_REFUSALS },
	{ Method::Post, "/api/v1/storage/netfs/fstab/unmount", AuthLevel::System,
	  "unmounts every network filesystem this box has mounted", "Runs the box's own start up "
	  "script to unmount every network filesystem currently mounted, whether or not it is named "
	  "in `/var/etc/fstab`. The answer carries the script's own output, as it wrote it; what "
	  "actually remains mounted is read back with `GET /api/v1/storage/mounts`.\n\n"
	  "**Side effects:** anything reading from or writing to one of those mounts, a recording "
	  "among it, loses access to it.\n\n"
	  "**Related:** `GET /api/v1/storage/mounts`, `POST /api/v1/storage/netfs/fstab/mount`.",
	  NULL, 0, &kRanSchema, &unmountAll, false,
	  Answers200, HTTPD_NO_REFUSALS },
	/* Its own act and not a second mount, because the automounter reads its
	   file when it starts and not while it runs: an entry written there is not
	   a mount until this has happened. */
	{ Method::Post, "/api/v1/storage/netfs/automount/reload", AuthLevel::System,
	  "starts the automounter again so that it reads its file afresh", "Restarts the "
	  "automounter daemon so it reads `/var/etc/auto.net` afresh. The automounter only reads "
	  "that file when it starts, not while it runs, so an entry written to the `automount` "
	  "table through this interface is not offered under /mnt/autofs until this has run. The "
	  "answer carries the restart script's own output, as it wrote it.\n\n"
	  "**Side effects:** entries the automounter was already serving are offered again once it "
	  "has come back up; anything actively reading from one during the restart loses access to "
	  "it briefly.\n\n"
	  "**Related:** `PUT /api/v1/storage/netfs/{table}/{slot}`, "
	  "`GET /api/v1/storage/netfs/{table}`.",
	  NULL, 0, &kRanSchema, &reloadAutomounter, false,
	  Answers200, HTTPD_NO_REFUSALS },
};

} // namespace

extern const RouteTable netfsTable = {
	HTTPD_TABLE("netfs", kNetfsEndpoints)
};

} // namespace httpd
