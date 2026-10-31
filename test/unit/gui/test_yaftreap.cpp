/*
 * test_yaftreap.cpp - the terminal collects its own child and no other
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * yaft::reapOwnChild is what the terminal's loop asks once per turn. These
 * cases drive the shipped object with real children: one that stands for
 * the terminal's own, and others that end beside it the way a converter, a
 * command started in the background and a shell contract child do while a
 * terminal window is open.
 */

#include "support/catch.hpp"

#include <gui/widget/yaft/reap.h>

#include <cerrno>
#include <csignal>
#include <cstring>

#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

namespace
{

void pauseMs(unsigned ms)
{
	struct timespec ts;
	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (long) (ms % 1000) * 1000000L;
	while (nanosleep(&ts, &ts) != 0 && errno == EINTR)
		;
}

// A child that ends at once with the status given.
pid_t endsWith(int status)
{
	const pid_t pid = fork();
	REQUIRE(pid >= 0);
	if (pid == 0)
		_exit(status);
	return pid;
}

/* A child that runs until the pipe it reads is closed, then ends with 0.
   *release is the writing end; closing it is what ends the child. */
pid_t runsUntilReleased(int *release)
{
	int p[2];
	REQUIRE(pipe(p) == 0);
	const pid_t pid = fork();
	REQUIRE(pid >= 0);
	if (pid == 0)
	{
		close(p[1]);
		char c;
		while (read(p[0], &c, 1) > 0)
			;
		_exit(0);
	}
	close(p[0]);
	*release = p[1];
	return pid;
}

// Waits until the child has ended without collecting it.
void waitEndedNotCollected(pid_t pid)
{
	siginfo_t info;
	memset(&info, 0, sizeof(info));
	REQUIRE(waitid(P_PID, (id_t) pid, &info, WEXITED | WNOWAIT) == 0);
	REQUIRE(info.si_pid == pid);
}

/* What the terminal's loop does: asks once per turn until the answer is not
   "running", for at most five seconds. */
yaft::Reaped askUntilSettled(pid_t pid, int *code, int *err)
{
	for (int turn = 0; turn < 500; ++turn)
	{
		const yaft::Reaped r = yaft::reapOwnChild(pid, code, err);
		if (r != yaft::ChildRunning)
			return r;
		pauseMs(10);
	}
	return yaft::ChildRunning;
}

} // namespace

TEST_CASE("a terminal child that ends at once is collected with its status", "[yaft][reap]")
{
	const pid_t pid = endsWith(3);
	int code = -1;
	int err = 0;
	REQUIRE(askUntilSettled(pid, &code, &err) == yaft::ChildExited);
	REQUIRE(code == 3);

	// Collected: nothing is left under that number.
	errno = 0;
	REQUIRE(waitpid(pid, NULL, WNOHANG) == -1);
	REQUIRE(errno == ECHILD);
}

TEST_CASE("a terminal child a signal ended reports 128 and the signal", "[yaft][reap]")
{
	/* SIGKILL, because a child starts with the signal mask and the ignored
	   signals of the thread that forked it, and the suite blocks or ignores
	   others in some of its cases. */
	int release = -1;
	const pid_t pid = runsUntilReleased(&release);
	REQUIRE(kill(pid, SIGKILL) == 0);

	int code = -1;
	int err = 0;
	const yaft::Reaped r = askUntilSettled(pid, &code, &err);
	close(release);
	REQUIRE(r == yaft::ChildExited);
	REQUIRE(code == 128 + SIGKILL);
}

TEST_CASE("a terminal child that still runs is left running and asked again", "[yaft][reap]")
{
	int release = -1;
	const pid_t pid = runsUntilReleased(&release);

	int code = -7;
	int err = 0;
	for (int i = 0; i < 3; ++i)
	{
		REQUIRE(yaft::reapOwnChild(pid, &code, &err) == yaft::ChildRunning);
		pauseMs(5);
	}
	// Asking did not touch the status it hands back.
	REQUIRE(code == -7);

	close(release);
	REQUIRE(askUntilSettled(pid, &code, &err) == yaft::ChildExited);
	REQUIRE(code == 0);
}

TEST_CASE("only the terminal's child is collected while others end beside it", "[yaft][reap]")
{
	/* The terminal's own child keeps running while three others end: one in
	   the role of a converter, one of a command started in the background
	   and one of a shell contract child. A handler for SIGCHLD runs for every
	   one of them; asking by number must neither collect nor wait for any of
	   them, and each keeps its status for its own waitpid. */
	int release = -1;
	const pid_t own = runsUntilReleased(&release);
	const pid_t converter = endsWith(7);
	const pid_t background = endsWith(9);
	const pid_t contract = endsWith(11);

	waitEndedNotCollected(converter);
	waitEndedNotCollected(background);
	waitEndedNotCollected(contract);

	int code = -1;
	int err = 0;
	for (int i = 0; i < 5; ++i)
		REQUIRE(yaft::reapOwnChild(own, &code, &err) == yaft::ChildRunning);

	int status = 0;
	REQUIRE(waitpid(converter, &status, WNOHANG) == converter);
	REQUIRE(WIFEXITED(status));
	REQUIRE(WEXITSTATUS(status) == 7);
	REQUIRE(waitpid(background, &status, WNOHANG) == background);
	REQUIRE(WEXITSTATUS(status) == 9);
	REQUIRE(waitpid(contract, &status, WNOHANG) == contract);
	REQUIRE(WEXITSTATUS(status) == 11);

	// And the terminal's own one, once it ends, with its own status.
	close(release);
	REQUIRE(askUntilSettled(own, &code, &err) == yaft::ChildExited);
	REQUIRE(code == 0);
}

