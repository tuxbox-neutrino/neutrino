/*
 * streamprobe_version.h -- the tool's own version line.
 *
 * streamprobe is versioned separately from neutrino: this line moves when
 * the tool changes, not when the tree around it does. The three numbers
 * are rewritten by .github/workflows/streamprobe-version.yaml, so keep
 * them plain integers on their own lines -- no quotes, no '=' -- that is
 * what the workflow's sed keys on, and it keeps tagit's built-in schemes
 * from ever mistaking them for something else.
 *
 * A change to this file alone does not rebuild streamprobe.o in a Yocto
 * devtool workspace: that build configures with
 * --disable-dependency-tracking. Touch streamprobe.c or clean there.
 *
 * Copyright (C) 2026 Thilo Graf
 * License: GPL-2.0-or-later
 */

#ifndef STREAMPROBE_VERSION_H
#define STREAMPROBE_VERSION_H

#define STREAMPROBE_VERSION_MAJOR 1
#define STREAMPROBE_VERSION_MINOR 0
#define STREAMPROBE_VERSION_PATCH 1

/* Two levels, or the macro names end up in the string instead of their
 * values. */
#define STREAMPROBE_STR_(x) #x
#define STREAMPROBE_STR(x) STREAMPROBE_STR_(x)
#define STREAMPROBE_VERSION \
	STREAMPROBE_STR(STREAMPROBE_VERSION_MAJOR) "." \
	STREAMPROBE_STR(STREAMPROBE_VERSION_MINOR) "." \
	STREAMPROBE_STR(STREAMPROBE_VERSION_PATCH)

#endif /* STREAMPROBE_VERSION_H */
