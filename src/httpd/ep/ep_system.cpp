/*
 * ep_system.cpp - routes for box facts and power
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (additions, fixes)
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

#include "coreapi/backup.h"
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

/* The web server's name and version, as it gives them in the head of each
   answer (WEBSERVERNAME), for the interface's About window. */
#include "yconfig.h"

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

const FieldDesc kCpuFields[] = {
	HTTPD_MEMBER("model", FieldType::String, "the processor as the kernel names it"),
	HTTPD_MEMBER("cores", FieldType::UInt, "how many cores it has"),
	HTTPD_MEMBER("load_percent", FieldType::Int,
		"how busy it was in percent: since the previous reading where that was 0.2 to 10 seconds ago, "
		"otherwise over the tenth of a second this was read in; -1 where that could not be read"),
};

const Schema kCpuSchema = { "cpu", HTTPD_FIELDS(kCpuFields) };

const FieldDesc kSupportFields[] = {
	HTTPD_MEMBER("homepage", FieldType::String, "the project's home page, and empty where there is none"),
	HTTPD_MEMBER("docs", FieldType::String, "its documentation"),
	HTTPD_MEMBER("forum", FieldType::String, "where to ask for help"),
	HTTPD_MEMBER("copyright", FieldType::String, "whose the program is, as one line"),
};

const Schema kSupportSchema = { "support", HTTPD_FIELDS(kSupportFields) };

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
		"how much of that is unused, counting what the buffers and the cache hold, in bytes; under a memory_total of 0 this is a reading that failed, not an empty box"),
	HTTPD_MEMBER("root_total", FieldType::UInt,
		"how large the root filesystem is in bytes, which on the smallest box is the whole of what an image has to fit in"),
	HTTPD_MEMBER("root_free", FieldType::UInt,
		"how much of the root filesystem is unwritten, in bytes; under a root_total of 0 this is a reading that failed"),
	HTTPD_MEMBER("api_doc", FieldType::Bool,
		"whether this build carries the API documentation, which is the prose in the document this server writes about itself"),
	HTTPD_OBJECT("api", &kApiSchema,
		"what this interface is, so a caller knows before it asks anything else"),
	HTTPD_OBJECT("cpu", &kCpuSchema, "the processor and how busy it is"),
	HTTPD_LIST_OF_VALUES("load_average", ElementType::Number,
		"what waits to run, averaged over 1, 5 and 15 minutes, as the kernel keeps it"),
	HTTPD_MEMBER("processes_running", FieldType::UInt, "tasks running now"),
	HTTPD_MEMBER("processes_total", FieldType::UInt, "tasks in all"),
	HTTPD_MEMBER("temperature_known", FieldType::Bool, "whether the box has a sensor for the processor's temperature"),
	HTTPD_MEMBER("temperature", FieldType::Int,
		"the processor's temperature in degrees Celsius, and 0 where temperature_known is false"),
	HTTPD_MEMBER("os", FieldType::String, "the system the image is built from, as /etc/os-release names it"),
	HTTPD_MEMBER("machine", FieldType::String, "the machine the kernel runs on, such as armv7l or x86_64"),
	HTTPD_MEMBER("neutrino_version", FieldType::String, "the version of the program"),
	HTTPD_MEMBER("neutrino_git", FieldType::String, "the revision the program was built from, and empty where the build does not say"),
	HTTPD_MEMBER("webserver_name", FieldType::String, "the web server answering, as it names itself in the head of each answer"),
	HTTPD_MEMBER("webserver_version", FieldType::String, "that web server's version"),
	HTTPD_OBJECT("support", &kSupportSchema, "where to find help, as the box's own information screen says"),
};

const Schema kBoxSchema = { "box", HTTPD_FIELDS(kBoxFields) };

/* The sorts of thing this one list carries: one for now, as softcams are left
   out until they come the way any extension from another source does.
   Answered all the same, so a second sort can come back without a reader
   having to work it out from the name. */
const char *kindName(coreapi::daemons::Kind k)
{
	switch (k)
	{
		case coreapi::daemons::Kind::Service: break;
	}
	return "service";
}

