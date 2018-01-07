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
#ifdef DEBUG
/* maybe on command line? */
#undef DEBUG
#endif
#include "yaft.h"
#include "conf.h"
#include "util.h"
#include "fb/common.h"
#include "terminal.h"
#include "ctrlseq/esc.h"
#include "ctrlseq/csi.h"
#include "ctrlseq/osc.h"
#include "parse.h"
#include "yaft_class.h"
#include <driver/framebuffer.h>
#ifdef DEBUG
#warning DEBUG redefined!
#undef DEBUG
#endif

/* this will not fly with more than one instance */
static pid_t childpid;
static int exitcode;

static void sig_handler(int signo)
{
	/* global */
	extern volatile sig_atomic_t child_alive;

	logging(DEBUG, "caught signal! no:%d\n", signo);

	if (signo != SIGCHLD)
		return;

	int wstatus;
	int ret = waitpid(childpid, &wstatus, 0);
	if (ret < 0) {
		int e = errno;
		logging(ERROR, "terminal: wait for %d returned %m\n", childpid);
		if (e == ECHILD)
			return; /* don_t reset child_alive */
	}
	if (WIFEXITED(wstatus))
		exitcode = WEXITSTATUS(wstatus);
	else if (WIFSIGNALED(wstatus))
		exitcode = 128 + WTERMSIG(wstatus); /* simulate shell behaviour */
	logging(WARN, "terminal: %d exited with %d\n", childpid, exitcode);
	child_alive = false;
}

bool tty_init(void)
{
	struct sigaction sigact;

	memset(&sigact, 0, sizeof(struct sigaction));
	sigact.sa_handler = sig_handler;
	sigact.sa_flags   = SA_RESTART;
	esigaction(SIGCHLD, &sigact, NULL);
#if 0
	if (VT_CONTROL) {
		esigaction(SIGUSR1, &sigact, NULL);
		esigaction(SIGUSR2, &sigact, NULL);

		struct vt_mode vtm;
		vtm.mode   = VT_PROCESS;
		vtm.waitv  = 0;
		vtm.acqsig = SIGUSR1;
		vtm.relsig = SIGUSR2;
		vtm.frsig  = 0;

		if (ioctl(STDIN_FILENO, VT_SETMODE, &vtm))
			logging(WARN, "ioctl: VT_SETMODE failed (maybe here is not console)\n");

		if (FORCE_TEXT_MODE == false) {
			if (ioctl(STDIN_FILENO, KDSETMODE, KD_GRAPHICS))
				logging(WARN, "ioctl: KDSETMODE failed (maybe here is not console)\n");
		}
	}

	etcgetattr(STDIN_FILENO, termios_orig);
	set_rawmode(STDIN_FILENO, termios_orig);
	ewrite(STDIN_FILENO, "\033[?25l", 6); /* make cusor invisible */
#endif
	return true;
}

static const char * const *yaft_argv;
static pid_t fork_and_exec(int *master, int lines, int cols)
{
	struct winsize ws;
	ws.ws_row = lines;
	ws.ws_col = cols;
	/* XXX: this variables are UNUSED (man tty_ioctl),
		but useful for calculating terminal cell size */
	ws.ws_ypixel = CELL_HEIGHT * lines;
	ws.ws_xpixel = CELL_WIDTH * cols;

	pid_t pid;
	pid = eforkpty(master, NULL, NULL, &ws);
	if (pid == 0) { /* child */
		esetenv("TERM", term_name, 1);
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
	tv->tv_usec = SELECT_TIMEOUT;
	return eselect(master + 1, fds, NULL, NULL, tv);
}

YaFT::YaFT(const char * const *argv, int *Res, sigc::signal<void, std::string*, int*, bool*>func)
{
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
	struct framebuffer_t fb;
	struct terminal_t term;
	bool ok = true;
	/* global */
	extern volatile sig_atomic_t need_redraw;
	extern volatile sig_atomic_t child_alive;
	term.cfb = fb.info.cfb = CFrameBuffer::getInstance();
	term.txt.push("");
	term.lines_available = 0;
	term.nlseen = false;
	exitcode = 0;

	/* init */
	if (setlocale(LC_ALL, "") == NULL) /* for wcwidth() */
		logging(WARN, "setlocale falied\n");

	if (!fb_init(&fb)) {
		logging(FATAL, "framebuffer initialize failed\n");
		goto fb_init_failed;
	}

	if (!term_init(&term, fb.info.width, fb.info.height)) {
		logging(FATAL, "terminal initialize failed\n");
		goto term_init_failed;
	}

	if (!tty_init()) {
		logging(FATAL, "tty initialize failed\n");
		goto tty_init_failed;
	}

	/* fork and exec shell */
	if ((childpid = fork_and_exec(&term.fd, term.lines, term.cols)) < 0) {
		logging(FATAL, "forkpty failed\n");
		goto tty_init_failed;
	}
	child_alive = true;

	/* main loop */
	while (child_alive) {
		if (need_redraw) {
			need_redraw = false;
			redraw(&term);
			refresh(&fb, &term);
		}

		if (check_fds(&fds, &tv, STDIN_FILENO, term.fd) == -1)
			continue;
#if 0
		if (FD_ISSET(STDIN_FILENO, &fds)) {
			if ((size = read(STDIN_FILENO, buf, BUFSIZE)) > 0)
				ewrite(term.fd, buf, size);
		}
#endif
		if (FD_ISSET(term.fd, &fds)) {
			while ((size = read(term.fd, buf, BUFSIZE)) > 0) {
				if (VERBOSE)
					ewrite(STDOUT_FILENO, buf, size);
				parse(&term, buf, size);
				while (term.lines_available > 0) {
					std::string s = term.txt.front();
					OnShellOutputLoop(&s, res, &ok);
#if 0
					if (res)
						printf("[CTermWindow] [%s:%d] res=%d ok=%d\n", __func__, __LINE__, *res, ok);
					else
						printf("[CTermWindow] [%s:%d] res=NULL ok=%d\n", __func__, __LINE__, ok);
#endif
					// fprintf(stderr, "%d %s\n", term.lines_available, term.txt.front().c_str());
					term.txt.pop();
					term.lines_available--;
				}
				if (LAZY_DRAW && size == BUFSIZE)
					continue; /* maybe more data arrives soon */
				refresh(&fb, &term);
			}
		}
	}
	refresh(&fb, &term);

	/* normal exit */
	term_die(&term);
	return exitcode;

	/* error exit */
tty_init_failed:
	term_die(&term);
term_init_failed:
fb_init_failed:
	return EXIT_FAILURE;
}
