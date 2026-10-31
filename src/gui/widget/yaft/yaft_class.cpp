/*
 * YAFT framebuffer terminal integration for Neutrino.
 * Copyright (C) 2018 Stefan Seyfried
 *
 * Neutrino integration license: GPL-2.0-only.
 *
 * This file contains code derived from upstream yaft/yaft.c
 * (https://github.com/uobikiemukot/yaft), originally:
 * Copyright (c) 2012 haru <uobikiemukot at gmail dot com>
 *
 * Upstream yaft is MIT-licensed; see LICENSE in this directory for the
 * original permission notice and warranty disclaimer. This integration is
 * distributed with Neutrino under the GPL terms above, without warranty.
 */

#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <pty.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <locale.h>

#include "yaft_class.h"
#include "yaft_priv.h"
#include "reap.h"
#include <driver/framebuffer.h>
#include <driver/abstime.h>

const char *term_name = "linux";

/* this will not fly with more than one instance */
static int exitcode;

static const char * const *yaft_argv;
static pid_t fork_and_exec(int *master, int lines, int cols)
{
	struct winsize ws;
	ws.ws_row = lines;
	ws.ws_col = cols;
	/* XXX: these values are unused by tty_ioctl, but kept for completeness */
	ws.ws_ypixel = 0;
	ws.ws_xpixel = 0;

	pid_t pid;
	pid = forkpty(master, NULL, NULL, &ws);
	if (pid == 0) { /* child */
		for (int i = 3; i < getdtablesize(); i++)
			close(i);
		setenv("TERM", term_name, 1);
		execvp(yaft_argv[0], (char * const *)yaft_argv);
		/* never reach here */
		exit(EXIT_FAILURE);
	}
	return pid;
}

static int check_fds(fd_set *fds, struct timeval *tv, int input, int master)
{
	FD_ZERO(fds);
	if (input != STDIN_FILENO)
		FD_SET(input, fds);
	FD_SET(master, fds);
	tv->tv_sec  = 0;
	tv->tv_usec = 100000;
	return select(master + 1, fds, NULL, NULL, tv);
}

YaFT::YaFT(const char * const *argv, int *Res, bool Paint, sigc::signal<void, std::string*, int*, bool*> func)
{
	paint = Paint;
	yaft_argv = argv;
	res = Res;
	OnShellOutputLoop = func;
}

YaFT::~YaFT(void)
{
}

/* Hands what the pty holds right now to the parser. Answers false once the
   other side is closed: read() then reports the end of the file, or EIO,
   which is what Linux answers on the master of a pty whose slave side nobody
   holds open any more. */
static bool read_pty(YaFT_p *term, uint8_t *buf, bool paint, int *res,
		     sigc::signal<void, std::string*, int*, bool*> &out, bool &ok, bool &need_redraw)
{
	ssize_t size;
	while ((size = read(term->fd, buf, BUFSIZE)) > 0) {
		term->parse(buf, size);
		while (term->txt.size() > 1) {
			std::string s = term->txt.front();
			out(&s, res, &ok);
			term->txt.pop();
		}
		if (!paint)
			continue;
		need_redraw = true;
		if (size == (ssize_t) BUFSIZE)
			continue; /* maybe more data arrives soon */
		time_t now = time_monotonic_ms();
		if (now - term->last_paint < 100)
			continue;
		term->refresh();
		need_redraw = false;
	}
	if (size == 0)
		return false;
	return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
}

int YaFT::run(void)
{
	uint8_t buf[BUFSIZE];
	fd_set fds;
	struct timeval tv;
	bool ok = true;
	bool need_redraw = false;
	bool pty_open = true;
	bool child_alive = true;
	bool wait_failed = false;
	pid_t childpid;
	exitcode = EXIT_FAILURE;
	YaFT_p *term = new YaFT_p(paint);
	int flags;

	if (setlocale(LC_ALL, "") == NULL) /* for wcwidth() */
		logging(NORMAL, "setlocale failed\n");
	if (!term->init())
		goto init_failed;

	if ((childpid = fork_and_exec(&term->fd, term->lines, term->cols)) < 0) {
		logging(NORMAL, "forkpty failed\n");
		goto init_failed;
	}
	exitcode = 0;
	flags = fcntl(term->fd, F_GETFL);
	fcntl(term->fd, F_SETFL, flags | O_NONBLOCK);

	/* The child is asked after once per turn, by its number and without
	   waiting, so the end of another child of this process is never taken
	   for its end, nor collected here. A turn lasts at most the 100 ms the
	   select waits. */
	while (child_alive) {
		if (need_redraw) {
			need_redraw = false;
			term->refresh();
		}
		if (pty_open) {
			if (check_fds(&fds, &tv, STDIN_FILENO, term->fd) > 0 && FD_ISSET(term->fd, &fds))
				pty_open = read_pty(term, buf, paint, res, OnShellOutputLoop, ok, need_redraw);
		} else
			usleep(100000); /* a closed pty reads ready at once; do not spin on it */

		int code = 0;
		int err = 0;
		const yaft::Reaped reaped = yaft::reapOwnChild(childpid, &code, &err);
		switch (reaped) {
		case yaft::ChildRunning:
			break;
		case yaft::ChildExited:
			exitcode = code;
			logging(NORMAL, "child %d exited with %d\n", childpid, exitcode);
			break;
		case yaft::ChildGone:
			/* The kernel collected it while SIGCHLD was ignored: it has
			   ended, and its status is gone, so the run counts as failed. */
			exitcode = EXIT_FAILURE;
			logging(NORMAL, "child %d was collected elsewhere, its status is lost\n", childpid);
			break;
		case yaft::ReapFailed:
			if (!wait_failed) {
				logging(NORMAL, "wait for child %d: %s, reading until the terminal closes\n", childpid, strerror(err));
				wait_failed = true;
			}
			exitcode = EXIT_FAILURE;
			break;
		}
		child_alive = !yaft::endsTerminal(reaped, pty_open);
	}
	/* What the child wrote last can still sit in the pty after it ended. */
	if (pty_open)
		read_pty(term, buf, paint, res, OnShellOutputLoop, ok, need_redraw);
	term->refresh();

	/*
	 * txt always keeps one open line while parsing. Flush it here so command
	 * output without a trailing newline is still delivered to the caller.
	 */
	while (!term->txt.empty()) {
		std::string s = term->txt.front();
		OnShellOutputLoop(&s, res, &ok);
		term->txt.pop();
	}
init_failed:
	delete term;
	term = NULL;
	return exitcode;
}