const char kDaemonKindDocs[] =
	"service: a background service this image can carry";

const FieldDesc kDaemonFields[] = {
	/* The same set the three routes that act on a daemon take, out of the same
	   function, so that a reader crossing from this listing to one of those
	   routes is holding one kind of thing and not a free text that happens to
	   be accepted there. */
	HTTPD_MEMBER_FROM_ASKED_SET("id",
		"the identifier the start, stop and restart routes take; the full set and what each one is comes from GET /api/v1/daemons",
		&daemonNames),
	HTTPD_MEMBER_OF_SET("kind", "service",
		"the kind of daemon this is, which decides where a caller controls it from", kDaemonKindDocs),
	HTTPD_MEMBER("running", FieldType::Bool,
		"whether a process of its name is in the process table, which is not the same as its having been asked for"),
	HTTPD_MEMBER("installed", FieldType::Bool,
		"whether this box carries the thing at all: whether its init script is in either init directory or, on a box run by systemd, its unit file is installed; false for a name every image knows but this build was not given"),
};

const Schema kDaemonSchema = { "daemon", HTTPD_FIELDS(kDaemonFields) };

const FieldDesc kDaemonListFields[] = {
	HTTPD_LIST_OF("items", &kDaemonSchema,
		"every daemon this box can drive, in the order it knows them"),
};

const Schema kDaemonListSchema = { "daemon-list", HTTPD_FIELDS(kDaemonListFields) };

