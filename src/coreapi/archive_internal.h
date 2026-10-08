/*
 * archive_internal.h - archive internals the tests reach
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

#ifndef __coreapi_archive_internal_h__
#define __coreapi_archive_internal_h__

#include <string>

namespace coreapi
{
namespace archive
{
namespace internal
{

// The shipped values; a negative one puts the shipped one back.
const long kWaitMs = 2000;
const long kMaxAgeMs = 30000;
void setTimesForTest(long wait_ms, long max_age_ms);

/* Called on the worker before it touches a directory, so a case can stand in
   for a share that never answers. Cleared by passing NULL. */
typedef void (*RootProbe)(const std::string &root);
void setRootProbeForTest(RootProbe p);

/* Called on the worker once it has reached the directory and before it reads
   it, so a case can stand in for a slow first read. Cleared by passing NULL. */
void setReadingForTest(RootProbe p);

/* Whether a directory takes a removal. No case can make a medium read-only for
   a suite that runs as root, so a case answers in its place. NULL puts the
   real check back. */
typedef bool (*Writable)(const std::string &dir);
void setWritableForTest(Writable w);

/* Stands in for unlink, so a case can give the errors a share gives a suite
   that runs as root. NULL puts the real call back. */
typedef int (*Unlink)(const char *path);
void setUnlinkForTest(Unlink u);

} // namespace internal
} // namespace archive
} // namespace coreapi

#endif
