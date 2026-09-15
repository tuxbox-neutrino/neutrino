/*
 * storage_internal.h - storage internals shared by the endpoints and the tests
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

#ifndef __coreapi_storage_internal_h__
#define __coreapi_storage_internal_h__

#include "coreapi/base/types.h"

namespace coreapi
{
namespace storage
{
namespace internal
{

// The file mounts() reads. Named here so the suite can point it at a line it
// wrote itself, which is the only way to drive the escaping a mount table uses
// for a name carrying a space.
extern const char *mounts_path;

/* The list a refresh starts from, which is what defaultRoots() answers. Named
   here so the suite can narrow it: /tmp is one of the media this build ships in
   the list, so a case that makes a directory there, sets the roots to it and
   then drives a route through a refresh would be confined to nothing at all. */
extern const char *default_roots;

// The file a refresh reads the movie browser's directories out of. Named here
// so the suite can point it at one it wrote itself, the shipped name sitting in
// the directory a box on the same machine keeps its own configuration in.
extern const char *moviebrowser_config_path;

// The number a directory puts beside a name, turned into what this layer calls
// it. Named here so the suite can drive the two kinds no directory it can make
// will ever report: the one a union filesystem uses to hide a name, and the one
// a filesystem that does not keep the kind answers with.
FileKind kindOf(unsigned char d_type);

/* Two moments a case can get between, and installed by nothing in the product.

   The first is the gap every path check in this tree admits to. A name is
   resolved, found to be inside the roots, and then opened; the directories it
   sits in are writable on a box, so between those two anything in it can be put
   back differently and the open would follow what was put there. What refuses
   that is a flag on the open, and that half is invisible from outside: it can be
   taken away and every request answers exactly as it did. So a case installs a
   probe here and does what a racer would have to win a race to do.

   The second is the moment a write is half done. The bytes go beside the file
   and take its place at the end, and from outside a write that did that and one
   that went straight at the name look identical once either has finished. A
   probe here reads the name at the one moment the two differ.

   Cleared by passing NULL. */
typedef void (*Probe)(void);
void setOpenProbeForTest(Probe p);
void setWriteProbeForTest(Probe p);

} // namespace internal
} // namespace storage
} // namespace coreapi

#endif