TEST_CASE("a terminal child the kernel collected leaves nothing to collect", "[yaft][reap]")
{
	/* A command started in the background ignores SIGCHLD while it runs, and
	   a child ending in that time is collected by the kernel. Its status is
	   gone, and reapOwnChild says so with ChildGone. */
	struct sigaction ignore, old;
	memset(&ignore, 0, sizeof(ignore));
	ignore.sa_handler = SIG_IGN;
	sigemptyset(&ignore.sa_mask);
	REQUIRE(sigaction(SIGCHLD, &ignore, &old) == 0);

	const pid_t pid = endsWith(5);
	// Gone from the table once the kernel has collected it.
	bool gone = false;
	for (int i = 0; i < 500 && !gone; ++i)
	{
		if (kill(pid, 0) != 0 && errno == ESRCH)
			gone = true;
		else
			pauseMs(10);
	}
	REQUIRE(sigaction(SIGCHLD, &old, NULL) == 0);
	REQUIRE(gone);

	int code = -1;
	int err = 0;
	REQUIRE(yaft::reapOwnChild(pid, &code, &err) == yaft::ChildGone);
	REQUIRE(err == ECHILD);
	REQUIRE(code == -1);
}

TEST_CASE("no child number at all is nothing to collect", "[yaft][reap]")
{
	int code = -1;
	int err = 0;
	REQUIRE(yaft::reapOwnChild(0, &code, &err) == yaft::ChildGone);
	REQUIRE(err == ECHILD);
	REQUIRE(yaft::reapOwnChild(-1, &code, &err) == yaft::ChildGone);
}

TEST_CASE("a terminal ends once its child exited or is gone, and not before", "[yaft][reap]")
{
	REQUIRE(yaft::endsTerminal(yaft::ChildExited, true));
	REQUIRE(yaft::endsTerminal(yaft::ChildExited, false));
	REQUIRE(yaft::endsTerminal(yaft::ChildGone, true));
	REQUIRE(yaft::endsTerminal(yaft::ChildGone, false));
	REQUIRE_FALSE(yaft::endsTerminal(yaft::ChildRunning, true));
	REQUIRE_FALSE(yaft::endsTerminal(yaft::ChildRunning, false));
	// A failed wait says nothing about the child: only the pty closing ends it.
	REQUIRE_FALSE(yaft::endsTerminal(yaft::ReapFailed, true));
	REQUIRE(yaft::endsTerminal(yaft::ReapFailed, false));
}

TEST_CASE("a terminal child the kernel collected ends the terminal while a grandchild holds the pty", "[yaft][reap]")
{
	/* The shell the terminal started put a command in the background and
	   ended; the command keeps the terminal's side open. A pipe stands for
	   the pty here: its writing end is the grandchild's. The child is
	   collected by the kernel while SIGCHLD is ignored, so the terminal only
	   learns that it is gone - and has to end anyway, not wait for the
	   grandchild. */
	int pty[2];
	int hold[2];
	REQUIRE(pipe(pty) == 0);
	REQUIRE(pipe(hold) == 0);

	struct sigaction ignore, old;
	memset(&ignore, 0, sizeof(ignore));
	ignore.sa_handler = SIG_IGN;
	sigemptyset(&ignore.sa_mask);
	REQUIRE(sigaction(SIGCHLD, &ignore, &old) == 0);

	const pid_t child = fork();
	REQUIRE(child >= 0);
	if (child == 0)
	{
		if (fork() == 0)
		{
			// The grandchild: keeps the pty's writing end until let go.
			close(pty[0]);
			close(hold[1]);
			char c;
			while (read(hold[0], &c, 1) > 0)
				;
			_exit(0);
		}
		_exit(0);
	}
	close(pty[1]);
	close(hold[0]);

	bool gone = false;
	for (int i = 0; i < 500 && !gone; ++i)
	{
		if (kill(child, 0) != 0 && errno == ESRCH)
			gone = true;
		else
			pauseMs(10);
	}
	REQUIRE(sigaction(SIGCHLD, &old, NULL) == 0);
	REQUIRE(gone);

	int code = -1;
	int err = 0;
	const yaft::Reaped r = yaft::reapOwnChild(child, &code, &err);

	// The pty is still held open: no end of file in sight.
	struct pollfd p;
	p.fd = pty[0];
	p.events = POLLIN;
	p.revents = 0;
	const int ready = poll(&p, 1, 100);

	close(hold[1]);     // lets the grandchild go
	close(pty[0]);

	REQUIRE(r == yaft::ChildGone);
	REQUIRE(ready == 0);
	REQUIRE(yaft::endsTerminal(r, true));
}
