/*
 * test_daemons.cpp - tests for the daemon list
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

#include "support/catch.hpp"
#include "coreapi/daemons.h"
#include "coreapi/shelltable.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace coreapi;

namespace
{

typedef Result<std::vector<daemons::Entry> > DaemonList;

bool writeBytes(const std::string &path, const char *data, size_t len, mode_t mode)
{
	int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, mode);
	if (fd < 0)
		return false;
	ssize_t n = write(fd, data, len);
	if (close(fd) != 0)
		return false;
	return n == (ssize_t) len;
}

bool writeText(const std::string &path, const std::string &body)
{
	return writeBytes(path, body.data(), body.size(), 0600);
}

std::string readAll(const std::string &path)
{
	FILE *f = fopen(path.c_str(), "r");
	if (f == NULL)
		return std::string();
	char buf[512];
	size_t got = fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);
	buf[got] = '\0';
	return std::string(buf);
}

const daemons::Entry &named(const std::vector<daemons::Entry> &all, const char *name)
{
	for (size_t i = 0; i < all.size(); i++)
	{
		if (all[i].name == name)
			return all[i];
	}
	FAIL("the list carries no daemon called " << name);
	return all[0];
}

bool runs(const std::vector<daemons::Entry> &all, const char *name)
{
	return named(all, name).running;
}

bool installed(const std::vector<daemons::Entry> &all, const char *name)
{
	return named(all, name).installed;
}

// A process table and a directory of scripts of its own, and both settings put
// back the way they were found.
struct Fixture
{
	std::string      dir;
	std::string      proc_before;
	std::string      system_before;
	std::string      root_before;
	struct sigaction chld_before;
	bool             chld_saved;

	Fixture() : chld_saved(false)
	{
		char tmpl[] = "/tmp/coreapi_daemons_XXXXXX";
		if (mkdtemp(tmpl) != NULL)
			dir = tmpl;
		proc_before = daemons::procRoot();
		system_before = daemons::systemRoot();
		root_before = shell::scriptRoot();
		/* A machine running the suite has an /etc/init.d of its own, and
		   dropbear or crond may well be in it. Pointed at nothing, so a case
		   that says nothing about what is installed reads the same answer
		   wherever it runs. */
		daemons::setSystemRoot(dir + "/nowhere");

		// The runner refuses to start anything in a process that discards what
		// its children exit with, and another case may have left it that way.
		struct sigaction dfl;
		chld_saved = (sigaction(SIGCHLD, NULL, &chld_before) == 0);
		dfl.sa_handler = SIG_DFL;
		dfl.sa_flags = 0;
		sigemptyset(&dfl.sa_mask);
		sigaction(SIGCHLD, &dfl, NULL);
	}

	~Fixture()
	{
		daemons::setProcRoot(proc_before);
		daemons::setSystemRoot(system_before);
		shell::setScriptRoot(root_before);
		if (chld_saved)
			sigaction(SIGCHLD, &chld_before, NULL);
		if (!dir.empty())
			(void) system(("rm -rf " + dir).c_str());
	}

	std::string at(const std::string &name) const { return dir + "/" + name; }
};

} // namespace

TEST_CASE("the daemons are the ones the table can drive", "[daemons]")
{
	Fixture fx;
	REQUIRE_FALSE(fx.dir.empty());
	REQUIRE(mkdir(fx.at("proc").c_str(), 0700) == 0);
	daemons::setProcRoot(fx.at("proc"));

	DaemonList r = daemons::list();
	REQUIRE(r.ok());
	const std::vector<daemons::Entry> &all = r.value();

	static const char *const expected[] = {
		"mgcamd", "gbox", "oscam", "osmod", "ncam", "cccam", "doscam",
		"fritzcallmonitor", "nfs", "samba", "tuxcald", "tuxmaild",
		"inadyn", "dropbear", "ushare", "djmount", "minidlna", "xupnpd",
		"crond"
	};
	REQUIRE(all.size() == sizeof(expected) / sizeof(expected[0]));
	for (size_t i = 0; i < all.size(); i++)
		REQUIRE(all[i].name == expected[i]);

	// Every name in the list can be started and stopped, or the list would be
	// offering something nothing behind it carries.
	for (size_t i = 0; i < all.size(); i++)
	{
		REQUIRE(shell::find(all[i].name + ".start") != NULL);
		REQUIRE(shell::find(all[i].name + ".stop") != NULL);
	}

	/* And every one of them says which of the two sorts it is, because the
	   list is drawn as two groups and a sort worked out from the name on the
	   other side would be a second copy of this table. The seven that restart
	   are the seven softcams: the branch that restarts one is the camd script's
	   own, and the services have a start and a stop and nothing else. */
	for (size_t i = 0; i < all.size(); i++)
	{
		INFO(all[i].name);
		const bool camd = all[i].kind == daemons::Kind::Softcam;
		REQUIRE(camd == (shell::find(all[i].name + ".restart") != NULL));
	}
}

