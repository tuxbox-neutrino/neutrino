/*
 * test_shellcontract.cpp - tests for the shell contract
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
#include "coreapi/shellcontract.h"
#include "coreapi/box/shellcontract_internal.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#include <dlfcn.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

using namespace coreapi;

namespace
{

typedef std::vector<std::pair<std::string, std::string> > ArgList;

ArgList arg(const std::string &k, const std::string &v)
{
	ArgList a;
	a.push_back(std::make_pair(k, v));
	return a;
}

ArgList arg2(const std::string &k1, const std::string &v1, const std::string &k2, const std::string &v2)
{
	ArgList a;
	a.push_back(std::make_pair(k1, v1));
	a.push_back(std::make_pair(k2, v2));
	return a;
}

bool writeFile(const std::string &path, const std::string &body, mode_t mode)
{
	int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, mode);
	if (fd < 0)
		return false;
	ssize_t n = write(fd, body.data(), body.size());
	if (close(fd) != 0)
		return false;
	return n == (ssize_t)body.size();
}

int64_t nowMs()
{
	struct timespec t;
	if (clock_gettime(CLOCK_MONOTONIC, &t) != 0)
		return 0;
	return (int64_t)t.tv_sec * 1000 + (int64_t)(t.tv_nsec / 1000000);
}

// A zombie still answers kill(pid, 0), so the state field is what separates a
// process that is gone from one that is merely waiting to be collected.
bool stillRunning(pid_t pid)
{
	char path[64];
	snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
	FILE *f = fopen(path, "r");
	if (f == NULL)
		return false;
	char buf[512];
	size_t n = fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);
	buf[n] = '\0';
	const char *end = strrchr(buf, ')');
	if (end == NULL || end[1] == '\0' || end[2] == '\0')
		return false;
	return end[2] != 'Z' && end[2] != 'X';
}

// A signal set in /proc/<pid>/status is one hexadecimal word. A field that is
// not there reads as set, so a miss fails the case instead of passing it.
bool holdsTerm(const std::string &status, const char *field)
{
	size_t at = status.find(field);
	if (at == std::string::npos)
		return true;
	size_t start = status.find_first_of("0123456789abcdefABCDEF", at + strlen(field));
	if (start == std::string::npos)
		return true;
	unsigned long long bits = strtoull(status.c_str() + start, NULL, 16);
	return (bits & (1ULL << (SIGTERM - 1))) != 0;
}

// The scripts the cases below need. Built once, on the first case that asks.
struct Scripts
{
	bool        ready;
	std::string dir;
	std::string root;

	Scripts() : ready(false)
	{
		char tmpl[] = "/tmp/coreapi_shell_XXXXXX";
		if (mkdtemp(tmpl) == NULL)
			return;
		dir = tmpl;
		root = dir + "/root";
		if (mkdir(root.c_str(), 0700) != 0)
			return;
		if (!writeFile(root + "/ok.txt", "content\n", 0600))
			return;
		if (mkdir((root + "/sub").c_str(), 0700) != 0)
			return;
		// A way out of the root that holds no ".." at all, so that what
		// refuses it can only be the resolving.
		if (symlink("/etc", (root + "/esc").c_str()) != 0)
			return;
		if (!writeFile(dir + "/leakfile", "x\n", 0600))
			return;
		if (!writeFile(dir + "/echo1.sh",
			       "#!/bin/sh\nprintf '%s\\n' \"$1\"\n", 0700))
			return;
		if (!writeFile(dir + "/echo2.sh",
			       "#!/bin/sh\nprintf '%s|%s\\n' \"$1\" \"$2\"\n", 0700))
			return;
		if (!writeFile(dir + "/fdcheck.sh",
			       "#!/bin/sh\nif [ -e \"/proc/self/fd/$1\" ] ; then printf 'LEAK\\n' ; "
			       "else printf 'CLEAN\\n' ; fi\n", 0700))
			return;
		if (!writeFile(dir + "/marker.sh",
			       "#!/bin/sh\nprintf 'ran\\n' > " + dir + "/marker\nprintf 'ran\\n'\n", 0700))
			return;
		if (!writeFile(dir + "/endless.sh",
			       "#!/bin/sh\nwhile : ; do printf '%s\\n' "
			       "0123456789012345678901234567890123456789 ; done\n", 0700))
			return;
		// The backgrounded part ignores the first signal, so only the second
		// one ends it.
		if (!writeFile(dir + "/stubborn.sh",
			       "#!/bin/sh\nprintf '%s\\n' \"$$\" > " + dir + "/pids2\n"
			       "( trap '' TERM ; sleep 300 ) &\n"
			       "printf '%s\\n' \"$!\" >> " + dir + "/pids2\nsleep 300\n", 0700))
			return;
		if (!writeFile(dir + "/slow.sh",
			       "#!/bin/sh\nsleep 0.3\nprintf 'x\\n'\n", 0700))
			return;
		if (!writeFile(dir + "/argsall.sh",
			       "#!/bin/sh\nfor a in \"$@\" ; do printf '%s|' \"$a\" ; done\nprintf '\\n'\n", 0700))
			return;
		if (!writeFile(dir + "/noisy.sh",
			       "#!/bin/sh\nprintf 'err\\n' >&2\nprintf 'out\\n'\n", 0700))
			return;
		if (!writeFile(dir + "/quiet.sh",
			       "#!/bin/sh\nprintf 'done\\n'\nexec 1>&-\nsleep 300\n", 0700))
			return;
		if (!writeFile(dir + "/hang.sh",
			       "#!/bin/sh\nprintf '%s\\n' \"$$\" > " + dir + "/pids\n"
			       "sleep 300 &\nprintf '%s\\n' \"$!\" >> " + dir + "/pids\nsleep 300\n", 0700))
			return;
		ready = true;
	}

	~Scripts()
	{
		if (dir.empty())
			return;
		static const char *const names[] = {
			"echo1.sh", "echo2.sh", "fdcheck.sh", "marker.sh",
			"endless.sh", "hang.sh", "quiet.sh", "noisy.sh", "stubborn.sh", "argsall.sh", "slow.sh",
			"leakfile", "marker", "pids", "pids2"
		};
		for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
			unlink((dir + "/" + names[i]).c_str());
		unlink((root + "/ok.txt").c_str());
		unlink((root + "/esc").c_str());
		rmdir((root + "/sub").c_str());
		rmdir(root.c_str());
		rmdir(dir.c_str());
	}
};

const Scripts &fx()
{
	static Scripts s;
	return s;
}

const Param echo_params[] = {
	{ "text", ParamType::String, true, "text to echo", 0, 0, 0, 0 }
};

const ShellCommand echo_cmd = {
	"/bin/echo", "", echo_params, 1, OutputFormat::Raw, 2000, 4096, false
};

const Param num_params[] = {
	{ "count", ParamType::Int, true, "how many", 1, 10, 0, 0 }
};

const ShellCommand num_cmd = {
	"/bin/echo", "", num_params, 1, OutputFormat::Raw, 2000, 4096, false
};

void runMany(const ShellCommand *cmd, int rounds, int *failures)
{
	int bad = 0;
	for (int i = 0; i < rounds; i++)
	{
		Result<std::string> r = shell::run(*cmd, arg("text", "x"));
		if (!r.ok() || r.value() != "x\n")
			bad++;
	}
	*failures = bad;
}

void ignoreChildrenAfter(int delay_ms)
{
	usleep(delay_ms * 1000);
	struct sigaction ignore;
	ignore.sa_handler = SIG_IGN;
	ignore.sa_flags = 0;
	sigemptyset(&ignore.sa_mask);
	sigaction(SIGCHLD, &ignore, NULL);
}

// A foreign fork lands between this layer's pipe2() and its own fork() only if
// something puts it there. The definition below does, so that the close on exec
// flag on the write end is the difference between an answer and a timeout.
volatile bool g_wedge_fork = false;
pid_t g_wedged = -1;

} // namespace

extern "C" pid_t fork(void)
{
	static pid_t (*real_fork)(void) = NULL;
	if (real_fork == NULL)
		real_fork = (pid_t (*)(void))dlsym(RTLD_NEXT, "fork");
	if (g_wedge_fork)
	{
		g_wedge_fork = false;
		pid_t other = real_fork();
		if (other == 0)
		{
			execl("/bin/sleep", "sleep", "5", (char *)NULL);
			_exit(127);
		}
		g_wedged = other;
	}
	return real_fork();
}

TEST_CASE("a parameter never becomes part of a command string", "[shell]")
{
	// Named with the process: the canary is never actually created while the
	// guard holds, but two runs racing the same fixed name would still both
	// unlink and fopen it out from under each other.
	const std::string marker = "/tmp/pwned." + std::to_string(getpid());
	const std::string touch_it = "a; touch " + marker;
	const std::string pipe_it = "$(id) `id` ${HOME} | tee " + marker;

	// If this were interpolated into a shell line, the semicolon would run.
	unlink(marker.c_str());
	Result<std::string> r = shell::run(echo_cmd, arg("text", touch_it));
	REQUIRE(r.ok());
	REQUIRE(r.value().find("touch " + marker) != std::string::npos);
	REQUIRE(r.value() == touch_it + "\n");

	FILE *f = fopen(marker.c_str(), "r");
	if (f != NULL)
		fclose(f);
	REQUIRE(f == NULL);

	Result<std::string> s = shell::run(echo_cmd, arg("text", pipe_it));
	REQUIRE(s.ok());
	REQUIRE(s.value() == pipe_it + "\n");

	f = fopen(marker.c_str(), "r");
	if (f != NULL)
		fclose(f);
	REQUIRE(f == NULL);
}

TEST_CASE("a missing required parameter is rejected before execution", "[shell]")
{
	ArgList none;
	Result<std::string> r = shell::run(echo_cmd, none);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
}

TEST_CASE("an out-of-range integer is rejected", "[shell]")
{
	REQUIRE(shell::run(num_cmd, arg("count", "5")).ok());

	Result<std::string> low = shell::run(num_cmd, arg("count", "0"));
	REQUIRE_FALSE(low.ok());
	REQUIRE(low.error().status == Status::InvalidArgument);

	Result<std::string> high = shell::run(num_cmd, arg("count", "11"));
	REQUIRE_FALSE(high.ok());
	REQUIRE(high.error().status == Status::InvalidArgument);

	Result<std::string> nan = shell::run(num_cmd, arg("count", "abc"));
	REQUIRE_FALSE(nan.ok());
	REQUIRE(nan.error().status == Status::InvalidArgument);

	Result<std::string> trailing = shell::run(num_cmd, arg("count", "5x"));
	REQUIRE_FALSE(trailing.ok());
	REQUIRE(trailing.error().status == Status::InvalidArgument);
}

TEST_CASE("a non-zero exit code is an error rather than an empty success", "[shell]")
{
	const ShellCommand false_cmd = { "/bin/false", "", 0, 0, OutputFormat::Raw, 2000, 4096, false };
	ArgList none;
	Result<std::string> r = shell::run(false_cmd, none);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);
}

TEST_CASE("a hanging script is killed and reported", "[shell]")
{
	const ShellCommand sleeper = { "/bin/sleep", "30", 0, 0, OutputFormat::Raw, 300, 4096, false };
	ArgList none;
	int64_t began = nowMs();
	Result<std::string> r = shell::run(sleeper, none);
	int64_t took = nowMs() - began;
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Busy);
	REQUIRE(took < 3000);
}

TEST_CASE("a script the table does not name absolutely is refused", "[shell]")
{
	const ShellCommand relative = { "echo", "", 0, 0, OutputFormat::Raw, 2000, 4096, false };
	ArgList none;
	Result<std::string> r = shell::run(relative, none);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);

	const ShellCommand walked = { "/bin/../bin/echo", "", 0, 0, OutputFormat::Raw, 2000, 4096, false };
	Result<std::string> w = shell::run(walked, none);
	REQUIRE_FALSE(w.ok());
	REQUIRE(w.error().status == Status::Internal);
}

TEST_CASE("a name the table does not declare is refused", "[shell]")
{
	Result<std::string> r = shell::run(echo_cmd, arg2("text", "hello", "extra", "x"));
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
}

TEST_CASE("a name given twice is refused", "[shell]")
{
	Result<std::string> r = shell::run(echo_cmd, arg2("text", "hello", "text", "goodbye"));
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
}

TEST_CASE("a value carrying a zero byte is refused", "[shell]")
{
	std::string sneaky("safe", 4);
	sneaky.push_back('\0');
	sneaky += "/../../etc/passwd";
	Result<std::string> r = shell::run(echo_cmd, arg("text", sneaky));
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
}

TEST_CASE("a value longer than this layer's own limit is refused", "[shell]")
{
	// Room for the whole value and the newline behind it, so that what
	// refuses the longer one is the check and not the output limit.
	const ShellCommand roomy = { "/bin/echo", "", echo_params, 1, OutputFormat::Raw, 2000, 8192, false };

	Result<std::string> r = shell::run(roomy, arg("text", std::string(4097, 'a')));
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(shell::run(roomy, arg("text", std::string(4096, 'a'))).ok());
}

TEST_CASE("a value that fails its check never reaches the script", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/marker.sh";
	std::string marker = fx().dir + "/marker";
	const Param p[] = { { "count", ParamType::Int, true, "how many", 1, 10, 0, 0 } };
	const ShellCommand cmd = { script.c_str(), "", p, 1, OutputFormat::Raw, 2000, 4096, false };

	unlink(marker.c_str());
	Result<std::string> bad = shell::run(cmd, arg("count", "99"));
	REQUIRE_FALSE(bad.ok());
	REQUIRE(bad.error().status == Status::InvalidArgument);
	REQUIRE(access(marker.c_str(), F_OK) != 0);

	// The same marker after a request that passes, so that its absence above
	// is the refusal and not a script that never writes it.
	Result<std::string> good = shell::run(cmd, arg("count", "5"));
	REQUIRE(good.ok());
	REQUIRE(access(marker.c_str(), F_OK) == 0);
}

TEST_CASE("a boolean reaches the script as the form this layer chose", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/echo1.sh";
	const Param p[] = { { "on", ParamType::Bool, true, "switch", 0, 0, 0, 0 } };
	const ShellCommand cmd = { script.c_str(), "", p, 1, OutputFormat::Raw, 2000, 4096, false };

	Result<std::string> t = shell::run(cmd, arg("on", "TRUE"));
	REQUIRE(t.ok());
	REQUIRE(t.value() == "1\n");

	Result<std::string> f = shell::run(cmd, arg("on", "off"));
	REQUIRE(f.ok());
	REQUIRE(f.value() == "0\n");

	Result<std::string> bad = shell::run(cmd, arg("on", "maybe"));
	REQUIRE_FALSE(bad.ok());
	REQUIRE(bad.error().status == Status::InvalidArgument);
}

TEST_CASE("an integer reaches the script as its decimal form", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/echo1.sh";
	const Param p[] = { { "count", ParamType::Int, true, "how many", 1, 10, 0, 0 } };
	const ShellCommand cmd = { script.c_str(), "", p, 1, OutputFormat::Raw, 2000, 4096, false };

	Result<std::string> r = shell::run(cmd, arg("count", "+7"));
	REQUIRE(r.ok());
	REQUIRE(r.value() == "7\n");
}

TEST_CASE("an enum value outside the list is refused", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/echo1.sh";
	const Param p[] = { { "mode", ParamType::Enum, true, "which", 0, 0, "on,off,auto", 0 } };
	const ShellCommand cmd = { script.c_str(), "", p, 1, OutputFormat::Raw, 2000, 4096, false };

	Result<std::string> good = shell::run(cmd, arg("mode", "auto"));
	REQUIRE(good.ok());
	REQUIRE(good.value() == "auto\n");

	Result<std::string> bad = shell::run(cmd, arg("mode", "maybe"));
	REQUIRE_FALSE(bad.ok());
	REQUIRE(bad.error().status == Status::InvalidArgument);

	// A prefix of a listed value, to pin that the whole element has to match.
	Result<std::string> part = shell::run(cmd, arg("mode", "of"));
	REQUIRE_FALSE(part.ok());
	REQUIRE(part.error().status == Status::InvalidArgument);
}

TEST_CASE("an enum whose table lists nothing is refused", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/echo1.sh";
	const Param p[] = { { "mode", ParamType::Enum, true, "which", 0, 0, 0, 0 } };
	const ShellCommand cmd = { script.c_str(), "", p, 1, OutputFormat::Raw, 2000, 4096, false };

	Result<std::string> r = shell::run(cmd, arg("mode", "on"));
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);
}

TEST_CASE("a parameter left out keeps the positions behind it", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/echo2.sh";
	const Param p[] = {
		{ "first",  ParamType::String, false, "may be left out", 0, 0, 0, 0 },
		{ "second", ParamType::String, true,  "must be there",   0, 0, 0, 0 }
	};
	const ShellCommand cmd = { script.c_str(), "", p, 2, OutputFormat::Raw, 2000, 4096, false };

	Result<std::string> r = shell::run(cmd, arg("second", "B"));
	REQUIRE(r.ok());
	REQUIRE(r.value() == "|B\n");

	// The order the caller wrote them in does not move them either.
	Result<std::string> both = shell::run(cmd, arg2("second", "B", "first", "A"));
	REQUIRE(both.ok());
	REQUIRE(both.value() == "A|B\n");
}

TEST_CASE("a path is handed to the script as what it resolved to", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/echo1.sh";
	const Param p[] = { { "file", ParamType::Path, true, "a file", 0, 0, 0, fx().root.c_str() } };
	const ShellCommand cmd = { script.c_str(), "", p, 1, OutputFormat::Raw, 2000, 4096, false };

	char rootbuf[PATH_MAX];
	REQUIRE(realpath(fx().root.c_str(), rootbuf) != NULL);

	Result<std::string> r = shell::run(cmd, arg("file", "ok.txt"));
	REQUIRE(r.ok());
	REQUIRE(r.value() == std::string(rootbuf) + "/ok.txt\n");

	Result<std::string> absolute = shell::run(cmd, arg("file", fx().root + "/ok.txt"));
	REQUIRE(absolute.ok());
	REQUIRE(absolute.value() == std::string(rootbuf) + "/ok.txt\n");
}

TEST_CASE("a path that walks up out of its directory is refused", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/echo1.sh";
	const Param p[] = { { "file", ParamType::Path, true, "a file", 0, 0, 0, fx().root.c_str() } };
	const ShellCommand cmd = { script.c_str(), "", p, 1, OutputFormat::Raw, 2000, 4096, false };

	Result<std::string> up = shell::run(cmd, arg("file", "../../etc/passwd"));
	REQUIRE_FALSE(up.ok());
	REQUIRE(up.error().status == Status::InvalidArgument);

	Result<std::string> buried = shell::run(cmd, arg("file", "sub/../../../etc/passwd"));
	REQUIRE_FALSE(buried.ok());
	REQUIRE(buried.error().status == Status::InvalidArgument);

	// This one comes back inside, so resolving alone would let it through.
	Result<std::string> returned = shell::run(cmd, arg("file", "../root/ok.txt"));
	REQUIRE_FALSE(returned.ok());
	REQUIRE(returned.error().status == Status::InvalidArgument);
}

TEST_CASE("a script that closes its output and keeps running is still cut", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/quiet.sh";
	const ShellCommand cmd = { script.c_str(), "", 0, 0, OutputFormat::Raw, 500, 4096, false };
	ArgList none;

	int64_t began = nowMs();
	Result<std::string> r = shell::run(cmd, none);
	int64_t took = nowMs() - began;

	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Busy);
	REQUIRE(took < 3000);
}

TEST_CASE("the verb the table names comes before the parameters", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/echo2.sh";
	const Param p[] = { { "text", ParamType::String, true, "text to echo", 0, 0, 0, 0 } };
	const ShellCommand cmd = { script.c_str(), "status", p, 1, OutputFormat::Raw, 2000, 4096, false };

	Result<std::string> r = shell::run(cmd, arg("text", "B"));
	REQUIRE(r.ok());
	REQUIRE(r.value() == "status|B\n");
}

TEST_CASE("a path outside the root is refused rather than pulled back into it", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/echo1.sh";
	const Param p[] = { { "file", ParamType::Path, true, "a file", 0, 0, 0, fx().root.c_str() } };
	const ShellCommand cmd = { script.c_str(), "", p, 1, OutputFormat::Raw, 2000, 4096, false };

	Result<std::string> outside = shell::run(cmd, arg("file", "/etc/passwd"));
	REQUIRE_FALSE(outside.ok());
	REQUIRE(outside.error().status == Status::InvalidArgument);

	// Same answer as for a name that resolves to nothing, so that the two
	// cannot be told apart from outside.
	Result<std::string> gone = shell::run(cmd, arg("file", "nosuch.txt"));
	REQUIRE_FALSE(gone.ok());
	REQUIRE(gone.error().status == Status::InvalidArgument);
	REQUIRE(gone.error().code == outside.error().code);
}

TEST_CASE("a path that leaves the root through a symlink is refused", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/echo1.sh";
	const Param p[] = { { "file", ParamType::Path, true, "a file", 0, 0, 0, fx().root.c_str() } };
	const ShellCommand cmd = { script.c_str(), "", p, 1, OutputFormat::Raw, 2000, 4096, false };

	Result<std::string> r = shell::run(cmd, arg("file", "esc/passwd"));
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
}

TEST_CASE("a path parameter whose table names no directory is refused", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/echo1.sh";
	const Param p[] = { { "file", ParamType::Path, true, "a file", 0, 0, 0, 0 } };
	const ShellCommand cmd = { script.c_str(), "", p, 1, OutputFormat::Raw, 2000, 4096, false };

	Result<std::string> r = shell::run(cmd, arg("file", "/etc/passwd"));
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);
}

TEST_CASE("a script that never exits is collected and the group it led is ended", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/hang.sh";
	std::string pidfile = fx().dir + "/pids";
	unlink(pidfile.c_str());
	const ShellCommand cmd = { script.c_str(), "", 0, 0, OutputFormat::Raw, 500, 4096, false };
	ArgList none;

	int64_t began = nowMs();
	Result<std::string> r = shell::run(cmd, none);
	int64_t took = nowMs() - began;

	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Busy);
	REQUIRE(took < 3000);

	FILE *f = fopen(pidfile.c_str(), "r");
	REQUIRE(f != NULL);
	int child = 0;
	int grandchild = 0;
	int read_back = fscanf(f, "%d %d", &child, &grandchild);
	fclose(f);
	REQUIRE(read_back == 2);
	REQUIRE(child > 0);
	REQUIRE(grandchild > 0);
	REQUIRE(child != grandchild);

	// ESRCH rather than a state check: a zombie would still be signalable,
	// so this is what says the child was collected and not just stopped.
	REQUIRE(kill((pid_t)child, 0) == -1);
	REQUIRE(errno == ESRCH);

	// The grandchild is nobody's child of ours to collect, so whether it has
	// been reaped is up to whatever adopted it; that it stopped running is
	// the part this layer answers for.
	bool gone = false;
	for (int i = 0; i < 200 && !gone; i++)
	{
		gone = !stillRunning((pid_t)grandchild);
		if (!gone)
			usleep(5000);
	}
	REQUIRE(gone);
}

TEST_CASE("a child in the group that ignores the first signal is ended by the second", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/stubborn.sh";
	std::string pidfile = fx().dir + "/pids2";
	unlink(pidfile.c_str());
	const ShellCommand cmd = { script.c_str(), "", 0, 0, OutputFormat::Raw, 500, 4096, false };
	ArgList none;

	Result<std::string> r = shell::run(cmd, none);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Busy);

	FILE *f = fopen(pidfile.c_str(), "r");
	REQUIRE(f != NULL);
	int shell_pid = 0;
	int stubborn = 0;
	int read_back = fscanf(f, "%d %d", &shell_pid, &stubborn);
	fclose(f);
	REQUIRE(read_back == 2);
	REQUIRE(stubborn > 0);

	bool gone = false;
	for (int i = 0; i < 200 && !gone; i++)
	{
		gone = !stillRunning((pid_t)stubborn);
		if (!gone)
			usleep(5000);
	}
	REQUIRE(gone);
}

TEST_CASE("a signal the caller blocked is let through before the script starts", "[shell]")
{
	// Not a shell script: dash clears an inherited mask on its own, which
	// would answer for this whether the child cleared it or not.
	const ShellCommand status = { "/bin/cat", "/proc/self/status", 0, 0,
				      OutputFormat::Raw, 5000, 65536, false };
	ArgList none;

	sigset_t block;
	sigset_t previous;
	REQUIRE(sigemptyset(&block) == 0);
	REQUIRE(sigaddset(&block, SIGTERM) == 0);
	REQUIRE(pthread_sigmask(SIG_BLOCK, &block, &previous) == 0);

	sigset_t current;
	int held = 0;
	if (pthread_sigmask(SIG_BLOCK, NULL, &current) == 0)
		held = sigismember(&current, SIGTERM);
	Result<std::string> r = shell::run(status, none);

	REQUIRE(pthread_sigmask(SIG_SETMASK, &previous, NULL) == 0);
	REQUIRE(held == 1);
	REQUIRE(r.ok());
	REQUIRE_FALSE(holdsTerm(r.value(), "SigBlk:"));
}

TEST_CASE("a signal the caller ignores is put back before the script starts", "[shell]")
{
	const ShellCommand status = { "/bin/cat", "/proc/self/status", 0, 0,
				      OutputFormat::Raw, 5000, 65536, false };
	ArgList none;

	struct sigaction ignore;
	struct sigaction previous;
	ignore.sa_handler = SIG_IGN;
	ignore.sa_flags = 0;
	REQUIRE(sigemptyset(&ignore.sa_mask) == 0);
	REQUIRE(sigaction(SIGTERM, &ignore, &previous) == 0);

	struct sigaction current;
	int ignored = 0;
	if (sigaction(SIGTERM, NULL, &current) == 0)
		ignored = (current.sa_handler == SIG_IGN) ? 1 : 0;
	Result<std::string> r = shell::run(status, none);

	REQUIRE(sigaction(SIGTERM, &previous, NULL) == 0);
	REQUIRE(ignored == 1);
	REQUIRE(r.ok());
	REQUIRE_FALSE(holdsTerm(r.value(), "SigIgn:"));
}

TEST_CASE("what the script writes to its error output stays out of the answer", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/noisy.sh";
	const ShellCommand cmd = { script.c_str(), "", 0, 0, OutputFormat::Raw, 2000, 4096, false };
	ArgList none;

	Result<std::string> r = shell::run(cmd, none);
	REQUIRE(r.ok());
	REQUIRE(r.value() == "out\n");
}

TEST_CASE("a script that writes without end is cut at the output limit", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/endless.sh";
	// A timeout far above what this should take, so that a pass says the
	// output limit cut it and not the clock.
	const ShellCommand cmd = { script.c_str(), "", 0, 0, OutputFormat::Raw, 20000, 4096, false };
	ArgList none;

	int64_t began = nowMs();
	Result<std::string> r = shell::run(cmd, none);
	int64_t took = nowMs() - began;

	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);
	REQUIRE(r.error().code == ErrorCode::OutputTooLarge);
	REQUIRE(took < 5000);
}

TEST_CASE("the script inherits no descriptor of ours beyond its own three", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/fdcheck.sh";
	const Param p[] = { { "fd", ParamType::Int, true, "which descriptor", 0, 4095, 0, 0 } };
	const ShellCommand cmd = { script.c_str(), "", p, 1, OutputFormat::Raw, 2000, 4096, false };

	// Descriptor 1 is the pipe the script writes into, so this is the script
	// saying it can tell an open descriptor from a closed one.
	Result<std::string> control = shell::run(cmd, arg("fd", "1"));
	REQUIRE(control.ok());
	REQUIRE(control.value() == "LEAK\n");

	int f = open((fx().dir + "/leakfile").c_str(), O_RDONLY);
	REQUIRE(f >= 0);
	REQUIRE(dup2(f, 900) == 900);
	REQUIRE(close(f) == 0);
	int flags = fcntl(900, F_GETFD);
	REQUIRE(flags >= 0);
	REQUIRE((flags & FD_CLOEXEC) == 0);

	Result<std::string> r = shell::run(cmd, arg("fd", "900"));
	REQUIRE(close(900) == 0);
	REQUIRE(r.ok());
	REQUIRE(r.value() == "CLEAN\n");
}

TEST_CASE("a command that sets no timeout is refused", "[shell]")
{
	const ShellCommand no_timeout = { "/bin/echo", "", 0, 0, OutputFormat::Raw, 0, 4096, false };
	ArgList none;
	Result<std::string> r = shell::run(no_timeout, none);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);
}

TEST_CASE("a script that cannot be run is reported as missing", "[shell]")
{
	const ShellCommand absent = { "/nonexistent/coreapi-shell-test", "", 0, 0,
				      OutputFormat::Raw, 2000, 4096, false };
	ArgList none;
	Result<std::string> r = shell::run(absent, none);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::NotFound);
}

TEST_CASE("runs from several threads at once all finish", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/echo1.sh";
	const Param p[] = { { "text", ParamType::String, true, "text to echo", 0, 0, 0, 0 } };
	const ShellCommand cmd = { script.c_str(), "", p, 1, OutputFormat::Raw, 20000, 4096, false };

	const int workers = 4;
	int failures[workers];
	std::thread t[workers];
	for (int i = 0; i < workers; i++)
	{
		failures[i] = -1;
		t[i] = std::thread(runMany, &cmd, 15, &failures[i]);
	}
	for (int i = 0; i < workers; i++)
		t[i].join();
	for (int i = 0; i < workers; i++)
		REQUIRE(failures[i] == 0);
}

TEST_CASE("a run this process could not supervise is refused before a script is started", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/marker.sh";
	std::string marker = fx().dir + "/marker";
	const Param p[] = { { "count", ParamType::Int, true, "how many", 1, 10, 0, 0 } };
	const ShellCommand cmd = { script.c_str(), "", COREAPI_PARAMS(p), OutputFormat::Raw, 2000, 4096, false };

	unlink(marker.c_str());
	struct sigaction ignore;
	struct sigaction previous;
	ignore.sa_handler = SIG_IGN;
	ignore.sa_flags = 0;
	REQUIRE(sigemptyset(&ignore.sa_mask) == 0);
	REQUIRE(sigaction(SIGCHLD, &ignore, &previous) == 0);

	Result<std::string> r = shell::run(cmd, arg("count", "5"));

	REQUIRE(sigaction(SIGCHLD, &previous, NULL) == 0);

	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);
	REQUIRE(r.error().code == ErrorCode::NoSupervision);
	// Nothing was started, so nothing was left for the timeout to reach.
	REQUIRE(access(marker.c_str(), F_OK) != 0);
}

TEST_CASE("a cut this process stopped being able to finish is not called a timeout", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/stubborn.sh";
	std::string pidfile = fx().dir + "/pids2";
	unlink(pidfile.c_str());
	const ShellCommand cmd = { script.c_str(), "", 0, 0, OutputFormat::Raw, 500, 4096, false };
	ArgList none;

	struct sigaction previous;
	REQUIRE(sigaction(SIGCHLD, NULL, &previous) == 0);
	std::thread flip(ignoreChildrenAfter, 150);
	Result<std::string> r = shell::run(cmd, none);
	flip.join();
	REQUIRE(sigaction(SIGCHLD, &previous, NULL) == 0);

	// What the run would not signal is this case's to clean up, which is what
	// not aiming at a number this process no longer holds costs.
	FILE *f = fopen(pidfile.c_str(), "r");
	if (f != NULL)
	{
		int shell_pid = 0;
		int stubborn = 0;
		if (fscanf(f, "%d %d", &shell_pid, &stubborn) == 2 && shell_pid > 0)
			killpg((pid_t)shell_pid, SIGKILL);
		fclose(f);
	}

	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);
	// Its own code, and not the one a run refused before it started gets: a
	// caller holding this one has a process group that may still be running.
	REQUIRE(r.error().code == ErrorCode::SupervisionLost);
}

TEST_CASE("a script that ended while this process was discarding statuses is reported", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/slow.sh";
	const ShellCommand cmd = { script.c_str(), "", 0, 0, OutputFormat::Raw, 5000, 4096, false };
	ArgList none;

	struct sigaction previous;
	REQUIRE(sigaction(SIGCHLD, NULL, &previous) == 0);
	std::thread flip(ignoreChildrenAfter, 100);
	Result<std::string> r = shell::run(cmd, none);
	flip.join();
	REQUIRE(sigaction(SIGCHLD, &previous, NULL) == 0);

	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);
	REQUIRE(r.error().code == ErrorCode::NoExitStatus);
}

TEST_CASE("a value that would be read as an option is refused", "[shell]")
{
	Result<std::string> dash = shell::run(echo_cmd, arg("text", "-n"));
	REQUIRE_FALSE(dash.ok());
	REQUIRE(dash.error().status == Status::InvalidArgument);
	REQUIRE(dash.error().code == ErrorCode::LeadingDash);

	Result<std::string> longer = shell::run(echo_cmd, arg("text", "--colour=never"));
	REQUIRE_FALSE(longer.ok());
	REQUIRE(longer.error().status == Status::InvalidArgument);

	// A dash anywhere but first is text, and stays text.
	Result<std::string> inner = shell::run(echo_cmd, arg("text", "a-n"));
	REQUIRE(inner.ok());
	REQUIRE(inner.value() == "a-n\n");
}

TEST_CASE("a table that asks for the end of options gets it and its dashes through", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/argsall.sh";
	const Param p[] = { { "text", ParamType::String, true, "text", 0, 0, 0, 0 } };
	const ShellCommand cmd = { script.c_str(), "", p, 1, OutputFormat::Raw, 2000, 4096, true };

	Result<std::string> r = shell::run(cmd, arg("text", "-n"));
	REQUIRE(r.ok());
	REQUIRE(r.value() == "--|-n|\n");
}

TEST_CASE("the end of options element goes behind the verb", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/argsall.sh";
	const Param p[] = { { "text", ParamType::String, true, "text", 0, 0, 0, 0 } };
	const ShellCommand cmd = { script.c_str(), "status", p, 1, OutputFormat::Raw, 2000, 4096, true };

	Result<std::string> r = shell::run(cmd, arg("text", "-n"));
	REQUIRE(r.ok());
	REQUIRE(r.value() == "status|--|-n|\n");
}

TEST_CASE("a negative number reaches the script when the table declared one", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/echo1.sh";
	const Param signed_p[] = { { "n", ParamType::Int, true, "how far", -10, 10, 0, 0 } };
	const ShellCommand signed_cmd = { script.c_str(), "", signed_p, 1, OutputFormat::Raw, 2000, 4096, false };

	Result<std::string> r = shell::run(signed_cmd, arg("n", "-5"));
	REQUIRE(r.ok());
	REQUIRE(r.value() == "-5\n");

	// The same value against a table that declared no negative range.
	const Param unsigned_p[] = { { "n", ParamType::Int, true, "how far", 0, 10, 0, 0 } };
	const ShellCommand unsigned_cmd = { script.c_str(), "", unsigned_p, 1, OutputFormat::Raw, 2000, 4096, false };
	Result<std::string> refused = shell::run(unsigned_cmd, arg("n", "-5"));
	REQUIRE_FALSE(refused.ok());
	REQUIRE(refused.error().status == Status::InvalidArgument);
}

TEST_CASE("the script is handed the environment this layer names", "[shell]")
{
	const ShellCommand env_cmd = { "/usr/bin/env", "", 0, 0, OutputFormat::Raw, 5000, 65536, false };
	ArgList none;

	REQUIRE(setenv("LD_PRELOAD", "/nonexistent/wedge.so", 1) == 0);
	REQUIRE(setenv("COREAPI_SHELL_MARKER", "seen", 1) == 0);
	Result<std::string> r = shell::run(env_cmd, none);
	REQUIRE(unsetenv("LD_PRELOAD") == 0);
	REQUIRE(unsetenv("COREAPI_SHELL_MARKER") == 0);

	REQUIRE(r.ok());
	REQUIRE(r.value().find("LD_PRELOAD") == std::string::npos);
	REQUIRE(r.value().find("COREAPI_SHELL_MARKER") == std::string::npos);
	REQUIRE(r.value() == "PATH=/sbin:/usr/sbin:/bin:/usr/bin\nHOME=/\nLC_ALL=C\n");
}

TEST_CASE("the loop the box's kernel has to use closes a descriptor the child must not keep", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/fdcheck.sh";
	const Param p[] = { { "fd", ParamType::Int, true, "which descriptor", 0, 4095, 0, 0 } };
	const ShellCommand cmd = { script.c_str(), "", p, 1, OutputFormat::Raw, 5000, 4096, false };

	int f = open((fx().dir + "/leakfile").c_str(), O_RDONLY);
	REQUIRE(f >= 0);
	REQUIRE(dup2(f, 900) == 900);
	REQUIRE(close(f) == 0);

	shell::internal::try_close_range = false;
	Result<std::string> control = shell::run(cmd, arg("fd", "1"));
	Result<std::string> r = shell::run(cmd, arg("fd", "900"));
	shell::internal::try_close_range = true;

	REQUIRE(close(900) == 0);
	REQUIRE(control.ok());
	REQUIRE(control.value() == "LEAK\n");
	REQUIRE(r.ok());
	REQUIRE(r.value() == "CLEAN\n");
}

TEST_CASE("a command that leaves the script no room to answer is refused", "[shell]")
{
	const ShellCommand no_room = { "/bin/echo", "", 0, 0, OutputFormat::Raw, 2000, 0, false };
	ArgList none;
	Result<std::string> r = shell::run(no_room, none);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);
	REQUIRE(r.error().code == ErrorCode::NoOutputRoom);
}

TEST_CASE("an enum list ending in a comma does not thereby accept an empty value", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/echo1.sh";
	const Param p[] = { { "mode", ParamType::Enum, false, "which", 0, 0, "on,off,", 0 } };
	const ShellCommand cmd = { script.c_str(), "", p, 1, OutputFormat::Raw, 2000, 4096, false };

	Result<std::string> empty = shell::run(cmd, arg("mode", ""));
	REQUIRE_FALSE(empty.ok());
	REQUIRE(empty.error().status == Status::InvalidArgument);

	Result<std::string> listed = shell::run(cmd, arg("mode", "off"));
	REQUIRE(listed.ok());
	REQUIRE(listed.value() == "off\n");
}

TEST_CASE("an unrelated fork in the window does not keep the answer from ending", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/echo1.sh";
	const Param p[] = { { "text", ParamType::String, true, "text to echo", 0, 0, 0, 0 } };
	const ShellCommand cmd = { script.c_str(), "", p, 1, OutputFormat::Raw, 1500, 4096, false };

	g_wedged = -1;
	g_wedge_fork = true;
	int64_t began = nowMs();
	Result<std::string> r = shell::run(cmd, arg("text", "x"));
	int64_t took = nowMs() - began;
	g_wedge_fork = false;

	pid_t other = g_wedged;
	if (other > 0)
	{
		kill(other, SIGKILL);
		waitpid(other, NULL, 0);
	}
	REQUIRE(other > 0);
	REQUIRE(r.ok());
	REQUIRE(r.value() == "x\n");
	REQUIRE(took < 1000);
}

TEST_CASE("the loop closes a high descriptor after the limit has been lowered under it", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/fdcheck.sh";
	const Param p[] = { { "fd", ParamType::Int, true, "which descriptor", 0, 4095, 0, 0 } };
	const ShellCommand cmd = { script.c_str(), "", COREAPI_PARAMS(p), OutputFormat::Raw, 5000, 4096, false };

	int f = open((fx().dir + "/leakfile").c_str(), O_RDONLY);
	REQUIRE(f >= 0);
	REQUIRE(dup2(f, 900) == 900);
	REQUIRE(close(f) == 0);

	struct rlimit before;
	struct rlimit tight;
	REQUIRE(getrlimit(RLIMIT_NOFILE, &before) == 0);
	tight = before;
	tight.rlim_cur = 64;
	REQUIRE(setrlimit(RLIMIT_NOFILE, &tight) == 0);

	shell::internal::try_close_range = false;
	Result<std::string> r = shell::run(cmd, arg("fd", "900"));
	shell::internal::try_close_range = true;

	REQUIRE(setrlimit(RLIMIT_NOFILE, &before) == 0);
	REQUIRE(close(900) == 0);
	REQUIRE(r.ok());
	REQUIRE(r.value() == "CLEAN\n");
}

TEST_CASE("a file parameter takes a regular file and refuses a directory", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/echo1.sh";
	const Param p[] = { { "f", ParamType::File, true, "a file", 0, 0, 0, fx().root.c_str() } };
	const ShellCommand cmd = { script.c_str(), "", COREAPI_PARAMS(p), OutputFormat::Raw, 2000, 4096, false };

	char rootbuf[PATH_MAX];
	REQUIRE(realpath(fx().root.c_str(), rootbuf) != NULL);

	Result<std::string> good = shell::run(cmd, arg("f", "ok.txt"));
	REQUIRE(good.ok());
	REQUIRE(good.value() == std::string(rootbuf) + "/ok.txt\n");

	Result<std::string> a_dir = shell::run(cmd, arg("f", "sub"));
	REQUIRE_FALSE(a_dir.ok());
	REQUIRE(a_dir.error().status == Status::InvalidArgument);

	Result<std::string> the_root = shell::run(cmd, arg("f", "."));
	REQUIRE_FALSE(the_root.ok());
	REQUIRE(the_root.error().status == Status::InvalidArgument);

	// The same directory against a path, which takes one on purpose.
	const Param q[] = { { "f", ParamType::Path, true, "a path", 0, 0, 0, fx().root.c_str() } };
	const ShellCommand anypath = { script.c_str(), "", COREAPI_PARAMS(q), OutputFormat::Raw, 2000, 4096, false };
	Result<std::string> dir_ok = shell::run(anypath, arg("f", "sub"));
	REQUIRE(dir_ok.ok());
	REQUIRE(dir_ok.value() == std::string(rootbuf) + "/sub\n");
}

TEST_CASE("a path outside its declared length is refused", "[shell]")
{
	REQUIRE(fx().ready);
	std::string script = fx().dir + "/echo1.sh";
	const Param p[] = { { "f", ParamType::Path, true, "a path", 8, 64, 0, fx().root.c_str() } };
	const ShellCommand cmd = { script.c_str(), "", COREAPI_PARAMS(p), OutputFormat::Raw, 2000, 4096, false };

	Result<std::string> too_short = shell::run(cmd, arg("f", "ok.txt"));
	REQUIRE_FALSE(too_short.ok());
	REQUIRE(too_short.error().status == Status::InvalidArgument);
	REQUIRE(too_short.error().code == ErrorCode::ValueTooLong);

	const Param q[] = { { "f", ParamType::Path, true, "a path", 0, 64, 0, fx().root.c_str() } };
	const ShellCommand nomin = { script.c_str(), "", COREAPI_PARAMS(q), OutputFormat::Raw, 2000, 4096, false };
	REQUIRE(shell::run(nomin, arg("f", "ok.txt")).ok());
}
