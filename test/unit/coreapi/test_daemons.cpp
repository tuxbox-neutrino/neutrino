/*
 * test_daemons.cpp - tests for the daemon list
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (fixes)
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

// A process table and a box of its own, and both settings put back the way
// they were found.
struct Fixture
{
	std::string      dir;
	std::string      proc_before;
	std::string      system_before;
	struct sigaction chld_before;
	bool             chld_saved;

	Fixture() : chld_saved(false)
	{
		char tmpl[] = "/tmp/coreapi_daemons_XXXXXX";
		if (mkdtemp(tmpl) != NULL)
			dir = tmpl;
		proc_before = daemons::procRoot();
		system_before = daemons::systemRoot();
		/* A machine running the suite has an /etc/init.d and a systemctl
		   of its own, and dropbear or crond may well be in it. Pointed at
		   nothing, so a case that says nothing about what is installed reads
		   the same answer wherever it runs. */
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
		if (chld_saved)
			sigaction(SIGCHLD, &chld_before, NULL);
		if (!dir.empty())
			(void) system(("rm -rf " + dir).c_str());
	}

	std::string at(const std::string &name) const { return dir + "/" + name; }

	/* A box under box/: run by systemd or not, with a systemctl that writes
	   what it was handed to argv and answers as told. */
	std::string box(bool systemd, int exit_code = 0) const
	{
		const std::string root = at("box");
		const char *const dirs[] = { "", "/bin", "/etc", "/etc/init.d", "/var", "/var/etc",
					     "/var/etc/init.d", "/lib", "/lib/systemd", "/lib/systemd/system",
					     "/run", "/run/systemd", "/run/systemd/system" };
		for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++)
		{
			if (!systemd && std::string(dirs[i]) == "/run/systemd/system")
				continue;
			mkdir((root + dirs[i]).c_str(), 0700);
		}
		char code[16];
		snprintf(code, sizeof(code), "%d", exit_code);
		const std::string systemctl = "#!/bin/sh\nprintf 'systemctl %s\\n' \"$*\" >> " + at("argv") +
			"\nexit " + code + "\n";
		writeBytes(root + "/bin/systemctl", systemctl.data(), systemctl.size(), 0700);
		return root;
	}

	// An init script that writes its own name and what it was handed to argv.
	void script(const std::string &path) const
	{
		const std::string body = "#!/bin/sh\nprintf '%s %s\\n' \"${0##*/}\" \"$*\" >> " + at("argv") + "\n";
		writeBytes(path, body.data(), body.size(), 0700);
	}
};

} // namespace

TEST_CASE("the daemons are the services the box can drive", "[daemons]")
{
	Fixture fx;
	REQUIRE_FALSE(fx.dir.empty());
	REQUIRE(mkdir(fx.at("proc").c_str(), 0700) == 0);
	daemons::setProcRoot(fx.at("proc"));

	DaemonList r = daemons::list();
	REQUIRE(r.ok());
	const std::vector<daemons::Entry> &all = r.value();

	// Softcams are left out until they come as extensions from another source.
	static const char *const expected[] = {
		"fritzcallmonitor", "nfs", "samba", "tuxcald", "tuxmaild",
		"inadyn", "dropbear", "ushare", "djmount", "minidlna", "xupnpd",
		"crond"
	};
	REQUIRE(all.size() == sizeof(expected) / sizeof(expected[0]));
	for (size_t i = 0; i < all.size(); i++)
	{
		REQUIRE(all[i].name == expected[i]);
		REQUIRE(all[i].kind == daemons::Kind::Service);
	}
}

