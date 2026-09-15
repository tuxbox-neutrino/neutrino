/*
 * shellcontract.cpp - what the shell may be asked to do, and with which arguments
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

#include "shellcontract.h"
#include "coreapi/base/errors.h"
#include "coreapi/box/shellcontract_internal.h"

#include <cerrno>
#include <cstdio>
#include <dirent.h>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdint.h>
#include <signal.h>
#include <strings.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

namespace coreapi
{
namespace shell
{

namespace internal
{
bool try_close_range = true;
}

namespace
{

// An argv element carries a parameter, not a payload, and the kernel caps a
// single one anyway.
const size_t kMaxValueBytes = 4096;

// Named here rather than taken from the server, so whatever reached the server's
// environment does not reach the script. PATH is const and lands beside the
// tables, out of reach of anything that can write this process's memory.
const char kPath[] = "PATH=/sbin:/usr/sbin:/bin:/usr/bin";
const char kHome[] = "HOME=/";
const char kLocale[] = "LC_ALL=C";
const char *const kChildEnv[] = { kPath, kHome, kLocale, NULL };

const char kEndOptions[] = "--";

const int kTermGraceMs = 200;
const int kKillGraceMs = 2000;

// Negative when the clock cannot be read, which every caller treats as time
// already spent, so an unreadable clock cuts a run short instead of removing its
// deadline.
int64_t monoMs()
{
	struct timespec t;
	if (clock_gettime(CLOCK_MONOTONIC, &t) != 0)
		return -1;
	return (int64_t)t.tv_sec * 1000 + (int64_t)(t.tv_nsec / 1000000);
}

int64_t remainingMs(int64_t deadline)
{
	int64_t now = monoMs();
	if (now < 0)
		return 0;
	return deadline - now;
}

std::string num(long v)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "%ld", v);
	return std::string(buf);
}

// execv() and realpath() both stop at the first zero byte, so a value carrying
// one would run as something other than what was checked.
bool hasZeroByte(const std::string &v)
{
	return v.find('\0') != std::string::npos;
}

bool parseLong(const std::string &v, long &out)
{
	if (v.empty())
		return false;
	errno = 0;
	char *end = NULL;
	long n = strtol(v.c_str(), &end, 10);
	if (errno != 0 || end == v.c_str() || *end != '\0')
		return false;
	out = n;
	return true;
}

bool parseBool(const std::string &v, bool &out)
{
	static const char *const yes[] = { "1", "true", "yes", "on" };
	static const char *const no[]  = { "0", "false", "no", "off" };
	for (size_t i = 0; i < 4; i++)
	{
		if (strcasecmp(v.c_str(), yes[i]) == 0)
		{
			out = true;
			return true;
		}
		if (strcasecmp(v.c_str(), no[i]) == 0)
		{
			out = false;
			return true;
		}
	}
	return false;
}

bool inList(const char *list, const std::string &v)
{
	const char *p = list;
	for (;;)
	{
		const char *e = strchr(p, ',');
		size_t n = (e != NULL) ? (size_t)(e - p) : strlen(p);
		// an empty element, from a doubled or trailing comma, would otherwise
		// make the empty value acceptable
		if (n > 0 && n == v.size() && memcmp(p, v.data(), n) == 0)
			return true;
		if (e == NULL)
			return false;
		p = e + 1;
	}
}

bool hasDotDot(const std::string &v)
{
	size_t i = 0;
	while (i < v.size())
	{
		size_t e = v.find('/', i);
		if (e == std::string::npos)
			e = v.size();
		if (e - i == 2 && v[i] == '.' && v[i + 1] == '.')
			return true;
		i = e + 1;
	}
	return false;
}

// A value that is not absolute is read below the root rather than below
// whatever directory the process happens to sit in.
bool resolveInside(const std::string &root, const std::string &v, std::string &out)
{
	std::string cand = (!v.empty() && v[0] == '/') ? v : root + "/" + v;
	if (cand.size() >= PATH_MAX)
		return false;
	char buf[PATH_MAX];
	if (realpath(cand.c_str(), buf) == NULL)
		return false;
	std::string res(buf);
	if (res != root)
	{
		std::string prefix = (root == "/") ? root : root + "/";
		if (res.compare(0, prefix.size(), prefix) != 0)
			return false;
	}
	out = res;
	return true;
}

// Everything between fork() and execv() has to be async signal safe: the web
// layer runs on its own thread and the child holds whatever locks the other
// threads were inside. Nothing here allocates; maxfd and argv are handed in
// because reading and building them is not safe here.
void childExec(int wfd, int maxfd, bool use_close_range, char *const argv[])
{
	if (setpgid(0, 0) != 0)
		_exit(126);

	// An inherited signal mask survives execv(), so a blocked SIGTERM would
	// outlast the timeout that is supposed to end this.
	sigset_t empty;
	if (sigemptyset(&empty) != 0 || sigprocmask(SIG_SETMASK, &empty, NULL) != 0)
		_exit(126);

	// execv() resets handled signals but keeps ignored ones. Ignored SIGTERM
	// and SIGPIPE would each defeat one half of the timeout; ignored SIGCHLD
	// would break the script's own waits.
	struct sigaction dfl;
	dfl.sa_handler = SIG_DFL;
	dfl.sa_flags = 0;
	if (sigemptyset(&dfl.sa_mask) != 0)
		_exit(126);
	if (sigaction(SIGCHLD, &dfl, NULL) != 0 ||
		sigaction(SIGPIPE, &dfl, NULL) != 0 ||
		sigaction(SIGTERM, &dfl, NULL) != 0)
		_exit(126);

	// Before /dev/null is opened, so the open cannot land on the slot the pipe
	// is about to take.
	if (wfd == STDOUT_FILENO)
	{
		// dup2() onto itself is a no-op that would leave the close on exec
		// flag the pipe was created with in place.
		int fl = fcntl(wfd, F_GETFD);
		if (fl < 0 || fcntl(wfd, F_SETFD, fl & ~FD_CLOEXEC) < 0)
			_exit(126);
	}
	else if (dup2(wfd, STDOUT_FILENO) < 0)
	{
		_exit(126);
	}

	// Diagnostics would otherwise land in the middle of the answer.
	int devnull = open("/dev/null", O_RDWR);
	if (devnull < 0)
		_exit(126);
	if (dup2(devnull, STDIN_FILENO) < 0 || dup2(devnull, STDERR_FILENO) < 0)
		_exit(126);

	// The web server's listening socket and its open connections are created
	// without close on exec, so the pipe's own flag does not cover them.
	bool ranged = false;
#if defined(SYS_close_range)
	if (use_close_range)
		ranged = (syscall(SYS_close_range, 3U, ~0U, 0U) == 0);
#else
	(void)use_close_range;
#endif
	if (!ranged)
	{
		// EBADF on a descriptor that was never open is the expected answer
		// for most of this range.
		for (int i = 3; i < maxfd; i++)
			close(i);
	}

	execve(argv[0], argv, const_cast<char *const *>(kChildEnv));
	_exit(127);
}

// 1 reaped with a status, 0 still running when the budget ran out, -1 no status
// to be had. -1 means the process wide SIGCHLD disposition is SIG_IGN, under
// which the kernel reaps a child itself and keeps no status for anyone.
int waitFor(pid_t pid, int64_t budget_ms, int *status)
{
	if (pid <= 0)
		return -1;
	int64_t now = monoMs();
	if (now < 0)
		return 0;
	int64_t deadline = now + (budget_ms > 0 ? budget_ms : 0);
	for (;;)
	{
		pid_t r = waitpid(pid, status, WNOHANG);
		if (r == pid)
			return 1;
		if (r < 0)
		{
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (remainingMs(deadline) <= 0)
			return 0;
		struct timespec ts;
		ts.tv_sec = 0;
		ts.tv_nsec = 2000000;
		nanosleep(&ts, NULL);
	}
}

// The call closes to the top; the loop closes only what it is told to. The soft
// limit bounds what any open from here on can return, and this finds anything
// above it opened before the limit came down, so the two together bound every
// descriptor the child could inherit.
int highestOpenFd()
{
	DIR *d = opendir("/proc/self/fd");
	if (d == NULL)
		return -1;
	int highest = -1;
	for (;;)
	{
		struct dirent *e = readdir(d);
		if (e == NULL)
			break;
		int n = atoi(e->d_name);
		if (n > highest)
			highest = n;
	}
	closedir(d);
	return highest;
}

// 1 the child has exited and has not been collected yet, 0 still running, -1 no
// longer this process's to wait for. Looks without collecting, because
// collecting is what frees the number.
int peekChild(pid_t pid)
{
	for (;;)
	{
		siginfo_t info;
		memset(&info, 0, sizeof(info));
		if (waitid(P_PID, (id_t)pid, &info, WEXITED | WNOHANG | WNOWAIT) == 0)
			return (info.si_pid == pid) ? 1 : 0;
		if (errno == EINTR)
			continue;
		return -1;
	}
}

int peekUntil(pid_t pid, int64_t budget_ms)
{
	int64_t now = monoMs();
	if (now < 0)
		return peekChild(pid);
	int64_t deadline = now + (budget_ms > 0 ? budget_ms : 0);
	for (;;)
	{
		int state = peekChild(pid);
		if (state != 0)
			return state;
		if (remainingMs(deadline) <= 0)
			return 0;
		struct timespec ts;
		ts.tv_sec = 0;
		ts.tv_nsec = 2000000;
		nanosleep(&ts, NULL);
	}
}

// The child is already known to have exited, so this does not wait.
int collect(pid_t pid, int *status)
{
	for (;;)
	{
		if (waitpid(pid, status, 0) == pid)
			return 1;
		if (errno == EINTR)
			continue;
		return -1;
	}
}

// A number, and the group number with it, stay reserved for exactly as long as
// this process has not collected the child. So nothing here collects until every
// signal has gone out, and one rule covers both kinds: aim at neither once a
// look says the child is no longer ours.
//
// Stopping short is answered for rather than passed off as a stop, because it
// leaves whatever the child started still running. It costs a run whose
// disposition changed under it, which is why that state is refused before a run
// begins.
//
// 1 collected with a status, 0 alive after everything, -1 exited but not
// collectable, -2 no longer this process's to stop.
int stopGroup(pid_t pid, int *status)
{
	if (pid <= 0)
		return -1;

	int state = peekChild(pid);
	if (state < 0)
		return -2;

	killpg(pid, SIGTERM);
	if (state == 0)
	{
		kill(pid, SIGTERM); // the child may not have reached setpgid() yet
		state = peekUntil(pid, kTermGraceMs);
		if (state < 0)
			return -2;
	}

	// Whatever became of the child, anything it left running is still in the
	// group, and this run is being cut off rather than let finish.
	killpg(pid, SIGKILL);
	if (state == 0)
	{
		kill(pid, SIGKILL);
		state = peekUntil(pid, kKillGraceMs);
		if (state < 0)
			return -2;
		if (state == 0)
			return 0;
	}

	return collect(pid, status);
}

// A program that reads options reads them wherever they sit, so a value that
// begins with a dash is one. The table says when that is meant: by asking for
// the end of options element, or, for an Int, by declaring that a negative
// number is in range. An Enum can only carry a value the table wrote itself.
bool dashIsMeant(const ShellCommand &cmd, const Param &p)
{
	if (cmd.end_options)
		return true;
	if (p.type == ParamType::Int)
		return p.min < 0;
	return p.type == ParamType::Enum;
}

Failure badParam(ErrorCode code, const char *name, const std::string &what)
{
	return fail(Status::InvalidArgument, code, std::string("parameter ") + name + " " + what);
}

// The value itself is never quoted back: it is the one part of this a caller
// wrote, and the answer travels back to places that render it.
Result<void> checkValue(const Param &p, const std::string &raw, std::string &out)
{
	if (hasZeroByte(raw))
		return badParam(ErrorCode::ValueHasZeroByte, p.name, "carries a zero byte");
	if (raw.size() > kMaxValueBytes)
		return badParam(ErrorCode::ValueTooLong, p.name, "is longer than " + num((long)kMaxValueBytes) + " bytes");

	switch (p.type)
	{
		case ParamType::Int:
		{
			long v = 0;
			if (!parseLong(raw, v))
				return badParam(ErrorCode::BadInt, p.name, "is not a whole number");
			if (v < p.min || v > p.max)
				return badParam(ErrorCode::BadInt, p.name, "is outside " + num(p.min) + " to " + num(p.max));
			out = num(v);
			return ok();
		}
		case ParamType::Bool:
		{
			bool v = false;
			if (!parseBool(raw, v))
				return badParam(ErrorCode::BadBool, p.name, "is not a yes or a no");
			out = v ? "1" : "0";
			return ok();
		}
		case ParamType::Enum:
		{
			if (p.values == NULL || p.values[0] == '\0')
				return fail(Status::Internal, ErrorCode::NoValues,
					    std::string("parameter ") + p.name + " lists no accepted values");
			if (!inList(p.values, raw))
				return badParam(ErrorCode::BadEnum, p.name, std::string("is not one of ") + p.values);
			out = raw;
			return ok();
		}
		case ParamType::Path:
		case ParamType::File:
		{
			if (p.root == NULL || p.root[0] == '\0')
				return fail(Status::Internal, ErrorCode::NoRoot,
					    std::string("parameter ") + p.name + " names no directory to stay inside");
			char rootbuf[PATH_MAX];
			if (realpath(p.root, rootbuf) == NULL)
				return fail(Status::Internal, ErrorCode::NoRoot,
					    std::string("parameter ") + p.name + " names a directory that is not there");
			if (raw.empty())
				return badParam(ErrorCode::BadPath, p.name, "is empty");
			if (p.max > 0 && ((long)raw.size() > p.max || (long)raw.size() < p.min))
				return badParam(ErrorCode::ValueTooLong, p.name,
						"is not between " + num(p.min) + " and " + num(p.max) + " bytes");
			if (hasDotDot(raw))
				return badParam(ErrorCode::BadPath, p.name, "walks up out of its directory");
			// One answer for a path that cannot be resolved and for one that
			// resolves outside, so the two cannot be told apart from outside
			// and used to ask what exists elsewhere on the box.
			if (!resolveInside(std::string(rootbuf), raw, out))
				return badParam(ErrorCode::BadPath, p.name, "does not name anything inside its directory");
			if (p.type == ParamType::File)
			{
				struct stat st;
				// The same answer again, so that what a name is stays as
				// unreadable from outside as whether it is there at all.
				if (stat(out.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
					return badParam(ErrorCode::BadPath, p.name,
							"does not name anything inside its directory");
			}
			return ok();
		}
		case ParamType::String:
		default:
		{
			if (p.max > 0 && ((long)raw.size() > p.max || (long)raw.size() < p.min))
				return badParam(ErrorCode::BadString, p.name,
						"is not between " + num(p.min) + " and " + num(p.max) + " bytes");
			out = raw;
			return ok();
		}
	}
}

} // namespace

Result<std::string> run(const ShellCommand &cmd,
			const std::vector<std::pair<std::string, std::string> > &args)
{
	// A relative name would be resolved against PATH by the caller's idea of
	// it, and a name with a walk in it against nothing anyone declared.
	if (cmd.script == NULL || cmd.script[0] != '/' || hasDotDot(cmd.script))
		return fail(Status::Internal, ErrorCode::BadScript, "the command names no absolute script");
	// A timeout that cannot expire reads as a guard and is none.
	if (cmd.timeout_ms <= 0)
		return fail(Status::Internal, ErrorCode::NoTimeout, "the command sets no timeout");
	// A ceiling of nothing makes the first byte the script writes too much,
	// which is what a table that stopped early would ask for.
	if (cmd.max_output == 0)
		return fail(Status::Internal, ErrorCode::NoOutputRoom, "the command leaves the script no room to answer");
	// A process that has told the kernel to discard what its children exit with
	// is never told when this one ends, and the number is free the moment it
	// does, so a timeout would have nothing left it could safely aim at.
	// Nothing is started that could not then be stopped.
	struct sigaction chld;
	if (sigaction(SIGCHLD, NULL, &chld) == 0 && chld.sa_handler == SIG_IGN)
		return fail(Status::Internal, ErrorCode::NoSupervision,
			    "this process discards what its children exit with, so a script cannot be supervised");
	if (cmd.param_count > 0 && cmd.params == NULL)
		return fail(Status::Internal, ErrorCode::BadTable, "the command declares parameters it does not carry");
	for (size_t i = 0; i < cmd.param_count; i++)
	{
		if (cmd.params[i].name == NULL || cmd.params[i].name[0] == '\0')
			return fail(Status::Internal, ErrorCode::BadTable, "the command declares a parameter without a name");
	}

	std::vector<std::string> values(cmd.param_count);
	std::vector<char> given(cmd.param_count, 0);

	for (size_t a = 0; a < args.size(); a++)
	{
		size_t idx = cmd.param_count;
		for (size_t i = 0; i < cmd.param_count; i++)
		{
			if (args[a].first == cmd.params[i].name)
			{
				idx = i;
				break;
			}
		}
		if (idx == cmd.param_count)
			return fail(Status::InvalidArgument, ErrorCode::NoSuchParameter,
				    "the command declares no parameter of that name");
		if (given[idx])
			return badParam(ErrorCode::DuplicateParameter, cmd.params[idx].name, "was given twice");

		Result<void> checked = checkValue(cmd.params[idx], args[a].second, values[idx]);
		if (!checked.ok())
			return fail(checked.error());
		if (!values[idx].empty() && values[idx][0] == '-' && !dashIsMeant(cmd, cmd.params[idx]))
			return badParam(ErrorCode::LeadingDash, cmd.params[idx].name,
					"would be read as an option and not as a value");
		given[idx] = 1;
	}

	for (size_t i = 0; i < cmd.param_count; i++)
	{
		if (cmd.params[i].required && (!given[i] || values[i].empty()))
			return badParam(ErrorCode::MissingParameter, cmd.params[i].name, "is required");
	}

	// values is complete, so the pointers taken from it stay put.
	std::vector<char *> argv;
	argv.reserve(cmd.param_count + 4);
	argv.push_back(const_cast<char *>(cmd.script));
	if (cmd.verb != NULL && cmd.verb[0] != '\0')
		argv.push_back(const_cast<char *>(cmd.verb));
	if (cmd.end_options)
		argv.push_back(const_cast<char *>(kEndOptions));
	for (size_t i = 0; i < values.size(); i++)
		argv.push_back(const_cast<char *>(values[i].c_str()));
	argv.push_back(NULL);

	// A missing script would otherwise arrive as an exit code no different
	// from one the script itself could return.
	if (access(cmd.script, X_OK) != 0)
		return fail(Status::NotFound, ErrorCode::ScriptMissing, "the script cannot be run");

	// Without close on exec a fork in any other thread inherits the write end
	// and holds it open, and the read below would wait for that unrelated
	// child instead of for this one.
	int fds[2];
	if (pipe2(fds, O_CLOEXEC) != 0)
		return fail(Status::Internal, ErrorCode::CommandNotStarted, "no pipe for the script's output");

	int maxfd = getdtablesize();
	int highest = highestOpenFd();
	if (highest >= maxfd)
		maxfd = highest + 1;
	bool use_close_range = internal::try_close_range;

	int64_t start = monoMs();
	if (start < 0)
	{
		close(fds[0]);
		close(fds[1]);
		return fail(Status::Internal, ErrorCode::CommandNotStarted, "the clock that bounds the run cannot be read");
	}
	int64_t deadline = start + cmd.timeout_ms;

	pid_t pid = fork();
	if (pid < 0)
	{
		close(fds[0]);
		close(fds[1]);
		return fail(Status::Internal, ErrorCode::CommandNotStarted, "the script could not be started");
	}
	if (pid == 0)
		childExec(fds[1], maxfd, use_close_range, &argv[0]);

	// Both sides set it, because a signal sent before the child got there
	// would otherwise find no group. Whichever loses the race fails, and that
	// failure is the other side having already done it.
	setpgid(pid, pid);
	close(fds[1]);

	std::string out;
	bool timed_out = false;
	bool too_much = false;
	int io_errno = 0;

	for (;;)
	{
		int64_t left = remainingMs(deadline);
		if (left <= 0)
		{
			timed_out = true;
			break;
		}
		struct pollfd pfd;
		pfd.fd = fds[0];
		pfd.events = POLLIN;
		pfd.revents = 0;
		int pr = poll(&pfd, 1, (int)left);
		if (pr < 0)
		{
			if (errno == EINTR)
				continue;
			io_errno = errno;
			break;
		}
		if (pr == 0)
		{
			timed_out = true;
			break;
		}
		char buf[4096];
		ssize_t got = read(fds[0], buf, sizeof(buf));
		if (got < 0)
		{
			if (errno == EINTR)
				continue;
			io_errno = errno;
			break;
		}
		if (got == 0)
			break;
		if (out.size() + (size_t)got > cmd.max_output)
		{
			too_much = true;
			break;
		}
		out.append(buf, (size_t)got);
	}

	// Closing here is what ends a grandchild that ignored the signal below:
	// it holds the same write end and its next write finds nobody.
	close(fds[0]);

	int status = 0;
	int reaped;
	bool cut = timed_out || too_much || io_errno != 0;
	if (cut)
	{
		reaped = stopGroup(pid, &status);
	}
	else
	{
		// The end of the output is not the end of the script: one that closes
		// its output and keeps running would hold this thread for as long as
		// it liked.
		reaped = waitFor(pid, remainingMs(deadline), &status);
		if (reaped == 0)
		{
			timed_out = true;
			reaped = stopGroup(pid, &status);
		}
	}

	if (reaped == -2)
		return fail(Status::Internal, ErrorCode::SupervisionLost,
			    "the run was cut but this process stopped being able to stop it, "
			    "so what the script left may still be running");
	if (timed_out)
		return fail(Status::Busy, ErrorCode::Timeout,
			    "the script did not finish within " + num(cmd.timeout_ms) + " ms");
	if (too_much)
		return fail(Status::Internal, ErrorCode::OutputTooLarge,
			    "the script wrote more than " + num((long)cmd.max_output) + " bytes");
	if (io_errno != 0)
		return fail(Status::Internal, ErrorCode::CommandOutputUnreadable, "the script's output could not be read");
	if (reaped == 0)
		return fail(Status::Internal, ErrorCode::NotStopped, "the script could not be stopped");
	if (reaped < 0)
		return fail(Status::Internal, ErrorCode::NoExitStatus,
			    "the script ran but this process discards what its children exit with");
	if (WIFSIGNALED(status))
		return fail(Status::Internal, ErrorCode::Killed,
			    "the script was killed by signal " + num(WTERMSIG(status)));
	if (!WIFEXITED(status))
		return fail(Status::Internal, ErrorCode::DidNotExit, "the script did not exit");
	if (WEXITSTATUS(status) != 0)
		return fail(Status::Internal, ErrorCode::ExitStatus,
			    "the script exited with " + num(WEXITSTATUS(status)));

	return ok(std::move(out));
}

} // namespace shell
} // namespace coreapi
