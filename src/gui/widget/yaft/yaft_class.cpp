/*
 * yaft framebuffer terminal as C++ class for embedding in neutrino-MP
 * (C) 2018 Stefan Seyfried
 * License: GPL-2.0
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * derived from yaft/yaft.c,
 * original code
 * Copyright (c) 2012 haru <uobikiemukot at gmail dot com>
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 */

#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pty.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "yaft_class.h"
#include "yaft_priv.h"
#include <driver/framebuffer.h>
#include <driver/abstime.h>

const char *term_name = "linux";

/* this will not fly with more than one instance */
static pid_t childpid;
static int exitcode;
static volatile sig_atomic_t child_alive;

static void sig_handler(int signo)
{
	/* global */
	extern volatile sig_atomic_t child_alive;

	logging(INFO, "caught signal! no:%d\n", signo);

	if (signo != SIGCHLD)
		return;

	int wstatus;
	int ret = waitpid(childpid, &wstatus, 0);
	if (ret < 0) {
		int e = errno;
		logging(NORMAL, "wait for child %d returned %m\n", childpid);
		if (e == ECHILD)
			return; /* don_t reset child_alive */
	}
	if (WIFEXITED(wstatus))
		exitcode = WEXITSTATUS(wstatus);
	else if (WIFSIGNALED(wstatus))
		exitcode = 128 + WTERMSIG(wstatus); /* simulate shell behaviour */
	logging(NORMAL, "child %d exited with %d\n", childpid, exitcode);
	child_alive = false;
}

static const char * const *yaft_argv;
static pid_t fork_and_exec(int *master, int lines, int cols)
{
	struct winsize ws;
	ws.ws_row = lines;
	ws.ws_col = cols;
	/* XXX: this variables are UNUSED (man tty_ioctl),
		but useful for calculating terminal cell size */
	ws.ws_ypixel = 0; //CELL_HEIGHT * lines;
	ws.ws_xpixel = 0; //CELL_WIDTH * cols;

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

YaFT::YaFT(const char * const *argv, int *Res, bool Paint, sigc::signal<void, std::string*, int*, bool*>func)
{
	paint = Paint;
	yaft_argv = argv;
	res = Res;
	OnShellOutputLoop = func;
}

YaFT::~YaFT(void)
{
}

int YaFT::run(void)
{
	uint8_t buf[BUFSIZE];
	ssize_t size;
	fd_set fds;
	struct timeval tv;
	bool ok = true;
	bool need_redraw = false;
	exitcode = EXIT_FAILURE;
	YaFT_p *term = new YaFT_p(paint);
	int flags;
	/* init */
	if (setlocale(LC_ALL, "") == NULL) /* for wcwidth() */
		logging(NORMAL, "setlocale falied\n");
	if (!term->init())
		goto init_failed;

	struct sigaction sigact, oldact;
	memset(&sigact, 0, sizeof(struct sigaction));
	memset(&oldact, 0, sizeof(struct sigaction));
	sigact.sa_handler = sig_handler;
	sigact.sa_flags   = SA_RESTART;
	sigaction(SIGCHLD, &sigact, &oldact);

	/* fork and exec shell */
	if ((childpid = fork_and_exec(&term->fd, term->lines, term->cols)) < 0) {
		logging(NORMAL, "forkpty failed\n");
		goto exec_failed;
	}
	exitcode = 0;
	child_alive = true;
	flags = fcntl(term->fd, F_GETFL);
	fcntl(term->fd, F_SETFL, flags | O_NONBLOCK);
# if 0
	fprintf(stderr, "forked child %d, command: '", childpid);
	const char * const *p = yaft_argv;
	while (*p) {
		fprintf(stderr, "%s ", *p);
		p++;
	}
	fprintf(stderr, "'\n");
#endif
	/* main loop */
	while (child_alive) {
		if (need_redraw) {
			need_redraw = false;
			term->refresh();
		}
		if (check_fds(&fds, &tv, STDIN_FILENO, term->fd) == -1)
			continue;
#if 0
		if (FD_ISSET(STDIN_FILENO, &fds)) {
			if ((size = read(STDIN_FILENO, buf, BUFSIZE)) > 0)
				ewrite(term.fd, buf, size);
		}
#endif
		if (FD_ISSET(term->fd, &fds)) {
			while ((size = read(term->fd, buf, BUFSIZE)) > 0) {
				term->parse(buf, size);
				while (term->txt.size() > 1) {
					std::string s = term->txt.front();
					OnShellOutputLoop(&s, res, &ok);
#if 0
					if (res)
						printf("[CTermWindow] [%s:%d] res=%d ok=%d\n", __func__, __LINE__, *res, ok);
					else
						printf("[CTermWindow] [%s:%d] res=NULL ok=%d\n", __func__, __LINE__, ok);
#endif
					// fprintf(stderr, "size %d '%s'\n", term->txt.size(), s.c_str());
					term->txt.pop();
				}
				if (! paint)
					continue;
				need_redraw = true;
				if (/*LAZY_DRAW && */size == BUFSIZE)
					continue; /* maybe more data arrives soon */
				time_t now = time_monotonic_ms();
				if (now - term->last_paint < 100)
					continue;
				term->refresh();
				need_redraw = false;
			}
		}
	}
	term->refresh();
	/* get the last partial line if there was no newline. The loop should only ever run once... */
	while (!term->txt.empty()) {
		std::string s = term->txt.front();
		OnShellOutputLoop(&s, res, &ok);
		term->txt.pop();
	}
exec_failed:
	sigaction(SIGCHLD, &oldact, NULL);
init_failed:
	delete term;
	term = NULL;
	return exitcode;
}
