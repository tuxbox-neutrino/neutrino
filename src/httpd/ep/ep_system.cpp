/*
 * ep_system.cpp - routes for box facts and power
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

#include "coreapi/channels.h"
#include "coreapi/daemons.h"
#include "coreapi/decryption.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"
#include "coreapi/system.h"
#include "coreapi/base/types.h"
#include "coreapi/base/version.h"

/* mode_standby. The mode the box settles into is a NeutrinoModes value and this
   is where that enumeration is written down. */
#include "neutrinoMessages.h"

/* What this build was compiled with, which two of the answers below carry:
   whether the copied control API is in it at all, so legacy-usage can say
   there is nothing to count rather than going missing, and whether the API
   documentation is, so a page that cannot see a compile time flag need not
   offer a way into a reader nobody installed (see configure.ac and
   src/httpd/Makefile.am). */
#include <config.h>

#ifndef DISABLE_LEGACY_API
#include "httpd/compat/mount.h"
#endif

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace httpd
{

namespace
{

const FieldDesc kApiFields[] = {
	HTTPD_MEMBER("major", FieldType::UInt, "changes when something a caller reads changes shape"),
	HTTPD_MEMBER("minor", FieldType::UInt, "changes when something is added and nothing moves"),
};

const Schema kApiSchema = { "api-version", HTTPD_FIELDS(kApiFields) };

const FieldDesc kBoxFields[] = {
	HTTPD_MEMBER("vendor", FieldType::String,
		"who built the box, which the model name on its own does not say"),
	HTTPD_MEMBER("model", FieldType::String,
		"the model name of the box, as the vendor markets it, for example a specific set-top box line"),
	HTTPD_MEMBER("chipset", FieldType::String, "the part the picture and the sound go through"),
	HTTPD_MEMBER("image_version", FieldType::String,
		"the version string of the firmware image installed on the box"),
	HTTPD_MEMBER("kernel", FieldType::String,
		"the release string of the Linux kernel the firmware is currently running"),
	HTTPD_MEMBER("hostname", FieldType::String, "what the box calls itself on the network"),
	HTTPD_MEMBER("uptime", FieldType::UInt,
		"how long the box has been up, in seconds, so 2 readings 60 seconds apart differ by 60, and 0 where it could not be read"),
	HTTPD_MEMBER("memory_total", FieldType::UInt,
		"how much memory the box has, in bytes, and 0 where it could not be read"),
	HTTPD_MEMBER("memory_free", FieldType::UInt,
		"how much of that is unused, in bytes; under a memory_total of 0 this is a reading that failed, not an empty box"),
	HTTPD_MEMBER("root_total", FieldType::UInt,
		"how large the root filesystem is in bytes, which on the smallest box is the whole of what an image has to fit in"),
	HTTPD_MEMBER("root_free", FieldType::UInt,
		"how much of the root filesystem is unwritten, in bytes; under a root_total of 0 this is a reading that failed"),
	HTTPD_MEMBER("api_doc", FieldType::Bool,
		"whether this build carries the API documentation, which is the prose in the document this server writes about itself and the reader page beside it, both under one switch"),
	HTTPD_OBJECT("api", &kApiSchema,
		"what this interface is, so a caller knows before it asks anything else"),
};

const Schema kBoxSchema = { "box", HTTPD_FIELDS(kBoxFields) };

/* The two sorts of thing this one list carries.

   Answered rather than left for a reader to work out from the name, because
   working it out means keeping a second copy of the seven softcam names
   wherever the list is drawn, and a copy is what goes out of step. The surface
   this replaces had no word for the difference at all and put both on one page
   under a heading that named neither. */
const char *kindName(coreapi::daemons::Kind k)
{
	switch (k)
	{
		case coreapi::daemons::Kind::Softcam: return "softcam";
		case coreapi::daemons::Kind::Service: break;
	}
	return "service";
}

const char kDaemonKindDocs[] =
	"softcam: a conditional access daemon the box descrambles with and can drive from its own screen\n"
	"service: any other background service this image can carry";

const FieldDesc kDaemonFields[] = {
	/* The same set the three routes that act on a daemon take, out of the same
	   function, so that a reader crossing from this listing to one of those
	   routes is holding one kind of thing and not a free text that happens to
	   be accepted there. */
	HTTPD_MEMBER_FROM_ASKED_SET("id",
		"the identifier the start, stop and restart routes take; the full set and what each one is comes from GET /api/v1/daemons",
		&daemonNames),
	HTTPD_MEMBER_OF_SET("kind", "softcam,service",
		"the kind of daemon this is, which decides where a caller controls it from", kDaemonKindDocs),
	HTTPD_MEMBER("running", FieldType::Bool,
		"whether a process of its name is in the process table, which is not the same as its having been asked for"),
	HTTPD_MEMBER("installed", FieldType::Bool,
		"whether this box carries the thing at all: for a softcam, whether its program exists under /var/bin; for a service, whether its start script exists in either init directory; false for a name every image knows but this build was not given"),
};

const Schema kDaemonSchema = { "daemon", HTTPD_FIELDS(kDaemonFields) };

const FieldDesc kDaemonListFields[] = {
	HTTPD_LIST_OF("items", &kDaemonSchema,
		"every daemon this box can drive, in the order it knows them"),
};

const Schema kDaemonListSchema = { "daemon-list", HTTPD_FIELDS(kDaemonListFields) };

const FieldDesc kUsageFields[] = {
	HTTPD_MEMBER("name", FieldType::String,
		"one of the legacy table's own 74 names, or a name /control/ was asked for that the table never had"),
	HTTPD_MEMBER("calls", FieldType::UInt,
		"how many times this name has been asked for since this program started"),
};

const Schema kUsageSchema = { "legacy-usage-entry", HTTPD_FIELDS(kUsageFields) };

const FieldDesc kUsageListFields[] = {
	HTTPD_LIST_OF("items", &kUsageSchema,
		"every name this program has counted a call for: the known table first, in its own order and each carried even at 0 calls, then every other name this layer still remembers"),
};

const Schema kUsageListSchema = { "legacy-usage", HTTPD_FIELDS(kUsageListFields) };

/* One row of what the route answers with, gathered here rather than written
   straight out, because where the counts come from depends on the build and
   what is written does not. */
struct UsageRow
{
	std::string   name;
	unsigned long calls;
};

std::vector<UsageRow> legacyUsageRows()
{
	std::vector<UsageRow> rows;
#ifndef DISABLE_LEGACY_API
	const std::vector<compat::Usage> all = compat::usage();
	rows.reserve(all.size());
	for (size_t i = 0; i < all.size(); ++i)
	{
		UsageRow one;
		one.name = all[i].name;
		one.calls = all[i].calls;
		rows.push_back(one);
	}
#endif
	/* A build without the copied surface has nothing to count, and says so with a
	   list of no names. That is a different answer from a route that is not there,
	   and it is the one a page can be written against: the page is the same bytes on
	   both kinds of box and cannot see which one it is talking to. */
	return rows;
}

Response legacyUsage(const Request &)
{
	const std::vector<UsageRow> all = legacyUsageRows();

	Response out = okJson();
	Json j(out.body, 64 + 48 * all.size());
	j.beginObject();
	j.key("items");
	j.beginArray();
	for (size_t i = 0; i < all.size(); ++i)
	{
		j.beginObject();
		j.key("name");
		j.value(all[i].name);
		j.key("calls");
		j.value(all[i].calls);
		j.endObject();
	}
	j.endArray();
	j.endObject();
	return out;
}

Response boxInfo(const Request &)
{
	coreapi::Result<coreapi::BoxInfo> got = coreapi::system::info();
	if (!got.ok())
		return problemFor(got.error());

	const coreapi::BoxInfo box = std::move(got).value();

	Response out = okJson();
	Json j(out.body, 320);
	j.beginObject();
	j.key("vendor");
	j.value(box.vendor);
	j.key("model");
	j.value(box.model);
	j.key("chipset");
	j.value(box.chipset);
	j.key("image_version");
	j.value(box.image_version);
	j.key("kernel");
	j.value(box.kernel);
	j.key("hostname");
	j.value(box.hostname);
	/* A duration and never a moment, so a reading below nought is not a shorter one:
	   it is a reading that failed. The layer below carries it in the clock's own
	   type, which is signed, and a box that could not read it at all leaves nought
	   behind, so the two answer alike. */
	j.key("uptime");
	j.value((unsigned long long)(box.uptime > 0 ? box.uptime : 0));

	/* Four numbers about the two places a box runs out of, written whether or
	   not they could be read. Nought under a total of nought is a reading that
	   failed and nought under a total that is not is a place that is full, and
	   both of those are things a caller can act on; a member that comes and
	   goes is one every caller has to test for before it reads anything. */
	j.key("memory_total");
	j.value(box.memory_total);
	j.key("memory_free");
	j.value(box.memory_free);
	j.key("root_total");
	j.value(box.root_total);
	j.key("root_free");
	j.value(box.root_free);

	/* Read off the build and not off the box. The page is the same bytes on
	   every box and cannot see a compile time flag, so a build told to leave
	   the API documentation out would otherwise be a page offering a way into
	   a reader that was never installed. Read here the same way the counter
	   above reads whether the copied surface is in this build. */
	j.key("api_doc");
#ifdef DISABLE_API_DOC
	j.value(false);
#else
	j.value(true);
#endif

	/* Answered here rather than left to a caller to infer from what it finds,
	   and answered by the one route a caller reaches before it knows anything
	   else about the box. */
	j.key("api");
	j.beginObject();
	j.key("major");
	j.value((unsigned long) NEUTRINO_API_VERSION_MAJOR);
	j.key("minor");
	j.value((unsigned long) NEUTRINO_API_VERSION_MINOR);
	j.endObject();
	j.endObject();
	return out;
}

Response listDaemons(const Request &)
{
	coreapi::Result<std::vector<coreapi::daemons::Entry> > got = coreapi::daemons::list();
	if (!got.ok())
		return problemFor(got.error());

	const std::vector<coreapi::daemons::Entry> all = std::move(got).value();

	Response out = okJson();
	Json j(out.body, 32 + 80 * all.size());
	j.beginObject();
	j.key("items");
	j.beginArray();
	for (size_t i = 0; i < all.size(); ++i)
	{
		j.beginObject();
		// The daemon's own name and never where it sits in this answer, for
		// the reason every list here carries an identifier of its own.
		j.key("id");
		j.value(all[i].name);
		j.key("kind");
		j.value(std::string(kindName(all[i].kind)));
		j.key("running");
		j.value(all[i].running);
		j.key("installed");
		j.value(all[i].installed);
		j.endObject();
	}
	j.endArray();
	j.endObject();
	return out;
}

/* The four below are commands, and every one of them answers accepted. The message
   reaches the loop's socket and nothing comes back from what does it, so this server
   cannot say the box went to standby, only that it asked. Two of them the box will not
   answer at all afterwards.

   All four are System, not because a reboot is more dangerous than a zap, but because
   a caller that can take the box off the air can stop every recording on it. */

/* WHETHER THE BOX IS IN STANDBY, WHICH THE COMMAND BELOW CANNOT SAY.

   The four commands answer accepted and nothing more, so a page offering
   standby had two buttons where the design has one switch: a switch with
   nothing to read is a control that states a fact it does not have.

   Standby is one of the modes the box settles into, so this is the mode read
   and not a flag of its own; the old surface answers the same question the
   same way (compat/controlapi.cpp). A box that has not settled on a mode yet
   is not in standby, which is what a failed read is taken as: at that moment
   the box is coming up, and coming up is the opposite of standby. */
const FieldDesc kStandbyFields[] = {
	HTTPD_MEMBER("on", FieldType::Bool,
		"whether the box is in standby right now; also false while the box is still starting up and has not settled on a mode"),
};

const Schema kStandbyStateSchema = { "standby", HTTPD_FIELDS(kStandbyFields) };

Response standbyState(const Request &)
{
	coreapi::Result<int> mode = coreapi::channels::mode();
	const bool on = mode.ok() && mode.value() == NeutrinoModes::mode_standby;

	Response out = okJson();
	Json j(out.body, 32);
	j.beginObject();
	j.key("on");
	j.value(on);
	j.endObject();
	return out;
}

/* WHAT THE RUNNING CHANNEL IS BEING DESCRAMBLED WITH.

   Two members and no third saying the channel is scrambled at all. That is a
   property of the channel, it is already answered where the channel is, and a
   second route stating it is a second place for it to be stale: both of these
   are false on a channel in the clear and both are false on a scrambled
   channel nothing can open, and the channel's own answer is what parts those.

   Both read afresh per request. The display that reads this is the one a
   person watches while they push a card into the slot, and the surface this
   replaces asked whether a module was there once at startup and never again.

   Read and not System. It says how the box is descrambling and not what with:
   no card number, no key and no name of a card server, which is the whole of
   what would raise it above the box facts beside it. */
const FieldDesc kDecryptionFields[] = {
	HTTPD_MEMBER("softcam", FieldType::Bool,
		"whether a softcam is answering for what the box is showing right now, which is not the same as one being installed or its process being up"),
	HTTPD_MEMBER("ci_module", FieldType::Bool,
		"whether a module is seated in a common interface slot and the box is sending the channel through it, which is false on a box that has no such slot"),
};

const Schema kDecryptionSchema = { "decryption", HTTPD_FIELDS(kDecryptionFields) };

Response decryptionState(const Request &)
{
	coreapi::Result<coreapi::decryption::State> got = coreapi::decryption::state();
	if (!got.ok())
		return problemFor(got.error());

	const coreapi::decryption::State how = got.value();

	Response out = okJson();
	Json j(out.body, 48);
	j.beginObject();
	j.key("softcam");
	j.value(how.softcam);
	j.key("ci_module");
	j.value(how.ci_module);
	j.endObject();
	return out;
}

Response standby(const Request &r)
{
	coreapi::Result<void> done = coreapi::system::standby(r.asBool("on"));
	if (!done.ok())
		return problemFor(done.error());
	return accepted();
}

Response reboot(const Request &)
{
	coreapi::Result<void> done = coreapi::system::reboot();
	if (!done.ok())
		return problemFor(done.error());
	return accepted();
}

Response shutdown(const Request &)
{
	coreapi::Result<void> done = coreapi::system::shutdown();
	if (!done.ok())
		return problemFor(done.error());
	return accepted();
}

Response restart(const Request &)
{
	coreapi::Result<void> done = coreapi::system::restart();
	if (!done.ok())
		return problemFor(done.error());
	return accepted();
}

const Param kStandbyParams[] = {
	/* Required, and not a route per direction. Standby off while not in standby
	   changes nothing and is accepted, so a caller that meant one and sent the
	   other would be told nothing about it; naming which of the two is meant is
	   the one thing that keeps the request from being ambiguous. */
	HTTPD_BODY_REQUIRED("on", ParamType::Bool, "whether the box is to go to standby or come out of it"),
};

const RouteRefusal kBoxInfoRefusals[] = {
	HTTPD_REFUSES(Internal, BoxUnreadable,
		"the box could not be identified"),
};

const Endpoint kSystemEndpoints[] = {
	{ Method::Get, "/api/v1/system/info", AuthLevel::Read,
	  "what the box is and what it is running", "Reads what the box is and what it is currently "
	  "running: the vendor, the model, the chipset, the firmware version, the kernel release, "
	  "the hostname, its uptime, its memory and root filesystem usage, whether this build "
	  "carries the API documentation, and the version of this interface itself. Every field is "
	  "read fresh for each request.\n\n"
	  "**Refusals:**\n"
	  "- `500 box-unreadable`: the box could not be identified at all. This is a box fault, not "
	  "a request problem; try again later.",
	  NULL, 0, &kBoxSchema, &boxInfo, false,
	  Answers200, HTTPD_REFUSALS(kBoxInfoRefusals) },
	{ Method::Get, "/api/v1/system/standby", AuthLevel::Read,
	  "whether the box is in standby", "Reads whether the box is in standby right now. The "
	  "answer is read from the box's own mode, the same one `POST /api/v1/system/standby` "
	  "changes, so it reflects a change made through the remote control or through the box's "
	  "own menus just as well as one made through this interface. A box that is still starting "
	  "up and has not settled on a mode yet answers false, because coming up is the opposite "
	  "of standby.\n\n"
	  "**Related:** `POST /api/v1/system/standby`.",
	  NULL, 0, &kStandbyStateSchema, &standbyState, false,
	  Answers200, HTTPD_NO_REFUSALS },
	{ Method::Get, "/api/v1/system/decryption", AuthLevel::Read,
	  "what the running channel is being descrambled with", "Reads what the channel currently "
	  "playing is being descrambled with: whether a softcam is answering for it right now, and "
	  "whether a module seated in a common interface slot is being sent the channel. Both "
	  "members are read afresh for each request and both can be false at once, which is a "
	  "channel in the clear or one nothing can open. This says how the box is descrambling and "
	  "never with what: no card number, no key and no name of a card server.\n\n"
	  "**Related:** `GET /api/v1/daemons`, `GET /api/v1/channels/current`.",
	  NULL, 0, &kDecryptionSchema, &decryptionState, false,
	  Answers200, HTTPD_NO_REFUSALS },
	{ Method::Get, "/api/v1/daemons", AuthLevel::Read,
	  "the daemons this box can drive and which of them are running", "Lists every daemon this "
	  "box can drive: the softcams it can descramble with and the background services an image "
	  "can carry, such as nfs, samba or dropbear. For each one, says whether a process of its "
	  "name is currently in the process table and whether this box carries the program or "
	  "start script for it at all, which may be false for a name every image knows but this "
	  "build was not given.\n\n"
	  "**Related:** `POST /api/v1/daemons/{name}/start`, `POST /api/v1/daemons/{name}/stop`, "
	  "`POST /api/v1/daemons/{name}/restart`.",
	  NULL, 0, &kDaemonListSchema, &listDaemons, false,
	  Answers200, HTTPD_NO_REFUSALS },
	{ Method::Get, "/api/v1/system/legacy-usage", AuthLevel::Read,
	  "how many times /control/ has been asked for each of its names, so retiring one can rest on what still calls it rather than on a guess, and no names at all where this build carries none of that surface",
	  "Reads how many times the copied `/control/` surface has been asked for each of its own "
	  "74 names since this program started, plus any name it was asked for that table never "
	  "had. It exists so that retiring a legacy name can rest on what still calls it rather "
	  "than on a guess.\n\n"
	  "**Preconditions:** none, but on a build without the copied `/control/` surface at all "
	  "this answers an empty list rather than refusing, so a page written against it works the "
	  "same on both kinds of box.",
	  NULL, 0, &kUsageListSchema, &legacyUsage, false,
	  Answers200, HTTPD_NO_REFUSALS },
	{ Method::Post, "/api/v1/system/standby", AuthLevel::System,
	  "asks the box to go to standby or to come out of it", "Asks the box to go to standby or "
	  "to come out of it, depending on `on`. `202` means the request has reached the box's own "
	  "event loop, not that standby has changed yet: watch the `standby` event on "
	  "`GET /api/v1/events`, or poll `GET /api/v1/system/standby`. Going to standby while "
	  "already in it, or coming out of it while not in it, changes nothing and still answers "
	  "`202`, which is why `on` must name the direction rather than this route toggling it.\n\n"
	  "**Side effects:** switching to standby stops live playback; a recording or a timer "
	  "already running is not stopped by it. While the box is in standby, `POST /api/v1/zap` "
	  "and `POST /api/v1/mode` are refused with `409 box-in-standby` unless sent with "
	  "`wake: true`, which switches the box on again as part of carrying out that request.\n\n"
	  "**Related:** `GET /api/v1/system/standby`, `POST /api/v1/zap`, `POST /api/v1/mode`.",
	  HTTPD_PARAMS(kStandbyParams), NULL, &standby, false,
	  Answers202, HTTPD_NO_REFUSALS },
	{ Method::Post, "/api/v1/system/reboot", AuthLevel::System,
	  "asks the box to restart itself", "Asks the box to restart itself: the Linux system "
	  "reboots, not only this program. `202` means the request has reached the box's own event "
	  "loop before this answer was sent, not that the box has restarted. If a recording is "
	  "running, the box first shows a confirmation on its own screen; unless that is answered "
	  "there, the reboot does not happen and this server keeps running, even though the answer "
	  "already said `202`. When the reboot does go ahead, the connection this request arrived "
	  "on is dropped and nothing further is answered on it. A client should expect the "
	  "connection to close and should retry `GET /api/v1/system/info` after a pause to learn "
	  "when the box is back.\n\n"
	  "**Side effects:** live playback and every daemon on the box stop; a recording that is "
	  "allowed to proceed past the confirmation above is stopped as well.\n\n"
	  "**Related:** `POST /api/v1/system/shutdown`, `POST /api/v1/system/restart`.",
	  NULL, 0, NULL, &reboot, false,
	  Answers202, HTTPD_NO_REFUSALS },
	{ Method::Post, "/api/v1/system/shutdown", AuthLevel::System,
	  "asks the box to switch off", "Asks the box to switch off. `202` means the request has "
	  "reached the box's own event loop, not that the box is off. While the box's stream server "
	  "sends a channel to a client the request is dropped and nothing happens at all; while a recording "
	  "is running or a timer is about to start one, the box instead goes into standby and "
	  "keeps retrying on its own until nothing is pending, rather than fully powering off "
	  "straight away. Once it does power off, the connection this request arrived on is "
	  "dropped and nothing further is answered on it. Unlike standby, shutdown is not reversed "
	  "by a request to this interface: the box has to be switched on again at the box itself, "
	  "or by whatever external power control it has.\n\n"
	  "**Side effects:** live playback and every daemon on the box stop once the box actually "
	  "powers off.\n\n"
	  "**Related:** `POST /api/v1/system/reboot`, `POST /api/v1/system/standby`.",
	  NULL, 0, NULL, &shutdown, false,
	  Answers202, HTTPD_NO_REFUSALS },
	{ Method::Post, "/api/v1/system/restart", AuthLevel::System,
	  "asks the box to start its own program again without restarting the box", "Asks the box "
	  "to start its own application again without rebooting the Linux system underneath it. "
	  "`202` means the request has reached the box's own event loop, not that the restart has "
	  "happened. If a recording is running, the box first shows a confirmation on its own "
	  "screen; unless that is answered there, the restart does not happen and this server "
	  "keeps running, even though the answer already said `202`. When the restart does go "
	  "ahead, the connection this request arrived on, and every other connection this server "
	  "holds, is dropped as the program ends, and this server is unreachable until the new "
	  "process has come up. A client should retry `GET /api/v1/system/info` after a pause.\n\n"
	  "**Side effects:** live playback stops and is set up again from scratch; a recording "
	  "that is allowed to proceed past the confirmation above is stopped as well.\n\n"
	  "**Related:** `POST /api/v1/system/reboot`.",
	  NULL, 0, NULL, &restart, false,
	  Answers202, HTTPD_NO_REFUSALS },
};

} // namespace

extern const RouteTable systemTable = {
	HTTPD_TABLE("system", kSystemEndpoints)
};

} // namespace httpd
