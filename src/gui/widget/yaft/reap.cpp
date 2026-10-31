/*
 * reap.cpp - collect the terminal's own child, and no other
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
 */

#include "reap.h"

#include <errno.h>
#include <sys/wait.h>

namespace yaft
{

Reaped reapOwnChild(pid_t pid, int *exitcode, int *err)
{
	if (pid <= 0)
	{
		if (err)
			*err = ECHILD;
		return ChildGone;
	}

	int status = 0;
	const pid_t got = waitpid(pid, &status, WNOHANG);
	if (got == pid)
	{
		if (exitcode)
		{
			if (WIFEXITED(status))
				*exitcode = WEXITSTATUS(status);
			else if (WIFSIGNALED(status))
				*exitcode = 128 + WTERMSIG(status);
		}
		return ChildExited;
	}
	if (got == 0)
		return ChildRunning;

	const int e = errno;
	// Interrupted before it looked: the next turn of the loop asks again.
	if (e == EINTR)
		return ChildRunning;
	if (err)
		*err = e;
	return (e == ECHILD) ? ChildGone : ReapFailed;
}

bool endsTerminal(Reaped r, bool pty_open)
{
	switch (r)
	{
	case ChildExited:
	case ChildGone:
		return true;
	case ReapFailed:
		return !pty_open;
	case ChildRunning:
	default:
		return false;
	}
}

} // namespace yaft
