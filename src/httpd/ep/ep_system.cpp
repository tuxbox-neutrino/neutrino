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
	HTTPD_MEMBER("model", FieldType::String, "what the box is"),
	HTTPD_MEMBER("chipset", FieldType::String, "the part the picture and the sound go through"),
	HTTPD_MEMBER("image_version", FieldType::String, "the firmware on it"),
	HTTPD_MEMBER("kernel", FieldType::String, "the kernel that firmware runs"),
	HTTPD_MEMBER("hostname", FieldType::String, "what the box calls itself on the network"),
	HTTPD_MEMBER("uptime", FieldType::UInt,
		"how long the box has been up in seconds, so that two readings a minute apart differ by a minute, and nought where it could not be read"),
	HTTPD_MEMBER("memory_total", FieldType::UInt,
		"how much memory the box has in bytes, and nought where it could not be read"),
	HTTPD_MEMBER("memory_free", FieldType::UInt,
		"how much of that is unused in bytes, which under a total of nought is a reading that failed"),
	HTTPD_MEMBER("root_total", FieldType::UInt,
		"how large the root filesystem is in bytes, which on the smallest box is the whole of what an image has to fit in"),
	HTTPD_MEMBER("root_free", FieldType::UInt,
		"how much of the root filesystem is unwritten in bytes, which under a total of nought is a reading that failed"),
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

const FieldDesc kDaemonFields[] = {
	/* The same set the three routes that act on a daemon take, out of the same
	   function, so that a reader crossing from this listing to one of those
	   routes is holding one kind of thing and not a free text that happens to
	   be accepted there. */
	HTTPD_MEMBER_FROM_ASKED_SET("id", "what the routes that drive this daemon name it by", &daemonNames),
	HTTPD_MEMBER_OF_SET("kind", "softcam,service",
		"a softcam the box descrambles with and drives from its own screen, or a service an image was built to carry"),
	HTTPD_MEMBER("running", FieldType::Bool,
		"whether a process of its name is in the process table, which is not the same as its having been asked for"),
	HTTPD_MEMBER("installed", FieldType::Bool,
		"whether this box carries the thing at all, read where the two scripts that start one read it, and false for a name every image knows and this one was not built with"),
};

const Schema kDaemonSchema = { "daemon", HTTPD_FIELDS(kDaemonFields) };

const FieldDesc kDaemonListFields[] = {
	HTTPD_LIST_OF("items", &kDaemonSchema,
		"every daemon this box can drive, in the order it knows them"),
};

const Schema kDaemonListSchema = { "daemon-list", HTTPD_FIELDS(kDaemonListFields) };

const FieldDesc kUsageFields[] = {
	HTTPD_MEMBER("name", FieldType::String,
		"one of the legacy table's own seventy-four names, or a name /control/ was asked for that the table never had"),
	HTTPD_MEMBER("calls", FieldType::UInt,
		"how many times this name has been asked for since this program started"),
};

const Schema kUsageSchema = { "legacy-usage-entry", HTTPD_FIELDS(kUsageFields) };

const FieldDesc kUsageListFields[] = {
	HTTPD_LIST_OF("items", &kUsageSchema,
		"every name this program has counted a call for: the known table first, in its own order and each carried even at zero, then every other name this layer still remembers"),
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
	HTTPD_MEMBER("on", FieldType::Bool, "whether the box is in standby"),
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

const Endpoint kSystemEndpoints[] = {
	{ Method::Get, "/api/v1/system/info", AuthLevel::Read,
	  "what the box is and what it is running",
	  NULL, 0, &kBoxSchema, &boxInfo, false },
	{ Method::Get, "/api/v1/system/standby", AuthLevel::Read,
	  "whether the box is in standby",
	  NULL, 0, &kStandbyStateSchema, &standbyState, false },
	{ Method::Get, "/api/v1/system/decryption", AuthLevel::Read,
	  "what the running channel is being descrambled with",
	  NULL, 0, &kDecryptionSchema, &decryptionState, false },
	{ Method::Get, "/api/v1/daemons", AuthLevel::Read,
	  "the daemons this box can drive and which of them are running",
	  NULL, 0, &kDaemonListSchema, &listDaemons, false },
	{ Method::Get, "/api/v1/system/legacy-usage", AuthLevel::Read,
	  "how many times /control/ has been asked for each of its names, so retiring one can rest on what still calls it rather than on a guess, and no names at all where this build carries none of that surface",
	  NULL, 0, &kUsageListSchema, &legacyUsage, false },
	{ Method::Post, "/api/v1/system/standby", AuthLevel::System,
	  "asks the box to go to standby or to come out of it",
	  HTTPD_PARAMS(kStandbyParams), NULL, &standby, false },
	{ Method::Post, "/api/v1/system/reboot", AuthLevel::System,
	  "asks the box to restart itself",
	  NULL, 0, NULL, &reboot, false },
	{ Method::Post, "/api/v1/system/shutdown", AuthLevel::System,
	  "asks the box to switch off",
	  NULL, 0, NULL, &shutdown, false },
	{ Method::Post, "/api/v1/system/restart", AuthLevel::System,
	  "asks the box to start its own program again without restarting the box",
	  NULL, 0, NULL, &restart, false },
};

} // namespace

extern const RouteTable systemTable = {
	HTTPD_TABLE("system", kSystemEndpoints)
};

} // namespace httpd