const FieldDesc kUsageFields[] = {
	HTTPD_MEMBER("name", FieldType::String,
		"one of the legacy table's own names, or a name /control/ was asked for that the table never had"),
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

	j.key("cpu");
	j.beginObject();
	j.key("model");
	j.value(box.cpu_model);
	j.key("cores");
	j.value((unsigned long) box.cpu_cores);
	j.key("load_percent");
	j.value((long) box.cpu_load_percent);
	j.endObject();
	j.key("load_average");
	j.beginArray();
	for (int i = 0; i < 3; ++i)
		j.value(box.load_average[i]);
	j.endArray();
	j.key("processes_running");
	j.value((unsigned long) box.processes_running);
	j.key("processes_total");
	j.value((unsigned long) box.processes_total);
	j.key("temperature_known");
	j.value(box.temperature_known);
	j.key("temperature");
	j.value((long) (box.temperature_known ? box.temperature : 0));
	j.key("os");
	j.value(box.os_name);
	j.key("machine");
	j.value(box.machine);
	j.key("neutrino_version");
	j.value(PACKAGE_VERSION);
	j.key("neutrino_git");
#ifdef PACKAGE_VERSION_GIT
	j.value(PACKAGE_VERSION_GIT);
#else
	j.value("");
#endif
	j.key("webserver_name");
	j.value(HTTPD_NAME);
	j.key("webserver_version");
	j.value(HTTPD_VERSION);
	j.key("support");
	j.beginObject();
	j.key("homepage");
	j.value(box.homepage);
	j.key("docs");
	j.value(box.docs);
	j.key("forum");
	j.value(box.forum);
	j.key("copyright");
	j.value(box.copyright);
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

/* The archive as backup.sh packed it, handed over as a file to keep. Its name
   says which box and when; only what a name on a download can carry is kept. */
Response backupDownload(const Request &)
{
	coreapi::Result<coreapi::backup::Archive> made = coreapi::backup::make();
	if (!made.ok())
		return problemFor(made.error());
	coreapi::backup::Archive a = std::move(made).value();
	std::string name;
	for (size_t i = 0; i < a.name.size(); ++i)
	{
		const char c = a.name[i];
		const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
		                   c == '.' || c == '_' || c == '-';
		name += plain ? c : '_';
	}
	Response out;
	out.code = StatusOk;
	out.content_type = "application/gzip";
	out.body.swap(a.bytes);
	out.headers.push_back(std::make_pair(std::string("Content-Disposition"),
	                                     "attachment; filename=\"" + name + "\""));
	return out;
}

Response restoreSettings(const Request &r)
{
	coreapi::Result<void> done = coreapi::backup::restore(r.asString("file"));
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

const RouteRefusal kBackupRefusals[] = {
	HTTPD_REFUSES(Internal, BackupFailed,
		"backup.sh left no archive, or one larger than 64 MiB"),
};

const Param kRestoreParams[] = {
	HTTPD_BODY_REQUIRED_TEXT("file",
		"the archive's name in /tmp, where the caller uploaded it (PUT and PATCH /api/v1/storage/file), "
		"ending in .tar.gz, .tgz or .tar", 255),
};

const RouteRefusal kRestoreRefusals[] = {
	HTTPD_REFUSES(InvalidArgument, BadName,
		"the archive is a plain name in the temporary directory, of letters, digits, dots, dashes and "
		"underscores, ending in .tar.gz, .tgz or .tar"),
	HTTPD_REFUSES(NotFound, NoSuchName,
		"there is no uploaded archive of that name"),
	HTTPD_REFUSES(InvalidArgument, RestoreRefused,
		"the archive would write where the box backs nothing up: /etc/passwd"),
	HTTPD_REFUSES(Conflict, RestoreRefused,
		"another restore is under way"),
	HTTPD_REFUSES(Internal, RestoreRefused,
		"/var/tuxbox/config/neutrino.conf could not be written; the box is not restarted"),
};

const Param kStandbyParams[] = {
	/* Required, and not a route per direction. Standby off while not in standby
	   changes nothing and is accepted, so a caller that meant one and sent the
	   other would be told nothing about it; naming which of the two is meant is
	   the one thing that keeps the request from being ambiguous. */
	HTTPD_BODY_REQUIRED("on", ParamType::Bool, "whether the box is to go to standby or come out of it"),
};

const FieldDesc kProcessFields[] = {
	HTTPD_MEMBER("id", FieldType::Int, "the process number"),
	HTTPD_MEMBER("ppid", FieldType::Int, "the number of the process that started it"),
	HTTPD_MEMBER("uid", FieldType::UInt, "the user it runs as, as the numeric id and not as a name"),
	HTTPD_MEMBER("state", FieldType::String,
		"the state letter the kernel gives it, for example R running, S sleeping, Z ended and not yet collected"),
	HTTPD_MEMBER("rss", FieldType::UInt, "how much memory it holds right now, in bytes"),
	HTTPD_MEMBER("vsize", FieldType::UInt, "how much address space it has mapped, in bytes"),
	HTTPD_MEMBER("name", FieldType::String, "the name the kernel keeps for it"),
	HTTPD_MEMBER("command", FieldType::String,
		"the command line it was started with, its arguments apart by spaces; a kernel thread, which has none, shows its name in brackets"),
};

const Schema kProcessSchema = { "process", HTTPD_FIELDS(kProcessFields) };

const FieldDesc kProcessListFields[] = {
	HTTPD_LIST_OF("items", &kProcessSchema, "every process running when the list was read, by number"),
};

const Schema kProcessListSchema = { "process-list", HTTPD_FIELDS(kProcessListFields) };

const FieldDesc kKernelLogLineFields[] = {
	HTTPD_MEMBER("level", FieldType::Int,
		"the level the line was written at, 0 the most urgent and 7 debug, -1 where the line carries none"),
	HTTPD_MEMBER("text", FieldType::String, "the line as the kernel keeps it, without its level"),
};

const Schema kKernelLogLineSchema = { "kernel-log-line", HTTPD_FIELDS(kKernelLogLineFields) };

const FieldDesc kKernelLogFields[] = {
	HTTPD_LIST_OF("items", &kKernelLogLineSchema, "every line the kernel's log holds, oldest first"),
};

const Schema kKernelLogSchema = { "kernel-log", HTTPD_FIELDS(kKernelLogFields) };

/* What the web interface used to get by running ps and dmesg through a route
   that ran any command at all. These two answer only what they are, read and
   not started. */
Response listProcesses(const Request &)
{
	coreapi::Result<std::vector<coreapi::system::ProcessInfo> > got = coreapi::system::processes();
	if (!got.ok())
		return problemFor(got.error());
	const std::vector<coreapi::system::ProcessInfo> all = std::move(got).value();

	Response out = okJson();
	Json j(out.body, 32 + 160 * all.size());
	j.beginObject();
	j.key("items");
	j.beginArray();
	for (size_t i = 0; i < all.size(); ++i)
	{
		const coreapi::system::ProcessInfo &p = all[i];
		j.beginObject();
		j.key("id");
		j.value((long long) p.pid);
		j.key("ppid");
		j.value((long long) p.ppid);
		j.key("uid");
		j.value((unsigned long long) p.uid);
		j.key("state");
		j.value(std::string(1, p.state));
		j.key("rss");
		j.value(p.rss_bytes);
		j.key("vsize");
		j.value(p.vsize_bytes);
		j.key("name");
		j.value(p.name);
		j.key("command");
		j.value(p.command);
		j.endObject();
	}
	j.endArray();
	j.endObject();
	return out;
}

Response kernelLog(const Request &)
{
	coreapi::Result<std::vector<coreapi::system::KernelLogLine> > got = coreapi::system::kernelLog();
	if (!got.ok())
		return problemFor(got.error());
	const std::vector<coreapi::system::KernelLogLine> all = std::move(got).value();

	Response out = okJson();
	Json j(out.body, 32 + 96 * all.size());
	j.beginObject();
	j.key("items");
	j.beginArray();
	for (size_t i = 0; i < all.size(); ++i)
	{
		j.beginObject();
		j.key("level");
		j.value((long long) all[i].level);
		j.key("text");
		j.value(all[i].text);
		j.endObject();
	}
	j.endArray();
	j.endObject();
	return out;
}

const RouteRefusal kProcessesRefusals[] = {
	HTTPD_REFUSES(NotSupported, ProcessesUnreadable,
		"there is no process list to read at /proc"),
};

const RouteRefusal kKernelLogRefusals[] = {
	HTTPD_REFUSES(NotSupported, KernelLogUnreadable,
		"the kernel does not hand its log to this program"),
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
	  "read fresh for each request but `image_version`, `os` and `support`, which are read once "
	  "and kept for as long as the program runs.\n\n"
	  "**Refusals:**\n"
	  "- `500 box-unreadable`: the box could not be identified at all. This is a box fault, not "
	  "a request problem; try again later.",
	  NULL, 0, &kBoxSchema, &boxInfo, false,
	  Answers200, HTTPD_REFUSALS(kBoxInfoRefusals) },
	/* System and not Read: a command line can carry what a program was started
	   with, a password among it, and the kernel's log names devices, addresses
	   and what failed where. */
	{ Method::Get, "/api/v1/system/processes", AuthLevel::System,
	  "every process running on the box", "Lists every process running when the list is read, by "
	  "process number: its parent, the user it runs as, its state, the memory it holds and has "
	  "mapped, its name and the command line it was started with. Read out of the kernel's "
	  "process table and not by starting a program, so asking changes nothing on the box. A "
	  "process that ends while the list is read is left out.\n\n"
	  "**Refusals:**\n"
	  "- `501 processes-unreadable`: the box keeps no process list where this reads it.\n\n"
	  "**Related:** `GET /api/v1/system/kernel-log`, `GET /api/v1/system/info`.",
	  NULL, 0, &kProcessListSchema, &listProcesses, false,
	  Answers200, HTTPD_REFUSALS(kProcessesRefusals) },
	{ Method::Get, "/api/v1/system/kernel-log", AuthLevel::System,
	  "the kernel's log, what dmesg prints", "Reads the kernel's log as it stands, oldest line "
	  "first, each line with the level it was written at. Reading leaves the log as it is for "
	  "every other reader.\n\n"
	  "**Refusals:**\n"
	  "- `501 kernel-log-unreadable`: the kernel does not hand its log to this program, which "
	  "kernel.dmesg_restrict does for a program not running as root.\n\n"
	  "**Related:** `GET /api/v1/system/processes`.",
	  NULL, 0, &kKernelLogSchema, &kernelLog, false,
	  Answers200, HTTPD_REFUSALS(kKernelLogRefusals) },
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
	  "box can drive: the background services an image can carry, such as nfs, samba or crond. "
	  "For each one, says whether a process of its name is currently in the process table and "
	  "whether this box carries its systemd unit or init script at all, which may be false for "
	  "a name every image knows but this build was not given. Softcams are not in this list.\n\n"
	  "**Related:** `POST /api/v1/daemons/{name}/start`, `POST /api/v1/daemons/{name}/stop`, "
	  "`POST /api/v1/daemons/{name}/restart`.",
	  NULL, 0, &kDaemonListSchema, &listDaemons, false,
	  Answers200, HTTPD_NO_REFUSALS },
	{ Method::Get, "/api/v1/system/legacy-usage", AuthLevel::Read,
	  "how many times /control/ has been asked for each of its names, so retiring one can rest on what still calls it rather than on a guess, and no names at all where this build carries none of that surface",
	  "Reads how many times the `/control/` surface has been asked for each of the "
	  "names in its own table since this program started, plus any name it was asked for that "
	  "table never had. It exists so that retiring a legacy name can rest on what still calls it rather "
	  "than on a guess.\n\n"
	  "**Preconditions:** none, but on a build without the `/control/` surface at all "
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
	  "**Side effects:** switching to standby stops live playback and the shift the box keeps "
	  "of what it is showing; a recording or a timer already running is not stopped by it. "
	  "While the box is in standby, `POST /api/v1/zap` "
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
	  "sends a channel to a client the request is dropped and nothing happens at all. While a "
	  "recording is running, the box first shows a confirmation on its own screen; unless that "
	  "is answered there, it does not switch off. With `shutdown_block_while_recording` on, the "
	  "box instead goes into standby while a recording is running or a recording timer is still "
	  "to come, and keeps retrying on its own until nothing is pending, rather than fully "
	  "powering off straight away. Once it does power off, the connection this request arrived on is "
	  "dropped and nothing further is answered on it. Unlike standby, shutdown is not reversed "
	  "by a request to this interface: the box has to be switched on again at the box itself, "
	  "by a timer that is due to wake it, or by whatever external power control it has.\n\n"
	  "**Side effects:** live playback and every daemon on the box stop once the box actually "
	  "powers off.\n\n"
	  "**Related:** `POST /api/v1/system/reboot`, `POST /api/v1/system/standby`.",
	  NULL, 0, NULL, &shutdown, false,
	  Answers202, HTTPD_NO_REFUSALS },
	{ Method::Get, "/api/v1/system/backup", AuthLevel::System,
	  "the box's settings as one archive to keep",
	  "Packs the box's settings with its own backup.sh, the script the settings menu on the screen "
	  "runs, and answers the archive as a file to keep (`application/gzip`, named "
	  "`settings_<host>_<date>.tar.gz`): the files the box's tobackup.conf names, or its whole "
	  "configuration directory where there is no such list. The archive holds the box's passwords "
	  "and keys as its settings do, so it is for the box's own credential only.\n\n"
	  "**Refusals:**\n"
	  "- `500 backup-failed`: the box has no backup.sh, it failed, or it left no archive or one "
	  "larger than 64 MiB.\n\n"
	  "**Related:** `POST /api/v1/system/restore`.",
	  NULL, 0, NULL, &backupDownload, false,
	  Answers200, HTTPD_REFUSALS(kBackupRefusals) },
	{ Method::Post, "/api/v1/system/restore", AuthLevel::System,
	  "puts the settings of an uploaded archive back and restarts the box",
	  "Puts back an archive uploaded to /tmp beforehand and restarts the box at once, as the settings "
	  "menu does, so the program does not save its own settings over what was put back. A build for a "
	  "PC that runs other things besides restarts only the program, started again in its own place "
	  "without saving, and not the system (`--with-restore-restart`). `202` says the settings are back "
	  "and the restart starts a few seconds later; the box, or the program, is then gone until it is up "
	  "again. One restore runs at a time.\n\n"
	  "The box first takes the upload over: it copies it into a directory of its own under a name it "
	  "chooses and removes the upload, so what it checks is what it puts back. Then every name in the "
	  "archive is read from its headers. Each has to land in the configuration directory or on a path "
	  "the box's tobackup.conf names: a directory, a file or a pattern. Names land below the box's "
	  "/var/tuxbox where any of them begins with `config/`, and below `/` otherwise. An absolute name, "
	  "one that climbs with `..`, one with a `.` or an empty part, one beginning with `.restore-`, a name "
	  "with a control character, a link of either kind or a device in the archive, a link already on the "
	  "box below where a name lands, a file where the box has a directory or the other way round, and an "
	  "archive that holds one name as a file and as a directory or puts an entry below one of its files "
	  "are refused, and so is an archive that cannot be read, one larger than 64 MiB, one whose headers "
	  "say it unpacks to more than 256 MiB, or one of more than 20000 names.\n\n"
	  "The box unpacks the archive itself, with the same reading of its headers that checked it. From "
	  "then on until the restart it saves no settings of its own, the saves under way finished first. "
	  "Each file is written beside its place and takes its name only once every file is whole, with the "
	  "permissions its entry carries, no set-id bits, and the owner the file had on the box where it had "
	  "one; directories on the way, also a listed one the box does not have yet, are made where they are "
	  "missing and not followed where they are links, and a directory entry sets its directory's "
	  "permissions last.\n\n"
	  "The login stays as the box has it, whatever the archive carries: nhttpd-access.conf (the "
	  "password, the API tokens and the rest of who may reach the box), the name that store had "
	  "before and the working files the two servers write their files through are not put back, "
	  "and an nhttpd.conf in the archive is put back with its lines for the user name and the "
	  "password replaced by the box's own. So a restore never sets a login, also not the one of "
	  "the box the archive was made on; the restore in the settings menu at the box does.\n\n"
	  "**Preconditions:** the archive was uploaded to /tmp, as `PUT` and `PATCH "
	  "/api/v1/storage/file` do in pieces for a file larger than one request.\n\n"
	  "**Refusals:**\n"
	  "- `400 bad-name`: `file` is not a plain name in /tmp ending in .tar.gz, .tgz or .tar.\n"
	  "- `404 no-such-name`: no file of that name was uploaded.\n"
	  "- `400 restore-refused`: the archive cannot be read or holds a name it may not; the detail "
	  "names the first. Nothing was written.\n"
	  "- `409 restore-refused`: another restore is under way, or settings put back before wait for "
	  "their restart.\n"
	  "- `500 restore-refused`: the box cannot restart itself, and nothing was put back; or it could not "
	  "take the archive over or write a file of it, and its settings are as they were and it is not "
	  "restarted; or, rarely, a file could not take its place after others had, and the settings are "
	  "back in part and the box is not restarted; or the settings are back and the restart could not be "
	  "started. The detail says which; in the last two the box saves no settings and has to be "
	  "restarted by hand.\n\n"
	  "**Related:** `GET /api/v1/system/backup`.",
	  HTTPD_PARAMS(kRestoreParams), NULL, &restoreSettings, false,
	  Answers202, HTTPD_REFUSALS_AND_BODY(kRestoreRefusals, "{\"file\":\"settings.tar.gz\"}") },
	{ Method::Post, "/api/v1/system/restart", AuthLevel::System,
	  "asks the box to start its own program again without restarting the box", "Asks the box "
	  "to start its own application again without rebooting the Linux system underneath it. "
	  "`202` means the request has reached the box's own event loop, not that the restart has "
	  "happened. If a recording is running, the box refuses the restart with a message on its "
	  "own screen, and this server keeps running, even though the answer already said `202`. "
	  "When the restart does go "
	  "ahead, the connection this request arrived on, and every other connection this server "
	  "holds, is dropped as the program ends, and this server is unreachable until the new "
	  "process has come up. A client should retry `GET /api/v1/system/info` after a pause.\n\n"
	  "**Side effects:** live playback stops and is set up again from scratch.\n\n"
	  "**Related:** `POST /api/v1/system/reboot`.",
	  NULL, 0, NULL, &restart, false,
	  Answers202, HTTPD_NO_REFUSALS },
};

} // namespace

extern const RouteTable systemTable = {
	HTTPD_TABLE("system", kSystemEndpoints)
};

} // namespace httpd