TEST_CASE("a name every image knows is not a thing this box carries", "[daemons]")
{
	Fixture fx;
	REQUIRE_FALSE(fx.dir.empty());
	const std::string root = fx.at("box");
	REQUIRE(mkdir(root.c_str(), 0700) == 0);
	REQUIRE(mkdir((root + "/var").c_str(), 0700) == 0);
	REQUIRE(mkdir((root + "/var/bin").c_str(), 0700) == 0);
	REQUIRE(mkdir((root + "/etc").c_str(), 0700) == 0);
	REQUIRE(mkdir((root + "/etc/init.d").c_str(), 0700) == 0);
	REQUIRE(mkdir((root + "/var/etc").c_str(), 0700) == 0);
	REQUIRE(mkdir((root + "/var/etc/init.d").c_str(), 0700) == 0);

	// A softcam is where the television's own softcam screen looks for it.
	REQUIRE(writeBytes(root + "/var/bin/oscam", "#!/bin/sh\n", 10, 0700));
	// A service is an executable init script in either of the two directories
	// the box's own service command searches.
	REQUIRE(writeBytes(root + "/etc/init.d/dropbear", "#!/bin/sh\n", 10, 0700));
	REQUIRE(writeBytes(root + "/var/etc/init.d/xupnpd", "#!/bin/sh\n", 10, 0700));
	// There and not runnable is not there: the box cannot start it either.
	REQUIRE(writeBytes(root + "/etc/init.d/samba", "#!/bin/sh\n", 10, 0600));
	// And an init script is not what makes a softcam present.
	REQUIRE(writeBytes(root + "/etc/init.d/ncam", "#!/bin/sh\n", 10, 0700));

	daemons::setSystemRoot(root);
	DaemonList r = daemons::list();
	REQUIRE(r.ok());
	const std::vector<daemons::Entry> &all = r.value();

	REQUIRE(installed(all, "oscam"));
	REQUIRE(installed(all, "dropbear"));
	REQUIRE(installed(all, "xupnpd"));
	REQUIRE_FALSE(installed(all, "samba"));
	REQUIRE_FALSE(installed(all, "ncam"));
	REQUIRE_FALSE(installed(all, "fritzcallmonitor"));

	// The name it is looked for under is the init script's and not the
	// program's, and those two differ for three of the twelve.
	REQUIRE(writeBytes(root + "/etc/init.d/smbd", "#!/bin/sh\n", 10, 0700));
	REQUIRE(writeBytes(root + "/etc/init.d/minidlnad", "#!/bin/sh\n", 10, 0700));
	DaemonList again = daemons::list();
	REQUIRE(again.ok());
	REQUIRE_FALSE(installed(again.value(), "samba"));
	REQUIRE(installed(again.value(), "minidlna"));
}

TEST_CASE("what is installed and what is running are two questions", "[daemons]")
{
	Fixture fx;
	REQUIRE_FALSE(fx.dir.empty());
	const std::string proc = fx.at("proc");
	REQUIRE(mkdir(proc.c_str(), 0700) == 0);
	REQUIRE(mkdir((proc + "/101").c_str(), 0700) == 0);
	REQUIRE(writeText(proc + "/101/comm", "oscam\n"));
	daemons::setProcRoot(proc);

	// Nothing is installed under this root, and one of them is running all the
	// same: a program started by hand out of a directory nothing here names is
	// still a process, and saying it is not there would be the reading that is
	// wrong.
	DaemonList r = daemons::list();
	REQUIRE(r.ok());
	REQUIRE(runs(r.value(), "oscam"));
	REQUIRE_FALSE(installed(r.value(), "oscam"));
}