TEST_CASE("on a box run by systemd a service is there by its unit or its init script", "[daemons]")
{
	Fixture fx;
	REQUIRE_FALSE(fx.dir.empty());
	const std::string root = fx.box(true);

	// a unit file in one of the unit directories, under the unit's own name
	REQUIRE(writeText(root + "/lib/systemd/system/smb.service", "[Unit]\n"));
	REQUIRE(writeText(root + "/lib/systemd/system/crond.service", "[Unit]\n"));
	// an init script in /etc/init.d, which systemd makes a unit of
	fx.script(root + "/etc/init.d/tuxcald");
	// one in the second init directory, run as it is
	fx.script(root + "/var/etc/init.d/xupnpd");
	// there and not runnable is not there: the box cannot start it either
	REQUIRE(writeBytes(root + "/etc/init.d/inadyn", "#!/bin/sh\n", 10, 0600));
	// a unit under the program's name and not the unit's is not that unit
	REQUIRE(writeText(root + "/lib/systemd/system/samba.service", "[Unit]\n"));
	REQUIRE(writeText(root + "/lib/systemd/system/minidlnad.service", "[Unit]\n"));

	daemons::setSystemRoot(root);
	DaemonList r = daemons::list();
	REQUIRE(r.ok());
	const std::vector<daemons::Entry> &all = r.value();
	REQUIRE(installed(all, "samba"));
	REQUIRE(installed(all, "crond"));
	REQUIRE(installed(all, "tuxcald"));
	REQUIRE(installed(all, "xupnpd"));
	REQUIRE_FALSE(installed(all, "inadyn"));
	REQUIRE_FALSE(installed(all, "minidlna"));
	REQUIRE_FALSE(installed(all, "dropbear"));

	// without a systemctl to hand them to, the units are not a way to start anything
	REQUIRE(unlink((root + "/bin/systemctl").c_str()) == 0);
	DaemonList bare = daemons::list();
	REQUIRE(bare.ok());
	REQUIRE_FALSE(installed(bare.value(), "samba"));
	REQUIRE(installed(bare.value(), "tuxcald"));
	REQUIRE(installed(bare.value(), "xupnpd"));
}

TEST_CASE("on a box without systemd a service is there by its init script", "[daemons]")
{
	Fixture fx;
	REQUIRE_FALSE(fx.dir.empty());
	const std::string root = fx.box(false);

	REQUIRE(writeText(root + "/lib/systemd/system/smb.service", "[Unit]\n"));
	fx.script(root + "/etc/init.d/dropbear");
	fx.script(root + "/var/etc/init.d/minidlnad");

	daemons::setSystemRoot(root);
	DaemonList r = daemons::list();
	REQUIRE(r.ok());
	const std::vector<daemons::Entry> &all = r.value();
	REQUIRE_FALSE(installed(all, "samba"));
	REQUIRE(installed(all, "dropbear"));
	REQUIRE(installed(all, "minidlna"));
	REQUIRE_FALSE(installed(all, "fritzcallmonitor"));
}

TEST_CASE("what is installed and what is running are two questions", "[daemons]")
{
	Fixture fx;
	REQUIRE_FALSE(fx.dir.empty());
	const std::string proc = fx.at("proc");
	REQUIRE(mkdir(proc.c_str(), 0700) == 0);
	REQUIRE(mkdir((proc + "/101").c_str(), 0700) == 0);
	REQUIRE(writeText(proc + "/101/comm", "crond\n"));
	daemons::setProcRoot(proc);

	// Nothing is installed under this root, and one of them is running all the
	// same: a program started by hand out of a directory nothing here names is
	// still a process, and saying it is not there would be the reading that is
	// wrong.
	DaemonList r = daemons::list();
	REQUIRE(r.ok());
	REQUIRE(runs(r.value(), "crond"));
	REQUIRE_FALSE(installed(r.value(), "crond"));
}

