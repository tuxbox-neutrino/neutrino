/*
 * reap.h - collect the terminal's own child, and no other
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-only
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * The terminal asks after its child in its own loop rather than in a
 * SIGCHLD handler. A handler runs for the end of any child of the process,
 * on whichever thread the signal lands on: a converter or a background
 * command ending would have it wait there for the terminal's child. Asked
 * by number and without waiting, only that child is collected, and only
 * once it has ended.
 */

#ifndef __yaft_reap_h__
#define __yaft_reap_h__

#include <sys/types.h>

namespace yaft
{

enum Reaped
{
	// Still running.
	ChildRunning,
	// Collected now; *exitcode holds its status, 128 + the signal for one a
	// signal ended, the way a shell reports it.
	ChildExited,
	/* Nothing to collect under that number: a SIGCHLD the process ignored
	   at the time had the kernel collect it. It has ended; its status is
	   lost. */
	ChildGone,
	// waitpid failed otherwise; *err holds errno.
	ReapFailed
};

Reaped reapOwnChild(pid_t pid, int *exitcode, int *err);

/* Whether the terminal ends after this answer. A child that exited or is
   gone has ended, whoever else still holds the pty: what is left in it is
   read, and the terminal ends. A waitpid that failed otherwise says
   nothing about the child, so then only its side of the pty closing ends
   the terminal. */
bool endsTerminal(Reaped r, bool pty_open);

} // namespace yaft

#endif