TEST_CASE("running is read from the process table", "[daemons]")
{
	Fixture fx;
	REQUIRE_FALSE(fx.dir.empty());
	const std::string proc = fx.at("proc");
	REQUIRE(mkdir(proc.c_str(), 0700) == 0);

	REQUIRE(mkdir((proc + "/101").c_str(), 0700) == 0);
	REQUIRE(writeText(proc + "/101/comm", "oscam\n"));

	// Its name in the process table is cut at fifteen characters, so only the
	// arguments still carry the whole of it.
	REQUIRE(mkdir((proc + "/102").c_str(), 0700) == 0);
	REQUIRE(writeText(proc + "/102/comm", "fritzcallmonito\n"));
	static const char kFcm[] = "/usr/bin/fritzcallmonitor\0-d";
	REQUIRE(writeBytes(proc + "/102/cmdline", kFcm, sizeof(kFcm) - 1, 0600));

	// The daemon and the program it runs are not called the same thing.
	REQUIRE(mkdir((proc + "/103").c_str(), 0700) == 0);
	REQUIRE(writeText(proc + "/103/comm", "nfsd\n"));

	// Not a process at all, so whatever it says is not an answer.
	REQUIRE(mkdir((proc + "/notapid").c_str(), 0700) == 0);
	REQUIRE(writeText(proc + "/notapid/comm", "ncam\n"));

	daemons::setProcRoot(proc);
	DaemonList r = daemons::list();
	REQUIRE(r.ok());
	const std::vector<daemons::Entry> &all = r.value();

	REQUIRE(runs(all, "oscam"));
	REQUIRE(runs(all, "fritzcallmonitor"));
	REQUIRE(runs(all, "nfs"));
	REQUIRE_FALSE(runs(all, "ncam"));
	REQUIRE_FALSE(runs(all, "gbox"));
	REQUIRE_FALSE(runs(all, "crond"));
}

TEST_CASE("a process table that cannot be read leaves everything stopped", "[daemons]")
{
	Fixture fx;
	daemons::setProcRoot(fx.at("no-such-process-table"));

	DaemonList r = daemons::list();
	REQUIRE(r.ok());
	const std::vector<daemons::Entry> &all = r.value();
	REQUIRE_FALSE(all.empty());
	for (size_t i = 0; i < all.size(); i++)
		REQUIRE_FALSE(all[i].running);
}

TEST_CASE("a daemon this box has no name for is not found", "[daemons]")
{
	Result<void> started = daemons::start("nosuchcam");
	REQUIRE_FALSE(started.ok());
	REQUIRE(started.error().status == Status::NotFound);
	REQUIRE(started.error().code == ErrorCode::NoSuchDaemon);

	Result<void> stopped = daemons::stop("nosuchcam");
	REQUIRE_FALSE(stopped.ok());
	REQUIRE(stopped.error().status == Status::NotFound);
	REQUIRE(stopped.error().code == ErrorCode::NoSuchDaemon);

	Result<void> restarted = daemons::restart("nosuchcam");
	REQUIRE_FALSE(restarted.ok());
	REQUIRE(restarted.error().status == Status::NotFound);
	REQUIRE(restarted.error().code == ErrorCode::NoSuchDaemon);

	// A name the table happens to carry a command for is still not a daemon:
	// the two lists are asked in that order and the first one decides.
	Result<void> command = daemons::start("oscam.start");
	REQUIRE_FALSE(command.ok());
	REQUIRE(command.error().code == ErrorCode::NoSuchDaemon);
}

TEST_CASE("a daemon with no branch for a verb says so and starts nothing", "[daemons]")
{
	Fixture fx;
	REQUIRE_FALSE(fx.dir.empty());
	REQUIRE(writeBytes(fx.at("Y_Plugins.sh"),
			   "#!/bin/sh\nprintf 'ran\\n'\n", 25, 0700));
	shell::setScriptRoot(fx.dir);

	// The plugin scripts have a start and a stop and nothing that restarts.
	Result<void> r = daemons::restart("dropbear");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::NotSupported);
	// Its own code, not the one a name nothing knows would get: the caller has
	// to be able to tell a daemon it may not restart from one that is not there.
	REQUIRE(r.error().code == ErrorCode::NoSuchVerb);

	// A camd has all three.
	REQUIRE(shell::find("oscam.restart") != NULL);
}