TEST_CASE("running is read from the process table", "[daemons]")
{
	Fixture fx;
	REQUIRE_FALSE(fx.dir.empty());
	const std::string proc = fx.at("proc");
	REQUIRE(mkdir(proc.c_str(), 0700) == 0);

	REQUIRE(mkdir((proc + "/101").c_str(), 0700) == 0);
	REQUIRE(writeText(proc + "/101/comm", "crond\n"));

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
	REQUIRE(writeText(proc + "/notapid/comm", "xupnpd\n"));

	daemons::setProcRoot(proc);
	DaemonList r = daemons::list();
	REQUIRE(r.ok());
	const std::vector<daemons::Entry> &all = r.value();

	REQUIRE(runs(all, "crond"));
	REQUIRE(runs(all, "fritzcallmonitor"));
	REQUIRE(runs(all, "nfs"));
	REQUIRE_FALSE(runs(all, "xupnpd"));
	REQUIRE_FALSE(runs(all, "samba"));
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
	const char *const names[] = { "nosuchcam", "oscam", "samba.start" };
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
	{
		INFO(names[i]);
		Result<void> started = daemons::start(names[i]);
		REQUIRE_FALSE(started.ok());
		REQUIRE(started.error().status == Status::NotFound);
		REQUIRE(started.error().code == ErrorCode::NoSuchDaemon);

		Result<void> stopped = daemons::stop(names[i]);
		REQUIRE_FALSE(stopped.ok());
		REQUIRE(stopped.error().code == ErrorCode::NoSuchDaemon);

		Result<void> restarted = daemons::restart(names[i]);
		REQUIRE_FALSE(restarted.ok());
		REQUIRE(restarted.error().code == ErrorCode::NoSuchDaemon);
	}
}

TEST_CASE("a service is driven with systemctl and its unit where systemd runs the box", "[daemons]")
{
	Fixture fx;
	REQUIRE_FALSE(fx.dir.empty());
	const std::string root = fx.box(true);
	REQUIRE(writeText(root + "/lib/systemd/system/smb.service", "[Unit]\n"));
	REQUIRE(writeText(root + "/lib/systemd/system/crond.service", "[Unit]\n"));
	fx.script(root + "/etc/init.d/tuxcald");
	fx.script(root + "/var/etc/init.d/xupnpd");
	daemons::setSystemRoot(root);

	REQUIRE(daemons::start("samba").ok());
	REQUIRE(daemons::stop("tuxcald").ok());
	REQUIRE(daemons::restart("crond").ok());
	REQUIRE(daemons::start("xupnpd").ok());

	REQUIRE(readAll(fx.at("argv")) ==
		"systemctl start smb.service\n"
		"systemctl stop tuxcald.service\n"
		"systemctl restart crond.service\n"
		"xupnpd start\n");
}

TEST_CASE("a service is driven with its init script where systemd does not run the box", "[daemons]")
{
	Fixture fx;
	REQUIRE_FALSE(fx.dir.empty());
	const std::string root = fx.box(false);
	REQUIRE(writeText(root + "/lib/systemd/system/smb.service", "[Unit]\n"));
	fx.script(root + "/etc/init.d/samba");
	fx.script(root + "/var/etc/init.d/minidlnad");
	daemons::setSystemRoot(root);

	REQUIRE(daemons::start("samba").ok());
	REQUIRE(daemons::restart("minidlna").ok());
	REQUIRE(daemons::stop("samba").ok());

	REQUIRE(readAll(fx.at("argv")) ==
		"samba start\n"
		"minidlnad restart\n"
		"samba stop\n");
}

TEST_CASE("a service this box does not carry is said, and nothing runs", "[daemons]")
{
	Fixture fx;
	REQUIRE_FALSE(fx.dir.empty());
	daemons::setSystemRoot(fx.box(true));

	Result<void> r = daemons::start("samba");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::NotFound);
	REQUIRE(r.error().code == ErrorCode::ScriptMissing);
	REQUIRE(readAll(fx.at("argv")).empty());
}

TEST_CASE("a command that failed is carried up rather than read as success", "[daemons]")
{
	Fixture fx;
	REQUIRE_FALSE(fx.dir.empty());
	const std::string root = fx.box(true, 5);
	REQUIRE(writeText(root + "/lib/systemd/system/smb.service", "[Unit]\n"));
	daemons::setSystemRoot(root);

	Result<void> r = daemons::start("samba");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);
	// The runner's own code, carried up as it was given rather than replaced
	// with one of this layer's, or a caller cannot tell a command that failed
	// from one that was never there.
	REQUIRE(r.error().code == ErrorCode::ExitStatus);
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