TEST_CASE("starting a daemon runs the branch the table names", "[daemons]")
{
	Fixture fx;
	REQUIRE_FALSE(fx.dir.empty());
	// The two scripts write different marks, so the record says which of them
	// ran and not only which branch was asked for.
	const std::string camds =
		"#!/bin/sh\nprintf 'camds %s\\n' \"$1\" >> " + fx.at("argv") + "\nprintf 'ok\\n'\n";
	REQUIRE(writeBytes(fx.at("Y_Camds.sh"), camds.data(), camds.size(), 0700));
	const std::string plugins =
		"#!/bin/sh\nprintf 'plugins %s\\n' \"$1\" >> " + fx.at("argv") + "\nprintf 'ok\\n'\n";
	REQUIRE(writeBytes(fx.at("Y_Plugins.sh"), plugins.data(), plugins.size(), 0700));
	shell::setScriptRoot(fx.dir);

	REQUIRE(daemons::start("oscam").ok());
	REQUIRE(daemons::stop("cccam").ok());
	REQUIRE(daemons::restart("mgcamd").ok());
	REQUIRE(daemons::start("dropbear").ok());
	REQUIRE(daemons::stop("minidlna").ok());

	REQUIRE(readAll(fx.at("argv")) ==
		"camds oscam_start\n"
		"camds cccam_stop\n"
		"camds mgcamd_reset\n"
		"plugins dropbear_start\n"
		"plugins minidlna_stop\n");
}

TEST_CASE("a script that failed is carried up rather than read as success", "[daemons]")
{
	Fixture fx;
	REQUIRE_FALSE(fx.dir.empty());
	REQUIRE(writeBytes(fx.at("Y_Camds.sh"), "#!/bin/sh\nexit 3\n", 17, 0700));
	shell::setScriptRoot(fx.dir);

	Result<void> r = daemons::start("oscam");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);
	// The runner's own code, carried up as it was given rather than replaced
	// with one of this layer's, or a caller cannot tell a script that failed
	// from one that was never there.
	REQUIRE(r.error().code == ErrorCode::ExitStatus);
}

TEST_CASE("a script that is not there is told apart from one that failed", "[daemons]")
{
	Fixture fx;
	REQUIRE_FALSE(fx.dir.empty());
	shell::setScriptRoot(fx.dir);

	Result<void> r = daemons::start("oscam");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::NotFound);
	REQUIRE(r.error().code == ErrorCode::ScriptMissing);
}

TEST_CASE("each daemon is looked for under the name its program carries", "[daemons]")
{
	Fixture fx;
	REQUIRE_FALSE(fx.dir.empty());
	const std::string proc = fx.at("proc");
	REQUIRE(mkdir(proc.c_str(), 0700) == 0);

	// The daemon and the program are not called the same thing wherever the
	// init script and the binary disagree, and a mapping that drifted would
	// report a running daemon as stopped with nothing else failing.
	struct Pair { const char *daemon; const char *process; };
	static const Pair expected[] = {
		{ "mgcamd", "mgcamd" },
		{ "gbox", "gbox" },
		{ "oscam", "oscam" },
		{ "osmod", "osmod" },
		{ "ncam", "ncam" },
		{ "cccam", "cccam" },
		{ "doscam", "doscam" },
		{ "fritzcallmonitor", "fritzcallmonitor" },
		{ "nfs", "nfsd" },
		{ "samba", "smbd" },
		{ "tuxcald", "tuxcald" },
		{ "tuxmaild", "tuxmaild" },
		{ "inadyn", "inadyn" },
		{ "dropbear", "dropbear" },
		{ "ushare", "ushare" },
		{ "djmount", "djmount" },
		{ "minidlna", "minidlnad" },
		{ "xupnpd", "xupnpd" },
		{ "crond", "crond" }
	};
	const size_t count = sizeof(expected) / sizeof(expected[0]);

	for (size_t i = 0; i < count; i++)
	{
		char number[16];
		snprintf(number, sizeof(number), "%d", (int) (1000 + i));
		const std::string pid = proc + "/" + number;
		REQUIRE(mkdir(pid.c_str(), 0700) == 0);
		REQUIRE(writeText(pid + "/comm", std::string(expected[i].process) + "\n"));
	}

	daemons::setProcRoot(proc);
	DaemonList r = daemons::list();
	REQUIRE(r.ok());
	const std::vector<daemons::Entry> &all = r.value();
	REQUIRE(all.size() == count);
	for (size_t i = 0; i < count; i++)
	{
		REQUIRE(all[i].name == expected[i].daemon);
		REQUIRE(all[i].running);
	}
}
